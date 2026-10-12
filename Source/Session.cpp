#include "I18n.h"
#include "Session.h"
#include "Audio/Instruments.h"
#include "Audio/Analysis.h"
#include "Audio/Mp3Format.h"

namespace mp
{
Session::Session()
{
    juce::PropertiesFile::Options o;
    o.applicationName = "MeuPadStudio";
    o.filenameSuffix = ".settings";
    o.folderName = "MeuPadStudio";
    o.osxLibrarySubFolder = "Application Support";
    props.setStorageParameters (o);

    stretch = std::make_unique<StretchCache> (sources);
    stretch->onJobFinished = [this] { rebuildSnapshot(); notify (kChClips); };
    engine.onSampleRateChanged = [this] { rebuildSnapshot(); };

    std::unique_ptr<juce::XmlElement> devState (props.getUserSettings()->getXmlValue ("audioDevice"));
    engine.initialise (devState.get());
    engine.enableAllMidiInputs();
    bank.onLoaded = [this] (instruments::Id) { rebuildSnapshot(); notify (kChPatterns); };
    rebuildSnapshot();
    startTimerHz (30);
}

Session::~Session()
{
    *aliveFlag = false;
    stopTimer();
    if (auto xml = engine.devices().createStateXml())
        props.getUserSettings()->setValue ("audioDevice", xml.get());
    props.saveIfNeeded();
    engine.shutdown();
    stretch.reset();
}

void Session::notify (int what)
{
    listeners.call ([what] (Listener& l) { l.sessionChanged (what); });
}

SnapshotPtr Session::buildSnapshot (double sampleRate)
{
    for (auto& c : project.channels) bank.request ((instruments::Id) c.instrument);   // gera em segundo plano
    return SnapshotBuilder::build (project, sources, stretch.get(), padCache, sampleRate, &bank);
}

void Session::rebuildSnapshot()
{
    auto snap = buildSnapshot (engine.getSampleRate());
    engine.setSnapshot (snap);
    graveyard.push_back (snap);
    graveyard.erase (std::remove_if (graveyard.begin(), graveyard.end() - 1,
                                     [] (const SnapshotPtr& s) { return s.use_count() <= 1; }),
                     graveyard.end() - 1);
}

void Session::beginEdit (const juce::String& label)
{
    history.push (project.toJson(), label);
    dirty = true;
}

void Session::commit (int what)
{
    dirty = true;
    rebuildSnapshot();
    notify (what);
}

void Session::undo()
{
    if (! history.canUndo()) return;
    const auto label = history.undoLabel();
    project.fromJson (history.undo (project.toJson()));
    selectedClips.clear();
    reloadAllSources();
    setStatus (TR ("Desfeito: ") + label);
    commit (kChAll);
}

void Session::redo()
{
    if (! history.canRedo()) return;
    const auto label = history.redoLabel();
    project.fromJson (history.redo (project.toJson()));
    selectedClips.clear();
    reloadAllSources();
    setStatus (TR ("Refeito: ") + label);
    commit (kChAll);
}

// ---------------------------------------------------------------------------
juce::String Session::sourceName (int id) const
{
    if (auto* s = project.findSource (id)) return s->name;
    return "?";
}

void Session::loadSourceInfo (SourceInfo& info)
{
    juce::File f;
    if (juce::File::isAbsolutePath (info.path)) f = juce::File (info.path);
    if (! f.existsAsFile() && info.relPath.isNotEmpty() && project.projectFile != juce::File())
    {
        auto rel = project.projectFile.getParentDirectory().getChildFile (info.relPath);
        if (rel.existsAsFile()) { f = rel; info.path = rel.getFullPathName(); }
    }
    if (! f.existsAsFile())
    {
        setStatus (TR ("Arquivo de áudio não encontrado: ") + info.path);
        return;
    }
    const int id = info.id;
    sources.load (id, f, ! info.bpmDetected, [this, id] (SourcePtr s)
    {
        if (! s->ready) { setStatus (s->error); notify (kChSources); return; }
        if (auto* si = project.findSource (id))
            if (! si->bpmDetected && s->detectedBpm > 0) { si->bpm = s->detectedBpm; si->bpmDetected = true; }
        rebuildSnapshot();
        notify (kChSources | kChClips | kChPads);
    });
}

void Session::reloadAllSources()
{
    for (auto& s : project.sources)
        if (sources.get (s.id) == nullptr && ! sources.isLoading (s.id))
            loadSourceInfo (s);
}

int Session::importFile (const juce::File& f, std::function<void (int)> onReady)
{
    for (auto& s : project.sources)
        if (juce::File (s.path) == f)
        {
            const int id = s.id;
            if (sources.get (id) != nullptr) { if (onReady) onReady (id); }
            else if (! sources.isLoading (id)) loadSourceInfo (s);
            return id;
        }

    SourceInfo info;
    info.id = project.newId();
    info.path = f.getFullPathName();
    info.name = f.getFileNameWithoutExtension();
    info.bpm = project.bpm;
    project.sources.push_back (info);
    const int id = info.id;
    setStatus (TR ("Carregando ") + f.getFileName() + "...");
    notify (kChSources);

    sources.load (id, f, true, [this, id, onReady, name = f.getFileName()] (SourcePtr s)
    {
        if (! s->ready)
        {
            setStatus (TR ("Erro: ") + s->error);
            project.sources.erase (std::remove_if (project.sources.begin(), project.sources.end(),
                                                   [id] (auto& x) { return x.id == id; }), project.sources.end());
            notify (kChSources);
            return;
        }
        if (auto* si = project.findSource (id))
        {
            if (s->detectedBpm > 0) { si->bpm = s->detectedBpm; si->bpmDetected = true; }
            const int secs = (int) s->lengthSec;
            setStatus (name + TR (" carregado  |  ") + juce::String (secs / 60) + ":" + juce::String (secs % 60).paddedLeft ('0', 2)
                       + TR ("  |  BPM detectado: ") + (s->detectedBpm > 0 ? juce::String (s->detectedBpm, 2) : juce::String ("?")));
        }
        dirty = true;
        rebuildSnapshot();
        notify (kChSources | kChClips | kChPads);
        if (onReady) onReady (id);
    });
    return id;
}

// ---------------------------------------------------------------------------
int Session::trackIdAt (int index) const
{
    return juce::isPositiveAndBelow (index, (int) project.tracks.size()) ? project.tracks[(size_t) index].id : -1;
}

int Session::addAudioClip (int trackId, int sourceId, double beat, double offSec, double lenSec, bool adoptBpm)
{
    auto src = getSource (sourceId);
    auto* info = project.findSource (sourceId);
    auto* track = project.findTrack (trackId);
    if (src == nullptr || info == nullptr || track == nullptr || track->type != TrackType::Audio) return -1;

    bool firstAudio = true;
    for (auto& c : project.clips) if (c.kind == ClipKind::Audio) { firstAudio = false; break; }

    Clip c;
    c.id = project.newId();
    c.trackId = trackId;
    c.kind = ClipKind::Audio;
    c.startBeat = juce::jmax (0.0, beat);
    c.sourceId = sourceId;
    c.sourceOffsetSec = juce::jlimit (0.0, src->lengthSec, offSec);
    c.sourceLenSec = lenSec < 0 ? src->lengthSec - c.sourceOffsetSec : juce::jmin (lenSec, src->lengthSec - c.sourceOffsetSec);
    if (c.sourceLenSec < 0.005) return -1;
    c.name = info->name;
    c.colour = track->colour;
    c.sourceBpm = info->bpm;
    c.warp = info->bpmDetected;
    c.keepPitch = true;
    if (adoptBpm && firstAudio && info->bpmDetected && info->bpm > 40)
    {
        project.bpm = info->bpm;  // primeira musica define o andamento do projeto (como um DJ)
        setStatus (TR ("BPM do projeto ajustado para ") + juce::String (info->bpm, 2) + TR (" (BPM da música)"));
    }
    project.clips.push_back (c);
    return c.id;
}

int Session::addPatternClip (int trackId, int patternId, double beat)
{
    auto* pat = project.findPattern (patternId);
    auto* track = project.findTrack (trackId);
    if (pat == nullptr || track == nullptr || track->type != TrackType::Pattern) return -1;
    Clip c;
    c.id = project.newId();
    c.trackId = trackId;
    c.kind = ClipKind::Pattern;
    c.startBeat = juce::jmax (0.0, beat);
    c.patternId = patternId;
    c.lengthBeatsPattern = pat->lengthBeats();
    c.name = pat->name;
    c.colour = track->colour;
    project.clips.push_back (c);
    return c.id;
}

void Session::deleteSelectedClips()
{
    if (selectedClips.empty()) return;
    beginEdit (TR ("Apagar clipes"));
    project.clips.erase (std::remove_if (project.clips.begin(), project.clips.end(),
                                         [this] (const Clip& c) { return selectedClips.count (c.id) > 0; }), project.clips.end());
    selectedClips.clear();
    commit (kChClips | kChSelection);
}

void Session::duplicateSelectedClips()
{
    if (selectedClips.empty()) return;
    double minS = 1e18, maxE = -1e18;
    for (auto& c : project.clips)
        if (selectedClips.count (c.id)) { minS = std::min (minS, c.startBeat); maxE = std::max (maxE, project.clipEndBeat (c)); }
    // arredonda o deslocamento para a grade do compasso quando possivel
    double span = maxE - minS;
    if (snapBeats > 0) span = std::ceil (span / snapBeats - 1e-6) * snapBeats;
    beginEdit (TR ("Duplicar clipes"));
    std::vector<Clip> added;
    for (auto& c : project.clips)
        if (selectedClips.count (c.id)) { Clip n = c; n.id = project.newId(); n.startBeat += span; added.push_back (n); }
    selectedClips.clear();
    for (auto& n : added) { project.clips.push_back (n); selectedClips.insert (n.id); }
    commit (kChClips | kChSelection);
}

void Session::splitClip (int clipId, double beat)
{
    auto* cp = project.findClip (clipId);
    if (cp == nullptr) return;
    Clip c = *cp;
    const double start = c.startBeat, end = project.clipEndBeat (c);
    if (beat <= start + 1e-4 || beat >= end - 1e-4) return;
    const double d = beat - start;
    Clip left = c, right = c;
    right.id = project.newId();
    right.startBeat = beat;
    if (c.kind == ClipKind::Audio)
    {
        const double consumed = project.beatsToSeconds (d) * project.clipRate (c);
        if (c.reverse)
        {
            left.sourceOffsetSec = c.sourceOffsetSec + c.sourceLenSec - consumed;
            left.sourceLenSec = consumed;
            right.sourceOffsetSec = c.sourceOffsetSec;
            right.sourceLenSec = c.sourceLenSec - consumed;
        }
        else
        {
            left.sourceLenSec = consumed;
            right.sourceOffsetSec = c.sourceOffsetSec + consumed;
            right.sourceLenSec = c.sourceLenSec - consumed;
        }
        left.fadeOutBeats = 0;
        right.fadeInBeats = 0;
        left.fadeInBeats = std::min (left.fadeInBeats, d);
        right.fadeOutBeats = std::min (right.fadeOutBeats, end - beat);
    }
    else
    {
        left.lengthBeatsPattern = d;
        right.lengthBeatsPattern = c.lengthBeatsPattern - d;
        right.patternOffsetBeats = c.patternOffsetBeats + d;
    }
    *cp = left;
    project.clips.push_back (right);
    if (selectedClips.count (clipId)) selectedClips.insert (right.id);
}

void Session::splitAt (double beat)
{
    std::vector<int> targets;
    auto contains = [&] (const Clip& c) { return beat > c.startBeat + 1e-4 && beat < project.clipEndBeat (c) - 1e-4; };
    for (auto& c : project.clips) if (selectedClips.count (c.id) && contains (c)) targets.push_back (c.id);
    if (targets.empty())
        for (auto& c : project.clips) if (c.trackId == selectedTrackId && contains (c)) targets.push_back (c.id);
    if (targets.empty())
        for (auto& c : project.clips) if (contains (c)) targets.push_back (c.id);
    if (targets.empty()) { setStatus (TR ("Nenhum clipe sob o cursor para cortar")); return; }
    beginEdit (TR ("Cortar clipe"));
    for (int id : targets) splitClip (id, beat);
    commit (kChClips);
}

void Session::copySelection()
{
    clipboard.clear();
    clipboardOrigin = 1e18;
    for (auto& c : project.clips)
        if (selectedClips.count (c.id)) { clipboard.push_back (c); clipboardOrigin = std::min (clipboardOrigin, c.startBeat); }
    if (! clipboard.empty()) setStatus (juce::String ((int) clipboard.size()) + TR (" clipe(s) copiado(s)"));
}

void Session::paste (double atBeat, int trackId)
{
    if (clipboard.empty()) return;
    beginEdit (TR ("Colar"));
    selectedClips.clear();
    auto* target = project.findTrack (trackId);
    for (auto c : clipboard)
    {
        c.id = project.newId();
        c.startBeat = atBeat + (c.startBeat - clipboardOrigin);
        if (clipboard.size() == 1 && target != nullptr
            && ((target->type == TrackType::Audio) == (c.kind == ClipKind::Audio)))
            c.trackId = target->id;
        if (project.findTrack (c.trackId) == nullptr) continue;
        project.clips.push_back (c);
        selectedClips.insert (c.id);
    }
    commit (kChClips | kChSelection);
}

void Session::setClipWarp (Clip& c, bool warp)
{
    c.warp = warp;
}

void Session::normaliseClip (int clipId)
{
    auto* c = project.findClip (clipId);
    if (c == nullptr || c->kind != ClipKind::Audio) return;
    auto src = getSource (c->sourceId);
    if (src == nullptr) return;
    const int s0 = (int) (c->sourceOffsetSec * src->sampleRate);
    const int n = (int) (c->sourceLenSec * src->sampleRate);
    const float pk = Analysis::peakLevel (*src->buffer, s0, n);
    if (pk <= 1e-6f) return;
    beginEdit (TR ("Normalizar clipe"));
    c->gainDb = juce::jlimit (-60.0f, 24.0f, -juce::Decibels::gainToDecibels (pk) - 0.3f);
    commit (kChClips);
}

// ---------------------------------------------------------------------------
int Session::addTrack (TrackType t)
{
    if ((int) project.tracks.size() >= kMaxTracks) { setStatus (TR ("Limite de faixas atingido")); return -1; }
    beginEdit (TR ("Nova faixa"));
    int n = 1;
    for (auto& tr : project.tracks) if (tr.type == t) ++n;
    auto& tr = project.addTrack (t, t == TrackType::Audio ? TR ("Faixa ") + juce::String (n) : TR ("Padrões ") + juce::String (n));
    const int id = tr.id;
    commit (kChTracks | kChMixer);
    return id;
}

void Session::deleteTrack (int trackId)
{
    if (project.findTrack (trackId) == nullptr) return;
    beginEdit (TR ("Apagar faixa"));
    project.clips.erase (std::remove_if (project.clips.begin(), project.clips.end(),
                                         [trackId] (const Clip& c) { return c.trackId == trackId; }), project.clips.end());
    project.tracks.erase (std::remove_if (project.tracks.begin(), project.tracks.end(),
                                          [trackId] (const Track& t) { return t.id == trackId; }), project.tracks.end());
    if (project.liveTrackId == trackId)
    {
        project.liveTrackId = -1;
        for (auto& t : project.tracks) if (t.type == TrackType::Pattern) { project.liveTrackId = t.id; break; }
        if (project.liveTrackId < 0) project.liveTrackId = project.addTrack (TrackType::Pattern, TR ("Pads / Batida")).id;
    }
    selectedClips.clear();
    commit (kChAll);
}

void Session::moveTrack (int trackId, int delta)
{
    const int i = project.trackIndex (trackId);
    const int j = i + delta;
    if (i < 0 || j < 0 || j >= (int) project.tracks.size()) return;
    beginEdit (TR ("Mover faixa"));
    std::swap (project.tracks[(size_t) i], project.tracks[(size_t) j]);
    commit (kChTracks | kChMixer | kChClips);
}

// ---------------------------------------------------------------------------
void Session::assignPad (int pad, int sourceId, double s, double e, const juce::String& name)
{
    if (! juce::isPositiveAndBelow (pad, kNumPads)) return;
    beginEdit (TR ("Atribuir pad"));
    auto& p = project.pads[(size_t) pad];
    p.sourceId = sourceId;
    p.startSec = juce::jmin (s, e);
    p.endSec = juce::jmax (s, e);
    if (name.isNotEmpty()) p.name = name;
    commit (kChPads);
}

void Session::loadInstrument (int instrument, int rootMidi, int scale, int bank, std::function<void (bool)> done)
{
    using namespace instruments;
    const auto id = (Id) juce::jlimit (0, (int) Id::count - 1, instrument);
    const bool allBanks = bank < 0;     // -1 = os 4 bancos: 64 notas seguidas (varias oitavas sem trocar de banco)
    bank = juce::jlimit (0, kNumBanks - 1, bank);
    const auto notes = scaleNotes (rootMidi, (Scale) juce::jlimit (0, (int) Scale::count - 1, scale), allBanks ? kNumPads : kPadsPerBank);
    const auto name = displayName (id);
    setStatus (TR ("Gerando o instrumento ") + name + "...");
    auto folder = dataFolder().getChildFile ("Instrumentos");
    auto alive = aliveFlag;
    juce::Thread::launch ([this, alive, id, notes, folder, name, bank, allBanks, done]
    {
        Rendered r;
        const bool ok = instruments::renderToFile (id, notes, folder, r);
        juce::MessageManager::callAsync ([this, alive, ok, r, id, name, bank, allBanks, done]
        {
            if (! *alive) return;
            if (! ok) { setStatus (TR ("Não foi possível gerar o instrumento")); if (done) done (false); return; }
            importFile (r.file, [this, r, id, name, bank, allBanks, done] (int sourceId)
            {
                const auto& in = info (id);
                beginEdit (TR ("Carregar instrumento"));
                const int first = allBanks ? 0 : bank * kPadsPerBank;
                const int count = allBanks ? kNumPads : kPadsPerBank;
                for (int i = 0; i < count && i < (int) r.regions.size(); ++i)
                {
                    auto& p = project.pads[(size_t) (first + i)];
                    p = Pad();
                    p.sourceId = sourceId;
                    p.startSec = r.regions[(size_t) i].first;
                    p.endSec = r.regions[(size_t) i].second;
                    p.name = name + " " + noteName (r.notes[(size_t) i]);
                    p.mode = in.sustained ? PadMode::Gate : PadMode::OneShot;
                    p.releaseMs = in.sustained ? 220.0f : 60.0f;
                    // baixos sao monofonicos: uma nota corta a outra
                    p.chokeGroup = (id == Id::Bass808 || id == Id::SynthBass) ? 8 : 0;
                    p.colour = juce::Colour (in.colour);
                }
                if (auto* si = project.findSource (sourceId)) { si->name = name; si->bpm = project.bpm; si->bpmDetected = false; }
                currentBank = allBanks ? 0 : bank;
                selectedPad = first;
                commit (kChPads | kChSources);
                setStatus (name + (allBanks ? TR (" carregado nos 4 bancos (64 notas). No Sequenciador, ligue \"Todos os bancos\" para ver todas") : TR (" carregado no banco ") + juce::String::charToString ((juce::juce_wchar) ('A' + bank)))
                           + TR ("  |  toque nos pads ou no teclado (Z X C V / A S D F / Q W E R / 1 2 3 4)"));
                if (done) done (true);
            });
        });
    });
}

// ---------------------------------------------------------------------------
//  Instrumentos
// ---------------------------------------------------------------------------
float Session::defaultRelease (int instrument)
{
    using namespace instruments;
    switch ((Id) instrument)
    {
        case Id::Piano: case Id::ElectricPiano: return 320.0f;
        case Id::Organ:     return 90.0f;
        case Id::SynthLead: return 160.0f;
        case Id::SynthPad:  return 700.0f;
        case Id::Pluck:     return 450.0f;
        case Id::Bass808:   return 250.0f;
        case Id::SynthBass: return 90.0f;
        case Id::Strings:   return 550.0f;
        case Id::Brass:     return 180.0f;
        case Id::Bells:     return 1500.0f;
        case Id::Marimba:   return 700.0f;
        default: break;
    }
    return 250.0f;
}

int Session::addChannel (int instrument)
{
    using namespace instruments;
    instrument = juce::jlimit (0, (int) Id::count - 1, instrument);
    beginEdit (TR ("Adicionar instrumento"));
    auto name = displayName ((Id) instrument);
    int same = 0;
    for (auto& c : project.channels) if (c.instrument == instrument) ++same;
    if (same > 0) name << " " << (same + 1);
    auto& c = project.addChannel (instrument, name);
    c.colour = juce::Colour (info ((Id) instrument).colour);
    c.releaseMs = defaultRelease (instrument);
    project.currentChannelId = c.id;
    const int id = c.id;
    bank.request ((Id) instrument);
    commit (kChPatterns);
    return id;
}

void Session::removeChannel (int channelId)
{
    if (project.findChannel (channelId) == nullptr) return;
    beginEdit (TR ("Remover instrumento"));
    for (auto& p : project.patterns)
        p.notes.erase (std::remove_if (p.notes.begin(), p.notes.end(), [channelId] (const Note& n) { return n.channelId == channelId; }), p.notes.end());
    project.channels.erase (std::remove_if (project.channels.begin(), project.channels.end(),
                                            [channelId] (const InstrumentChannel& c) { return c.id == channelId; }), project.channels.end());
    if (project.findChannel (project.currentChannelId) == nullptr)
        project.currentChannelId = project.channels.empty() ? -1 : project.channels.front().id;
    commit (kChPatterns);
}

void Session::setChannelInstrument (int channelId, int instrument)
{
    auto* c = project.findChannel (channelId);
    if (c == nullptr || c->instrument == instrument) return;
    beginEdit (TR ("Trocar instrumento"));
    const bool autoName = c->name.startsWith (instruments::displayName ((instruments::Id) c->instrument));
    c->instrument = instrument;
    c->colour = juce::Colour (instruments::info ((instruments::Id) instrument).colour);
    c->releaseMs = defaultRelease (instrument);
    if (autoName) c->name = instruments::displayName ((instruments::Id) instrument);
    commit (kChPatterns);
}

void Session::selectChannel (int channelId)
{
    if (project.currentChannelId == channelId || project.findChannel (channelId) == nullptr) return;
    project.currentChannelId = channelId;
    rebuildSnapshot();
    notify (kChPatterns | kChSelection);
}

void Session::noteDown (int pitch, uint8_t vel)
{
    if (project.channels.empty()) addChannel ((int) instruments::Id::Piano);
    engine.noteDown (pitch, vel);
}

void Session::noteUp (int pitch) { engine.noteUp (pitch); }

void Session::finishRecordedNote (const RecordedNote& on, double endAbs)
{
    auto* p = project.findPattern (on.patternId);
    if (p == nullptr || project.findChannel (on.channelId) == nullptr) return;
    const double patLen = p->lengthBeats();
    double len = endAbs - on.absBeat;
    if (len <= 0) len += patLen;                       // o padrao deu a volta (modo PADRAO)
    len = juce::jlimit (1.0 / 32.0, patLen, len);
    double start = on.beat;
    if (noteQuantize > 0)
    {
        start = std::round (start / noteQuantize) * noteQuantize;
        len = juce::jmax (noteQuantize * 0.5, std::round (len / noteQuantize) * noteQuantize);
        if (start >= patLen - 1.0e-9) start = 0;    // arredondou para o fim: volta ao inicio do padrao
    }
    if (! recordUndoPushed) { beginEdit (TR ("Gravar notas")); recordUndoPushed = true; }
    // grava por cima: tira uma nota igual que comece no mesmo lugar
    p->notes.erase (std::remove_if (p->notes.begin(), p->notes.end(), [&] (const Note& n)
    {
        return n.channelId == on.channelId && n.pitch == on.pitch && std::abs (n.start - start) < 1.0e-6;
    }), p->notes.end());
    Note n;
    n.channelId = on.channelId;
    n.pitch = on.pitch;
    n.start = start;
    n.length = len;
    n.vel = on.vel;
    p->notes.push_back (n);
}

void Session::clearPad (int pad)
{
    if (! juce::isPositiveAndBelow (pad, kNumPads)) return;
    beginEdit (TR ("Limpar pad"));
    auto& p = project.pads[(size_t) pad];
    auto col = p.colour;
    p = Pad();
    p.colour = col;
    p.name = juce::String::charToString ((juce::juce_wchar) ('A' + pad / kPadsPerBank)) + juce::String (pad % kPadsPerBank + 1);
    commit (kChPads);
}

void Session::padDown (int pad, uint8_t vel)
{
    engine.padDown (pad, vel);
    if (selectedPad != pad)
    {
        selectedPad = pad;
        notify (kChSelection);
    }
}

void Session::padUp (int pad)
{
    engine.padUp (pad);
}

int Session::chopToPads (int sourceId, const std::vector<double>& points, double endSec, int firstPad)
{
    if (points.empty()) return 0;
    auto src = getSource (sourceId);
    if (src == nullptr) return 0;
    beginEdit (TR ("Fatiar para pads"));
    int count = 0;
    for (size_t i = 0; i < points.size() && firstPad + count < kNumPads; ++i)
    {
        const double s = points[i];
        const double e = i + 1 < points.size() ? points[i + 1] : endSec;
        if (e - s < 0.01) continue;
        auto& p = project.pads[(size_t) (firstPad + count)];
        p.sourceId = sourceId;
        p.startSec = s;
        p.endSec = e;
        p.name = TR ("Fatia ") + juce::String ((int) i + 1);
        ++count;
    }
    commit (kChPads);
    return count;
}

// ---------------------------------------------------------------------------
int Session::newPattern (bool duplicateCurrent)
{
    beginEdit (TR ("Novo padrão"));
    Pattern p;
    if (duplicateCurrent)
        if (auto* cur = currentPattern()) p = *cur;
    p.id = project.newId();
    p.name = TR ("Padrão ") + juce::String ((int) project.patterns.size() + 1);
    project.patterns.push_back (p);
    project.currentPatternId = p.id;
    commit (kChPatterns);
    return p.id;
}

void Session::setPatternLength (int steps)
{
    auto* p = currentPattern();
    steps = juce::jlimit (1, kMaxSteps, steps);
    if (p == nullptr || steps == p->lengthSteps) return;
    beginEdit (TR ("Tamanho do padrão"));
    const int old = p->lengthSteps;
    const double oldBeats = p->lengthBeats();
    if (steps > old)
        for (int pad = 0; pad < kNumPads; ++pad)
            for (int st = old; st < steps; ++st)
            {
                p->set (pad, st, p->get (pad, st % old));
                p->setLen (pad, st, p->get (pad, st) ? p->getLen (pad, st % old) : 0);
            }
    p->lengthSteps = steps;
    // clipes que mostravam o padrao inteiro acompanham o novo tamanho
    for (auto& c : project.clips)
        if (c.kind == ClipKind::Pattern && c.patternId == p->id && c.patternOffsetBeats == 0
            && std::abs (c.lengthBeatsPattern - oldBeats) < 1e-6)
            c.lengthBeatsPattern = p->lengthBeats();
    commit (kChPatterns | kChClips);
}

void Session::deletePattern (int id)
{
    if (project.patterns.size() <= 1) { setStatus (TR ("O projeto precisa de pelo menos um padrão")); return; }
    beginEdit (TR ("Apagar padrão"));
    project.clips.erase (std::remove_if (project.clips.begin(), project.clips.end(),
                                         [id] (const Clip& c) { return c.kind == ClipKind::Pattern && c.patternId == id; }), project.clips.end());
    project.patterns.erase (std::remove_if (project.patterns.begin(), project.patterns.end(),
                                            [id] (const Pattern& p) { return p.id == id; }), project.patterns.end());
    if (project.findPattern (project.currentPatternId) == nullptr) project.currentPatternId = project.patterns.front().id;
    commit (kChPatterns | kChClips);
}

// ---------------------------------------------------------------------------
void Session::togglePlay()
{
    if (isAudioRecording()) { stopAudioRecording(); return; }
    if (engine.isPlaying()) engine.stop();
    else engine.play();
    notify (kChTransport);
}

void Session::stopTransport()
{
    if (isAudioRecording()) { stopAudioRecording(); return; }
    if (engine.isPlaying()) engine.stop();
    else engine.seek (songMode && project.loopOn ? project.loopStart : 0.0);
    notify (kChTransport);
}

void Session::setSongMode (bool song)
{
    songMode = song;
    engine.setPatternMode (! song);
    if (! song) engine.seek (0);
    notify (kChTransport);
}

void Session::toggleRecord()
{
    recording = ! recording;
    recordUndoPushed = false;
    if (recording && songMode)
    {
        // sem padrao sob o cursor na faixa ao vivo: grava no modo PADRAO (loop do padrao atual)
        const double pos = engine.getPositionBeats();
        bool hasClip = false;
        for (auto& c : project.clips)
            if (c.kind == ClipKind::Pattern && c.trackId == project.liveTrackId && pos >= c.startBeat && pos < project.clipEndBeat (c))
                hasClip = true;
        if (! hasClip) setSongMode (false);
    }
    engine.setRecording (recording);
    if (recording && ! engine.isPlaying()) engine.play();
    setStatus (recording ? TR ("Gravando: toque os pads ou o teclado no ritmo (quantizado)") : TR ("Gravação parada"));
    notify (kChTransport);
}

// ---------------------------------------------------------------------------
juce::File Session::dataFolder()
{
    auto docs = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
    auto dir = docs.getChildFile ("Ja Mixei");
    auto old = docs.getChildFile ("MeuPadStudio");          // nome antigo do programa
    if (! dir.exists() && old.isDirectory()) old.moveFileTo (dir);
    return dir;
}

juce::File Session::recordingsFolder()
{
    return dataFolder().getChildFile (juce::String (juce::CharPointer_UTF8 ("Gravações")));
}

bool Session::startAudioRecording (const AudioRecOptions& o, juce::String& error)
{
    if (isAudioRecording()) return true;
    recOptions = o;
    if (o.fromInput) recOptions.mp3 = false;   // WAV mantem o alinhamento exato com a musica
    if (o.fromInput)
    {
        if (! engine.setInputEnabled (true, error)) return false;
        if (! songMode) setSongMode (true);
    }
    // loop atrapalha o alinhamento da gravacao: desliga durante a gravacao
    savedLoopOn = project.loopOn;
    if (project.loopOn && o.fromInput) { project.loopOn = false; rebuildSnapshot(); }

    const auto stamp = juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M-%S");
    const juce::String base = (o.fromInput ? TR ("Gravação ") : juce::String (TR ("Mix "))) + stamp;
    auto file = recordingsFolder().getNonexistentChildFile (base, recOptions.mp3 ? ".mp3" : ".wav", false);
    if (! engine.startAudioRecording (file, o.fromInput ? AudioEngine::RecSource::Input : AudioEngine::RecSource::Output,
                                      recOptions.mp3, o.monitor, error, recOptions.bits))
    {
        if (o.fromInput) { juce::String e2; engine.setInputEnabled (false, e2); }
        return false;
    }
    if (! engine.isPlaying()) engine.play();
    setStatus (TR ("Gravando áudio... (Espaço ou Parar para terminar)"));
    notify (kChTransport);
    return true;
}

void Session::stopAudioRecording()
{
    if (! isAudioRecording()) return;
    auto info = engine.stopAudioRecording();
    engine.stop();
    if (info.fromInput)
    {
        juce::String err;
        engine.setInputEnabled (false, err);
        if (savedLoopOn) { project.loopOn = true; rebuildSnapshot(); }
    }
    notify (kChTransport);
    if (! info.gotAudio)
    {
        info.file.deleteFile();
        setStatus (TR ("Gravação vazia (a música não chegou a tocar)"));
        return;
    }
    const bool fromInput = info.fromInput;
    const double startBeat = info.startBeat, latency = info.latencySec, recBpm = project.bpm;
    const juce::File file = info.file;
    importFile (file, [this, fromInput, startBeat, latency, recBpm, file] (int sourceId)
    {
        if (! fromInput)
        {
            setStatus (TR ("Mix salvo: ") + file.getFullPathName() + TR ("  (também está em Áudios no projeto)"));
            return;
        }
        // coloca a gravacao no arranjo, alinhada (compensa a latencia da placa de som)
        int trackId = -1;
        if (auto* t = project.findTrack (selectedTrackId)) if (t->type == TrackType::Audio) trackId = t->id;
        bool used = true;
        if (trackId >= 0)
        {
            used = false;
            for (auto& c : project.clips)
                if (c.trackId == trackId && project.clipEndBeat (c) > startBeat) used = true;
        }
        beginEdit (TR ("Gravação de áudio"));
        if (trackId < 0 || used)
        {
            int n = 1;
            for (auto& t : project.tracks) if (t.name.startsWith (TR ("Gravação"))) ++n;
            auto& t = project.addTrack (TrackType::Audio, TR ("Gravação ") + juce::String (n));
            trackId = t.id;
        }
        if (auto* si = project.findSource (sourceId)) { si->bpm = recBpm; si->bpmDetected = true; }
        const int id = addAudioClip (trackId, sourceId, startBeat, juce::jmax (0.0, latency), -1, false);
        if (auto* c = project.findClip (id)) { c->warp = true; c->sourceBpm = recBpm; }
        if (id >= 0) selectedClips = { id };
        commit (kChAll);
        setStatus (TR ("Gravação adicionada ao arranjo: ") + file.getFileName());
    });
}

// ---------------------------------------------------------------------------
void Session::newProject()
{
    engine.stop();
    engine.panic();
    engine.stopPreview();
    project.reset();
    history.clear();
    selectedClips.clear();
    selectedPad = 0; currentBank = 0; editorSourceId = -1;
    padCache.clear();
    sources.clear();
    engine.seek (0);
    dirty = false;
    commit (kChAll);
    dirty = false;
}

bool Session::openProject (const juce::File& f)
{
    auto text = f.loadFileAsString();
    auto v = juce::JSON::parse (text);
    if (! v.isObject() || v.getProperty ("format", {}).toString() != "MeuPadStudio")
    {
        setStatus (TR ("Arquivo de projeto inválido: ") + f.getFileName());
        return false;
    }
    engine.stop();
    engine.panic();
    engine.stopPreview();
    project.fromVar (v);
    project.projectFile = f;
    history.clear();
    selectedClips.clear();
    editorSourceId = project.sources.empty() ? -1 : project.sources.front().id;
    padCache.clear();
    sources.clear();
    reloadAllSources();
    engine.seek (0);
    addRecentFile (f);
    setLastFolder (f.getParentDirectory());
    setStatus (TR ("Projeto aberto: ") + f.getFileName());
    commit (kChAll);
    dirty = false;
    return true;
}

bool Session::saveProject (const juce::File& file, bool collectAudio)
{
    auto f = file.withFileExtension (".mpad");
    if (collectAudio)
    {
        auto folder = f.getParentDirectory().getChildFile (f.getFileNameWithoutExtension() + "_audio");
        folder.createDirectory();
        for (auto& s : project.sources)
        {
            juce::File src (s.path);
            if (! src.existsAsFile() || src.isAChildOf (folder)) continue;
            auto dest = folder.getNonexistentChildFile (src.getFileNameWithoutExtension(), src.getFileExtension(), false);
            if (src.copyFileTo (dest)) s.path = dest.getFullPathName();
        }
    }
    for (auto& s : project.sources)
        s.relPath = juce::File (s.path).getRelativePathFrom (f.getParentDirectory());

    project.projectFile = f;
    if (! f.replaceWithText (project.toJson()))
    {
        setStatus (TR ("Erro ao salvar ") + f.getFullPathName());
        return false;
    }
    dirty = false;
    addRecentFile (f);
    setLastFolder (f.getParentDirectory());
    setStatus (TR ("Projeto salvo: ") + f.getFileName());
    notify (kChSources);
    return true;
}

juce::File Session::lastFolder() const
{
    auto p = const_cast<juce::ApplicationProperties&> (props).getUserSettings()->getValue ("lastFolder");
    juce::File f (p);
    if (p.isNotEmpty() && f.isDirectory()) return f;
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory);
}

void Session::setLastFolder (const juce::File& f)
{
    props.getUserSettings()->setValue ("lastFolder", f.getFullPathName());
}

void Session::addRecentFile (const juce::File& f)
{
    juce::RecentlyOpenedFilesList r;
    r.restoreFromString (props.getUserSettings()->getValue ("recent"));
    r.addFile (f);
    r.setMaxNumberOfItems (10);
    props.getUserSettings()->setValue ("recent", r.toString());
}

juce::StringArray Session::recentFiles() const
{
    juce::RecentlyOpenedFilesList r;
    r.restoreFromString (const_cast<juce::ApplicationProperties&> (props).getUserSettings()->getValue ("recent"));
    r.removeNonExistentFiles();
    juce::StringArray a;
    for (int i = 0; i < r.getNumFiles(); ++i) a.add (r.getFile (i).getFullPathName());
    return a;
}

// ---------------------------------------------------------------------------
bool Session::renderToFile (const ExportSettings& es, std::function<bool (float)> progress, juce::String& error)
{
    // 1) snapshot na taxa de exportacao, esperando o time-stretch terminar
    SnapshotPtr snap;
    for (int tries = 0; tries < 20 * 60 * 10; ++tries)
    {
        struct Ctx { Session* self; double sr; SnapshotPtr out; } ctx { this, es.sampleRate, nullptr };
        juce::MessageManager::getInstance()->callFunctionOnMessageThread ([] (void* c) -> void*
        {
            auto* x = static_cast<Ctx*> (c);
            x->out = x->self->buildSnapshot (x->sr);
            return nullptr;
        }, &ctx);
        snap = ctx.out;
        if (snap != nullptr && snap->pendingStretches == 0) break;
        if (progress && ! progress (-1.0f)) { error = TR ("Cancelado"); return false; }
        juce::Thread::sleep (100);
    }
    if (snap == nullptr) { error = TR ("Falha ao preparar o projeto"); return false; }

    auto s = std::make_shared<Snapshot> (*snap);
    s->loopOn = false;
    s->metronome = false;
    const bool patternExport = es.patternRepeats > 0;
    double patBeats = 4.0;
    if (auto* cp = currentPattern()) patBeats = cp->lengthBeats();
    const double startBeat = patternExport ? 0.0 : es.onlyLoop ? project.loopStart : 0.0;
    const double endBeat = patternExport ? patBeats * es.patternRepeats : es.onlyLoop ? project.loopEnd : s->songEndBeat;
    if (endBeat <= startBeat + 0.01)
    {
        error = TR ("Nada para exportar (o arranjo esta vazio). Dica: escolha \"Padrão atual\" em Trecho para exportar o beat do modo PADRÃO.");
        return false;
    }

    auto renderer = std::make_unique<Renderer>();
    renderer->prepare (es.sampleRate, 4096);
    renderer->posBeats = startBeat;
    renderer->playing = true;
    renderer->patternMode = patternExport;
    const juce::int64 bodySamples = (juce::int64) std::ceil ((endBeat - startBeat) * s->samplesPerBeat());
    const juce::int64 total = bodySamples + (juce::int64) (es.tailSeconds * es.sampleRate);

    es.file.deleteFile();
    std::unique_ptr<juce::OutputStream> stream (es.file.createOutputStream());
    if (stream == nullptr) { error = TR ("Não foi possível criar ") + es.file.getFullPathName(); return false; }
    std::unique_ptr<juce::AudioFormat> fmt;
    if (es.format == 1) fmt = std::make_unique<juce::FlacAudioFormat>();
    else if (es.format == 2) fmt = std::make_unique<juce::OggVorbisAudioFormat>();
    else if (es.format == 3) fmt = std::make_unique<Mp3AudioFormat>();
    else fmt = std::make_unique<juce::WavAudioFormat>();
    const int bits = es.format == 2 ? 16 : es.bitDepth;
    juce::StringPairArray meta;
    meta.set ("title", es.file.getFileNameWithoutExtension());
    const int quality = es.format == 2 ? 8 : es.format == 3 ? es.mp3Quality : 0;
    std::unique_ptr<juce::AudioFormatWriter> writer (fmt->createWriterFor (stream.get(), es.sampleRate, 2, bits, meta, quality));
    if (writer == nullptr) { error = TR ("Formato/taxa não suportados para exportação"); return false; }
    stream.release();

    juce::AudioBuffer<float> buf (2, 4096);
    juce::int64 done = 0;
    const bool dither = bits == 16 && (es.format == 0 || es.format == 1);
    juce::Random ditherRng (12345);
    while (done < total)
    {
        int n = (int) std::min<juce::int64> (4096, total - done);
        if (done < bodySamples) n = (int) std::min<juce::int64> (n, bodySamples - done);
        else renderer->playing = false;
        renderer->process (*s, buf, 0, n);
        if (dither)
        {
            // dither TPDF de 1 LSB: 16 bits (qualidade de CD) sem distorcao de quantizacao nas partes baixas
            const float lsb = 1.0f / 32768.0f;
            for (int c = 0; c < 2; ++c)
            {
                auto* d = buf.getWritePointer (c);
                for (int i = 0; i < n; ++i) d[i] += (ditherRng.nextFloat() - ditherRng.nextFloat()) * lsb;
            }
        }
        writer->writeFromAudioSampleBuffer (buf, 0, n);
        done += n;
        if (progress && ! progress ((float) done / (float) total))
        {
            writer.reset();
            es.file.deleteFile();
            error = TR ("Cancelado");
            return false;
        }
    }
    writer.reset();
    return true;
}

// ---------------------------------------------------------------------------
void Session::timerCallback()
{

    uiMasterPeakL = engine.getRenderer().masterPeakL.exchange (0.0f);
    uiMasterPeakR = engine.getRenderer().masterPeakR.exchange (0.0f);
    if (isAudioRecording())
    {
        const int secs = (int) engine.getRecordedSeconds();
        juce::String st = TR ("\xe2\x97\x8f GRAVANDO ") + (recOptions.fromInput ? TR ("ENTRADA") : "MIX") + "  "
                          + juce::String (secs / 60) + ":" + juce::String (secs % 60).paddedLeft ('0', 2);
        if (recOptions.fromInput)
        {
            const float pk = engine.readInputPeak();
            inputPeakHold = juce::jmax (pk, inputPeakHold * 0.9f);
            st << TR ("   nível da entrada: ") << juce::String (juce::Decibels::gainToDecibels (inputPeakHold, -60.0f), 1) << " dB";
            if (inputPeakHold >= 0.99f) st << TR ("  (CLIPANDO! baixe o volume do microfone)");
        }
        st << TR ("   |  Espaço ou Parar para salvar");
        setStatus (st);
    }

    RecordedHit h;
    bool any = false;
    while (engine.getRenderer().recordedHits.pop (h))
    {
        auto* p = project.findPattern (h.patternId);
        if (p == nullptr) continue;
        if (! recordUndoPushed) { beginEdit (TR ("Gravar pads")); recordUndoPushed = true; }
        p->set (h.pad, h.step, h.vel);
        any = true;
    }
    // notas tocadas no teclado durante a gravacao -> piano roll
    RecordedNote rn;
    while (engine.getRenderer().recordedNotes.pop (rn))
    {
        const auto key = std::make_pair (rn.channelId, rn.pitch);
        if (rn.on) pendingNotes[key] = rn;
        else if (auto it = pendingNotes.find (key); it != pendingNotes.end())
        {
            finishRecordedNote (it->second, rn.absBeat);
            pendingNotes.erase (it);
            any = true;
        }
    }
    if (! pendingNotes.empty() && ! engine.isPlaying())
    {
        // parou com a tecla ainda apertada: fecha as notas com a duracao da grade
        for (auto& [k, on] : pendingNotes) finishRecordedNote (on, on.absBeat + juce::jmax (0.25, noteQuantize));
        pendingNotes.clear();
        any = true;
    }
    if (any) commit (kChPatterns);

    // controladores MIDI conectados depois de abrir o programa
    if (--midiScanCountdown <= 0)
    {
        midiScanCountdown = 90;   // ~3 s
        engine.enableAllMidiInputs();
    }

    if (status.isNotEmpty() && juce::Time::getMillisecondCounter() - statusTime > 9000) status.clear();

    graveyard.erase (std::remove_if (graveyard.begin(), graveyard.size() > 1 ? graveyard.end() - 1 : graveyard.end(),
                                     [] (const SnapshotPtr& sp) { return sp.use_count() <= 1; }),
                     graveyard.size() > 1 ? graveyard.end() - 1 : graveyard.end());
}

} // namespace mp
