#pragma once
#include <JuceHeader.h>

namespace mp
{
// Formato MP3 (somente escrita) usando o encoder LAME 3.100 embutido.
// qualityOptionIndex escolhe a taxa: 0 = 320, 1 = 256, 2 = 192, 3 = 128 kbps (CBR).
class Mp3AudioFormat : public juce::AudioFormat
{
public:
    Mp3AudioFormat();
    juce::Array<int> getPossibleSampleRates() override { return { 32000, 44100, 48000 }; }
    juce::Array<int> getPossibleBitDepths() override { return { 16, 24, 32 }; }
    bool canDoStereo() override { return true; }
    bool canDoMono() override { return true; }
    bool isCompressed() override { return true; }
    juce::StringArray getQualityOptions() override { return { "320 kbps", "256 kbps", "192 kbps", "128 kbps" }; }
    juce::AudioFormatReader* createReaderFor (juce::InputStream*, bool) override { return nullptr; }
    juce::AudioFormatWriter* createWriterFor (juce::OutputStream* out, double sampleRate, unsigned int numChannels,
                                              int bitsPerSample, const juce::StringPairArray& metadata,
                                              int qualityOptionIndex) override;
    using juce::AudioFormat::createWriterFor;

    static int bitrateForQuality (int index);
};
} // namespace mp
