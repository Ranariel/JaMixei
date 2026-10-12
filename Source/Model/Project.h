#pragma once
#include <JuceHeader.h>
#include <vector>
#include <array>

// ============================================================================
//  MeuPad Studio - modelo de dados do projeto
//  Tudo aqui vive na thread de mensagens (UI). O motor de audio recebe
//  "snapshots" imutaveis construidos a partir deste modelo.
// ============================================================================

namespace mp
{
constexpr int kPadsPerBank = 16;
constexpr int kNumBanks    = 4;
constexpr int kNumPads     = kPadsPerBank * kNumBanks;
constexpr int kStepsPerBeat = 4;
constexpr int kMaxSteps    = 64;
constexpr int kMaxTracks   = 48;

enum class TrackType { Audio = 0, Pattern = 1 };
enum class ClipKind  { Audio = 0, Pattern = 1 };
enum class PadMode   { OneShot = 0, Gate = 1, Loop = 2 };
enum class AutoParam { None = 0, Volume = 1, Filter = 2 };

struct AutoPoint
{
    double beat = 0;
    float value = 0;  // Volume: 0..1 (multiplicador, 0.8 = 0 dB nominal?) ver Track
};

struct Track
{
    int id = 0;
    juce::String name;
    TrackType type = TrackType::Audio;
    juce::Colour colour { 0xff4fc3f7 };
    float volumeDb = 0.0f;
    float pan = 0.0f;           // -1..1
    bool mute = false, solo = false;
    float eqLow = 0, eqMid = 0, eqHigh = 0;   // dB  (-15..+15)
    float filter = 0.0f;        // -1 (LPF) .. 0 (off) .. +1 (HPF)
    float filterRes = 0.2f;     // 0..1
    float reverbSend = 0.0f;    // 0..1
    float delaySend = 0.0f;     // 0..1
    AutoParam autoView = AutoParam::None;
    std::vector<AutoPoint> volAuto;     // valor 0..1 -> ganho linear 0..1 (1 = sem atenuacao)
    std::vector<AutoPoint> filterAuto;  // valor -1..1 (substitui o knob de filtro quando ha pontos)
    float height = 84.0f;
};

struct Clip
{
    int id = 0;
    int trackId = 0;
    ClipKind kind = ClipKind::Audio;
    double startBeat = 0;
    juce::String name;
    juce::Colour colour { 0xff4fc3f7 };
    bool muted = false;

    // --- audio ---
    int sourceId = -1;
    double sourceOffsetSec = 0;  // inicio do trecho usado no audio fonte
    double sourceLenSec = 0;     // duracao do trecho usado (em segundos de audio fonte)
    float gainDb = 0;
    double fadeInBeats = 0, fadeOutBeats = 0;
    bool warp = false;           // sincroniza com o BPM do projeto
    double sourceBpm = 120;
    float semitones = 0;
    bool keepPitch = true;       // true = time-stretch de qualidade (Rubber Band); false = varispeed (estilo vinil)
    bool reverse = false;

    // --- padrao ---
    int patternId = -1;
    double lengthBeatsPattern = 4;
    double patternOffsetBeats = 0;
};

struct Pad
{
    int sourceId = -1;
    double startSec = 0, endSec = 0;
    juce::String name;
    float gainDb = 0;
    float pan = 0;
    float semitones = 0;
    float fineCents = 0;
    float attackMs = 1;
    float releaseMs = 40;
    PadMode mode = PadMode::OneShot;
    int chokeGroup = 0;    // 0 = nenhum
    bool reverse = false;
    float cutoff = 20000.0f;
    float resonance = 0.1f;
    juce::Colour colour { 0xffff8a3d };

    bool isEmpty() const { return sourceId < 0 || endSec <= startSec; }
};

// nota do piano roll (instrumentos): posicao e duracao em tempos (beats) dentro do padrao
struct Note
{
    int channelId = 0;
    int pitch = 60;            // MIDI (60 = C4, do central)
    double start = 0;          // tempos desde o inicio do padrao
    double length = 0.25;      // tempos
    uint8_t vel = 100;
    bool operator== (const Note& o) const
    {
        return channelId == o.channelId && pitch == o.pitch && start == o.start && length == o.length && vel == o.vel;
    }
};

// canal de instrumento ("channel rack"): piano, synth, baixo...
struct InstrumentChannel
{
    int id = 0;
    juce::String name;
    int instrument = 0;        // instruments::Id
    float volumeDb = 0.0f;
    float pan = 0.0f;
    float attackMs = 2.0f;
    float releaseMs = 250.0f;
    int transpose = 0;         // semitons
    bool mute = false;
    juce::Colour colour { 0xff4fc3f7 };
};

struct Pattern
{
    int id = 0;
    juce::String name;
    int lengthSteps = 16;
    std::vector<uint8_t> vel;  // kNumPads * kMaxSteps, indice [pad * kMaxSteps + step]
    std::vector<Note> notes;   // notas dos instrumentos (piano roll)

    Pattern() { vel.assign ((size_t) kNumPads * kMaxSteps, 0); }
    uint8_t get (int pad, int step) const { return vel[(size_t) pad * kMaxSteps + (size_t) step]; }
    void set (int pad, int step, uint8_t v) { vel[(size_t) pad * kMaxSteps + (size_t) step] = v; }
    double lengthBeats() const { return lengthSteps / (double) kStepsPerBeat; }
};

struct SourceInfo
{
    int id = 0;
    juce::String path;
    juce::String relPath;   // relativo ao arquivo do projeto
    juce::String name;
    double bpm = 120;
    bool bpmDetected = false;
};

class Project
{
public:
    Project() { reset(); }

    // -- estado global --
    double bpm = 120.0;
    float swing = 50.0f;           // 50..75 (%)
    int beatsPerBar = 4;
    bool loopOn = false;
    double loopStart = 0, loopEnd = 16;
    bool metronome = false;
    float masterVolDb = 0;
    float reverbSize = 0.6f, reverbDamp = 0.4f, reverbWidth = 1.0f;
    double delayBeats = 0.75;      // 1/8 pontuada
    float delayFeedback = 0.35f;
    float delayTone = 0.6f;
    int currentPatternId = -1;
    int liveTrackId = -1;          // faixa de padrao que recebe os pads tocados ao vivo
    int currentChannelId = -1;     // instrumento selecionado na aba Instrumentos

    std::vector<SourceInfo> sources;
    std::vector<Track> tracks;
    std::vector<Clip> clips;
    std::vector<Pattern> patterns;
    std::vector<InstrumentChannel> channels;
    std::array<Pad, kNumPads> pads;

    juce::File projectFile;

    int newId() { return nextId++; }

    void reset();

    // -- consultas --
    Track* findTrack (int id);
    const Track* findTrack (int id) const;
    int trackIndex (int id) const;
    Clip* findClip (int id);
    Pattern* findPattern (int id);
    const Pattern* findPattern (int id) const;
    InstrumentChannel* findChannel (int id);
    const InstrumentChannel* findChannel (int id) const;
    SourceInfo* findSource (int id);
    const SourceInfo* findSource (int id) const;

    // tempo
    double secondsToBeats (double s) const { return s * bpm / 60.0; }
    double beatsToSeconds (double b) const { return b * 60.0 / bpm; }

    // taxa de consumo do audio fonte por segundo de linha do tempo
    double clipRate (const Clip& c) const;
    double clipLengthBeats (const Clip& c) const;
    double clipEndBeat (const Clip& c) const { return c.startBeat + clipLengthBeats (c); }
    double songEndBeat() const;

    // -- serializacao --
    juce::var toVar() const;
    void fromVar (const juce::var& v);
    juce::String toJson() const;
    bool fromJson (const juce::String& json);

    Track& addTrack (TrackType type, const juce::String& name);
    Pattern& addPattern (const juce::String& name);
    InstrumentChannel& addChannel (int instrument, const juce::String& name);

    static juce::Colour trackColour (int index);

private:
    int nextId = 1;
};

// ============================================================================
//  Desfazer / Refazer baseado em snapshots JSON do projeto
// ============================================================================
class UndoHistory
{
public:
    void clear() { undoStack.clear(); redoStack.clear(); }
    void push (const juce::String& state, const juce::String& label)
    {
        undoStack.push_back ({ state, label });
        if (undoStack.size() > 200) undoStack.erase (undoStack.begin());
        redoStack.clear();
    }
    bool canUndo() const { return ! undoStack.empty(); }
    bool canRedo() const { return ! redoStack.empty(); }
    juce::String undo (const juce::String& current)
    {
        auto e = undoStack.back(); undoStack.pop_back();
        redoStack.push_back ({ current, e.label });
        return e.state;
    }
    juce::String redo (const juce::String& current)
    {
        auto e = redoStack.back(); redoStack.pop_back();
        undoStack.push_back ({ current, e.label });
        return e.state;
    }
    juce::String undoLabel() const { return undoStack.empty() ? juce::String() : undoStack.back().label; }
    juce::String redoLabel() const { return redoStack.empty() ? juce::String() : redoStack.back().label; }
private:
    struct Entry { juce::String state, label; };
    std::vector<Entry> undoStack, redoStack;
};

} // namespace mp
