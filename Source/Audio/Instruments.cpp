#include "Instruments.h"
#include "../I18n.h"

namespace mp
{
namespace instruments
{
namespace
{
    constexpr double twoPi = juce::MathConstants<double>::twoPi;

    const Info table[] = {
        { Id::Piano,         "Piano",            3, false, 3.6f, 0xfff5f0e6 },
        { Id::ElectricPiano, "Piano elétrico",   3, false, 3.2f, 0xffffb86b },
        { Id::Organ,         "Órgão",            3, true,  3.0f, 0xffd98c4a },
        { Id::SynthLead,     "Synth lead",       4, true,  2.6f, 0xff4fc3f7 },
        { Id::SynthPad,      "Synth pad",        3, true,  4.0f, 0xffb388ff },
        { Id::Pluck,         "Pluck",            4, false, 2.0f, 0xff6fdc8c },
        { Id::Bass808,       "Baixo 808",        1, false, 2.6f, 0xffff5c5c },
        { Id::SynthBass,     "Baixo synth",      2, true,  1.8f, 0xffff8a3d },
        { Id::Strings,       "Cordas",           3, true,  4.0f, 0xffffd166 },
        { Id::Brass,         "Metais",           3, true,  2.2f, 0xffffc94a },
        { Id::Bells,         "Sinos",            4, false, 4.0f, 0xff80deea },
        { Id::Marimba,       "Marimba",          4, false, 1.6f, 0xffbcaaa4 },
    };

    double mtof (double m) { return 440.0 * std::pow (2.0, (m - 69.0) / 12.0); }

    // dente de serra sem aliasing (PolyBLEP)
    struct Saw
    {
        double phase = 0;
        static double blep (double t, double dt)
        {
            if (t < dt) { t /= dt; return t + t - t * t - 1.0; }
            if (t > 1.0 - dt) { t = (t - 1.0) / dt; return t * t + t + t + 1.0; }
            return 0.0;
        }
        double next (double freq, double sr)
        {
            const double dt = juce::jlimit (1.0e-6, 0.5, freq / sr);
            double v = 2.0 * phase - 1.0 - blep (phase, dt);
            phase += dt;
            if (phase >= 1.0) phase -= 1.0;
            return v;
        }
    };

    // filtro de estado variavel (TPT), passa-baixa
    struct Svf
    {
        double ic1 = 0, ic2 = 0;
        double lowpass (double x, double fc, double q, double sr)
        {
            fc = juce::jlimit (20.0, sr * 0.45, fc);
            const double g = std::tan (juce::MathConstants<double>::pi * fc / sr);
            const double k = 1.0 / juce::jmax (0.5, q);
            const double a1 = 1.0 / (1.0 + g * (g + k)), a2 = g * a1, a3 = g * a2;
            const double v3 = x - ic2;
            const double v1 = a1 * ic1 + a2 * v3;
            const double v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0 * v1 - ic1;
            ic2 = 2.0 * v2 - ic2;
            return v2;
        }
    };

    // envelope ADSR simples para notas renderizadas (a nota "solta" em releaseAt)
    double adsr (double t, double a, double d, double s, double releaseAt, double r)
    {
        auto level = [&] (double tt)
        {
            if (tt < a) return tt / juce::jmax (1.0e-4, a);
            tt -= a;
            if (tt < d) return 1.0 - (1.0 - s) * (tt / juce::jmax (1.0e-4, d));
            return s;
        };
        if (t < releaseAt) return level (t);
        const double l = level (releaseAt);
        return l * std::exp (-(t - releaseAt) / juce::jmax (1.0e-3, r) * 4.0);
    }

    struct Rng
    {
        juce::uint32 s;
        explicit Rng (juce::uint32 seed) : s (seed ? seed : 1u) {}
        double next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (s / 4294967295.0) * 2.0 - 1.0; }
    };

    void fadeEdges (juce::AudioBuffer<float>& b, double sr, double fadeOutSec)
    {
        const int n = b.getNumSamples();
        const int fin = juce::jmin (n, (int) (sr * 0.0015));
        const int fout = juce::jmin (n, (int) (sr * fadeOutSec));
        for (int c = 0; c < b.getNumChannels(); ++c)
        {
            b.applyGainRamp (c, 0, fin, 0.0f, 1.0f);
            b.applyGainRamp (c, n - fout, fout, 1.0f, 0.0f);
        }
    }

    // ---------------------------------------------------------------- instrumentos
    void piano (juce::AudioBuffer<float>& out, int midi, double sr)
    {
        const int n = out.getNumSamples();
        auto* L = out.getWritePointer (0); auto* R = out.getWritePointer (1);
        const double f0 = mtof (midi);
        const double B = 0.00012 * std::pow (2.0, (midi - 60) / 18.0);           // inarmonicidade das cordas
        const double t0 = 7.0 * std::pow (2.0, -(midi - 48) / 20.0);              // graves soam mais tempo
        const int maxP = juce::jlimit (1, 40, (int) (sr * 0.45 / f0));
        for (int p = 1; p <= maxP; ++p)
        {
            const double fp = p * f0 * std::sqrt (1.0 + B * p * p);
            if (fp > sr * 0.45) break;
            const double amp = std::pow ((double) p, -1.15) * (p == 1 ? 1.0 : 0.85) * std::exp (-0.012 * p * (midi > 72 ? 2.0 : 1.0));
            const double T = t0 / (1.0 + 0.45 * (p - 1));
            // duas cordas levemente desafinadas -> batimento natural, uma em cada lado
            for (int s = 0; s < 2; ++s)
            {
                const double det = (s == 0 ? -0.7 : 0.7) * fp / 1731.0;   // ~0,7 cent
                const double w = twoPi * (fp + det) / sr;
                double ph = p * 0.37 + s;
                float* dst = s == 0 ? L : R;
                const double kFast = std::exp (-1.0 / (sr * T * 0.22)), kSlow = std::exp (-1.0 / (sr * T));
                double eF = 0.72 * amp, eS = 0.28 * amp;
                for (int i = 0; i < n; ++i)
                {
                    dst[i] += (float) ((eF + eS) * std::sin (ph));
                    ph += w;
                    eF *= kFast; eS *= kSlow;
                }
            }
        }
        // martelo: rajada curta de ruido filtrado
        Rng rng ((juce::uint32) midi * 7919u);
        Svf f;
        const int hn = juce::jmin (n, (int) (sr * 0.012));
        for (int i = 0; i < hn; ++i)
        {
            const double e = std::exp (-i / (sr * 0.0025));
            const double v = f.lowpass (rng.next(), f0 * 6.0, 0.7, sr) * 0.25 * e;
            L[i] += (float) v; R[i] += (float) v;
        }
        const double atk = sr * 0.002;
        for (int i = 0; i < juce::jmin (n, (int) atk); ++i) { L[i] *= (float) (i / atk); R[i] *= (float) (i / atk); }
    }

    void electricPiano (juce::AudioBuffer<float>& out, int midi, double sr)
    {
        const int n = out.getNumSamples();
        auto* L = out.getWritePointer (0); auto* R = out.getWritePointer (1);
        const double f0 = mtof (midi);
        double pc = 0, pm = 0, pt = 0;
        const double dec = 2.6 * std::pow (2.0, -(midi - 60) / 24.0);
        for (int i = 0; i < n; ++i)
        {
            const double t = i / sr;
            const double idx = 1.8 * std::exp (-t / 0.35) + 0.35;
            const double tine = 0.9 * std::exp (-t / 0.03);
            const double mod = std::sin (pm) * idx + std::sin (pt) * tine;
            const double env = std::exp (-t / dec) * (1.0 - std::exp (-t / 0.001));
            const double v = std::sin (pc + mod) * env * 0.7;
            const double trem = 0.12 * std::sin (twoPi * 4.2 * t);
            L[i] = (float) (v * (1.0 + trem));
            R[i] = (float) (v * (1.0 - trem));
            pc += twoPi * f0 / sr; pm += twoPi * f0 / sr; pt += twoPi * f0 * 14.0 / sr;
        }
    }

    void organ (juce::AudioBuffer<float>& out, int midi, double sr, double noteLen)
    {
        const int n = out.getNumSamples();
        auto* L = out.getWritePointer (0); auto* R = out.getWritePointer (1);
        const double f0 = mtof (midi);
        const double ratios[] = { 0.5, 1.0, 1.5, 2.0, 3.0, 4.0 };
        const double amps[]   = { 0.55, 1.0, 0.45, 0.5, 0.22, 0.18 };
        double ph[6] = {};
        for (int i = 0; i < n; ++i)
        {
            const double t = i / sr;
            const double vib = 1.0 + 0.0025 * std::sin (twoPi * 6.4 * t);
            double v = 0;
            for (int k = 0; k < 6; ++k)
            {
                if (f0 * ratios[k] > sr * 0.45) continue;
                v += amps[k] * std::sin (ph[k]);
                ph[k] += twoPi * f0 * ratios[k] * vib / sr;
            }
            v += 0.35 * std::sin (ph[1] * 3.0) * std::exp (-t / 0.18);           // percussao
            const double env = adsr (t, 0.004, 0.05, 0.92, noteLen - 0.25, 0.2);
            const double pan = 0.18 * std::sin (twoPi * 0.8 * t);                 // leslie lento
            v *= env * 0.28;
            L[i] = (float) (v * (1.0 + pan));
            R[i] = (float) (v * (1.0 - pan));
        }
    }

    void sawStack (juce::AudioBuffer<float>& out, int midi, double sr, int voices, double spreadCents,
                   double a, double d, double s, double releaseAt, double r,
                   std::function<double (double t, double f0)> cutoff, double q, double vibratoDepthSemis,
                   juce::uint32 seed, double subOctaveMix = 0.0)   // subOctaveMix: seno na fundamental
    {
        const int n = out.getNumSamples();
        auto* L = out.getWritePointer (0); auto* R = out.getWritePointer (1);
        const double f0 = mtof (midi);
        std::vector<Saw> saws ((size_t) voices);
        std::vector<double> det ((size_t) voices), pan ((size_t) voices);
        Rng rng (seed + (juce::uint32) midi);
        for (int v = 0; v < voices; ++v)
        {
            const double x = voices == 1 ? 0.0 : (v / (double) (voices - 1)) * 2.0 - 1.0;
            det[(size_t) v] = std::pow (2.0, x * spreadCents / 1200.0);
            pan[(size_t) v] = x * 0.8;
            saws[(size_t) v].phase = std::abs (rng.next());
        }
        Svf fl, fr;
        double subPh = 0;
        const double norm = 1.0 / std::sqrt ((double) voices);
        for (int i = 0; i < n; ++i)
        {
            const double t = i / sr;
            const double vib = vibratoDepthSemis > 0 ? std::pow (2.0, vibratoDepthSemis * juce::jmin (1.0, t / 0.6) * std::sin (twoPi * 5.4 * t) / 12.0) : 1.0;
            double l = 0, rr = 0;
            for (int v = 0; v < voices; ++v)
            {
                const double x = saws[(size_t) v].next (f0 * det[(size_t) v] * vib, sr);
                l += x * (1.0 - pan[(size_t) v]);
                rr += x * (1.0 + pan[(size_t) v]);
            }
            l *= norm * 0.5; rr *= norm * 0.5;
            if (subOctaveMix > 0)
            {
                // reforco senoidal na propria fundamental (deixa o grave cheio sem mudar a nota)
                const double sq = std::sin (subPh);
                subPh += twoPi * f0 * vib / sr;
                l += sq * subOctaveMix; rr += sq * subOctaveMix;
            }
            const double fc = cutoff (t, f0);
            const double env = adsr (t, a, d, s, releaseAt, r);
            L[i] = (float) (fl.lowpass (l, fc, q, sr) * env * 0.5);
            R[i] = (float) (fr.lowpass (rr, fc, q, sr) * env * 0.5);
        }
    }

    // Karplus-Strong: delay + media de 2 amostras (meia amostra de atraso) + passa-tudo para a afinacao fina
    void pluck (juce::AudioBuffer<float>& out, int midi, double sr)
    {
        const int n = out.getNumSamples();
        const double f0 = mtof (midi);
        for (int c = 0; c < 2; ++c)
        {
            auto* d = out.getWritePointer (c);
            const double f = f0 * (c == 0 ? 0.9995 : 1.0005);
            const double D = sr / f;
            const int N = juce::jmax (2, (int) std::floor (D - 0.5 - 0.1));
            const double frac = juce::jlimit (0.05, 1.5, D - 0.5 - N);
            const double apc = (1.0 - frac) / (1.0 + frac);
            std::vector<double> buf ((size_t) N);
            Rng rng ((juce::uint32) (midi * 31 + c * 101));
            Svf lp;
            for (auto& x : buf) x = lp.lowpass (rng.next(), juce::jmin (sr * 0.4, f0 * 9.0), 0.7, sr) * 1.4;
            const double damp = 0.9985 - 0.0012 * juce::jmax (0, midi - 60) / 12.0;
            int idx = 0;
            double last = 0, apX1 = 0, apY1 = 0;
            for (int i = 0; i < n; ++i)
            {
                const double x = buf[(size_t) idx];
                const double avg = damp * 0.5 * (x + last);
                last = x;
                const double y = apc * avg + apX1 - apc * apY1;
                apX1 = avg; apY1 = y;
                buf[(size_t) idx] = y;
                idx = (idx + 1) % N;
                d[i] = (float) (x * 0.6);
            }
        }
    }

    void bass808 (juce::AudioBuffer<float>& out, int midi, double sr)
    {
        const int n = out.getNumSamples();
        auto* L = out.getWritePointer (0); auto* R = out.getWritePointer (1);
        const double f0 = mtof (midi);
        double ph = 0;
        for (int i = 0; i < n; ++i)
        {
            const double t = i / sr;
            const double f = f0 * (1.0 + 1.2 * std::exp (-t / 0.018));
            const double env = std::exp (-t / 1.1) * (1.0 - std::exp (-t / 0.0008));
            double v = std::sin (ph) * env;
            v = std::tanh (v * 2.2) / std::tanh (2.2);
            v += 0.25 * std::exp (-t / 0.004) * std::sin (ph * 3.0);               // clique
            ph += twoPi * f / sr;
            L[i] = R[i] = (float) (v * 0.85);
        }
    }

    void bells (juce::AudioBuffer<float>& out, int midi, double sr)
    {
        const int n = out.getNumSamples();
        auto* L = out.getWritePointer (0); auto* R = out.getWritePointer (1);
        const double f0 = mtof (midi);
        double pc = 0, pm = 0, pc2 = 0;
        for (int i = 0; i < n; ++i)
        {
            const double t = i / sr;
            const double idx = 2.6 * std::exp (-t / 0.9);
            const double v1 = std::sin (pc + idx * std::sin (pm)) * std::exp (-t / 2.4);
            const double v2 = std::sin (pc2) * 0.22 * std::exp (-t / 0.45);   // parcial metalico (some rapido)
            const double env = 1.0 - std::exp (-t / 0.001);
            L[i] = (float) ((v1 * 0.95 + v2 * 1.05) * env * 0.45);
            R[i] = (float) ((v1 * 1.05 + v2 * 0.95) * env * 0.45);
            pc += twoPi * f0 / sr; pm += twoPi * f0 * 4.0 / sr; pc2 += twoPi * f0 * 2.76 / sr;
        }
    }

    void marimba (juce::AudioBuffer<float>& out, int midi, double sr)
    {
        const int n = out.getNumSamples();
        auto* L = out.getWritePointer (0); auto* R = out.getWritePointer (1);
        const double f0 = mtof (midi);
        const double ratios[] = { 1.0, 3.93, 9.2 };
        const double amps[]   = { 1.0, 0.32, 0.1 };
        const double scale = std::pow (2.0, -(midi - 60) / 24.0);
        const double decs[]   = { 0.55 * scale, 0.14 * scale, 0.04 * scale };
        double ph[3] = {};
        for (int i = 0; i < n; ++i)
        {
            const double t = i / sr;
            double v = 0;
            for (int k = 0; k < 3; ++k)
            {
                if (f0 * ratios[k] > sr * 0.45) continue;
                v += amps[k] * std::sin (ph[k]) * std::exp (-t / decs[k]);
                ph[k] += twoPi * f0 * ratios[k] / sr;
            }
            v *= (1.0 - std::exp (-t / 0.0007)) * 0.6;
            L[i] = (float) (v * 1.03); R[i] = (float) (v * 0.97);
        }
    }
} // namespace

const Info& info (Id id) { return table[juce::jlimit (0, (int) Id::count - 1, (int) id)]; }

juce::String displayName (Id id)
{
    switch (id)
    {
        case Id::Piano:         return TR ("Piano");
        case Id::ElectricPiano: return TR ("Piano elétrico");
        case Id::Organ:         return TR ("Órgão");
        case Id::SynthLead:     return TR ("Synth lead");
        case Id::SynthPad:      return TR ("Synth pad");
        case Id::Pluck:         return TR ("Pluck");
        case Id::Bass808:       return TR ("Baixo 808");
        case Id::SynthBass:     return TR ("Baixo synth");
        case Id::Strings:       return TR ("Cordas");
        case Id::Brass:         return TR ("Metais");
        case Id::Bells:         return TR ("Sinos");
        case Id::Marimba:       return TR ("Marimba");
        default: break;
    }
    return {};
}

juce::String scaleName (Scale s)
{
    switch (s)
    {
        case Scale::Chromatic:       return TR ("Cromática (todas as notas)");
        case Scale::Major:           return TR ("Maior");
        case Scale::Minor:           return TR ("Menor");
        case Scale::MinorPentatonic: return TR ("Pentatônica menor");
        case Scale::Blues:           return TR ("Blues");
        default: break;
    }
    return {};
}

std::vector<int> scaleNotes (int root, Scale s, int count)
{
    std::vector<int> steps;
    switch (s)
    {
        case Scale::Major:           steps = { 0, 2, 4, 5, 7, 9, 11 }; break;
        case Scale::Minor:           steps = { 0, 2, 3, 5, 7, 8, 10 }; break;
        case Scale::MinorPentatonic: steps = { 0, 3, 5, 7, 10 }; break;
        case Scale::Blues:           steps = { 0, 3, 5, 6, 7, 10 }; break;
        default:                     for (int i = 0; i < 12; ++i) steps.push_back (i); break;
    }
    std::vector<int> notes;
    for (int i = 0; (int) notes.size() < count; ++i)
        notes.push_back (root + 12 * (i / (int) steps.size()) + steps[(size_t) (i % (int) steps.size())]);
    return notes;
}

juce::String noteName (int midi)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return juce::String (names[((midi % 12) + 12) % 12]) + juce::String (midi / 12 - 1);
}

juce::AudioBuffer<float> renderNote (Id id, int midi, double sr)
{
    const auto& in = info (id);
    const double len = in.noteSeconds;
    juce::AudioBuffer<float> b (2, (int) (len * sr));
    b.clear();
    const double rel = len - 0.35;   // a nota "solta" perto do fim (modo Gate tem o proprio release no pad)
    switch (id)
    {
        case Id::Piano:         piano (b, midi, sr); break;
        case Id::ElectricPiano: electricPiano (b, midi, sr); break;
        case Id::Organ:         organ (b, midi, sr, len); break;
        case Id::SynthLead:
            sawStack (b, midi, sr, 7, 22.0, 0.004, 0.35, 0.75, rel, 0.25,
                      [] (double t, double f0) { return f0 * 5.0 + f0 * 18.0 * std::exp (-t / 0.25) + 900.0; }, 0.9, 0.18, 11u);
            break;
        case Id::SynthPad:
            sawStack (b, midi, sr, 6, 16.0, 0.55, 0.6, 0.85, rel, 0.4,
                      [] (double t, double) { return 900.0 + 700.0 * (0.5 + 0.5 * std::sin (twoPi * 0.35 * t - 1.5)); }, 0.8, 0.0, 23u);
            break;
        case Id::Pluck:         pluck (b, midi, sr); break;
        case Id::Bass808:       bass808 (b, midi, sr); break;
        case Id::SynthBass:
            sawStack (b, midi, sr, 2, 8.0, 0.003, 0.35, 0.65, rel, 0.12,
                      [] (double t, double f0) { return f0 * 3.0 + f0 * 14.0 * std::exp (-t / 0.12) + 120.0; }, 1.6, 0.0, 37u, 0.5);
            break;
        case Id::Strings:
            sawStack (b, midi, sr, 5, 18.0, 0.28, 0.5, 0.9, rel, 0.35,
                      [] (double, double f0) { return 1700.0 + f0 * 1.5; }, 0.7, 0.15, 41u);
            break;
        case Id::Brass:
            sawStack (b, midi, sr, 3, 9.0, 0.03, 0.25, 0.8, rel, 0.15,
                      [] (double t, double f0) { return f0 * (2.0 + 7.0 * juce::jmin (1.0, t / 0.06) * (0.6 + 0.4 * std::exp (-t / 0.4))) + 300.0; }, 1.1, 0.08, 53u);
            break;
        case Id::Bells:         bells (b, midi, sr); break;
        case Id::Marimba:       marimba (b, midi, sr); break;
        default: break;
    }
    fadeEdges (b, sr, in.sustained ? 0.03 : 0.25);
    return b;
}

bool renderToFile (Id id, const std::vector<int>& notes, const juce::File& folder, Rendered& out, std::function<bool (float)> progress)
{
    constexpr double sr = 44100.0;
    const auto& in = info (id);
    const double gap = 0.1;
    const double slot = in.noteSeconds + gap;

    juce::String key;
    for (int nn : notes) key << nn << ",";
    const auto hash = juce::String::toHexString ((juce::int64) key.hashCode64()).substring (0, 8);
    folder.createDirectory();
    out.file = folder.getChildFile (juce::String ("v1-inst") + juce::String ((int) id) + "-" + noteName (notes.front()) + "-" + hash + ".wav");
    out.regions.clear();
    out.notes = notes;
    for (size_t i = 0; i < notes.size(); ++i)
        out.regions.push_back ({ i * slot, i * slot + in.noteSeconds });

    const juce::int64 total = (juce::int64) (slot * notes.size() * sr);
    if (out.file.existsAsFile() && out.file.getSize() > total * 4)   // ja gerado antes
        return true;

    juce::AudioBuffer<float> all (2, (int) total);
    all.clear();
    for (size_t i = 0; i < notes.size(); ++i)
    {
        auto b = renderNote (id, notes[i], sr);
        const int start = (int) (i * slot * sr);
        const int len = juce::jmin (b.getNumSamples(), all.getNumSamples() - start);
        for (int c = 0; c < 2; ++c) all.copyFrom (c, start, b, c, 0, len);
        if (progress && ! progress ((float) (i + 1) / (float) notes.size())) return false;
    }
    // volume parecido entre os instrumentos: pico em -1 dB
    const float peak = all.getMagnitude (0, all.getNumSamples());
    if (peak > 0.0f) all.applyGain (0.89f / peak);

    auto tmp = out.file.getSiblingFile (out.file.getFileNameWithoutExtension() + ".part");
    tmp.deleteFile();
    {
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (tmp), sr, 2, 24, {}, 0));
        if (w == nullptr) return false;
        if (! w->writeFromAudioSampleBuffer (all, 0, all.getNumSamples())) return false;
    }
    out.file.deleteFile();
    return tmp.moveFileTo (out.file);
}

} // namespace instruments
} // namespace mp
