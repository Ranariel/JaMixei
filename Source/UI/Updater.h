#pragma once
#include <JuceHeader.h>

namespace mp
{
// ----------------------------------------------------------------------------
//  Atualizacao automatica (igual ao Ja Telei):
//    1. le https://jaclipei.com/jamixei/latest.json  (gerado pela VPS a cada release do GitHub)
//    2. se a versao for mais nova: "Atualizar agora / Depois / Pular esta versao"
//    3. baixa o instalador com barra de progresso e confere o SHA-256
//    4. roda o instalador em modo silencioso (/S /RELAUNCH) e fecha o programa;
//       o instalador reabre o Ja Mixei ja atualizado
// ----------------------------------------------------------------------------
struct UpdateInfo
{
    juce::String version, url, sha256, notes;
    juce::int64 size = 0;
};

class Updater : private juce::Thread
{
public:
    // quitForUpdate: chamado com o instalador ja baixado e conferido; deve fechar o programa
    Updater (juce::PropertiesFile& settings, std::function<void (juce::File installer)> quitForUpdate);
    ~Updater() override;

    void checkInBackground (bool manual);       // manual = pelo menu (mostra "ja esta atualizado")
    bool autoCheckEnabled() const;
    void setAutoCheck (bool on);

    static int compareVersions (const juce::String& a, const juce::String& b);   // <0, 0, >0
    static bool parseInfo (const juce::String& json, UpdateInfo& out);

    static constexpr const char* feedUrl = "https://jaclipei.com/jamixei/latest.json";

private:
    void run() override;
    void showResult (bool ok, UpdateInfo info, bool manual);
    void askToInstall (const UpdateInfo& info);
    void download (const UpdateInfo& info);

    juce::PropertiesFile& settings;
    std::function<void (juce::File)> quitForUpdate;
    std::atomic<bool> manualCheck { false };
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);
    class DownloadWindow;
};
} // namespace mp
