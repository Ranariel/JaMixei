#include "Widgets.h"

namespace mp
{
void drawWaveform (juce::Graphics& g, const LoadedSource& src, juce::Rectangle<float> r, double startSec, double endSec,
                   juce::Colour colour, bool reverse, float gain)
{
    if (! src.ready || src.buffer == nullptr || r.getWidth() < 1 || endSec <= startSec) return;
    const auto clip = g.getClipBounds().toFloat().getIntersection (r);
    if (clip.isEmpty()) return;

    const double sr = src.sampleRate;
    const double secPerPx = (endSec - startSec) / r.getWidth();
    const float midY = r.getCentreY();
    const float halfH = r.getHeight() * 0.5f * 0.95f;
    const auto& buf = *src.buffer;
    const int frames = buf.getNumSamples();
    const int blocks = (int) src.peakMax.size();
    const double framesPerPx = secPerPx * sr;

    juce::RectangleList<float> rects;
    const int x0 = (int) std::floor (clip.getX()), x1 = (int) std::ceil (clip.getRight());
    for (int x = x0; x < x1; ++x)
    {
        double px = x - r.getX();
        if (reverse) px = r.getWidth() - 1 - px;
        const double t0 = startSec + px * secPerPx;
        const double t1 = t0 + secPerPx;
        float mn = 0, mx = 0;
        if (framesPerPx >= LoadedSource::kPeakBlock)
        {
            int b0 = juce::jlimit (0, blocks, (int) (t0 * sr / LoadedSource::kPeakBlock));
            int b1 = juce::jlimit (b0 + 1, blocks, (int) std::ceil (t1 * sr / LoadedSource::kPeakBlock));
            if (b0 >= blocks) continue;
            for (int b = b0; b < b1; ++b) { mn = juce::jmin (mn, src.peakMin[(size_t) b]); mx = juce::jmax (mx, src.peakMax[(size_t) b]); }
        }
        else
        {
            int f0 = juce::jlimit (0, frames, (int) std::floor (t0 * sr));
            int f1 = juce::jlimit (f0, frames, (int) std::ceil (t1 * sr) + 1);
            if (f0 >= frames) continue;
            for (int c = 0; c < buf.getNumChannels(); ++c)
            {
                auto* d = buf.getReadPointer (c);
                for (int i = f0; i < f1; ++i) { mn = juce::jmin (mn, d[i]); mx = juce::jmax (mx, d[i]); }
            }
        }
        mn = juce::jlimit (-1.0f, 1.0f, mn * gain);
        mx = juce::jlimit (-1.0f, 1.0f, mx * gain);
        const float top = midY - mx * halfH, bot = midY - mn * halfH;
        rects.addWithoutMerging ({ (float) x, top, 1.0f, juce::jmax (1.0f, bot - top) });
    }
    g.setColour (colour);
    g.fillRectList (rects);
}

} // namespace mp
