#pragma once
#include <JuceHeader.h>
#include <vector>

namespace mp
{
// Envelope de "onset" (fluxo espectral) usado para BPM e deteccao de transientes
struct OnsetEnvelope
{
    std::vector<float> values;
    double frameRate = 0;   // quadros de envelope por segundo
    int hop = 512;
};

namespace Analysis
{
    OnsetEnvelope computeOnsetEnvelope (const juce::AudioBuffer<float>& buf, double sampleRate,
                                        double startSec = 0, double maxSeconds = 1.0e9, bool lowWeighted = false);

    // Retorna o BPM estimado (0 se nao conseguir). Faixa preferida 80..170.
    double detectBpm (const juce::AudioBuffer<float>& buf, double sampleRate);

    // Fase da batida: retorna o tempo (s) da primeira batida forte estimada
    double detectBeatOffset (const juce::AudioBuffer<float>& buf, double sampleRate, double bpm);

    // Transientes entre [startSec, endSec). sensitivity 0..1 (1 = mais fatias)
    std::vector<double> detectTransients (const juce::AudioBuffer<float>& buf, double sampleRate,
                                          double startSec, double endSec, float sensitivity,
                                          double minGapSec = 0.07);

    // Pico absoluto na regiao
    float peakLevel (const juce::AudioBuffer<float>& buf, int start, int num);

    // Converte um tempo para o cruzamento por zero mais proximo (melhora cortes)
    double snapToZeroCrossing (const juce::AudioBuffer<float>& buf, double sampleRate, double sec, double windowSec = 0.004);
}
} // namespace mp
