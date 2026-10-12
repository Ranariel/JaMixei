#pragma once
#include <JuceHeader.h>
#include <map>
#include <set>
#include <thread>

namespace mp
{
// ============================================================================
//  Instrumentos embutidos (piano, synths, baixos...). Cada instrumento e
//  sintetizado no proprio programa e gravado como um WAV com uma nota por
//  trecho; cada pad do banco aponta para uma nota (estilo "keygroup" da MPC).
// ============================================================================
namespace instruments
{
    enum class Id
    {
        Piano = 0, ElectricPiano, Organ, SynthLead, SynthPad, Pluck,
        Bass808, SynthBass, Strings, Brass, Bells, Marimba,
        count
    };

    struct Info
    {
        Id id;
        const char* namePt;      // nome (traduzido com TR)
        int defaultOctave;       // oitava da primeira nota (C4 = do central)
        bool sustained;          // true = modo Gate (segura enquanto o pad esta pressionado)
        float noteSeconds;       // duracao de cada nota no arquivo
        juce::uint32 colour;
    };

    const Info& info (Id id);
    juce::String displayName (Id id);       // nome traduzido

    enum class Scale { Chromatic = 0, Major, Minor, MinorPentatonic, Blues, count };
    juce::String scaleName (Scale s);
    // 16 notas MIDI a partir da tonica (midi), seguindo a escala
    std::vector<int> scaleNotes (int rootMidi, Scale s, int count = 16);
    juce::String noteName (int midi);       // ex.: "C#3"

    // gera um buffer estereo de uma nota (seconds <= 0: duracao padrao; hold: sustenta sem release)
    juce::AudioBuffer<float> renderNote (Id id, int midi, double sampleRate, double seconds = -1.0, bool hold = false);
    bool isMono (Id id);       // baixos: uma nota por vez (legato)

    // multisample usado pela aba Instrumentos (todo o teclado)
    struct MultiSample
    {
        struct Zone { int root = 60; int start = 0, end = 0; int loopStart = -1, loopEnd = -1; };
        std::shared_ptr<juce::AudioBuffer<float>> buffer;
        double sampleRate = 44100;
        bool sustained = false;
        std::vector<Zone> zones;
        const Zone* zoneFor (int pitch) const;
    };
    using MultiPtr = std::shared_ptr<const MultiSample>;
    MultiPtr renderMulti (Id id, std::function<bool (float)> progress = {});

    // gera os multisamples em segundo plano e guarda na memoria
    class InstrumentBank
    {
    public:
        InstrumentBank();
        ~InstrumentBank();
        MultiPtr get (Id id) const;          // nullptr enquanto nao estiver pronto
        void request (Id id);                // comeca a gerar (se ainda nao existe)
        bool isLoading (Id id) const;
        std::function<void (Id)> onLoaded;   // thread de mensagens
    private:
        juce::CriticalSection lock;
        std::map<int, MultiPtr> loaded;
        std::set<int> pending;
        juce::ThreadPool pool;
        std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
    };

    struct Rendered
    {
        juce::File file;
        std::vector<std::pair<double, double>> regions;   // inicio/fim (s) de cada nota no arquivo
        std::vector<int> notes;
    };
    // gera (ou reaproveita, se ja existir) o WAV com as 16 notas
    bool renderToFile (Id id, const std::vector<int>& notes, const juce::File& folder, Rendered& out,
                       std::function<bool (float)> progress = {});
}
} // namespace mp
