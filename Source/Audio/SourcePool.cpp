#include "../I18n.h"
#include "SourcePool.h"
#include "Analysis.h"
#include "MFDecoder.h"
#include <rubberband/RubberBandStretcher.h>

namespace mp
{
SourcePool::SourcePool()
{
    formatManager.registerBasicFormats();
}

SourcePool::~SourcePool()
{
    *alive = false;
    pool.removeAllJobs (true, 20000);
}

juce::String SourcePool::wildcard() const
{
   #if JUCE_WINDOWS
    return "*.wav;*.mp3;*.flac;*.ogg;*.aif;*.aiff;*.m4a;*.aac;*.mp4;*.wma;*.webm;*.opus";
   #else
    return "*.wav;*.mp3;*.flac;*.ogg;*.aif;*.aiff";
   #endif
}

SourcePtr SourcePool::decode (juce::AudioFormatManager& fm, int id, const juce::File& file, bool analyseBpm)
{
    auto src = std::make_shared<LoadedSource>();
    src->id = id;
    src->file = file;

    std::shared_ptr<juce::AudioBuffer<float>> buf;
    double fileRate = 0;
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader != nullptr && reader->sampleRate > 0 && reader->lengthInSamples > 0)
    {
        const int numCh = (int) juce::jlimit<unsigned int> (1, 2, reader->numChannels);
        const juce::int64 maxLen = (juce::int64) (reader->sampleRate * 60.0 * 40.0); // ate 40 minutos
        const int len = (int) juce::jmin (reader->lengthInSamples, maxLen);
        buf = std::make_shared<juce::AudioBuffer<float>> (numCh, len);
        buf->clear();
        reader->read (buf.get(), 0, len, 0, true, numCh > 1);
        fileRate = reader->sampleRate;
    }
    else
    {
        if (! file.existsAsFile())
        {
            src->error = TR ("Arquivo não encontrado: ") + file.getFullPathName();
            return src;
        }
        // M4A / AAC / WEBM etc: tenta o decodificador do Windows
        auto tmp = std::make_shared<juce::AudioBuffer<float>>();
        juce::String err;
        if (! decodeWithMediaFoundation (file, *tmp, fileRate, err))
        {
            src->error = err.isNotEmpty() ? err : TR ("Formato não suportado: ") + file.getFileExtension();
            return src;
        }
        buf = tmp;
    }
    reader.reset();
    const int numCh = buf->getNumChannels();
    const int len = buf->getNumSamples();
    if (len <= 0 || fileRate <= 0)
    {
        src->error = TR ("Arquivo vazio ou inválido");
        return src;
    }

    // remove DC e protege contra NaN de decodificadores ruins
    for (int c = 0; c < numCh; ++c)
    {
        auto* d = buf->getWritePointer (c);
        for (int i = 0; i < len; ++i)
            if (! std::isfinite (d[i])) d[i] = 0.0f;
    }

    src->sampleRate = fileRate;
    src->lengthSec = len / fileRate;

    // picos para desenho da forma de onda
    const int blocks = (len + LoadedSource::kPeakBlock - 1) / LoadedSource::kPeakBlock;
    src->peakMin.assign ((size_t) blocks, 0.0f);
    src->peakMax.assign ((size_t) blocks, 0.0f);
    for (int b = 0; b < blocks; ++b)
    {
        const int s0 = b * LoadedSource::kPeakBlock;
        const int s1 = juce::jmin (len, s0 + LoadedSource::kPeakBlock);
        float mn = 0, mx = 0;
        for (int c = 0; c < numCh; ++c)
        {
            auto* d = buf->getReadPointer (c);
            for (int i = s0; i < s1; ++i) { mn = juce::jmin (mn, d[i]); mx = juce::jmax (mx, d[i]); }
        }
        src->peakMin[(size_t) b] = mn;
        src->peakMax[(size_t) b] = mx;
    }

    if (analyseBpm)
    {
        src->detectedBpm = Analysis::detectBpm (*buf, src->sampleRate);
        if (src->detectedBpm > 0)
            src->beatOffsetSec = Analysis::detectBeatOffset (*buf, src->sampleRate, src->detectedBpm);
    }

    src->buffer = buf;
    src->ready = true;
    return src;
}

void SourcePool::load (int id, const juce::File& file, bool analyseBpm, std::function<void (SourcePtr)> onDone)
{
    int gen;
    {
        const juce::ScopedLock sl (lock);
        gen = ++generation;
        loading[id] = gen;
    }
    auto aliveFlag = alive;
    pool.addJob ([this, id, file, analyseBpm, onDone, gen, aliveFlag]
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        auto s = decode (fm, id, file, analyseBpm);
        juce::MessageManager::callAsync ([this, s, id, gen, onDone, aliveFlag]
        {
            if (! *aliveFlag) return;
            {
                const juce::ScopedLock sl (lock);
                auto it = loading.find (id);
                if (it == loading.end() || it->second != gen) return; // cancelado/substituido
                loading.erase (it);
                if (s->ready) sources[id] = s;
            }
            if (onDone) onDone (s);
        });
    });
}

SourcePtr SourcePool::loadNow (int id, const juce::File& file, bool analyseBpm)
{
    auto s = decode (formatManager, id, file, analyseBpm);
    if (s->ready) { const juce::ScopedLock sl (lock); sources[id] = s; }
    return s;
}

SourcePtr SourcePool::get (int id) const
{
    const juce::ScopedLock sl (lock);
    auto it = sources.find (id);
    return it == sources.end() ? nullptr : it->second;
}

bool SourcePool::isLoading (int id) const
{
    const juce::ScopedLock sl (lock);
    return loading.count (id) > 0;
}

int SourcePool::numLoading() const
{
    const juce::ScopedLock sl (lock);
    return (int) loading.size();
}

void SourcePool::remove (int id)
{
    const juce::ScopedLock sl (lock);
    sources.erase (id);
    loading.erase (id);
}

void SourcePool::clear()
{
    const juce::ScopedLock sl (lock);
    sources.clear();
    loading.clear();
}

void SourcePool::put (SourcePtr s)
{
    const juce::ScopedLock sl (lock);
    sources[s->id] = s;
}

// ============================================================================
StretchCache::StretchCache (SourcePool& p) : juce::Thread ("MeuPad Stretch"), sourcePool (p)
{
    startThread (juce::Thread::Priority::low);
}

StretchCache::~StretchCache()
{
    *alive = false;
    signalThreadShouldExit();
    wake.signal();
    stopThread (30000);
}

void StretchCache::beginRound()
{
    const juce::ScopedLock sl (lock);
    wantedThisRound.clear();
}

BufferPtr StretchCache::request (const Key& k)
{
    const juce::ScopedLock sl (lock);
    if (std::find (wantedThisRound.begin(), wantedThisRound.end(), k) == wantedThisRound.end())
        wantedThisRound.push_back (k);
    auto it = ready.find (k);
    if (it != ready.end()) return it->second;
    if (! (working == k) && std::find (queue.begin(), queue.end(), k) == queue.end())
    {
        queue.push_back (k);
        wake.signal();
    }
    return nullptr;
}

void StretchCache::endRound()
{
    const juce::ScopedLock sl (lock);
    wanted = wantedThisRound;
    // remove trabalhos na fila que ninguem mais quer (ex: arrastando o BPM)
    queue.erase (std::remove_if (queue.begin(), queue.end(), [this] (const Key& k)
    {
        return std::find (wanted.begin(), wanted.end(), k) == wanted.end();
    }), queue.end());
    // libera buffers sem uso (o snapshot antigo ainda pode ter uma referencia - shared_ptr cuida disso)
    for (auto it = ready.begin(); it != ready.end();)
    {
        const bool isWanted = std::find (wanted.begin(), wanted.end(), it->first) != wanted.end();
        if (! isWanted && it->second.use_count() <= 1) it = ready.erase (it);
        else ++it;
    }
}

bool StretchCache::hasPending() const
{
    const juce::ScopedLock sl (lock);
    return ! queue.empty() || working.sourceId >= 0;
}

juce::String StretchCache::currentJobName() const
{
    const juce::ScopedLock sl (lock);
    if (working.sourceId < 0) return {};
    if (auto s = sourcePool.get (working.sourceId)) return s->file.getFileNameWithoutExtension();
    return {};
}

void StretchCache::run()
{
    while (! threadShouldExit())
    {
        Key k { -1, 0, 0 };
        {
            const juce::ScopedLock sl (lock);
            if (! queue.empty())
            {
                k = queue.front();
                queue.erase (queue.begin());
                working = k;
            }
        }
        if (k.sourceId < 0) { wake.wait (500); continue; }

        BufferPtr result;
        if (auto src = sourcePool.get (k.sourceId))
        {
            currentProgress = 0.0f;
            result = render (*src->buffer, src->sampleRate, k.ratioQ / 10000.0, k.pitchQ / 10000.0,
                             [this] (float p) { currentProgress = p; return ! threadShouldExit(); });
        }
        {
            const juce::ScopedLock sl (lock);
            if (result) ready[k] = result;
            working = { -1, 0, 0 };
        }
        currentProgress = 1.0f;
        auto aliveFlag = alive;
        juce::MessageManager::callAsync ([this, aliveFlag] { if (*aliveFlag && onJobFinished) onJobFinished(); });
    }
}

BufferPtr StretchCache::render (const juce::AudioBuffer<float>& src, double sampleRate, double timeRatio, double pitchScale,
                                std::function<bool (float)> progressCb)
{
    using RB = RubberBand::RubberBandStretcher;
    const int nch = src.getNumChannels();
    const int n = src.getNumSamples();
    RB::Options opts = RB::OptionProcessOffline | RB::OptionEngineFaster | RB::OptionTransientsMixed
                     | RB::OptionPhaseLaminar | RB::OptionWindowStandard | RB::OptionChannelsTogether
                     | RB::OptionPitchHighQuality;
    RB rb ((size_t) sampleRate, (size_t) nch, opts, timeRatio, pitchScale);
    rb.setExpectedInputDuration ((size_t) n);
    const int block = 8192;
    rb.setMaxProcessSize ((size_t) block);

    std::vector<const float*> in ((size_t) nch);
    // fase de estudo
    for (int pos = 0; pos < n; pos += block)
    {
        const int len = juce::jmin (block, n - pos);
        for (int c = 0; c < nch; ++c) in[(size_t) c] = src.getReadPointer (c) + pos;
        rb.study (in.data(), (size_t) len, pos + len >= n);
        if (progressCb && ! progressCb (0.25f * pos / (float) n)) return nullptr;
    }

    const int expected = (int) std::ceil (n * timeRatio) + 4096;
    auto out = std::make_shared<juce::AudioBuffer<float>> (nch, expected);
    out->clear();
    int written = 0;
    std::vector<float*> outPtrs ((size_t) nch);
    juce::AudioBuffer<float> tmp (nch, block);

    auto drain = [&]
    {
        int avail;
        while ((avail = rb.available()) > 0)
        {
            const int take = juce::jmin (avail, block);
            for (int c = 0; c < nch; ++c) outPtrs[(size_t) c] = tmp.getWritePointer (c);
            const int got = (int) rb.retrieve (outPtrs.data(), (size_t) take);
            if (written + got > out->getNumSamples())
                out->setSize (nch, written + got + 65536, true, true, false);
            for (int c = 0; c < nch; ++c) out->copyFrom (c, written, tmp, c, 0, got);
            written += got;
        }
    };

    for (int pos = 0; pos < n; pos += block)
    {
        const int len = juce::jmin (block, n - pos);
        for (int c = 0; c < nch; ++c) in[(size_t) c] = src.getReadPointer (c) + pos;
        rb.process (in.data(), (size_t) len, pos + len >= n);
        drain();
        if (progressCb && ! progressCb (0.25f + 0.75f * pos / (float) n)) return nullptr;
    }
    for (int guard = 0; guard < 2000 && rb.available() >= 0; ++guard)
    {
        drain();
        if (rb.available() == 0) juce::Thread::sleep (1);
    }
    out->setSize (nch, juce::jmax (1, written), true, true, false);
    return out;
}

} // namespace mp
