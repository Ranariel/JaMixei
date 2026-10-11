#pragma once
#include <JuceHeader.h>

namespace mp
{
// Decodifica formatos extras pelo Windows (M4A, AAC, MP4, WMA, WEBM/Opus com extensoes).
// Retorna false (com mensagem) em outras plataformas ou se o Windows nao suportar o arquivo.
bool decodeWithMediaFoundation (const juce::File& file, juce::AudioBuffer<float>& out, double& sampleRate, juce::String& error);
}
