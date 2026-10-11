#include "Mp3Format.h"
#include <lame.h>

namespace mp
{
int Mp3AudioFormat::bitrateForQuality (int index)
{
    static const int rates[] = { 320, 256, 192, 128 };
    return rates[juce::jlimit (0, 3, index)];
}

Mp3AudioFormat::Mp3AudioFormat() : juce::AudioFormat ("MP3", ".mp3") {}

namespace
{
class LameWriter : public juce::AudioFormatWriter
{
public:
    LameWriter (juce::OutputStream* out, double sr, unsigned int numCh, int kbps, const juce::StringPairArray& meta)
        : juce::AudioFormatWriter (out, "MP3", sr, juce::jlimit (1u, 2u, numCh), 32)
    {
        usesFloatingPointData = true;
        lame = lame_init();
        if (lame == nullptr) return;
        lame_set_in_samplerate (lame, (int) sr);
        lame_set_out_samplerate (lame, (int) sr);
        lame_set_num_channels (lame, (int) numChannels);
        lame_set_mode (lame, numChannels == 1 ? MONO : JOINT_STEREO);
        lame_set_brate (lame, kbps);
        lame_set_quality (lame, 2);            // alta qualidade (0 = melhor/mais lento, 9 = pior)
        lame_set_bWriteVbrTag (lame, 0);
        id3tag_init (lame);
        id3tag_add_v2 (lame);
        const auto title = meta.getValue ("title", {});
        if (title.isNotEmpty()) id3tag_set_title (lame, title.toRawUTF8());
        id3tag_set_comment (lame, "Ja Mixei");
        ok = lame_init_params (lame) >= 0;
        mp3buf.resize (16384);
    }

    ~LameWriter() override
    {
        if (lame != nullptr)
        {
            if (ok)
            {
                const int n = lame_encode_flush (lame, mp3buf.data(), (int) mp3buf.size());
                if (n > 0 && output != nullptr) output->write (mp3buf.data(), (size_t) n);
            }
            lame_close (lame);
        }
        if (output != nullptr) output->flush();
    }

    bool isValid() const { return lame != nullptr && ok; }
    void releaseStream() { output = nullptr; }

    bool write (const int** samples, int numSamples) override
    {
        if (! isValid() || numSamples <= 0) return false;
        const float* left = reinterpret_cast<const float*> (samples[0]);
        const float* right = (numChannels > 1 && samples[1] != nullptr) ? reinterpret_cast<const float*> (samples[1]) : left;
        const size_t needed = (size_t) (1.25 * numSamples + 7200);
        if (mp3buf.size() < needed) mp3buf.resize (needed);
        int n;
        if (numChannels > 1)
            n = lame_encode_buffer_ieee_float (lame, left, right, numSamples, mp3buf.data(), (int) mp3buf.size());
        else
            n = lame_encode_buffer_ieee_float (lame, left, left, numSamples, mp3buf.data(), (int) mp3buf.size());
        if (n < 0) return false;
        if (n > 0) return output->write (mp3buf.data(), (size_t) n);
        return true;
    }

private:
    lame_global_flags* lame = nullptr;
    bool ok = false;
    std::vector<unsigned char> mp3buf;
};
} // namespace

juce::AudioFormatWriter* Mp3AudioFormat::createWriterFor (juce::OutputStream* out, double sampleRate, unsigned int numChannels,
                                                          int, const juce::StringPairArray& metadata, int qualityOptionIndex)
{
    if (out == nullptr || numChannels == 0) return nullptr;
    auto w = std::make_unique<LameWriter> (out, sampleRate, numChannels, bitrateForQuality (qualityOptionIndex), metadata);
    if (! w->isValid())
    {
        w->releaseStream();   // o chamador continua dono do stream em caso de falha
        return nullptr;
    }
    return w.release();
}

} // namespace mp
