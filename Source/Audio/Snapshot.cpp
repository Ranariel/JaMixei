#include "Snapshot.h"

namespace mp
{
BufferPtr PadBufferCache::getForPad (int padIndex, const Pad& pad, const SourcePtr& src)
{
    if (padIndex < 0 || padIndex >= kNumPads || src == nullptr || src->buffer == nullptr) return nullptr;
    auto& e = entries[(size_t) padIndex];
    if (e.buf && e.sourceId == pad.sourceId && e.s == pad.startSec && e.e == pad.endSec && e.rev == pad.reverse && e.srcPtr == src.get())
        return e.buf;

    const auto& sb = *src->buffer;
    const int s0 = juce::jlimit (0, sb.getNumSamples(), (int) std::round (pad.startSec * src->sampleRate));
    const int s1 = juce::jlimit (s0, sb.getNumSamples(), (int) std::round (pad.endSec * src->sampleRate));
    const int len = s1 - s0;
    if (len < 2) return nullptr;

    auto b = std::make_shared<juce::AudioBuffer<float>> (sb.getNumChannels(), len);
    for (int c = 0; c < sb.getNumChannels(); ++c)
    {
        b->copyFrom (c, 0, sb, c, s0, len);
        if (pad.reverse) b->reverse (c, 0, len);
    }
    // micro-fades anti-clique (0.3 ms entrada, 2 ms saida)
    const int fi = juce::jmin (len / 4, (int) (src->sampleRate * 0.0003));
    const int fo = juce::jmin (len / 4, (int) (src->sampleRate * 0.002));
    if (fi > 0) b->applyGainRamp (0, fi, 0.0f, 1.0f);
    if (fo > 0) b->applyGainRamp (len - fo, fo, 1.0f, 0.0f);

    e.sourceId = pad.sourceId; e.s = pad.startSec; e.e = pad.endSec; e.rev = pad.reverse; e.srcPtr = src.get();
    e.buf = b;
    return b;
}

static float dbToGain (float db) { return db <= -60.0f ? 0.0f : juce::Decibels::decibelsToGain (db); }

SnapshotPtr SnapshotBuilder::build (const Project& p, SourcePool& sources, StretchCache* stretch,
                                    PadBufferCache& padCache, double sampleRate)
{
    auto s = std::make_shared<Snapshot>();
    s->sampleRate = sampleRate > 0 ? sampleRate : 44100.0;
    s->bpm = juce::jlimit (20.0, 400.0, p.bpm);
    s->swing = juce::jlimit (50.0f, 75.0f, p.swing);
    s->beatsPerBar = juce::jmax (1, p.beatsPerBar);
    s->loopOn = p.loopOn && p.loopEnd > p.loopStart + 0.01;
    s->loopStart = p.loopStart;
    s->loopEnd = p.loopEnd;
    s->metronome = p.metronome;
    s->masterGain = dbToGain (p.masterVolDb);
    s->reverbSize = p.reverbSize; s->reverbDamp = p.reverbDamp; s->reverbWidth = p.reverbWidth;
    s->delayBeats = p.delayBeats; s->delayFeedback = p.delayFeedback; s->delayTone = p.delayTone;

    bool anySolo = false;
    for (auto& t : p.tracks) anySolo |= t.solo;

    const int numTracks = juce::jmin ((int) p.tracks.size(), kMaxTracks);
    for (int i = 0; i < numTracks; ++i)
    {
        auto& t = p.tracks[(size_t) i];
        SnapTrack st;
        st.id = t.id;
        st.isPattern = t.type == TrackType::Pattern;
        st.gain = (t.mute || (anySolo && ! t.solo)) ? 0.0f : dbToGain (t.volumeDb);
        st.pan = t.pan;
        st.eqLow = t.eqLow; st.eqMid = t.eqMid; st.eqHigh = t.eqHigh;
        st.filter = t.filter; st.filterRes = t.filterRes;
        st.reverbSend = t.reverbSend; st.delaySend = t.delaySend;
        st.volAuto = t.volAuto; st.filterAuto = t.filterAuto;
        std::sort (st.volAuto.begin(), st.volAuto.end(), [] (auto& a, auto& b) { return a.beat < b.beat; });
        std::sort (st.filterAuto.begin(), st.filterAuto.end(), [] (auto& a, auto& b) { return a.beat < b.beat; });
        s->tracks.push_back (std::move (st));
    }

    for (size_t i = 0; i < p.patterns.size(); ++i)
    {
        SnapPattern sp;
        sp.id = p.patterns[i].id;
        sp.length = juce::jlimit (1, kMaxSteps, p.patterns[i].lengthSteps);
        sp.vel = p.patterns[i].vel;
        if (sp.id == p.currentPatternId) s->currentPatternIndex = (int) i;
        s->patterns.push_back (std::move (sp));
    }
    s->liveTrackSlot = juce::jmax (0, juce::jmin (numTracks - 1, p.trackIndex (p.liveTrackId)));

    if (stretch) stretch->beginRound();

    for (auto& c : p.clips)
    {
        const int slot = p.trackIndex (c.trackId);
        if (slot < 0 || slot >= numTracks || c.muted) continue;
        SnapClip sc;
        sc.trackSlot = slot;
        sc.startBeat = c.startBeat;
        sc.endBeat = p.clipEndBeat (c);
        if (sc.endBeat <= sc.startBeat) continue;

        if (c.kind == ClipKind::Pattern)
        {
            sc.isPattern = true;
            for (size_t i = 0; i < p.patterns.size(); ++i)
                if (p.patterns[i].id == c.patternId) sc.patternIndex = (int) i;
            if (sc.patternIndex < 0) continue;
            sc.patternOffsetBeats = c.patternOffsetBeats;
        }
        else
        {
            auto src = sources.get (c.sourceId);
            if (src == nullptr || src->buffer == nullptr) continue;
            sc.dataSR = src->sampleRate;
            sc.reverse = c.reverse;
            sc.gain = dbToGain (c.gainDb);
            sc.fadeInBeats = c.fadeInBeats;
            sc.fadeOutBeats = c.fadeOutBeats;
            const double r = p.clipRate (c);
            bool done = false;
            if (c.keepPitch)
            {
                const double tr = 1.0 / r;
                const double ps = std::pow (2.0, c.semitones / 12.0);
                if (std::abs (tr - 1.0) > 1.0e-4 || std::abs (ps - 1.0) > 1.0e-4)
                {
                    auto key = StretchCache::makeKey (c.sourceId, tr, ps);
                    if (stretch)
                        if (auto b = stretch->request (key))
                        {
                            const double trq = key.ratioQ / 10000.0;
                            sc.data = b;
                            sc.varispeed = 1.0;
                            sc.dStart = c.sourceOffsetSec * sc.dataSR * trq;
                            sc.lenFrames = c.sourceLenSec * sc.dataSR * trq;
                            // pequeno ajuste: a duracao na linha do tempo vem do modelo
                            done = true;
                        }
                    if (! done) s->pendingStretches++;
                }
            }
            if (! done)
            {
                sc.data = src->buffer;
                sc.varispeed = r;
                sc.dStart = c.sourceOffsetSec * sc.dataSR;
                sc.lenFrames = c.sourceLenSec * sc.dataSR;
            }
        }
        s->clips.push_back (std::move (sc));
    }

    if (stretch) stretch->endRound();

    // crossfades automaticos em clipes sobrepostos na mesma faixa
    for (int slot = 0; slot < numTracks; ++slot)
    {
        std::vector<SnapClip*> list;
        for (auto& c : s->clips) if (c.trackSlot == slot && ! c.isPattern) list.push_back (&c);
        std::sort (list.begin(), list.end(), [] (auto* a, auto* b) { return a->startBeat < b->startBeat; });
        for (size_t i = 0; i + 1 < list.size(); ++i)
            for (size_t j = i + 1; j < list.size(); ++j)
            {
                auto* a = list[i]; auto* b = list[j];
                if (b->startBeat >= a->endBeat) break;
                if (b->endBeat <= a->endBeat) continue; // contido: sem crossfade automatico
                const double ov = a->endBeat - b->startBeat;
                a->fadeOutBeats = std::max (a->fadeOutBeats, ov);
                b->fadeInBeats = std::max (b->fadeInBeats, ov);
            }
    }

    for (int i = 0; i < kNumPads; ++i)
    {
        auto& pad = p.pads[(size_t) i];
        auto& sp = s->pads[(size_t) i];
        if (! pad.isEmpty())
        {
            auto src = sources.get (pad.sourceId);
            sp.buf = padCache.getForPad (i, pad, src);
            if (src) sp.bufSR = src->sampleRate;
        }
        sp.gain = dbToGain (pad.gainDb);
        sp.pan = pad.pan;
        sp.pitchRatio = std::pow (2.0, (pad.semitones + pad.fineCents / 100.0) / 12.0);
        sp.attackMs = pad.attackMs;
        sp.releaseMs = pad.releaseMs;
        sp.mode = pad.mode;
        sp.choke = pad.chokeGroup;
        sp.cutoff = pad.cutoff;
        sp.resonance = pad.resonance;
    }

    s->songEndBeat = p.songEndBeat();
    return s;
}

} // namespace mp
