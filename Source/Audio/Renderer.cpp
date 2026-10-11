#include "Renderer.h"

namespace mp
{
static inline float hermite (const float* d, int frames, double pos) noexcept
{
    const int i = (int) std::floor (pos);
    const float f = (float) (pos - i);
    auto at = [d, frames] (int k) noexcept { return (k >= 0 && k < frames) ? d[k] : 0.0f; };
    const float xm1 = at (i - 1), x0 = at (i), x1 = at (i + 1), x2 = at (i + 2);
    const float c1 = 0.5f * (x1 - xm1);
    const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * f + c2) * f + c1) * f + x0;
}

static inline void svfCoeffs (double sr, float cutoff, float q, float& g, float& k, float& a1, float& a2, float& a3)
{
    cutoff = juce::jlimit (16.0f, (float) (sr * 0.45), cutoff);
    g = (float) std::tan (juce::MathConstants<double>::pi * cutoff / sr);
    k = 1.0f / juce::jmax (0.5f, q);
    a1 = 1.0f / (1.0f + g * (g + k));
    a2 = g * a1;
    a3 = g * a2;
}

static inline float velToGain (uint8_t v) { const float x = v / 127.0f; return x * std::sqrt (x); }

Renderer::Renderer()
{
    for (auto& p : trackPeak) p = 0.0f;
    for (auto& p : padActivity) p = 0;
}

void Renderer::prepare (double sampleRate, int maxBlockSize)
{
    sr = sampleRate > 0 ? sampleRate : 44100.0;
    maxBlock = juce::jmax (kChunk, maxBlockSize);
    bus.setSize (kMaxTracks * 2, kChunk);
    fxBus.setSize (4, kChunk);
    reverb.setSampleRate (sr);
    delayL.assign ((size_t) (sr * 4.5), 0.0f);
    delayR.assign ((size_t) (sr * 4.5), 0.0f);
    delayWrite = 0;
    delayCurrent = 0;
    juce::dsp::ProcessSpec spec { sr, (juce::uint32) kChunk, 2 };
    limiter.prepare (spec);
    limiter.setThreshold (-0.3f);
    limiter.setRelease (60.0f);
    resetDsp();
}

void Renderer::resetDsp()
{
    for (auto& v : voices) { v.active = false; v.buf.reset(); }
    for (auto& t : tracks)
    {
        for (auto& band : t.eq) for (auto& f : band) f.reset();
        t.svf.reset();
        t.initialised = false;
    }
    reverb.reset();
    std::fill (delayL.begin(), delayL.end(), 0.0f);
    std::fill (delayR.begin(), delayR.end(), 0.0f);
    delayLpL = delayLpR = 0;
    limiter.reset();
    clickRemaining = 0;
}

void Renderer::Voice::startRelease (float ms, double sampleRate)
{
    if (stage == 2) return;
    stage = 2;
    const double samples = juce::jmax (1.0, ms * sampleRate / 1000.0);
    releaseCoef = (float) std::exp (std::log (0.0001) / samples);
}

void Renderer::padOn (const Snapshot& snap, int pad, uint8_t vel, int slot, int offset, int gateSamples, bool fromLive)
{
    if (pad < 0 || pad >= kNumPads || vel == 0) return;
    const auto& sp = snap.pads[(size_t) pad];
    if (fromLive && recording && playing) recordLiveHit (snap, pad, vel);
    if (sp.buf == nullptr || sp.buf->getNumSamples() < 2) return;
    padActivity[(size_t) pad].fetch_add (1);

    for (auto& v : voices)
    {
        if (! v.active || v.stage == 2) continue;
        const bool chokeIt = (sp.choke > 0 && v.choke == sp.choke) || (v.pad == pad && sp.mode != PadMode::OneShot);
        if (chokeIt)
        {
            if (v.delay > offset) v.active = false;   // ainda nao comecou
            else v.startRelease (4.0f, sr);
        }
    }

    Voice* target = nullptr;
    for (auto& v : voices) if (! v.active) { target = &v; break; }
    if (target == nullptr)
    {
        target = &voices[0];
        for (auto& v : voices) if (v.age < target->age) target = &v;
    }
    auto& v = *target;
    v = Voice();
    v.active = true;
    v.pad = pad;
    v.slot = slot;
    v.choke = sp.choke;
    v.buf = sp.buf;
    v.frames = sp.buf->getNumSamples();
    v.channels = sp.buf->getNumChannels();
    v.pos = 0;
    v.inc = sp.bufSR / sr * sp.pitchRatio;
    v.mode = sp.mode;
    const float g = sp.gain * velToGain (vel);
    v.gL = g * (sp.pan > 0 ? 1.0f - sp.pan : 1.0f);
    v.gR = g * (sp.pan < 0 ? 1.0f + sp.pan : 1.0f);
    v.delay = juce::jmax (0, offset);
    v.gateRemaining = (sp.mode != PadMode::OneShot && gateSamples > 0) ? gateSamples : -1;
    const double atk = juce::jmax (1.0, sp.attackMs * sr / 1000.0);
    v.attackInc = (float) (1.0 / atk);
    v.stage = 0;
    v.env = sp.attackMs <= 0.05f ? 1.0f : 0.0f;
    if (v.env >= 1.0f) v.stage = 1;
    v.releaseCoef = (float) std::exp (std::log (0.0001) / juce::jmax (1.0, sp.releaseMs * sr / 1000.0));
    v.useFilter = sp.cutoff < 19500.0f;
    if (v.useFilter)
        svfCoeffs (sr, sp.cutoff, 0.707f + sp.resonance * 8.0f, v.fg, v.fk, v.fa1, v.fa2, v.fa3);
    v.age = ++voiceCounter;
    // guarda o tempo de release desejado no proprio coeficiente (usado em padOff)
}

void Renderer::padOff (int pad)
{
    for (auto& v : voices)
        if (v.active && v.pad == pad && v.mode != PadMode::OneShot && v.stage != 2)
        {
            // releaseCoef ja foi calculado em padOn
            v.stage = 2;
        }
}

void Renderer::allNotesOff (bool hard)
{
    for (auto& v : voices)
    {
        if (! v.active) continue;
        if (hard) v.active = false;
        else v.startRelease (8.0f, sr);
    }
}

void Renderer::recordLiveHit (const Snapshot& snap, int pad, uint8_t vel)
{
    const SnapPattern* pat = nullptr;
    double local = 0;
    if (patternMode)
    {
        if (snap.currentPatternIndex < 0 || snap.currentPatternIndex >= (int) snap.patterns.size()) return;
        pat = &snap.patterns[(size_t) snap.currentPatternIndex];
        const double len = pat->length / (double) kStepsPerBeat;
        local = std::fmod (juce::jmax (0.0, posBeats), len);
    }
    else
    {
        for (auto& c : snap.clips)
            if (c.isPattern && c.trackSlot == snap.liveTrackSlot && posBeats >= c.startBeat && posBeats < c.endBeat)
            {
                pat = &snap.patterns[(size_t) c.patternIndex];
                const double len = pat->length / (double) kStepsPerBeat;
                local = std::fmod (posBeats - c.startBeat + c.patternOffsetBeats, len);
                break;
            }
        if (pat == nullptr) return;
    }
    int step = (int) std::floor (local * kStepsPerBeat + 0.5);
    step = ((step % pat->length) + pat->length) % pat->length;
    recordedHits.push ({ pat->id, pad, step, vel });
}

float Renderer::evalAuto (const std::vector<AutoPoint>& pts, double beat, float def)
{
    if (pts.empty()) return def;
    if (beat <= pts.front().beat) return pts.front().value;
    if (beat >= pts.back().beat) return pts.back().value;
    auto it = std::upper_bound (pts.begin(), pts.end(), beat, [] (double b, const AutoPoint& p) { return b < p.beat; });
    const auto& b = *it;
    const auto& a = *(it - 1);
    const double span = b.beat - a.beat;
    if (span <= 1.0e-9) return b.value;
    const double t = (beat - a.beat) / span;
    return (float) (a.value + (b.value - a.value) * t);
}

void Renderer::process (const Snapshot& snap, juce::AudioBuffer<float>& out, int startSample, int numSamples)
{
    juce::ScopedNoDenormals noDenormals;
    if (sr != snap.sampleRate && std::abs (sr - snap.sampleRate) > 1.0)
    {
        // snapshot de outra taxa (troca de dispositivo em andamento): silencio
        for (int c = 0; c < out.getNumChannels(); ++c) out.clear (c, startSample, numSamples);
        return;
    }
    {
        const juce::SpinLock::ScopedTryLockType sl (previewLock);
        if (sl.isLocked() && previewChanged) { preview = pendingPreview; previewChanged = false; }
    }

    int done = 0;
    float tmpL[kChunk], tmpR[kChunk];
    while (done < numSamples)
    {
        const int n = juce::jmin (kChunk, numSamples - done);
        float* L = out.getNumChannels() > 0 ? out.getWritePointer (0, startSample + done) : tmpL;
        float* R = out.getNumChannels() > 1 ? out.getWritePointer (1, startSample + done) : tmpR;
        renderChunk (snap, L, R, n);
        if (out.getNumChannels() == 1)
            for (int i = 0; i < n; ++i) L[i] = 0.5f * (L[i] + tmpR[i]);
        for (int c = 2; c < out.getNumChannels(); ++c) out.clear (c, startSample + done, n);
        done += n;
    }
}

void Renderer::renderChunk (const Snapshot& snap, float* L, float* R, int n)
{
    const int numTracks = (int) snap.tracks.size();
    for (int s = 0; s < numTracks; ++s)
    {
        juce::FloatVectorOperations::clear (bus.getWritePointer (s * 2), n);
        juce::FloatVectorOperations::clear (bus.getWritePointer (s * 2 + 1), n);
        busUsed[s] = false;
    }
    fxBus.clear (0, n);

    const double chunkStartBeat = posBeats;
    if (playing)
    {
        const double spb = snap.samplesPerBeat();
        int rem = n, off = 0;
        int guard = 0;
        while (rem > 0 && guard++ < 64)
        {
            bool loopOn = snap.loopOn;
            double ls = snap.loopStart, le = snap.loopEnd;
            if (patternMode)
            {
                if (snap.patterns.empty()) break;
                const auto& pat = snap.patterns[(size_t) juce::jlimit (0, (int) snap.patterns.size() - 1, snap.currentPatternIndex)];
                ls = 0; le = pat.length / (double) kStepsPerBeat; loopOn = true;
                if (posBeats >= le || posBeats < 0) posBeats = std::fmod (juce::jmax (0.0, posBeats), le);
            }
            int segLen = rem;
            bool wrap = false;
            if (loopOn && posBeats < le)
            {
                const double toEnd = (le - posBeats) * spb;
                if (toEnd <= rem)
                {
                    segLen = juce::jlimit (1, rem, (int) std::ceil (toEnd - 1.0e-7));
                    wrap = true;
                }
            }
            scheduleSegment (snap, off, segLen, posBeats);
            posBeats += segLen / spb;
            if (wrap && posBeats >= le - 1.0e-9) posBeats = ls + juce::jmax (0.0, posBeats - le);
            off += segLen;
            rem -= segLen;
        }
    }

    renderVoices (n);

    juce::FloatVectorOperations::clear (L, n);
    juce::FloatVectorOperations::clear (R, n);
    const double savedPos = posBeats;
    posBeats = chunkStartBeat;  // automacao avaliada no inicio do bloco
    for (int s = 0; s < numTracks; ++s) processTrack (snap, s, n, L, R);
    posBeats = savedPos;
    processMaster (snap, L, R, n);
}

void Renderer::scheduleSegment (const Snapshot& snap, int off, int len, double b0)
{
    const double spb = snap.samplesPerBeat();
    const double b1 = b0 + len / spb;
    if (patternMode)
    {
        if (! snap.patterns.empty() && ! snap.tracks.empty())
        {
            const auto& pat = snap.patterns[(size_t) juce::jlimit (0, (int) snap.patterns.size() - 1, snap.currentPatternIndex)];
            triggerPatternSteps (snap, pat, snap.liveTrackSlot, 0.0, -1.0e9, 1.0e9, off, len, b0);
        }
    }
    else
    {
        for (auto& c : snap.clips)
        {
            if (c.startBeat >= b1 || c.endBeat <= b0) continue;
            if (c.isPattern)
            {
                if (c.patternIndex >= 0 && c.patternIndex < (int) snap.patterns.size())
                    triggerPatternSteps (snap, snap.patterns[(size_t) c.patternIndex], c.trackSlot,
                                         c.startBeat - c.patternOffsetBeats, c.startBeat, c.endBeat, off, len, b0);
            }
            else
                renderAudioClip (snap, c, off, len, b0);
        }
    }

    if (snap.metronome)
    {
        for (double k = std::ceil (b0 - 1.0e-9); k < b1; k += 1.0)
        {
            const int o = off + juce::jlimit (0, len - 1, (int) std::round ((k - b0) * spb));
            const bool down = ((long long) std::llround (k) % snap.beatsPerBar) == 0;
            clickFreq = down ? 1760.0 : 1175.0;
            clickAmp = down ? 0.45f : 0.3f;
            clickRemaining = (int) (sr * 0.035);
            clickDelay = o;
            clickPhase = 0;
        }
    }
}

void Renderer::triggerPatternSteps (const Snapshot& snap, const SnapPattern& pat, int slot, double origin,
                                    double clipStart, double clipEnd, int off, int len, double b0)
{
    const double spb = snap.samplesPerBeat();
    const double b1 = b0 + len / spb;
    const double stepLen = 1.0 / kStepsPerBeat;
    const double swingDelay = ((snap.swing / 100.0) * 2.0 - 1.0) * stepLen;
    const long long kStart = (long long) std::floor ((b0 - origin) / stepLen) - 1;
    const long long kEnd = (long long) std::ceil ((b1 - origin) / stepLen) + 1;
    const int L = juce::jmax (1, pat.length);
    for (long long k = kStart; k <= kEnd; ++k)
    {
        const double sb = origin + k * stepLen + ((k & 1) ? swingDelay : 0.0);
        if (sb < b0 || sb >= b1) continue;
        if (sb < clipStart - 1.0e-9 || sb >= clipEnd - 1.0e-9) continue;
        const int step = (int) (((k % L) + L) % L);
        const int o = off + juce::jlimit (0, len - 1, (int) std::floor ((sb - b0) * spb));
        const int gate = (int) (stepLen * spb * 0.95);
        for (int pad = 0; pad < kNumPads; ++pad)
            if (const auto v = pat.get (pad, step))
                padOn (snap, pad, v, slot, o, gate, false);
    }
}

void Renderer::renderAudioClip (const Snapshot& snap, const SnapClip& c, int off, int len, double b0)
{
    if (c.data == nullptr || c.trackSlot < 0 || c.trackSlot >= (int) snap.tracks.size()) return;
    const double spb = snap.samplesPerBeat();
    const double tOut0 = (b0 - c.startBeat) * spb;
    const double clipLenOut = (c.endBeat - c.startBeat) * spb;
    const double fpo = c.dataSR / sr * c.varispeed;
    const double fi = c.fadeInBeats * spb, fo = c.fadeOutBeats * spb;
    const auto& data = *c.data;
    const int frames = data.getNumSamples();
    const float* d0 = data.getReadPointer (0);
    const float* d1 = data.getNumChannels() > 1 ? data.getReadPointer (1) : d0;
    float* L = bus.getWritePointer (c.trackSlot * 2) + off;
    float* R = bus.getWritePointer (c.trackSlot * 2 + 1) + off;
    constexpr float halfPi = juce::MathConstants<float>::halfPi;

    int i0 = 0, i1 = len;
    if (tOut0 < 0) i0 = juce::jmin (len, (int) std::ceil (-tOut0));
    if (tOut0 + len > clipLenOut) i1 = juce::jmax (i0, juce::jmin (len, (int) std::ceil (clipLenOut - tOut0)));
    if (i1 <= i0) return;
    busUsed[c.trackSlot] = true;

    for (int i = i0; i < i1; ++i)
    {
        const double t = tOut0 + i;
        if (t < 0 || t >= clipLenOut) continue;
        const double p = t * fpo;
        if (p > c.lenFrames + 1) continue;
        const double frame = c.reverse ? (c.dStart + c.lenFrames - p) : (c.dStart + p);
        float g = c.gain;
        if (t < fi) g *= std::sin (halfPi * (float) (t / fi));
        const double remain = clipLenOut - t;
        if (remain < fo) g *= std::sin (halfPi * (float) (remain / fo));
        if (g == 0.0f) continue;
        L[i] += hermite (d0, frames, frame) * g;
        R[i] += (d1 == d0 ? hermite (d0, frames, frame) : hermite (d1, frames, frame)) * g;
    }
}

void Renderer::renderVoices (int n)
{
    const int maxSlot = bus.getNumChannels() / 2;
    int active = 0;
    for (auto& v : voices)
    {
        if (! v.active) continue;
        if (v.buf == nullptr) { v.active = false; continue; }
        const int slot = juce::jlimit (0, maxSlot - 1, v.slot);
        float* L = bus.getWritePointer (slot * 2);
        float* R = bus.getWritePointer (slot * 2 + 1);
        busUsed[slot] = true;
        const float* d0 = v.buf->getReadPointer (0);
        const float* d1 = v.channels > 1 ? v.buf->getReadPointer (1) : d0;
        const int start = juce::jmin (n, v.delay);
        for (int i = start; i < n; ++i)
        {
            if (v.gateRemaining >= 0)
            {
                if (v.gateRemaining == 0) { v.stage = 2; v.gateRemaining = -1; }
                else --v.gateRemaining;
            }
            if (v.pos >= v.frames)
            {
                if (v.mode == PadMode::Loop) v.pos = std::fmod (v.pos, (double) v.frames);
                else { v.active = false; break; }
            }
            if (v.stage == 0)
            {
                v.env += v.attackInc;
                if (v.env >= 1.0f) { v.env = 1.0f; v.stage = 1; }
            }
            else if (v.stage == 2)
            {
                v.env *= v.releaseCoef;
                if (v.env < 1.0e-4f) { v.active = false; break; }
            }
            float s0 = hermite (d0, v.frames, v.pos);
            float s1 = d1 == d0 ? s0 : hermite (d1, v.frames, v.pos);
            if (v.useFilter)
            {
                float in[2] = { s0, s1 };
                for (int ch = 0; ch < 2; ++ch)
                {
                    const float v3 = in[ch] - v.ic2[ch];
                    const float v1 = v.fa1 * v.ic1[ch] + v.fa2 * v3;
                    const float v2 = v.ic2[ch] + v.fa2 * v.ic1[ch] + v.fa3 * v3;
                    v.ic1[ch] = 2.0f * v1 - v.ic1[ch];
                    v.ic2[ch] = 2.0f * v2 - v.ic2[ch];
                    in[ch] = v2;
                }
                s0 = in[0]; s1 = in[1];
            }
            L[i] += s0 * v.env * v.gL;
            R[i] += s1 * v.env * v.gR;
            v.pos += v.inc;
        }
        v.delay = juce::jmax (0, v.delay - n);
        if (v.active) ++active;
    }
    activeVoices = active;
}

void Renderer::processTrack (const Snapshot& snap, int slot, int n, float* mL, float* mR)
{
    const auto& st = snap.tracks[(size_t) slot];
    auto& dsp = tracks[(size_t) slot];
    float* L = bus.getWritePointer (slot * 2);
    float* R = bus.getWritePointer (slot * 2 + 1);

    const float volMult = st.volAuto.empty() ? 1.0f : juce::square (juce::jlimit (0.0f, 1.0f, evalAuto (st.volAuto, posBeats, 1.0f)));
    const float targetGain = st.gain * volMult;
    const float fval = st.filterAuto.empty() ? st.filter : juce::jlimit (-1.0f, 1.0f, evalAuto (st.filterAuto, posBeats, 0.0f));
    if (! dsp.initialised) { dsp.lastGain = targetGain; dsp.lastFilter = fval; dsp.initialised = true; }

    if (! busUsed[slot])
    {
        dsp.lastGain = targetGain;
        dsp.lastFilter = fval;
        dsp.svf.reset();
        trackPeak[(size_t) slot].store (juce::jmax (0.0f, trackPeak[(size_t) slot].load()));
        return;
    }

    // EQ de 3 bandas
    const float eqVals[3] = { st.eqLow, st.eqMid, st.eqHigh };
    for (int b = 0; b < 3; ++b)
    {
        if (eqVals[b] != dsp.eqVal[b] || ! dsp.eqActive[b] && std::abs (eqVals[b]) > 0.05f)
        {
            const float gf = juce::Decibels::decibelsToGain (eqVals[b]);
            juce::IIRCoefficients co = b == 0 ? juce::IIRCoefficients::makeLowShelf (sr, 110.0, 0.7, gf)
                                     : b == 1 ? juce::IIRCoefficients::makePeakFilter (sr, 1000.0, 0.8, gf)
                                              : juce::IIRCoefficients::makeHighShelf (sr, 7500.0, 0.7, gf);
            for (auto& f : dsp.eq[b]) f.setCoefficients (co);
            dsp.eqVal[b] = eqVals[b];
            dsp.eqActive[b] = std::abs (eqVals[b]) > 0.05f;
        }
        if (dsp.eqActive[b])
        {
            dsp.eq[b][0].processSamples (L, n);
            dsp.eq[b][1].processSamples (R, n);
        }
    }

    // filtro DJ (LP <- 0 -> HP)
    const float f0 = dsp.lastFilter, f1 = fval;
    if (std::abs (f0) > 0.002f || std::abs (f1) > 0.002f)
    {
        const float q = 0.707f + st.filterRes * 7.0f;
        for (int j = 0; j < n; j += 16)
        {
            const int m = juce::jmin (16, n - j);
            const float f = f0 + (f1 - f0) * (float) (j + m) / (float) n;
            const bool hp = f > 0;
            const float a = std::abs (f);
            const float cutoff = hp ? 20.0f * std::pow (600.0f, a) : 20000.0f * std::pow (60.0f / 20000.0f, a);
            float g, k, a1, a2, a3;
            svfCoeffs (sr, cutoff, q, g, k, a1, a2, a3);
            for (int i = j; i < j + m; ++i)
            {
                float* chans[2] = { L, R };
                for (int ch = 0; ch < 2; ++ch)
                {
                    const float x = chans[ch][i];
                    const float v3 = x - dsp.svf.ic2[ch];
                    const float v1 = a1 * dsp.svf.ic1[ch] + a2 * v3;
                    const float v2 = dsp.svf.ic2[ch] + a2 * dsp.svf.ic1[ch] + a3 * v3;
                    dsp.svf.ic1[ch] = 2.0f * v1 - dsp.svf.ic1[ch];
                    dsp.svf.ic2[ch] = 2.0f * v2 - dsp.svf.ic2[ch];
                    chans[ch][i] = hp ? (x - k * v1 - v2) : v2;
                }
            }
        }
    }
    else dsp.svf.reset();
    dsp.lastFilter = f1;

    // volume + pan (balanco) + envios
    const float panL = st.pan > 0 ? 1.0f - st.pan : 1.0f;
    const float panR = st.pan < 0 ? 1.0f + st.pan : 1.0f;
    const float g0 = dsp.lastGain;
    const float dg = (targetGain - g0) / (float) n;
    float* rvL = fxBus.getWritePointer (0); float* rvR = fxBus.getWritePointer (1);
    float* dlL = fxBus.getWritePointer (2); float* dlR = fxBus.getWritePointer (3);
    float peak = 0;
    const float pl0 = dsp.lastPanL, pr0 = dsp.lastPanR;
    for (int i = 0; i < n; ++i)
    {
        const float t = (float) (i + 1) / (float) n;
        const float g = g0 + dg * (float) (i + 1);
        const float l = L[i] * g * (pl0 + (panL - pl0) * t);
        const float r = R[i] * g * (pr0 + (panR - pr0) * t);
        mL[i] += l; mR[i] += r;
        if (st.reverbSend > 0) { rvL[i] += l * st.reverbSend; rvR[i] += r * st.reverbSend; }
        if (st.delaySend > 0)  { dlL[i] += l * st.delaySend;  dlR[i] += r * st.delaySend; }
        peak = juce::jmax (peak, std::abs (l), std::abs (r));
    }
    dsp.lastGain = targetGain;
    dsp.lastPanL = panL; dsp.lastPanR = panR;
    if (peak > trackPeak[(size_t) slot].load()) trackPeak[(size_t) slot].store (peak);
}

void Renderer::processMaster (const Snapshot& snap, float* L, float* R, int n)
{
    // reverb (retorno 100% molhado)
    juce::Reverb::Parameters rp;
    rp.roomSize = snap.reverbSize; rp.damping = snap.reverbDamp; rp.width = snap.reverbWidth;
    rp.wetLevel = 0.5f; rp.dryLevel = 0.0f; rp.freezeMode = 0.0f;
    if (rp.roomSize != lastRevParams.roomSize || rp.damping != lastRevParams.damping || rp.width != lastRevParams.width
        || lastRevParams.dryLevel != 0.0f)
    {
        reverb.setParameters (rp);
        lastRevParams = rp;
    }
    float* rvL = fxBus.getWritePointer (0); float* rvR = fxBus.getWritePointer (1);
    reverb.processStereo (rvL, rvR, n);

    // delay ping-pong sincronizado ao BPM
    float* dlL = fxBus.getWritePointer (2); float* dlR = fxBus.getWritePointer (3);
    const int dsize = (int) delayL.size();
    const double target = juce::jlimit (1.0, (double) dsize - 4.0, snap.delayBeats * 60.0 / snap.bpm * sr);
    if (delayCurrent <= 0) delayCurrent = target;
    const float fb = juce::jlimit (0.0f, 0.95f, snap.delayFeedback);
    const float lpc = 0.04f + 0.9f * juce::jlimit (0.0f, 1.0f, snap.delayTone);
    for (int i = 0; i < n; ++i)
    {
        delayCurrent += (target - delayCurrent) * 0.0008;
        double rp2 = delayWrite - delayCurrent;
        while (rp2 < 0) rp2 += dsize;
        const int i0 = (int) rp2;
        const int i1 = (i0 + 1) % dsize;
        const float fr = (float) (rp2 - i0);
        const float oL = delayL[(size_t) i0] + (delayL[(size_t) i1] - delayL[(size_t) i0]) * fr;
        const float oR = delayR[(size_t) i0] + (delayR[(size_t) i1] - delayR[(size_t) i0]) * fr;
        delayLpL += lpc * (oL - delayLpL);
        delayLpR += lpc * (oR - delayLpR);
        delayL[(size_t) delayWrite] = dlL[i] + delayLpR * fb;
        delayR[(size_t) delayWrite] = dlR[i] + delayLpL * fb;
        delayWrite = (delayWrite + 1) % dsize;
        L[i] += rvL[i] + oL;
        R[i] += rvR[i] + oR;
    }

    // metronomo
    if (clickRemaining > 0)
    {
        const int total = (int) (sr * 0.035);
        for (int i = juce::jmin (n, clickDelay); i < n && clickRemaining > 0; ++i)
        {
            const float env = (float) clickRemaining / (float) total;
            const float s = (float) std::sin (clickPhase) * clickAmp * env * env;
            clickPhase += juce::MathConstants<double>::twoPi * clickFreq / sr;
            L[i] += s; R[i] += s;
            --clickRemaining;
        }
        clickDelay = juce::jmax (0, clickDelay - n);
    }

    // pre-escuta
    if (preview.active && preview.buf)
    {
        const auto& b = *preview.buf;
        const float* d0 = b.getReadPointer (0);
        const float* d1 = b.getNumChannels() > 1 ? b.getReadPointer (1) : d0;
        const double inc = preview.sr / sr;
        const int frames = b.getNumSamples();
        for (int i = 0; i < n; ++i)
        {
            if (preview.pos >= preview.end || preview.pos >= frames) { preview.active = false; break; }
            float fade = 1.0f;
            const double rem = preview.end - preview.pos;
            if (rem < preview.sr * 0.005) fade = (float) (rem / (preview.sr * 0.005));
            L[i] += hermite (d0, frames, preview.pos) * preview.gain * fade;
            R[i] += hermite (d1, frames, preview.pos) * preview.gain * fade;
            preview.pos += inc;
        }
        previewPosSec = preview.active ? preview.pos / preview.sr : -1.0;
    }
    else previewPosSec = -1.0;

    // master + limitador
    const float g1 = snap.masterGain;
    const float g0 = masterGainLast;
    for (int i = 0; i < n; ++i)
    {
        const float g = g0 + (g1 - g0) * (float) (i + 1) / (float) n;
        L[i] *= g; R[i] *= g;
    }
    masterGainLast = g1;
    bool bad = false;
    for (int i = 0; i < n; ++i)
        if (! std::isfinite (L[i]) || ! std::isfinite (R[i])) { L[i] = R[i] = 0; bad = true; }
    if (bad) { reverb.reset(); std::fill (delayL.begin(), delayL.end(), 0.0f); std::fill (delayR.begin(), delayR.end(), 0.0f); delayLpL = delayLpR = 0; }
    float* chans[2] = { L, R };
    juce::dsp::AudioBlock<float> block (chans, 2, (size_t) n);
    limiter.process (juce::dsp::ProcessContextReplacing<float> (block));

    float pL = 0, pR = 0;
    for (int i = 0; i < n; ++i)
    {
        pL = juce::jmax (pL, std::abs (L[i]));
        pR = juce::jmax (pR, std::abs (R[i]));
    }
    if (pL > masterPeakL.load()) masterPeakL = pL;
    if (pR > masterPeakR.load()) masterPeakR = pR;
}

} // namespace mp
