#include "Analysis.h"
#include <cmath>
#include <numeric>

namespace mp::Analysis
{

OnsetEnvelope computeOnsetEnvelope (const juce::AudioBuffer<float>& buf, double sampleRate, double startSec, double maxSeconds, bool lowWeighted)
{
    OnsetEnvelope env;
    constexpr int order = 10;            // FFT 1024
    constexpr int N = 1 << order;
    // hop ~ 5.8 ms @ 44.1k, independente da taxa (escala aproximada)
    const int hop = juce::jmax (128, (int) std::round (sampleRate / 172.0));
    env.hop = hop;
    env.frameRate = sampleRate / hop;

    const int total = buf.getNumSamples();
    const int s0 = juce::jlimit (0, total, (int) (startSec * sampleRate));
    const int s1 = (int) juce::jmin ((double) total, s0 + maxSeconds * sampleRate);
    if (s1 - s0 < N) return env;

    juce::dsp::FFT fft (order);
    std::vector<float> window ((size_t) N), frame ((size_t) N * 2), prevMag ((size_t) N / 2 + 1, 0.0f), weight ((size_t) N / 2 + 1, 1.0f);
    if (lowWeighted)
        for (int k = 0; k <= N / 2; ++k) weight[(size_t) k] = 1.0f / (1.0f + k / 16.0f);
    for (int i = 0; i < N; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi * i / (N - 1));

    const int nch = buf.getNumChannels();
    const float chScale = 1.0f / (float) juce::jmax (1, nch);
    const int numFrames = (s1 - s0 - N) / hop + 1;
    env.values.reserve ((size_t) numFrames);

    // quadro anterior ao inicio (se existir) para nao gerar um "ataque" falso no comeco da regiao
    for (int f = (s0 >= hop ? -1 : 0); f < numFrames; ++f)
    {
        const int base = s0 + f * hop;
        std::fill (frame.begin(), frame.end(), 0.0f);
        for (int i = 0; i < N; ++i)
        {
            float v = 0;
            for (int c = 0; c < nch; ++c) v += buf.getReadPointer (c)[base + i];
            frame[(size_t) i] = v * chScale * window[(size_t) i];
        }
        fft.performFrequencyOnlyForwardTransform (frame.data(), true);
        float flux = 0;
        for (int k = 1; k <= N / 2; ++k)
        {
            float m = std::log1p (100.0f * frame[(size_t) k]);
            float d = m - prevMag[(size_t) k];
            if (d > 0) flux += d * weight[(size_t) k];
            prevMag[(size_t) k] = m;
        }
        if (f >= 0) env.values.push_back (flux);
    }
    return env;
}

static void normaliseEnvelope (std::vector<float>& v, int avgWin)
{
    // remove media movel (destaca picos) e retifica
    std::vector<float> out (v.size());
    double acc = 0;
    std::vector<double> prefix (v.size() + 1, 0.0);
    for (size_t i = 0; i < v.size(); ++i) prefix[i + 1] = prefix[i] + v[i];
    for (size_t i = 0; i < v.size(); ++i)
    {
        size_t a = i >= (size_t) avgWin ? i - (size_t) avgWin : 0;
        size_t b = std::min (v.size(), i + (size_t) avgWin + 1);
        double mean = (prefix[b] - prefix[a]) / (double) (b - a);
        out[i] = (float) std::max (0.0, v[i] - mean);
    }
    juce::ignoreUnused (acc);
    v.swap (out);
}

double detectBpm (const juce::AudioBuffer<float>& buf, double sampleRate)
{
    const double totalSec = buf.getNumSamples() / sampleRate;
    if (totalSec < 3.0) return 0;
    // analisa ate 120 s a partir de 10% da musica (evita intros silenciosas)
    double start = totalSec > 40 ? totalSec * 0.1 : 0.0;
    auto env = computeOnsetEnvelope (buf, sampleRate, start, 120.0);
    if (env.values.size() < 200) return 0;
    normaliseEnvelope (env.values, (int) (env.frameRate * 0.25));

    const double fr = env.frameRate;
    const int n = (int) env.values.size();
    const int maxLag = juce::jmin (n - 1, (int) (fr * 60.0 / 50.0 * 4.0) + 2);

    // autocorrelacao
    std::vector<double> ac ((size_t) maxLag + 1, 0.0);
    for (int lag = 1; lag <= maxLag; ++lag)
    {
        double s = 0;
        const float* x = env.values.data();
        for (int i = lag; i < n; ++i) s += (double) x[i] * x[i - lag];
        ac[(size_t) lag] = s / (n - lag);
    }
    auto acAt = [&] (double lag) -> double
    {
        if (lag < 1 || lag >= maxLag - 1) return 0.0;
        int i = (int) lag; double f = lag - i;
        return ac[(size_t) i] * (1 - f) + ac[(size_t) i + 1] * f;
    };

    double bestBpm = 0, bestScore = -1;
    std::vector<std::pair<double,double>> scores;
    for (double bpm = 60.0; bpm <= 200.0; bpm += 0.05)
    {
        const double lag = fr * 60.0 / bpm;
        double s = 0;
        // pulsos multiplos da batida (comb)
        s += acAt (lag) * 1.0;
        s += acAt (lag * 2) * 0.6;
        s += acAt (lag * 4) * 0.4;
        s += acAt (lag * 0.5) * 0.25;
        // preferencia perceptual por volta de 120 BPM
        const double o = std::log2 (bpm / 118.0) / 0.9;
        s *= std::exp (-0.5 * o * o);
        scores.push_back ({ bpm, s });
        if (s > bestScore) { bestScore = s; bestBpm = bpm; }
    }
    if (bestBpm <= 0) return 0;

    // refinamento fino em volta do melhor candidato
    double refined = bestBpm, refinedScore = -1;
    for (double bpm = bestBpm - 0.5; bpm <= bestBpm + 0.5; bpm += 0.01)
    {
        const double lag = fr * 60.0 / bpm;
        double s = acAt (lag) + 0.6 * acAt (lag * 2) + 0.4 * acAt (lag * 4) + 0.3 * acAt (lag * 8);
        if (s > refinedScore) { refinedScore = s; refined = bpm; }
    }
    // dobra para faixa util
    while (refined < 78) refined *= 2;
    while (refined > 175) refined *= 0.5;
    // arredonda para 0.01; se muito perto de inteiro, usa o inteiro
    double rounded = std::round (refined);
    if (std::abs (rounded - refined) < 0.12) refined = rounded;
    return std::round (refined * 100.0) / 100.0;
}

double detectBeatOffset (const juce::AudioBuffer<float>& buf, double sampleRate, double bpm)
{
    if (bpm <= 0) return 0;
    auto env = computeOnsetEnvelope (buf, sampleRate, 0, 60.0);
    if (env.values.empty()) return 0;
    const double period = env.frameRate * 60.0 / bpm;
    const int steps = juce::jmax (1, (int) period);
    double best = 0, bestScore = -1;
    for (int ph = 0; ph < steps; ++ph)
    {
        double s = 0;
        for (double p = ph; p < (double) env.values.size(); p += period) s += env.values[(size_t) p];
        if (s > bestScore) { bestScore = s; best = ph; }
    }
    double t = (best * env.hop) / sampleRate;
    return t;
}

std::vector<double> detectTransients (const juce::AudioBuffer<float>& buf, double sampleRate,
                                      double startSec, double endSec, float sensitivity, double minGapSec)
{
    std::vector<double> result;
    endSec = juce::jmin (endSec, buf.getNumSamples() / sampleRate);
    if (endSec - startSec < 0.05) return result;
    auto env = computeOnsetEnvelope (buf, sampleRate, startSec, endSec - startSec, true);
    auto& v = env.values;
    if (v.size() < 4) return result;
    constexpr int fftN = 1024;

    // limiar adaptativo: mediana local + k * desvio
    const int win = juce::jmax (4, (int) (env.frameRate * 0.4));
    const float k = juce::jmap (juce::jlimit (0.0f, 1.0f, sensitivity), 2.6f, 0.25f);
    float globalMax = *std::max_element (v.begin(), v.end());
    if (globalMax <= 0) return result;
    const float floorThr = globalMax * juce::jmap (sensitivity, 0.35f, 0.04f);
    const int minGap = juce::jmax (1, (int) (minGapSec * env.frameRate));
    int last = -minGap * 2;

    std::vector<double> prefix (v.size() + 1, 0.0), prefix2 (v.size() + 1, 0.0);
    for (size_t i = 0; i < v.size(); ++i) { prefix[i + 1] = prefix[i] + v[i]; prefix2[i + 1] = prefix2[i] + (double) v[i] * v[i]; }

    for (int i = 0; i + 1 < (int) v.size(); ++i)
    {
        if (! ((i == 0 || v[(size_t) i] >= v[(size_t) i - 1]) && v[(size_t) i] > v[(size_t) i + 1])) continue;
        int a = juce::jmax (0, i - win), b = juce::jmin ((int) v.size(), i + win + 1);
        double cnt = b - a;
        double mean = (prefix[(size_t) b] - prefix[(size_t) a]) / cnt;
        double var = (prefix2[(size_t) b] - prefix2[(size_t) a]) / cnt - mean * mean;
        double thr = mean + k * std::sqrt (std::max (0.0, var));
        if (v[(size_t) i] < thr || v[(size_t) i] < floorThr) continue;
        if (i - last < minGap) continue;
        last = i;
        // refina no dominio do tempo: maior subida de energia dentro da janela da FFT
        const int frameStart = (int) std::round (startSec * sampleRate) + i * env.hop;
        const int blk = 32;
        const int searchEnd = juce::jmin (buf.getNumSamples() - blk, frameStart + fftN);
        double bestRise = -1.0; int bestPos = frameStart + fftN / 2;
        double prevE = -1.0;
        for (int pos = juce::jmax (0, frameStart); pos < searchEnd; pos += blk)
        {
            double e = 0;
            for (int c = 0; c < buf.getNumChannels(); ++c)
            {
                auto* d = buf.getReadPointer (c) + pos;
                for (int j = 0; j < blk; ++j) e += (double) d[j] * d[j];
            }
            if (prevE >= 0 && e - prevE > bestRise) { bestRise = e - prevE; bestPos = pos; }
            prevE = e;
        }
        double t = juce::jmax (startSec, (bestPos - blk) / sampleRate);
        if (! result.empty() && t - result.back() < minGapSec) continue;
        result.push_back (t);
    }
    return result;
}

float peakLevel (const juce::AudioBuffer<float>& buf, int start, int num)
{
    float p = 0;
    start = juce::jlimit (0, buf.getNumSamples(), start);
    num = juce::jlimit (0, buf.getNumSamples() - start, num);
    for (int c = 0; c < buf.getNumChannels(); ++c)
        p = juce::jmax (p, buf.getMagnitude (c, start, num));
    return p;
}

double snapToZeroCrossing (const juce::AudioBuffer<float>& buf, double sampleRate, double sec, double windowSec)
{
    const int n = buf.getNumSamples();
    int centre = (int) std::round (sec * sampleRate);
    int w = (int) (windowSec * sampleRate);
    if (n < 2 || buf.getNumChannels() == 0) return sec;
    auto mono = [&] (int i) { float s = 0; for (int c = 0; c < buf.getNumChannels(); ++c) s += buf.getReadPointer (c)[i]; return s; };
    int best = centre;
    for (int d = 0; d <= w; ++d)
        for (int sgn : { -1, 1 })
        {
            int i = centre + d * sgn;
            if (i < 1 || i >= n) continue;
            float a = mono (i - 1), b = mono (i);
            if ((a <= 0 && b >= 0) || (a >= 0 && b <= 0))
                return i / sampleRate;   // cruzamento mais proximo
        }
    return best / sampleRate;
}

} // namespace mp::Analysis
