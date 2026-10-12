#pragma once
#include <JuceHeader.h>
#include "../Model/Project.h"
#include "SourcePool.h"
#include "Instruments.h"
#include <array>

namespace mp
{
// ============================================================================
//  Snapshot imutavel do projeto, lido pela thread de audio
// ============================================================================
struct SnapTrack
{
    int id = 0;
    bool isPattern = false;
    float gain = 1.0f;          // volume linear (ja considera mute/solo)
    float pan = 0;
    float eqLow = 0, eqMid = 0, eqHigh = 0;
    float filter = 0, filterRes = 0.2f;
    float reverbSend = 0, delaySend = 0;
    std::vector<AutoPoint> volAuto, filterAuto;
};

struct SnapClip
{
    int trackSlot = 0;
    bool isPattern = false;
    double startBeat = 0, endBeat = 0;
    // audio
    BufferPtr data;
    double dataSR = 44100;
    double varispeed = 1.0;      // fator extra de leitura (quando nao ha buffer esticado)
    double dStart = 0, lenFrames = 0;
    bool reverse = false;
    float gain = 1.0f;
    double fadeInBeats = 0, fadeOutBeats = 0;
    // padrao
    int patternIndex = -1;
    double patternOffsetBeats = 0;
};

struct SnapPad
{
    BufferPtr buf;
    double bufSR = 44100;
    float gain = 1, pan = 0;
    double pitchRatio = 1;
    float attackMs = 1, releaseMs = 40;
    PadMode mode = PadMode::OneShot;
    int choke = 0;
    float cutoff = 20000, resonance = 0.1f;
};

struct SnapNote
{
    int channel = 0;            // indice em Snapshot::channels
    int pitch = 60;
    double start = 0, length = 0.25;
    uint8_t vel = 100;
};

struct SnapChannel
{
    int id = 0;
    instruments::MultiPtr multi;    // nullptr enquanto o instrumento e gerado
    float gainL = 1, gainR = 1;
    float attackMs = 2, releaseMs = 250;
    int transpose = 0;
    bool mono = false;
};

struct SnapPattern
{
    int id = 0;
    int length = 16;
    std::vector<uint8_t> vel;
    std::vector<uint8_t> len;       // duracao em passos (0/1 = um passo)
    std::vector<SnapNote> notes;    // ordenadas pelo inicio
    uint8_t get (int pad, int step) const { return vel[(size_t) pad * kMaxSteps + (size_t) step]; }
    int getLen (int pad, int step) const { return len.empty() ? 1 : juce::jmax (1, (int) len[(size_t) pad * kMaxSteps + (size_t) step]); }
};

struct Snapshot
{
    double sampleRate = 44100;
    double bpm = 120;
    float swing = 50;
    int beatsPerBar = 4;
    bool loopOn = false;
    double loopStart = 0, loopEnd = 16;
    bool metronome = false;
    float masterGain = 1;
    float reverbSize = 0.6f, reverbDamp = 0.4f, reverbWidth = 1.0f;
    double delayBeats = 0.75;
    float delayFeedback = 0.35f, delayTone = 0.6f;
    std::vector<SnapTrack> tracks;
    std::vector<SnapClip> clips;
    std::array<SnapPad, kNumPads> pads;
    std::vector<SnapPattern> patterns;
    int currentPatternIndex = 0;
    int liveTrackSlot = 0;
    double songEndBeat = 0;
    std::vector<SnapChannel> channels;
    int currentChannel = -1;        // instrumento tocado ao vivo (teclado / MIDI)
    int pendingStretches = 0;

    double samplesPerBeat() const { return sampleRate * 60.0 / bpm; }
};
using SnapshotPtr = std::shared_ptr<const Snapshot>;

// Prepara buffers dos pads (fatias copiadas) e reutiliza entre snapshots
class PadBufferCache
{
public:
    BufferPtr getForPad (int padIndex, const Pad& pad, const SourcePtr& src);
    void clear() { for (auto& e : entries) e = {}; }
private:
    struct Entry { int sourceId = -1; double s = 0, e = 0; bool rev = false; const void* srcPtr = nullptr; BufferPtr buf; };
    std::array<Entry, kNumPads> entries;
};

struct SnapshotBuilder
{
    static SnapshotPtr build (const Project& p, SourcePool& sources, StretchCache* stretch,
                              PadBufferCache& padCache, double sampleRate,
                              const instruments::InstrumentBank* bank = nullptr);
};

} // namespace mp
