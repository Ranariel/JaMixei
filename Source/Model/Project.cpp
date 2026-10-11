#include "../I18n.h"
#include "Project.h"

namespace mp
{
static const juce::uint32 kPalette[] = {
    0xff4fc3f7, 0xffff8a3d, 0xff9ccc65, 0xffba68c8, 0xffffd54f, 0xff4db6ac,
    0xfff06292, 0xff7986cb, 0xffa1887f, 0xffe57373, 0xff81c784, 0xff64b5f6
};

juce::Colour Project::trackColour (int index)
{
    return juce::Colour (kPalette[(size_t) (index % (int) std::size (kPalette))]);
}

void Project::reset()
{
    nextId = 1;
    bpm = 120; swing = 50; beatsPerBar = 4;
    loopOn = false; loopStart = 0; loopEnd = 16;
    metronome = false; masterVolDb = 0;
    reverbSize = 0.6f; reverbDamp = 0.4f; reverbWidth = 1.0f;
    delayBeats = 0.75; delayFeedback = 0.35f; delayTone = 0.6f;
    sources.clear(); tracks.clear(); clips.clear(); patterns.clear();
    projectFile = juce::File();

    for (int i = 0; i < kNumPads; ++i)
    {
        pads[(size_t) i] = Pad();
        pads[(size_t) i].name = juce::String::charToString ((juce::juce_wchar) ('A' + i / kPadsPerBank)) + juce::String (i % kPadsPerBank + 1);
        pads[(size_t) i].colour = trackColour (i % kPadsPerBank + 1).withMultipliedSaturation (0.9f);
    }

    auto& pt = addTrack (TrackType::Pattern, TR ("Pads / Batida"));
    liveTrackId = pt.id;
    addTrack (TrackType::Audio, TR ("Faixa 1"));
    addTrack (TrackType::Audio, TR ("Faixa 2"));
    addTrack (TrackType::Audio, TR ("Faixa 3"));
    auto& p = addPattern (TR ("Padrão 1"));
    currentPatternId = p.id;
}

Track& Project::addTrack (TrackType type, const juce::String& name)
{
    Track t;
    t.id = newId();
    t.name = name;
    t.type = type;
    t.colour = type == TrackType::Pattern ? juce::Colour (0xffff8a3d) : trackColour ((int) tracks.size());
    tracks.push_back (t);
    return tracks.back();
}

Pattern& Project::addPattern (const juce::String& name)
{
    Pattern p;
    p.id = newId();
    p.name = name;
    patterns.push_back (p);
    return patterns.back();
}

Track* Project::findTrack (int id)            { for (auto& t : tracks) if (t.id == id) return &t; return nullptr; }
const Track* Project::findTrack (int id) const { for (auto& t : tracks) if (t.id == id) return &t; return nullptr; }
int Project::trackIndex (int id) const         { for (size_t i = 0; i < tracks.size(); ++i) if (tracks[i].id == id) return (int) i; return -1; }
Clip* Project::findClip (int id)               { for (auto& c : clips) if (c.id == id) return &c; return nullptr; }
Pattern* Project::findPattern (int id)         { for (auto& p : patterns) if (p.id == id) return &p; return nullptr; }
const Pattern* Project::findPattern (int id) const { for (auto& p : patterns) if (p.id == id) return &p; return nullptr; }
SourceInfo* Project::findSource (int id)       { for (auto& s : sources) if (s.id == id) return &s; return nullptr; }
const SourceInfo* Project::findSource (int id) const { for (auto& s : sources) if (s.id == id) return &s; return nullptr; }

double Project::clipRate (const Clip& c) const
{
    double r = 1.0;
    if (c.warp && c.sourceBpm > 1.0) r = bpm / c.sourceBpm;
    if (! c.keepPitch) r *= std::pow (2.0, c.semitones / 12.0);
    return juce::jlimit (0.05, 20.0, r);
}

double Project::clipLengthBeats (const Clip& c) const
{
    if (c.kind == ClipKind::Pattern) return c.lengthBeatsPattern;
    return secondsToBeats (c.sourceLenSec / clipRate (c));
}

double Project::songEndBeat() const
{
    double e = 0;
    for (auto& c : clips) e = std::max (e, clipEndBeat (c));
    return e;
}

// ---------------------------------------------------------------------------
//  Serializacao
// ---------------------------------------------------------------------------
using DO = juce::DynamicObject;

static juce::var autoToVar (const std::vector<AutoPoint>& pts)
{
    juce::Array<juce::var> a;
    for (auto& p : pts) { a.add (p.beat); a.add (p.value); }
    return a;
}
static std::vector<AutoPoint> autoFromVar (const juce::var& v)
{
    std::vector<AutoPoint> r;
    if (auto* a = v.getArray())
        for (int i = 0; i + 1 < a->size(); i += 2)
            r.push_back ({ (double) (*a)[i], (float) (double) (*a)[i + 1] });
    return r;
}

juce::var Project::toVar() const
{
    auto* root = new DO();
    root->setProperty ("format", "MeuPadStudio");
    root->setProperty ("version", 1);
    root->setProperty ("nextId", nextId);
    root->setProperty ("bpm", bpm);
    root->setProperty ("swing", swing);
    root->setProperty ("beatsPerBar", beatsPerBar);
    root->setProperty ("loopOn", loopOn);
    root->setProperty ("loopStart", loopStart);
    root->setProperty ("loopEnd", loopEnd);
    root->setProperty ("metronome", metronome);
    root->setProperty ("masterVolDb", masterVolDb);
    root->setProperty ("reverbSize", reverbSize);
    root->setProperty ("reverbDamp", reverbDamp);
    root->setProperty ("reverbWidth", reverbWidth);
    root->setProperty ("delayBeats", delayBeats);
    root->setProperty ("delayFeedback", delayFeedback);
    root->setProperty ("delayTone", delayTone);
    root->setProperty ("currentPatternId", currentPatternId);
    root->setProperty ("liveTrackId", liveTrackId);

    juce::Array<juce::var> srcs;
    for (auto& s : sources)
    {
        auto* o = new DO();
        o->setProperty ("id", s.id); o->setProperty ("path", s.path); o->setProperty ("relPath", s.relPath); o->setProperty ("name", s.name);
        o->setProperty ("bpm", s.bpm); o->setProperty ("bpmDetected", s.bpmDetected);
        srcs.add (juce::var (o));
    }
    root->setProperty ("sources", srcs);

    juce::Array<juce::var> trs;
    for (auto& t : tracks)
    {
        auto* o = new DO();
        o->setProperty ("id", t.id); o->setProperty ("name", t.name); o->setProperty ("type", (int) t.type);
        o->setProperty ("colour", (juce::int64) t.colour.getARGB());
        o->setProperty ("volumeDb", t.volumeDb); o->setProperty ("pan", t.pan);
        o->setProperty ("mute", t.mute); o->setProperty ("solo", t.solo);
        o->setProperty ("eqLow", t.eqLow); o->setProperty ("eqMid", t.eqMid); o->setProperty ("eqHigh", t.eqHigh);
        o->setProperty ("filter", t.filter); o->setProperty ("filterRes", t.filterRes);
        o->setProperty ("reverbSend", t.reverbSend); o->setProperty ("delaySend", t.delaySend);
        o->setProperty ("autoView", (int) t.autoView);
        o->setProperty ("volAuto", autoToVar (t.volAuto));
        o->setProperty ("filterAuto", autoToVar (t.filterAuto));
        o->setProperty ("height", t.height);
        trs.add (juce::var (o));
    }
    root->setProperty ("tracks", trs);

    juce::Array<juce::var> cls;
    for (auto& c : clips)
    {
        auto* o = new DO();
        o->setProperty ("id", c.id); o->setProperty ("trackId", c.trackId); o->setProperty ("kind", (int) c.kind);
        o->setProperty ("startBeat", c.startBeat); o->setProperty ("name", c.name);
        o->setProperty ("colour", (juce::int64) c.colour.getARGB()); o->setProperty ("muted", c.muted);
        o->setProperty ("sourceId", c.sourceId); o->setProperty ("sourceOffsetSec", c.sourceOffsetSec);
        o->setProperty ("sourceLenSec", c.sourceLenSec); o->setProperty ("gainDb", c.gainDb);
        o->setProperty ("fadeInBeats", c.fadeInBeats); o->setProperty ("fadeOutBeats", c.fadeOutBeats);
        o->setProperty ("warp", c.warp); o->setProperty ("sourceBpm", c.sourceBpm);
        o->setProperty ("semitones", c.semitones); o->setProperty ("keepPitch", c.keepPitch);
        o->setProperty ("reverse", c.reverse);
        o->setProperty ("patternId", c.patternId); o->setProperty ("lengthBeatsPattern", c.lengthBeatsPattern);
        o->setProperty ("patternOffsetBeats", c.patternOffsetBeats);
        cls.add (juce::var (o));
    }
    root->setProperty ("clips", cls);

    juce::Array<juce::var> pds;
    for (auto& p : pads)
    {
        auto* o = new DO();
        o->setProperty ("sourceId", p.sourceId); o->setProperty ("startSec", p.startSec); o->setProperty ("endSec", p.endSec);
        o->setProperty ("name", p.name); o->setProperty ("gainDb", p.gainDb); o->setProperty ("pan", p.pan);
        o->setProperty ("semitones", p.semitones); o->setProperty ("fineCents", p.fineCents);
        o->setProperty ("attackMs", p.attackMs); o->setProperty ("releaseMs", p.releaseMs);
        o->setProperty ("mode", (int) p.mode); o->setProperty ("chokeGroup", p.chokeGroup);
        o->setProperty ("reverse", p.reverse); o->setProperty ("cutoff", p.cutoff); o->setProperty ("resonance", p.resonance);
        o->setProperty ("colour", (juce::int64) p.colour.getARGB());
        pds.add (juce::var (o));
    }
    root->setProperty ("pads", pds);

    juce::Array<juce::var> pts;
    for (auto& p : patterns)
    {
        auto* o = new DO();
        o->setProperty ("id", p.id); o->setProperty ("name", p.name); o->setProperty ("lengthSteps", p.lengthSteps);
        // notas esparsas: [pad, step, vel, ...]
        juce::Array<juce::var> notes;
        for (int pad = 0; pad < kNumPads; ++pad)
            for (int s = 0; s < kMaxSteps; ++s)
                if (auto v = p.get (pad, s)) { notes.add (pad); notes.add (s); notes.add ((int) v); }
        o->setProperty ("notes", notes);
        pts.add (juce::var (o));
    }
    root->setProperty ("patterns", pts);
    return juce::var (root);
}

template <typename T> static T getOr (const juce::var& o, const char* name, T def)
{
    auto v = o.getProperty (name, juce::var());
    if (v.isVoid()) return def;
    if constexpr (std::is_same_v<T, bool>) return (bool) v;
    else if constexpr (std::is_same_v<T, int>) return (int) v;
    else if constexpr (std::is_same_v<T, float>) return (float) (double) v;
    else if constexpr (std::is_same_v<T, double>) return (double) v;
    else return T (v.toString());
}
static juce::Colour colourOr (const juce::var& o, const char* name, juce::Colour def)
{
    auto v = o.getProperty (name, juce::var());
    return v.isVoid() ? def : juce::Colour ((juce::uint32) (juce::int64) v);
}

void Project::fromVar (const juce::var& r)
{
    sources.clear(); tracks.clear(); clips.clear(); patterns.clear();
    nextId = getOr (r, "nextId", 1);
    bpm = getOr (r, "bpm", 120.0);
    swing = getOr (r, "swing", 50.0f);
    beatsPerBar = getOr (r, "beatsPerBar", 4);
    loopOn = getOr (r, "loopOn", false);
    loopStart = getOr (r, "loopStart", 0.0);
    loopEnd = getOr (r, "loopEnd", 16.0);
    metronome = getOr (r, "metronome", false);
    masterVolDb = getOr (r, "masterVolDb", 0.0f);
    reverbSize = getOr (r, "reverbSize", 0.6f);
    reverbDamp = getOr (r, "reverbDamp", 0.4f);
    reverbWidth = getOr (r, "reverbWidth", 1.0f);
    delayBeats = getOr (r, "delayBeats", 0.75);
    delayFeedback = getOr (r, "delayFeedback", 0.35f);
    delayTone = getOr (r, "delayTone", 0.6f);
    currentPatternId = getOr (r, "currentPatternId", -1);
    liveTrackId = getOr (r, "liveTrackId", -1);

    if (auto* a = r.getProperty ("sources", {}).getArray())
        for (auto& o : *a)
        {
            SourceInfo s;
            s.id = getOr (o, "id", 0); s.path = getOr (o, "path", juce::String()); s.relPath = getOr (o, "relPath", juce::String()); s.name = getOr (o, "name", juce::String());
            s.bpm = getOr (o, "bpm", 120.0); s.bpmDetected = getOr (o, "bpmDetected", false);
            sources.push_back (s);
        }
    if (auto* a = r.getProperty ("tracks", {}).getArray())
        for (auto& o : *a)
        {
            Track t;
            t.id = getOr (o, "id", 0); t.name = getOr (o, "name", juce::String()); t.type = (TrackType) getOr (o, "type", 0);
            t.colour = colourOr (o, "colour", t.colour);
            t.volumeDb = getOr (o, "volumeDb", 0.0f); t.pan = getOr (o, "pan", 0.0f);
            t.mute = getOr (o, "mute", false); t.solo = getOr (o, "solo", false);
            t.eqLow = getOr (o, "eqLow", 0.0f); t.eqMid = getOr (o, "eqMid", 0.0f); t.eqHigh = getOr (o, "eqHigh", 0.0f);
            t.filter = getOr (o, "filter", 0.0f); t.filterRes = getOr (o, "filterRes", 0.2f);
            t.reverbSend = getOr (o, "reverbSend", 0.0f); t.delaySend = getOr (o, "delaySend", 0.0f);
            t.autoView = (AutoParam) getOr (o, "autoView", 0);
            t.volAuto = autoFromVar (o.getProperty ("volAuto", {}));
            t.filterAuto = autoFromVar (o.getProperty ("filterAuto", {}));
            t.height = getOr (o, "height", 84.0f);
            tracks.push_back (t);
        }
    if (auto* a = r.getProperty ("clips", {}).getArray())
        for (auto& o : *a)
        {
            Clip c;
            c.id = getOr (o, "id", 0); c.trackId = getOr (o, "trackId", 0); c.kind = (ClipKind) getOr (o, "kind", 0);
            c.startBeat = getOr (o, "startBeat", 0.0); c.name = getOr (o, "name", juce::String());
            c.colour = colourOr (o, "colour", c.colour); c.muted = getOr (o, "muted", false);
            c.sourceId = getOr (o, "sourceId", -1); c.sourceOffsetSec = getOr (o, "sourceOffsetSec", 0.0);
            c.sourceLenSec = getOr (o, "sourceLenSec", 0.0); c.gainDb = getOr (o, "gainDb", 0.0f);
            c.fadeInBeats = getOr (o, "fadeInBeats", 0.0); c.fadeOutBeats = getOr (o, "fadeOutBeats", 0.0);
            c.warp = getOr (o, "warp", false); c.sourceBpm = getOr (o, "sourceBpm", 120.0);
            c.semitones = getOr (o, "semitones", 0.0f); c.keepPitch = getOr (o, "keepPitch", true);
            c.reverse = getOr (o, "reverse", false);
            c.patternId = getOr (o, "patternId", -1); c.lengthBeatsPattern = getOr (o, "lengthBeatsPattern", 4.0);
            c.patternOffsetBeats = getOr (o, "patternOffsetBeats", 0.0);
            clips.push_back (c);
        }
    if (auto* a = r.getProperty ("pads", {}).getArray())
        for (int i = 0; i < juce::jmin (kNumPads, a->size()); ++i)
        {
            auto o = (*a)[i];
            Pad p;
            p.sourceId = getOr (o, "sourceId", -1); p.startSec = getOr (o, "startSec", 0.0); p.endSec = getOr (o, "endSec", 0.0);
            p.name = getOr (o, "name", juce::String()); p.gainDb = getOr (o, "gainDb", 0.0f); p.pan = getOr (o, "pan", 0.0f);
            p.semitones = getOr (o, "semitones", 0.0f); p.fineCents = getOr (o, "fineCents", 0.0f);
            p.attackMs = getOr (o, "attackMs", 1.0f); p.releaseMs = getOr (o, "releaseMs", 40.0f);
            p.mode = (PadMode) getOr (o, "mode", 0); p.chokeGroup = getOr (o, "chokeGroup", 0);
            p.reverse = getOr (o, "reverse", false); p.cutoff = getOr (o, "cutoff", 20000.0f); p.resonance = getOr (o, "resonance", 0.1f);
            p.colour = colourOr (o, "colour", p.colour);
            pads[(size_t) i] = p;
        }
    if (auto* a = r.getProperty ("patterns", {}).getArray())
        for (auto& o : *a)
        {
            Pattern p;
            p.id = getOr (o, "id", 0); p.name = getOr (o, "name", juce::String());
            p.lengthSteps = juce::jlimit (1, kMaxSteps, getOr (o, "lengthSteps", 16));
            if (auto* n = o.getProperty ("notes", {}).getArray())
                for (int i = 0; i + 2 < n->size(); i += 3)
                {
                    int pad = (*n)[i], st = (*n)[i + 1], v = (*n)[i + 2];
                    if (pad >= 0 && pad < kNumPads && st >= 0 && st < kMaxSteps) p.set (pad, st, (uint8_t) juce::jlimit (0, 127, v));
                }
            patterns.push_back (p);
        }

    // garantias minimas
    if (patterns.empty()) { auto& p = addPattern (TR ("Padrão 1")); currentPatternId = p.id; }
    if (findPattern (currentPatternId) == nullptr) currentPatternId = patterns.front().id;
    if (findTrack (liveTrackId) == nullptr)
    {
        liveTrackId = -1;
        for (auto& t : tracks) if (t.type == TrackType::Pattern) { liveTrackId = t.id; break; }
        if (liveTrackId < 0) { auto& t = addTrack (TrackType::Pattern, TR ("Pads / Batida")); liveTrackId = t.id; }
    }
    int maxId = 0;
    for (auto& s : sources) maxId = std::max (maxId, s.id);
    for (auto& t : tracks) maxId = std::max (maxId, t.id);
    for (auto& c : clips) maxId = std::max (maxId, c.id);
    for (auto& p : patterns) maxId = std::max (maxId, p.id);
    nextId = std::max (nextId, maxId + 1);
}

juce::String Project::toJson() const
{
    return juce::JSON::toString (toVar(), false);
}

bool Project::fromJson (const juce::String& json)
{
    auto v = juce::JSON::parse (json);
    if (! v.isObject()) return false;
    auto keepFile = projectFile;
    fromVar (v);
    projectFile = keepFile;
    return true;
}

} // namespace mp
