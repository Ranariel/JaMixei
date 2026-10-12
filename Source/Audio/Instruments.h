#pragma once
#include <JuceHeader.h>

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

    // gera um buffer estereo (44,1 kHz) de uma nota
    juce::AudioBuffer<float> renderNote (Id id, int midi, double sampleRate);

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
