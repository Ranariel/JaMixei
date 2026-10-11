#pragma once
#include <JuceHeader.h>
#include <map>
#include <memory>
#include <functional>

namespace mp
{
using BufferPtr = std::shared_ptr<const juce::AudioBuffer<float>>;

// Audio decodificado de um arquivo (musica, loop, sample)
struct LoadedSource
{
    int id = 0;
    juce::File file;
    BufferPtr buffer;
    double sampleRate = 44100;
    double lengthSec = 0;
    double detectedBpm = 0;
    double beatOffsetSec = 0;
    std::vector<float> peakMin, peakMax;   // por bloco de kPeakBlock quadros (mix mono)
    static constexpr int kPeakBlock = 128;
    bool ready = false;
    juce::String error;
};
using SourcePtr = std::shared_ptr<const LoadedSource>;

class SourcePool
{
public:
    SourcePool();
    ~SourcePool();

    juce::AudioFormatManager& formats() { return formatManager; }
    juce::String wildcard() const;

    // Comeca a carregar (em segundo plano). onDone e chamado na thread de mensagens.
    void load (int id, const juce::File& file, bool analyseBpm, std::function<void (SourcePtr)> onDone);
    // Carrega imediatamente (bloqueante) - usado na exportacao/teste
    SourcePtr loadNow (int id, const juce::File& file, bool analyseBpm);

    SourcePtr get (int id) const;
    bool isLoading (int id) const;
    void remove (int id);
    void clear();
    int numLoading() const;

    // Cria um buffer novo (ex: gravacao/resample) e registra como fonte
    void put (SourcePtr s);

    static SourcePtr decode (juce::AudioFormatManager& fm, int id, const juce::File& file, bool analyseBpm);

private:
    juce::AudioFormatManager formatManager;
    juce::ThreadPool pool { 2 };
    mutable juce::CriticalSection lock;
    std::map<int, SourcePtr> sources;
    std::map<int, int> loading; // id -> geracao
    int generation = 0;
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);
};

// ============================================================================
//  Cache de time-stretch / pitch-shift (Rubber Band, modo offline de alta qualidade)
// ============================================================================
class StretchCache : private juce::Thread
{
public:
    struct Key
    {
        int sourceId; int ratioQ; int pitchQ;
        bool operator< (const Key& o) const { return std::tie (sourceId, ratioQ, pitchQ) < std::tie (o.sourceId, o.ratioQ, o.pitchQ); }
        bool operator== (const Key& o) const { return sourceId == o.sourceId && ratioQ == o.ratioQ && pitchQ == o.pitchQ; }
    };
    static Key makeKey (int sourceId, double timeRatio, double pitchScale)
    {
        return { sourceId, (int) std::lround (timeRatio * 10000.0), (int) std::lround (pitchScale * 10000.0) };
    }

    explicit StretchCache (SourcePool& p);
    ~StretchCache() override;

    // Marca o inicio de uma "rodada" de requisicoes (a cada reconstrucao de snapshot)
    void beginRound();
    // Retorna o buffer pronto, ou nullptr se ainda processando (agenda o trabalho)
    BufferPtr request (const Key& k);
    // Libera entradas nao usadas na ultima rodada
    void endRound();

    bool hasPending() const;
    float progress() const { return currentProgress.load(); }
    juce::String currentJobName() const;

    std::function<void()> onJobFinished;  // chamado na thread de mensagens

    // processamento sincrono (exportacao)
    static BufferPtr render (const juce::AudioBuffer<float>& src, double sampleRate, double timeRatio, double pitchScale,
                             std::function<bool (float)> progressCb = {});

private:
    void run() override;

    SourcePool& sourcePool;
    mutable juce::CriticalSection lock;
    std::map<Key, BufferPtr> ready;
    std::vector<Key> queue;
    std::vector<Key> wanted, wantedThisRound;
    Key working { -1, 0, 0 };
    std::atomic<float> currentProgress { 0.0f };
    juce::WaitableEvent wake;
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);
};

} // namespace mp
