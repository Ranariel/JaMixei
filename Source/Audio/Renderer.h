#pragma once
#include <JuceHeader.h>
#include "Snapshot.h"

namespace mp
{
struct PadEvent
{
    int pad = 0;
    uint8_t vel = 120;
    bool on = true;
};

struct NoteEvent
{
    int channel = -1;           // -1 = canal atual (Snapshot::currentChannel)
    int pitch = 60;
    uint8_t vel = 100;
    bool on = true;
};

// nota tocada ao vivo durante a gravacao (inicio e fim chegam separados)
struct RecordedNote
{
    int patternId = 0;
    int channelId = 0;
    int pitch = 60;
    double beat = 0;            // posicao dentro do padrao
    double absBeat = 0;         // posicao absoluta (para calcular a duracao)
    uint8_t vel = 0;
    bool on = true;
};

struct RecordedHit
{
    int patternId = 0;
    int pad = 0;
    int step = 0;
    uint8_t vel = 0;
};

// Fila simples lock-free (1 produtor / 1 consumidor)
template <typename T, int N>
class SpscQueue
{
public:
    bool push (const T& v)
    {
        int s1, n1, s2, n2;
        fifo.prepareToWrite (1, s1, n1, s2, n2);
        if (n1 + n2 < 1) return false;
        data[(size_t) (n1 > 0 ? s1 : s2)] = v;
        fifo.finishedWrite (1);
        return true;
    }
    bool pop (T& v)
    {
        int s1, n1, s2, n2;
        fifo.prepareToRead (1, s1, n1, s2, n2);
        if (n1 + n2 < 1) return false;
        v = data[(size_t) (n1 > 0 ? s1 : s2)];
        fifo.finishedRead (1);
        return true;
    }
private:
    juce::AbstractFifo fifo { N };
    std::array<T, (size_t) N> data;
};

// ============================================================================
//  Renderer: todo o DSP (sampler, linha do tempo, mixer, efeitos, master).
//  Usado em tempo real pelo AudioEngine e offline pela exportacao.
// ============================================================================
class Renderer
{
public:
    static constexpr int kMaxVoices = 64;
    static constexpr int kChunk = 256;

    Renderer();
    void prepare (double sampleRate, int maxBlock);
    void resetDsp();

    // Renderiza numSamples na saida (sobrescreve)
    void process (const Snapshot& snap, juce::AudioBuffer<float>& out, int startSample, int numSamples);

    // --- transporte (somente na thread de audio / dono) ---
    double posBeats = 0;
    bool playing = false;
    bool patternMode = false;
    bool recording = false;
    bool repeatSong = false;   // modo MUSICA sem loop: no fim volta ao comeco (o motor ao vivo liga; exportacao nao)

    void padOn (const Snapshot& snap, int pad, uint8_t vel, int slot, int offset, int gateSamples, bool fromLive);
    void padOff (int pad);
    void allNotesOff (bool hard);

    // --- instrumentos (piano roll / teclado) ---
    void noteOn (const Snapshot& snap, int channel, int pitch, uint8_t vel, int slot, int offset, int gateSamples, bool live);
    void noteOff (const Snapshot& snap, int channel, int pitch);   // so notas tocadas ao vivo
    SpscQueue<RecordedNote, 1024> recordedNotes;
    // teclas soando no instrumento atual (para acender o teclado na tela): bit = nota MIDI
    std::atomic<juce::uint64> soundingLo { 0 }, soundingHi { 0 };

    // gravacao de pads (consumido pela UI)
    SpscQueue<RecordedHit, 512> recordedHits;

    // medidores (picos lineares)
    std::array<std::atomic<float>, kMaxTracks> trackPeak;
    std::atomic<float> masterPeakL { 0 }, masterPeakR { 0 };
    std::array<std::atomic<int>, kNumPads> padActivity;   // contador de disparos (para piscar pads)
    std::atomic<int> activeVoices { 0 };

    // pre-escuta do editor de sample
    struct Preview
    {
        BufferPtr buf; double sr = 44100; double pos = 0, end = 0; bool active = false; float gain = 0.9f;
        bool loop = false; double loopStart = 0;   // repetir o trecho (em amostras da fonte)
    };
    juce::SpinLock previewLock;
    Preview pendingPreview; bool previewChanged = false;
    std::atomic<double> previewPosSec { -1.0 };

private:
    struct Voice
    {
        bool active = false;
        int pad = -1, slot = 0, choke = 0;
        BufferPtr buf;
        int frames = 0, channels = 1;
        double pos = 0, inc = 1;
        PadMode mode = PadMode::OneShot;
        float gL = 1, gR = 1;
        int delay = 0;
        int gateRemaining = -1;
        int stage = 0; // 0 ataque, 1 sustentacao, 2 release
        float env = 0, attackInc = 1, releaseCoef = 0.99f;
        bool useFilter = false;
        float fg = 0, fk = 0, fa1 = 0, fa2 = 0, fa3 = 0;
        float ic1[2] { 0, 0 }, ic2[2] { 0, 0 };
        juce::uint64 age = 0;
        void startRelease (float ms, double sr);
    };

    struct Svf
    {
        float ic1[2] { 0, 0 }, ic2[2] { 0, 0 };
        void reset() { ic1[0] = ic1[1] = ic2[0] = ic2[1] = 0; }
    };

    struct TrackDsp
    {
        juce::IIRFilter eq[3][2];
        float eqVal[3] { 0, 0, 0 };
        bool eqActive[3] { false, false, false };
        Svf svf;
        float lastFilter = 0;
        float lastGain = 0;
        float lastPanL = 1, lastPanR = 1;
        bool initialised = false;
    };

    void renderChunk (const Snapshot& snap, float* L, float* R, int n);
    void scheduleSegment (const Snapshot& snap, int off, int len, double b0);
    void triggerPatternSteps (const Snapshot& snap, const SnapPattern& pat, int slot, double origin,
                              double clipStart, double clipEnd, int off, int len, double b0);
    void renderAudioClip (const Snapshot& snap, const SnapClip& c, int off, int len, double b0);
    void renderVoices (int n);
    void processTrack (const Snapshot& snap, int slot, int n, float* masterL, float* masterR);
    void processMaster (const Snapshot& snap, float* L, float* R, int n);
    static float evalAuto (const std::vector<AutoPoint>& pts, double beat, float def);
    void recordLiveHit (const Snapshot& snap, int pad, uint8_t vel);
    bool livePatternPosition (const Snapshot& snap, const SnapPattern*& pat, double& local) const;
    void triggerPatternNotes (const Snapshot& snap, const SnapPattern& pat, int slot, double origin,
                              double clipStart, double clipEnd, int off, int len, double b0);
    void renderNoteVoices (const Snapshot& snap, int n);

    struct NoteVoice
    {
        bool active = false;
        int channel = -1, pitch = 60, slot = 0;
        bool live = false;
        instruments::MultiPtr multi;   // mantem o buffer vivo enquanto a voz toca
        const float* d0 = nullptr; const float* d1 = nullptr;
        int zoneStart = 0, zoneEnd = 0, loopStart = -1, loopEnd = -1;
        double pos = 0, inc = 1;
        float gL = 1, gR = 1;
        int delay = 0;
        int gateRemaining = -1;      // amostras ate soltar a nota (sequenciada); -1 = segura ate noteOff
        int stage = 0;               // 0 ataque, 1 sustentacao, 2 release
        float env = 0, attackInc = 1, releaseCoef = 0.999f;
        juce::uint64 age = 0;
    };
    static constexpr int kMaxNoteVoices = 96;
    std::array<NoteVoice, kMaxNoteVoices> noteVoices;

    double sr = 44100;
    int maxBlock = 512;
    juce::AudioBuffer<float> bus;        // kMaxTracks * 2 canais
    juce::AudioBuffer<float> fxBus;      // 0-1 reverb, 2-3 delay
    std::array<TrackDsp, kMaxTracks> tracks;
    std::array<Voice, kMaxVoices> voices;
    juce::uint64 voiceCounter = 0;
    bool busUsed[kMaxTracks] {};

    juce::Reverb reverb;
    juce::Reverb::Parameters lastRevParams;
    std::vector<float> delayL, delayR;
    int delayWrite = 0;
    double delayCurrent = 0;
    float delayLpL = 0, delayLpR = 0;

    juce::dsp::Limiter<float> limiter;
    float masterGainLast = 1;

    // metronomo
    double clickPhase = 0, clickFreq = 1000;
    int clickRemaining = 0, clickDelay = 0;
    float clickAmp = 0;

    Preview preview;
};

} // namespace mp
