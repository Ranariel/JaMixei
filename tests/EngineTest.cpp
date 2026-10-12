// Testes do motor do MeuPad Studio (executados em Linux durante o desenvolvimento)
#include <JuceHeader.h>
#include "Session.h"
#include "Audio/Analysis.h"
#include "Audio/Renderer.h"
#include "Audio/Instruments.h"

using namespace mp;

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { ++failures; std::cout << "  [FALHOU] " << msg << std::endl; } else std::cout << "  [ok] " << msg << std::endl; } while (0)

static juce::File makeBeat (const juce::String& name, double bpm, double seconds, double sr = 44100.0, float tone = 110.0f)
{
    const int n = (int) (seconds * sr);
    juce::AudioBuffer<float> b (2, n);
    b.clear();
    juce::Random rnd (42);
    const double spb = 60.0 / bpm * sr;
    for (int beat = 0; beat * spb < n; ++beat)
    {
        const int s0 = (int) (beat * spb);
        // bumbo
        for (int i = 0; i < (int) (0.25 * sr) && s0 + i < n; ++i)
        {
            const float env = std::exp (-i / (0.05f * (float) sr));
            const float f = 50.0f + 90.0f * std::exp (-i / (0.01f * (float) sr));
            const float v = 0.8f * env * std::sin (2.0f * juce::MathConstants<float>::pi * f * i / (float) sr);
            b.addSample (0, s0 + i, v); b.addSample (1, s0 + i, v);
        }
        // chimbal no contratempo
        const int h0 = (int) (s0 + spb / 2);
        for (int i = 0; i < (int) (0.04 * sr) && h0 + i < n; ++i)
        {
            const float v = 0.25f * std::exp (-i / (0.008f * (float) sr)) * (rnd.nextFloat() * 2 - 1);
            b.addSample (0, h0 + i, v); b.addSample (1, h0 + i, v);
        }
    }
    for (int i = 0; i < n; ++i)
    {
        const float v = 0.12f * std::sin (2.0f * juce::MathConstants<float>::pi * tone * i / (float) sr);
        b.addSample (0, i, v); b.addSample (1, i, v);
    }
    auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile (name);
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), sr, 2, 24, {}, 0));
    w->writeFromAudioSampleBuffer (b, 0, n);
    return f;
}

static void analyseFile (const juce::File& f, double& peak, double& rms, bool& finite, double& seconds)
{
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
    peak = 0; rms = 0; finite = true; seconds = 0;
    if (! r) { finite = false; return; }
    juce::AudioBuffer<float> b ((int) r->numChannels, (int) r->lengthInSamples);
    r->read (&b, 0, (int) r->lengthInSamples, 0, true, true);
    double acc = 0;
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float v = b.getSample (c, i);
            if (! std::isfinite (v)) finite = false;
            peak = std::max (peak, (double) std::abs (v));
            acc += v * v;
        }
    rms = std::sqrt (acc / juce::jmax (1, b.getNumSamples() * b.getNumChannels()));
    seconds = r->lengthInSamples / r->sampleRate;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    std::cout << "== Analise ==" << std::endl;
    auto f128 = makeBeat ("mp_test128.wav", 128.0, 40.0);
    auto f100 = makeBeat ("mp_test100.wav", 100.0, 40.0, 48000.0, 165.0f);
    auto f93 = makeBeat ("mp_test93.wav", 93.0, 40.0);
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    auto s128 = SourcePool::decode (fm, 1, f128, true);
    auto s100 = SourcePool::decode (fm, 2, f100, true);
    auto s93 = SourcePool::decode (fm, 3, f93, true);
    std::cout << "  BPM detectados: " << s128->detectedBpm << " / " << s100->detectedBpm << " / " << s93->detectedBpm << std::endl;
    CHECK (std::abs (s128->detectedBpm - 128.0) < 0.6, "BPM 128");
    CHECK (std::abs (s100->detectedBpm - 100.0) < 0.6, "BPM 100 @48k");
    CHECK (std::abs (s93->detectedBpm - 93.0) < 0.6, "BPM 93");

    auto tr = Analysis::detectTransients (*s128->buffer, s128->sampleRate, 0.0, 60.0 / 128.0 * 8, 0.5f);
    std::cout << "  transientes em 8 batidas: " << tr.size() << std::endl;
    CHECK (tr.size() >= 8 && tr.size() <= 20, "deteccao de transientes plausivel");
    if (! tr.empty()) { std::cout << "  primeiros: "; for (size_t i = 0; i < std::min<size_t> (6, tr.size()); ++i) std::cout << tr[i] << " "; std::cout << std::endl; CHECK (std::abs (tr[0]) < 0.02, "primeiro transiente no inicio"); }

    std::cout << "== Time-stretch ==" << std::endl;
    auto t0 = juce::Time::getMillisecondCounterHiRes();
    auto st = StretchCache::render (*s100->buffer, s100->sampleRate, 100.0 / 128.0, 1.0);
    auto ms = juce::Time::getMillisecondCounterHiRes() - t0;
    std::cout << "  40s stretch em " << ms << " ms; frames " << st->getNumSamples() << " esperado " << (int) (s100->buffer->getNumSamples() * 100.0 / 128.0) << std::endl;
    CHECK (st != nullptr && std::abs (st->getNumSamples() - s100->buffer->getNumSamples() * 100.0 / 128.0) < 4096, "duracao do stretch");
    auto stBpm = Analysis::detectBpm (*st, s100->sampleRate);
    std::cout << "  BPM apos stretch: " << stBpm << std::endl;
    CHECK (std::abs (stBpm - 128.0) < 1.0, "stretch levou a 128 BPM");

    std::cout << "== Sessao / Exportacao ==" << std::endl;
    {
        Session session;
        auto& p = session.project;
        // carrega fontes sincronamente
        for (auto [id, file] : { std::pair<int, juce::File> { 0, f128 }, { 0, f100 } })
        {
            SourceInfo si; si.id = p.newId(); si.path = file.getFullPathName(); si.name = file.getFileNameWithoutExtension();
            auto s = session.sources.loadNow (si.id, file, true);
            si.bpm = s->detectedBpm; si.bpmDetected = true;
            p.sources.push_back (si);
        }
        const int srcA = p.sources[0].id, srcB = p.sources[1].id;
        const int trA = session.trackIdAt (1), trB = session.trackIdAt (2), padTrack = session.trackIdAt (0);
        int cA = session.addAudioClip (trA, srcA, 0.0, 0.0, 20.0);
        CHECK (cA > 0, "clipe A criado");
        CHECK (std::abs (p.bpm - s128->detectedBpm) < 0.01, "BPM do projeto = primeira musica");
        int cB = session.addAudioClip (trB, srcB, 32.0, 2.0, 15.0);
        CHECK (cB > 0 && p.findClip (cB)->warp, "clipe B com warp");
        // clipe B sobreposto na mesma faixa A para crossfade
        int cC = session.addAudioClip (trA, srcB, 36.0, 0.0, 10.0);
        CHECK (cC > 0, "clipe C sobreposto");
        // pads + padrao
        session.chopToPads (srcA, Analysis::detectTransients (*s128->buffer, 44100, 0, 4, 0.5f), 4.0, 0);
        CHECK (! p.pads[0].isEmpty() && ! p.pads[1].isEmpty(), "pads fatiados");
        auto* pat = session.currentPattern();
        for (int s = 0; s < 16; s += 4) pat->set (0, s, 120);
        for (int s = 2; s < 16; s += 4) pat->set (1, s, 90);
        session.addPatternClip (padTrack, pat->id, 0.0);
        session.addPatternClip (padTrack, pat->id, 4.0);
        p.tracks[1].filterAuto = { { 30.0, 0.0f }, { 40.0, -0.8f } };
        p.tracks[1].reverbSend = 0.3f;
        p.tracks[2].delaySend = 0.3f;
        p.tracks[2].eqLow = 6;

        // serializacao ida e volta
        auto j1 = p.toJson();
        Project p2; p2.fromJson (j1);
        CHECK (p2.toJson() == j1, "JSON ida e volta identico");

        // corte + desfazer
        const size_t before = p.clips.size();
        session.selectedClips = { cA };
        session.splitAt (10.0);
        CHECK (p.clips.size() == before + 1, "corte criou novo clipe");
        double lenSum = 0;
        for (auto& c : p.clips) if (c.sourceId == srcA && c.trackId == trA) lenSum += c.sourceLenSec;
        CHECK (std::abs (lenSum - 20.0) < 1e-6, "corte preserva duracao");
        session.undo();
        CHECK (p.clips.size() == before, "desfazer corte");
        session.redo();
        CHECK (p.clips.size() == before + 1, "refazer corte");

        ExportSettings es;
        es.file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("mp_export.wav");
        es.sampleRate = 48000;
        juce::String err;
        t0 = juce::Time::getMillisecondCounterHiRes();
        bool ok = session.renderToFile (es, [] (float) { return true; }, err);
        std::cout << "  exportacao em " << (juce::Time::getMillisecondCounterHiRes() - t0) << " ms  " << err << std::endl;
        CHECK (ok, "exportacao concluida");
        double peak, rms, secs; bool finite;
        analyseFile (es.file, peak, rms, finite, secs);
        const double expect = p.beatsToSeconds (p.songEndBeat()) + 2.0;
        std::cout << "  pico " << peak << " rms " << rms << " dur " << secs << " esperado " << expect << std::endl;
        CHECK (finite, "sem NaN/Inf");
        CHECK (peak <= 1.0 && peak > 0.2, "nivel de pico dentro do limite");
        CHECK (rms > 0.02, "sinal presente");
        CHECK (std::abs (secs - expect) < 0.05, "duracao exportada correta");

        // exportacao 16 bits / 44,1 kHz (qualidade de CD, com dither)
        {
            ExportSettings cd;
            cd.file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("mp_export_cd.wav");
            cd.sampleRate = 44100; cd.bitDepth = 16; cd.format = 0;
            juce::String e3;
            CHECK (session.renderToFile (cd, [] (float) { return true; }, e3), "exportacao WAV 16 bits concluida");
            juce::AudioFormatManager fm2; fm2.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> rd (fm2.createReaderFor (cd.file));
            CHECK (rd != nullptr && rd->bitsPerSample == 16 && (int) rd->sampleRate == 44100, "arquivo 16 bits / 44100 Hz");
            double pk3, rms3, sec3; bool fin3;
            analyseFile (cd.file, pk3, rms3, fin3, sec3);
            CHECK (fin3 && std::abs (sec3 - expect) < 0.05 && pk3 <= 1.0, "duracao e nivel do arquivo de CD");
        }

        // fim da musica: o motor ao vivo volta ao comeco (repete); a exportacao nao
        {
            auto snapR = session.buildSnapshot (44100);
            const double end = snapR->songEndBeat;
            const double barEnd = std::ceil (end / snapR->beatsPerBar - 1e-9) * snapR->beatsPerBar;
            Renderer rr; rr.prepare (44100, 512);
            rr.playing = true; rr.repeatSong = true; rr.posBeats = barEnd - 0.5;
            juce::AudioBuffer<float> ob (2, 44100);
            rr.process (*snapR, ob, 0, ob.getNumSamples());
            CHECK (rr.posBeats < 3.0, "musica repete do comeco no fim (pos " + std::to_string (rr.posBeats) + ")");
            rr.posBeats = barEnd + 10.0;   // play depois do fim -> comeca do inicio
            rr.process (*snapR, ob, 0, 512);
            CHECK (rr.posBeats < 1.0, "play depois do fim comeca do inicio");
            Renderer re; re.prepare (44100, 512);
            re.playing = true; re.repeatSong = false; re.posBeats = barEnd - 0.5;
            re.process (*snapR, ob, 0, ob.getNumSamples());
            CHECK (re.posBeats > barEnd, "sem repetir (exportacao) segue em frente");

            // pre-escuta do editor com "Repetir"
            Renderer rp; rp.prepare (44100, 512);
            Renderer::Preview pv;
            pv.buf = session.getSource (srcA)->buffer; pv.sr = 44100; pv.pos = 0; pv.end = 0.25 * 44100; pv.active = true;
            pv.loop = true; pv.loopStart = 0;
            { const juce::SpinLock::ScopedLockType sl (rp.previewLock); rp.pendingPreview = pv; rp.previewChanged = true; }
            juce::AudioBuffer<float> pb (2, 44100);
            rp.process (*snapR, pb, 0, pb.getNumSamples());
            CHECK (rp.previewPosSec.load() >= 0.0 && rp.previewPosSec.load() < 0.26, "pre-escuta repete o trecho");
            CHECK (pb.getMagnitude (0, 30000, 4000) > 0.01f, "pre-escuta continua soando depois de repetir");
            pv.loop = false;
            { const juce::SpinLock::ScopedLockType sl (rp.previewLock); rp.pendingPreview = pv; rp.previewChanged = true; }
            rp.process (*snapR, pb, 0, pb.getNumSamples());
            CHECK (rp.previewPosSec.load() < 0.0, "sem repetir a pre-escuta para no fim");
        }

        // salvar com copia de audios + abrir
        {
            auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("mp_proj_test");
            dir.deleteRecursively();
            dir.createDirectory();
            auto pf = dir.getChildFile ("teste.mpad");
            const size_t nClips = p.clips.size();
            CHECK (session.saveProject (pf, true), "salvar projeto");
            CHECK (dir.getChildFile ("teste_audio").getNumberOfChildFiles (juce::File::findFiles) == 2, "audios copiados para a pasta do projeto");
            CHECK (p.sources[0].relPath.startsWith ("teste_audio"), "caminho relativo salvo");
            session.newProject();
            CHECK (p.clips.empty(), "novo projeto vazio");
            CHECK (session.openProject (pf), "abrir projeto");
            CHECK (p.clips.size() == nClips, "clipes restaurados");
            CHECK (juce::File (p.sources[0].path).existsAsFile(), "caminho do audio resolvido");
            // recarrega audios de forma sincrona para seguir os testes
            for (auto& si : p.sources) session.sources.loadNow (si.id, juce::File (si.path), false);
            session.rebuildSnapshot();
        }

        // exportacao MP3
        {
            ExportSettings mp;
            mp.file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("mp_export.mp3");
            mp.sampleRate = 44100; mp.format = 3; mp.mp3Quality = 0;
            juce::String e2;
            auto tt = juce::Time::getMillisecondCounterHiRes();
            CHECK (session.renderToFile (mp, [] (float) { return true; }, e2), "exportacao MP3 concluida " + e2.toStdString());
            std::cout << "  mp3 em " << (juce::Time::getMillisecondCounterHiRes() - tt) << " ms, " << mp.file.getSize() / 1024 << " KB" << std::endl;
            double pk2, rms2, secs2; bool fin2;
            analyseFile (mp.file, pk2, rms2, fin2, secs2);
            const double expect2 = p.beatsToSeconds (p.songEndBeat()) + 2.0;
            std::cout << "  mp3 pico " << pk2 << " rms " << rms2 << " dur " << secs2 << " esperado ~" << expect2 << std::endl;
            CHECK (fin2 && rms2 > 0.02 && std::abs (secs2 - expect2) < 0.2, "MP3 decodifica com duracao e nivel corretos");
            const double kbps = mp.file.getSize() * 8.0 / 1000.0 / secs2;
            CHECK (kbps > 300 && kbps < 340, "MP3 a ~320 kbps (" + std::to_string ((int) kbps) + ")");
        }

        // gravacao do mix (saida) com o dispositivo nulo
        if (session.engine.devices().getCurrentAudioDevice() != nullptr)
        {
            auto rf = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("mp_rec_mix.wav");
            juce::String e3;
            session.engine.seek (0);
            CHECK (session.engine.startAudioRecording (rf, AudioEngine::RecSource::Output, false, false, e3), "inicia gravacao do mix");
            session.engine.play();
            juce::Thread::sleep (1500);
            auto info = session.engine.stopAudioRecording();
            session.engine.stop();
            double pk3, rms3, secs3; bool fin3;
            analyseFile (rf, pk3, rms3, fin3, secs3);
            std::cout << "  gravacao: " << secs3 << " s, rms " << rms3 << ", inicio " << info.startBeat << std::endl;
            CHECK (info.gotAudio && fin3 && secs3 > 0.5 && rms3 > 0.01, "gravacao do mix contem audio");
            auto rf2 = rf.withFileExtension (".mp3");
            session.engine.seek (0);
            CHECK (session.engine.startAudioRecording (rf2, AudioEngine::RecSource::Output, true, false, e3), "inicia gravacao MP3");
            session.engine.play();
            juce::Thread::sleep (1000);
            session.engine.stopAudioRecording();
            session.engine.stop();
            analyseFile (rf2, pk3, rms3, fin3, secs3);
            std::cout << "  mp3 gravado: " << secs3 << " s rms " << rms3 << " bytes " << rf2.getSize() << std::endl;
            CHECK (fin3 && secs3 > 0.3 && rms3 > 0.01, "gravacao em MP3 contem audio");
            juce::String e4;
            const bool inOk = session.engine.setInputEnabled (true, e4);
            std::cout << "  entrada disponivel: " << inOk << " " << e4 << std::endl;
            session.engine.setInputEnabled (false, e4);
            CHECK (session.engine.devices().getCurrentAudioDevice() != nullptr, "dispositivo continua aberto apos alternar a entrada");
        }

        // modo padrao: renderer direto
        Renderer r;
        r.prepare (44100, 512);
        auto snap = session.buildSnapshot (44100);
        r.patternMode = true; r.playing = true; r.posBeats = 0;
        juce::AudioBuffer<float> out (2, 44100 * 4);
        r.process (*snap, out, 0, out.getNumSamples());
        CHECK (out.getMagnitude (0, 0, out.getNumSamples()) > 0.05f, "modo padrao produz som");
        CHECK (r.posBeats >= 0 && r.posBeats < 4.0, "posicao do padrao em loop");

        // pads ao vivo com gravacao
        r.recording = true;
        r.padOn (*snap, 3, 100, 0, 0, -1, true);
        RecordedHit h; int cnt = 0; while (r.recordedHits.pop (h)) ++cnt;
        CHECK (cnt == 1, "gravacao de pad ao vivo");
    }

    // ---------------------------------------------------------------- instrumentos
    {
        std::cout << "== Instrumentos ==" << std::endl;
        // estimativa de afinacao por autocorrelacao normalizada (diferenca ao quadrado, estilo YIN)
        auto pitchOf = [] (const juce::AudioBuffer<float>& b, double sr, double t0, double expected)
        {
            const int start = (int) (t0 * sr), W = (int) (0.08 * sr);
            std::vector<float> mono ((size_t) b.getNumSamples());
            for (int i = 0; i < b.getNumSamples(); ++i) mono[(size_t) i] = b.getSample (0, i) + b.getSample (1, i);
            const float* x = mono.data();
            const int minLag = (int) (sr / (expected * 1.5)), maxLag = (int) (sr / (expected / 1.5));
            std::vector<double> d ((size_t) maxLag + 2, 0.0);
            for (int lag = 1; lag <= maxLag + 1; ++lag)
                for (int i = 0; i < W; ++i) { const double e = x[start + i] - x[start + i + lag]; d[(size_t) lag] += e * e; }
            double run = 0; std::vector<double> cm (d.size(), 1.0);
            for (size_t lag = 1; lag < d.size(); ++lag) { run += d[lag]; cm[lag] = d[lag] * lag / juce::jmax (1e-12, run); }
            int best = minLag;
            for (int lag = minLag; lag <= maxLag; ++lag) if (cm[(size_t) lag] < cm[(size_t) best]) best = lag;
            const double a = cm[(size_t) best - 1], bb = cm[(size_t) best], c = cm[(size_t) best + 1];
            const double off = (a - 2 * bb + c) != 0 ? 0.5 * (a - c) / (a - 2 * bb + c) : 0.0;
            return sr / (best + off);
        };
        using namespace instruments;
        for (int k = 0; k < (int) Id::count; ++k)
        {
            const auto id = (Id) k;
            for (int midi : { 45, 57, 69 })
            {
                auto b = renderNote (id, midi, 44100.0);
                bool fin = true;
                for (int c = 0; c < 2; ++c) for (int i = 0; i < b.getNumSamples(); ++i) if (! std::isfinite (b.getSample (c, i))) fin = false;
                const float pk = b.getMagnitude (0, b.getNumSamples());
                const double exp = 440.0 * std::pow (2.0, (midi - 69) / 12.0);
                const double t0 = id == Id::Bass808 ? 0.4 : id == Id::Bells ? 1.2 : (id == Id::SynthPad || id == Id::Strings) ? 1.0 : 0.25;
                const double f = pitchOf (b, 44100.0, t0, exp);
                const double cents = 1200.0 * std::log2 (f / exp);
                const bool skipPitch = false;
                CHECK (fin && pk > 0.02f && pk < 4.0f && (skipPitch || std::abs (cents) < 12.0),
                       (displayName (id) + " " + noteName (midi) + ": pico " + juce::String (pk, 2) + ", afinacao " + juce::String (cents, 1) + " cents").toStdString());
            }
        }
        // tempo para gerar cada instrumento (multisample completo)
        {
            juce::String line = "  tempos de geracao:";
            double total = 0;
            for (int k = 0; k < (int) Id::count; ++k)
            {
                const auto t0 = juce::Time::getMillisecondCounterHiRes();
                auto ms = renderMulti ((Id) k);
                const double dt = juce::Time::getMillisecondCounterHiRes() - t0;
                total += dt;
                line << " " << displayName ((Id) k) << "=" << (int) dt << "ms";
                CHECK (ms != nullptr && ms->zones.size() >= 20, (displayName ((Id) k) + ": multisample com todas as zonas").toStdString());
            }
            std::cout << line << std::endl;
            CHECK (total < 20000.0, "todos os instrumentos gerados em menos de 20 s (" + std::to_string ((int) total) + " ms)");
        }
        // ---- aba Instrumentos: canais, notas com duracao, loop de sustentacao, gravacao
        {
            Session ses;
            const int pianoCh = ses.addChannel ((int) Id::Piano);
            const int padCh = ses.addChannel ((int) Id::SynthPad);
            ses.selectChannel (pianoCh);
            for (int w = 0; w < 600 && (ses.bank.get (Id::Piano) == nullptr || ses.bank.get (Id::SynthPad) == nullptr); ++w)
                juce::Thread::sleep (50);
            CHECK (ses.bank.get (Id::Piano) != nullptr && ses.bank.get (Id::SynthPad) != nullptr, "multisamples gerados (piano e pad)");
            auto* pat = ses.currentPattern();
            pat->lengthSteps = 32;   // 2 compassos
            pat->notes.push_back ({ pianoCh, 60, 0.0, 1.0, 110 });
            pat->notes.push_back ({ pianoCh, 64, 1.0, 0.5, 90 });
            pat->notes.push_back ({ padCh, 48, 0.0, 7.5, 100 });
            auto j = ses.project.toJson();
            Project pj; pj.fromJson (j);
            CHECK (pj.channels.size() == 2 && pj.findPattern (pat->id)->notes.size() == 3 && pj.toJson() == j, "notas e instrumentos salvos/abertos (JSON)");

            auto sn = ses.buildSnapshot (44100);
            CHECK (sn->channels.size() == 2 && sn->channels[0].multi != nullptr && sn->patterns[0].notes.size() == 3, "snapshot com canais e notas");
            Renderer rn; rn.prepare (44100, 512);
            rn.patternMode = true; rn.playing = true; rn.posBeats = 0;
            juce::AudioBuffer<float> ob (2, (int) (44100 * 3.8));   // 7,6 tempos a 120 BPM
            rn.process (*sn, ob, 0, ob.getNumSamples());
            CHECK (ob.getMagnitude (0, 0, 22050) > 0.05f, "notas do piano roll tocam");
            // pad segura 7,5 tempos (3,75 s) mesmo com amostra de 2,6 s: loop de sustentacao
            CHECK (ob.getRMSLevel (0, (int) (44100 * 3.3), 4410) > 0.01f, "nota longa sustenta (loop)");
            bool finiteAll = true;
            for (int c = 0; c < 2; ++c) for (int i = 0; i < ob.getNumSamples(); ++i) if (! std::isfinite (ob.getSample (c, i))) finiteAll = false;
            CHECK (finiteAll && ob.getMagnitude (0, ob.getNumSamples()) <= 1.0f, "sem NaN e sem passar de 0 dBFS");

            // afinacao entre zonas do multisample (zona a cada 3 semitons): C#4 vem do C4 esticado
            for (int pitch : { 61, 62, 45, 80 })
            {
                Renderer rl; rl.prepare (44100, 512);
                rl.noteOn (*sn, 0, pitch, 110, 0, 0, -1, true);
                juce::AudioBuffer<float> lb (2, 44100);
                rl.process (*sn, lb, 0, lb.getNumSamples());
                const double exp = 440.0 * std::pow (2.0, (pitch - 69) / 12.0);
                juce::AudioBuffer<float> mono (2, lb.getNumSamples());
                mono.makeCopyOf (lb);
                const double f = pitchOf (mono, 44100.0, 0.25, exp);
                const double cents = 1200.0 * std::log2 (f / exp);
                CHECK (std::abs (cents) < 12.0, ("piano ao vivo " + noteName (pitch) + ": " + juce::String (cents, 1) + " cents").toStdString());
                rl.noteOff (*sn, 0, pitch);
                rl.process (*sn, lb, 0, lb.getNumSamples());
                CHECK (lb.getRMSLevel (0, 30000, 10000) < 0.002f, "soltar a tecla encerra a nota (release)");
            }

            // gravacao do teclado no padrao
            Renderer rr2; rr2.prepare (44100, 512);
            rr2.patternMode = true; rr2.playing = true; rr2.recording = true; rr2.posBeats = 2.0;
            rr2.noteOn (*sn, -1, 67, 100, 0, 0, -1, true);
            rr2.posBeats = 3.0;
            rr2.noteOff (*sn, -1, 67);
            RecordedNote r1, r2;
            const bool got = rr2.recordedNotes.pop (r1) && rr2.recordedNotes.pop (r2);
            CHECK (got && r1.on && ! r2.on && r1.pitch == 67 && std::abs (r1.beat - 2.0) < 1e-6 && std::abs (r2.absBeat - r1.absBeat - 1.0) < 1e-6,
                   "gravacao de notas do teclado (inicio e fim)");

            // exportacao com instrumentos
            ExportSettings ex;
            ex.file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("mp_inst_export.wav");
            ses.addPatternClip (ses.project.liveTrackId, pat->id, 0.0);
            juce::String ee;
            CHECK (ses.renderToFile (ex, [] (float) { return true; }, ee), "exportacao com instrumentos");
            double pk4, rms4, s4; bool f4;
            analyseFile (ex.file, pk4, rms4, f4, s4);
            CHECK (f4 && rms4 > 0.01 && pk4 <= 1.0, "mix exportado tem os instrumentos");
            ses.removeChannel (padCh);
            CHECK (ses.project.channels.size() == 1 && ses.currentPattern()->notes.size() == 2, "remover instrumento apaga as notas dele");
        }

        Rendered rr;
        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("jm_inst_test");
        dir.deleteRecursively();
        const auto notes = scaleNotes (48, Scale::Minor);
        CHECK (notes.size() == 16 && notes[7] == 60 && notes[2] == 51, "escala menor a partir de C3");
        const bool ok = renderToFile (Id::Piano, notes, dir, rr);
        CHECK (ok && rr.file.existsAsFile() && rr.regions.size() == 16, "piano gravado em WAV com 16 regioes");
        double pk2, rms2, sec2; bool fin2;
        analyseFile (rr.file, pk2, rms2, fin2, sec2);
        CHECK (fin2 && pk2 > 0.8 && pk2 < 1.0 && sec2 > rr.regions.back().second, "WAV do instrumento normalizado (-1 dB)");
        for (int k = 0; k < (int) Id::count; ++k)
        {
            Rendered r2;
            renderToFile ((Id) k, scaleNotes (info ((Id) k).defaultOctave * 12 + 12, Scale::Chromatic), juce::File ("/tmp/jm_inst_all"), r2);
        }
    }

    std::cout << (failures == 0 ? "\nTODOS OS TESTES PASSARAM" : "\nFALHAS: " + std::to_string (failures)) << std::endl;
    return failures == 0 ? 0 : 1;
}
