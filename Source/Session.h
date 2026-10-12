#pragma once
#include <JuceHeader.h>
#include "Model/Project.h"
#include "Audio/SourcePool.h"
#include "Audio/Snapshot.h"
#include "Audio/AudioEngine.h"
#include <set>
#include <map>

namespace mp
{
enum ChangeFlags
{
    kChTracks   = 1 << 0,
    kChClips    = 1 << 1,
    kChPads     = 1 << 2,
    kChPatterns = 1 << 3,
    kChTransport= 1 << 4,
    kChSources  = 1 << 5,
    kChMixer    = 1 << 6,
    kChSelection= 1 << 7,
    kChAll      = 0xffff
};

struct ExportSettings
{
    juce::File file;
    double sampleRate = 44100;
    int bitDepth = 24;
    int format = 0;          // 0 WAV, 1 FLAC, 2 OGG, 3 MP3
    int mp3Quality = 0;      // 0 = 320, 1 = 256, 2 = 192, 3 = 128 kbps
    bool onlyLoop = false;
    int patternRepeats = 0;    // > 0: exporta o padrao atual (modo PADRAO) repetido N vezes
    double tailSeconds = 2.0;
    bool importAfter = false;  // "resample": importa o resultado como nova fonte
    int padTarget = -1;        // resample direto para um pad
};

class Session : private juce::Timer
{
public:
    Session();
    ~Session() override;

    Project project;
    UndoHistory history;
    SourcePool sources;
    std::unique_ptr<StretchCache> stretch;
    PadBufferCache padCache;
    instruments::InstrumentBank bank;      // multisamples dos instrumentos (aba Instrumentos)
    AudioEngine engine;
    juce::ApplicationProperties props;

    struct Listener { virtual ~Listener() = default; virtual void sessionChanged (int what) = 0; };
    void addListener (Listener* l) { listeners.add (l); }
    void removeListener (Listener* l) { listeners.remove (l); }

    // --- edicao ---
    void beginEdit (const juce::String& label);
    void commit (int what = kChAll);
    void undo();
    void redo();
    bool isDirty() const { return dirty; }

    // --- estado de selecao/UI ---
    int selectedPad = 0;
    int currentBank = 0;
    std::set<int> selectedClips;
    int selectedTrackId = -1;
    int editorSourceId = -1;
    double snapBeats = 0.25;
    bool songMode = true;
    bool recording = false;
    juce::String status;
    float uiMasterPeakL = 0, uiMasterPeakR = 0;   // atualizado a 30 Hz
    void setStatus (const juce::String& s) { status = s; statusTime = juce::Time::getMillisecondCounter(); }

    // --- fontes ---
    int importFile (const juce::File& f, std::function<void (int sourceId)> onReady = {});
    SourcePtr getSource (int id) const { return sources.get (id); }
    juce::String sourceName (int id) const;
    void reloadAllSources();
    bool isLoading() const { return sources.numLoading() > 0; }

    // --- clipes ---
    int addAudioClip (int trackId, int sourceId, double beat, double offSec = 0, double lenSec = -1, bool adoptBpm = true);
    int addPatternClip (int trackId, int patternId, double beat);
    void deleteSelectedClips();
    void duplicateSelectedClips();
    void splitAt (double beat);              // divide clipes selecionados (ou todos sob o cursor)
    void splitClip (int clipId, double beat);
    void copySelection();
    void paste (double atBeat, int trackId);
    bool hasClipboard() const { return ! clipboard.empty(); }
    void setClipWarp (Clip& c, bool warp);
    void normaliseClip (int clipId);
    int trackIdAt (int index) const;

    // --- faixas ---
    int addTrack (TrackType t);
    void deleteTrack (int trackId);
    void moveTrack (int trackId, int delta);

    // --- pads ---
    void assignPad (int pad, int sourceId, double s, double e, const juce::String& name);
    // gera o instrumento (piano, synth...) e coloca uma nota em cada pad do banco
    void loadInstrument (int instrument, int rootMidi, int scale, int bank, std::function<void (bool ok)> done = {});
    void clearPad (int pad);
    void padDown (int pad, uint8_t vel = 120);
    void padUp (int pad);
    int chopToPads (int sourceId, const std::vector<double>& points, double endSec, int firstPad);

    // --- instrumentos (channel rack + piano roll) ---
    int addChannel (int instrument);                 // devolve o id do canal novo (e seleciona)
    void removeChannel (int channelId);
    void setChannelInstrument (int channelId, int instrument);
    InstrumentChannel* currentChannel() { return project.findChannel (project.currentChannelId); }
    void selectChannel (int channelId);
    void noteDown (int pitch, uint8_t vel = 100);    // teclado da tela / PC: toca o canal atual
    void noteUp (int pitch);
    double noteQuantize = 0.25;                      // grade da gravacao de notas (tempos); 0 = livre
    static float defaultRelease (int instrument);

    // --- padroes ---
    Pattern* currentPattern() { return project.findPattern (project.currentPatternId); }
    int newPattern (bool duplicateCurrent);
    // muda o tamanho do padrao atual; ao aumentar repete os passos dos pads e estica os clipes do arranjo
    void setPatternLength (int steps);
    void deletePattern (int id);

    // --- transporte ---
    void togglePlay();
    void stopTransport();
    void setSongMode (bool song);
    void toggleRecord();
    void seek (double beat) { engine.seek (beat); notify (kChTransport); }

    // --- gravacao de audio ---
    struct AudioRecOptions { bool fromInput = false; bool mp3 = false; bool monitor = false; int bits = 24; };
    bool startAudioRecording (const AudioRecOptions& o, juce::String& error);
    void stopAudioRecording();
    bool isAudioRecording() const { return const_cast<AudioEngine&> (engine).isAudioRecording(); }
    static juce::File recordingsFolder();
    static juce::File dataFolder();      // Documentos\Ja Mixei

    // --- arquivo ---
    void newProject();
    bool openProject (const juce::File& f);
    bool saveProject (const juce::File& f, bool collectAudio);
    juce::File lastFolder() const;
    void setLastFolder (const juce::File& f);
    void addRecentFile (const juce::File& f);
    juce::StringArray recentFiles() const;

    // --- exportacao (bloqueante, chamada por thread de trabalho) ---
    bool renderToFile (const ExportSettings& s, std::function<bool (float)> progress, juce::String& error);
    SnapshotPtr buildSnapshot (double sampleRate);

    void notify (int what);
    void rebuildSnapshot();

private:
    void timerCallback() override;
    void loadSourceInfo (SourceInfo& s);

    juce::ListenerList<Listener> listeners;
    std::vector<SnapshotPtr> graveyard;
    std::vector<Clip> clipboard;
    double clipboardOrigin = 0;
    bool dirty = false;
    juce::uint32 statusTime = 0;
    bool recordUndoPushed = false;
    std::map<std::pair<int, int>, RecordedNote> pendingNotes;   // (canal, nota) -> inicio gravado
    int midiScanCountdown = 0;
    void finishRecordedNote (const RecordedNote& on, double endAbs);
    AudioRecOptions recOptions;
    float inputPeakHold = 0;
    bool savedLoopOn = false;
    std::shared_ptr<bool> aliveFlag = std::make_shared<bool> (true);
};

} // namespace mp
