#pragma once
#include <JuceHeader.h>
#include "Renderer.h"

namespace mp
{
// ============================================================================
//  AudioEngine: dispositivo de audio + ponte thread-safe com o Renderer
// ============================================================================
class AudioEngine : public juce::AudioIODeviceCallback, private juce::MidiInputCallback
{
public:
    AudioEngine();
    ~AudioEngine() override;

    void initialise (const juce::XmlElement* savedState);
    void shutdown();
    juce::AudioDeviceManager& devices() { return deviceManager; }
    double getSampleRate() const { return sampleRate.load(); }

    void setSnapshot (SnapshotPtr s);

    // --- transporte (thread de mensagens) ---
    void play();
    void stop();
    void togglePlay() { if (isPlaying()) stop(); else play(); }
    void seek (double beat);
    void setPatternMode (bool b) { patternModeReq = b; }
    void setRecording (bool b) { recordingReq = b; }
    bool isPlaying() const { return uiPlaying.load(); }
    double getPositionBeats() const { return uiPos.load(); }

    // --- pads ao vivo ---
    void padDown (int pad, uint8_t vel = 120) { padEvents.push ({ pad, vel, true }); }
    void padUp (int pad) { padEvents.push ({ pad, 0, false }); }
    void panic() { panicReq = true; }

    // --- instrumentos ao vivo (teclado da tela / PC) ---
    void noteDown (int pitch, uint8_t vel = 100, int channel = -1) { noteEvents.push ({ channel, pitch, vel, true }); }
    void noteUp (int pitch, int channel = -1) { noteEvents.push ({ channel, pitch, 0, false }); }
    juce::uint64 soundingLo() const { return renderer.soundingLo.load(); }
    juce::uint64 soundingHi() const { return renderer.soundingHi.load(); }

    // --- controladores MIDI (teclado, controlador de pads) ---
    void enableAllMidiInputs();                    // liga os dispositivos MIDI conectados (chamar de tempos em tempos)
    juce::StringArray midiInputNames() const;
    std::atomic<bool> midiToInstrument { false };  // true: notas MIDI tocam o instrumento; false: tocam os pads
    std::atomic<int> midiActivity { 0 };

    // --- pre-escuta ---
    void previewBuffer (BufferPtr buf, double sr, double startSec, double endSec, bool loop = false, double loopStartSec = -1);
    bool isPreviewing() const { return renderer.previewPosSec.load() >= 0.0; }
    void stopPreview();
    double getPreviewPositionSec() const { return renderer.previewPosSec.load(); }
    bool previewFile (const juce::File& f, juce::AudioFormatManager& fm);
    void stopFilePreview();
    bool isFilePreviewing() const;

    // --- gravacao de audio (entrada ou a propria saida) ---
    enum class RecSource { Input = 0, Output = 1 };
    struct RecordingInfo
    {
        juce::File file;
        double startBeat = 0;
        double latencySec = 0;
        double sampleRate = 44100;
        bool fromInput = false;
        bool gotAudio = false;
    };
    bool startAudioRecording (const juce::File& file, RecSource source, bool mp3, bool monitorInput, juce::String& error, int bits = 24);
    RecordingInfo stopAudioRecording();
    bool isAudioRecording() const { return audioRecording.load(); }
    double getRecordedSeconds() const { return recSamples.load() / juce::jmax (1.0, sampleRate.load()); }
    float readInputPeak() { return inputPeak.exchange (0.0f); }
    bool setInputEnabled (bool enabled, juce::String& error);
    bool hasInputDevice();

    Renderer& getRenderer() { return renderer; }
    float getCpu() const { return (float) deviceManager.getCpuUsage(); }

    std::function<void()> onSampleRateChanged; // chamado na thread de mensagens

    // AudioIODeviceCallback
    void audioDeviceIOCallbackWithContext (const float* const* in, int numIn, float* const* out, int numOut,
                                           int numSamples, const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m) override;

private:
    juce::AudioDeviceManager deviceManager;
    Renderer renderer;

    juce::SpinLock snapLock;
    SnapshotPtr pendingSnap;
    SnapshotPtr current;

    SpscQueue<PadEvent, 1024> padEvents;
    SpscQueue<NoteEvent, 1024> noteEvents;            // UI -> audio
    SpscQueue<NoteEvent, 1024> midiNoteEvents;        // thread MIDI -> audio
    SpscQueue<PadEvent, 1024> midiPadEvents;          // thread MIDI -> audio
    std::atomic<bool> sustainPedal { false };
    bool sustainApplied = false;
    std::array<bool, 128> heldBySustain {};
    juce::StringArray enabledMidi;
    std::atomic<int> playReq { -1 };
    std::atomic<bool> seekReq { false };
    std::atomic<double> seekPos { 0.0 };
    std::atomic<bool> patternModeReq { false }, recordingReq { false }, panicReq { false };
    std::atomic<double> uiPos { 0.0 };
    std::atomic<bool> uiPlaying { false };
    std::atomic<double> sampleRate { 44100.0 };

    // pre-escuta de arquivo (navegador)
    juce::TimeSliceThread previewThread { "MeuPad preview" };
    juce::SpinLock filePreviewLock;
    std::unique_ptr<juce::AudioTransportSource> filePreview;
    std::unique_ptr<juce::AudioFormatReaderSource> filePreviewReader;
    std::vector<std::unique_ptr<juce::AudioTransportSource>> oldPreviews;
    std::vector<std::unique_ptr<juce::AudioFormatReaderSource>> oldReaders;
    juce::AudioBuffer<float> previewTmp;

    // gravacao
    juce::TimeSliceThread recThread { "MeuPad gravacao" };
    juce::SpinLock recLock;
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> recWriter;
    std::atomic<bool> audioRecording { false };
    std::atomic<bool> recGotFirst { false };
    std::atomic<double> recStartBeat { 0.0 };
    std::atomic<juce::int64> recSamples { 0 };
    std::atomic<float> inputPeak { 0.0f };
    RecSource recSource = RecSource::Output;
    bool recMonitor = false;
    juce::File recFile;
    juce::AudioBuffer<float> inputTmp;
    int blockSize = 512;
};

} // namespace mp
