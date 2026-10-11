#include "Updater.h"
#include "../I18n.h"

namespace mp
{
// ----------------------------------------------------------------------------
Updater::Updater (juce::PropertiesFile& s, std::function<void (juce::File)> q)
    : juce::Thread ("JaMixei update check"), settings (s), quitForUpdate (std::move (q)) {}

Updater::~Updater()
{
    *alive = false;
    stopThread (6000);
}

bool Updater::autoCheckEnabled() const { return settings.getBoolValue ("autoUpdateCheck", true); }
void Updater::setAutoCheck (bool on)  { settings.setValue ("autoUpdateCheck", on); settings.saveIfNeeded(); }

void Updater::checkInBackground (bool manual)
{
    if (isThreadRunning()) return;
    manualCheck = manual;
    startThread();
}

int Updater::compareVersions (const juce::String& a, const juce::String& b)
{
    auto pa = juce::StringArray::fromTokens (a.trim().trimCharactersAtStart ("vV"), ".", {});
    auto pb = juce::StringArray::fromTokens (b.trim().trimCharactersAtStart ("vV"), ".", {});
    for (int i = 0; i < juce::jmax (pa.size(), pb.size()); ++i)
    {
        const int x = i < pa.size() ? pa[i].getIntValue() : 0;
        const int y = i < pb.size() ? pb[i].getIntValue() : 0;
        if (x != y) return x < y ? -1 : 1;
    }
    return 0;
}

bool Updater::parseInfo (const juce::String& json, UpdateInfo& out)
{
    auto v = juce::JSON::parse (json);
    if (! v.isObject()) return false;
    out.version = v.getProperty ("version", {}).toString().trim();
    out.url     = v.getProperty ("url", {}).toString().trim();
    out.sha256  = v.getProperty ("sha256", {}).toString().trim().toLowerCase();
    out.notes   = v.getProperty ("notes", {}).toString().trim();
    out.size    = (juce::int64) (double) v.getProperty ("size", 0);
    if (out.version.isEmpty() || compareVersions (out.version, "0.0.0") <= 0) return false;
    // so aceita instaladores do proprio servidor, por https, com SHA-256 valido
    if (! out.url.startsWith ("https://jaclipei.com/jamixei/releases/")) return false;
    if (out.sha256.length() != 64 || ! out.sha256.containsOnly ("0123456789abcdef")) return false;
    return true;
}

void Updater::run()
{
    UpdateInfo info;
    bool ok = false;
    // "?t=" evita resposta antiga guardada em cache (Cloudflare / proxy)
    juce::URL url (juce::String (feedUrl) + "?t=" + juce::String (juce::Time::currentTimeMillis()));
    int status = 0;
    if (auto in = url.createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                             .withConnectionTimeoutMs (8000)
                                             .withStatusCode (&status)
                                             .withExtraHeaders ("Cache-Control: no-cache\r\nUser-Agent: JaMixei/"
                                                                + juce::JUCEApplication::getInstance()->getApplicationVersion())))
    {
        auto text = in->readEntireStreamAsString();
        ok = (status == 0 || status == 200) && parseInfo (text, info);
        // ainda sem nenhum build publicado: nao e erro
        if (! ok && text.contains ("\"0.0.0\"")) { ok = true; info = {}; }
    }
    if (threadShouldExit()) return;
    const bool manual = manualCheck;
    juce::MessageManager::callAsync ([this, a = alive, ok, info, manual]
    {
        if (*a) showResult (ok, info, manual);
    });
}

void Updater::showResult (bool ok, UpdateInfo info, bool manual)
{
    if (! ok)
    {
        if (manual)
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, TR ("Procurar atualizações"),
                                                    TR ("Não foi possível verificar atualizações. Verifique sua internet."));
        return;
    }
    const auto current = juce::JUCEApplication::getInstance()->getApplicationVersion();
    if (info.version.isEmpty() || compareVersions (info.version, current) <= 0)
    {
        if (manual)
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, TR ("Nenhuma atualização"),
                                                    TR ("Você já está usando a versão mais recente do Ja Mixei.") + "\n\n" + TR ("Versão instalada: ") + current);
        return;
    }
    // versao pulada pela pessoa: so avisa de novo se ela procurar pelo menu
    if (! manual && settings.getValue ("skipUpdateVersion") == info.version)
        return;
    askToInstall (info);
}

void Updater::askToInstall (const UpdateInfo& info)
{
    const auto current = juce::JUCEApplication::getInstance()->getApplicationVersion();
    juce::String msg = TR ("Uma nova versão do Ja Mixei está disponível: ") + info.version + "\n"
                     + TR ("Versão instalada: ") + current;
    if (info.notes.isNotEmpty())
    {
        auto notes = info.notes.length() > 600 ? info.notes.substring (0, 600) + "..." : info.notes;
        msg << "\n\n" << TR ("Novidades:") << "\n" << notes;
    }
    msg << "\n\n" << TR ("O Ja Mixei vai fechar e instalar a nova versão. Ele abre de novo sozinho quando terminar.");

    auto* w = new juce::AlertWindow (TR ("Atualização disponível"), msg, juce::MessageBoxIconType::NoIcon);
    w->addButton (TR ("Atualizar agora"), 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton (TR ("Depois"), 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->addButton (TR ("Pular esta versão"), 2);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([this, a = alive, info] (int r)
    {
        if (! *a) return;
        if (r == 2) { settings.setValue ("skipUpdateVersion", info.version); settings.saveIfNeeded(); }
        if (r == 1) download (info);
    }), true);
}

// ----------------------------------------------------------------------------
//  Download com barra de progresso + conferencia do SHA-256
// ----------------------------------------------------------------------------
class Updater::DownloadWindow : public juce::ThreadWithProgressWindow
{
public:
    DownloadWindow (UpdateInfo i, std::function<void (juce::File)> done, std::shared_ptr<bool> a)
        : juce::ThreadWithProgressWindow (TR ("Baixando a atualização..."), true, true, 10000),
          info (std::move (i)), onDone (std::move (done)), alive (std::move (a))
    {
        target = juce::File::getSpecialLocation (juce::File::tempDirectory)
                    .getChildFile ("Ja-Mixei-Atualizacao-" + info.version + ".exe");
    }

    void run() override
    {
        setProgress (-1.0);
        target.deleteFile();
        int status = 0;
        auto in = juce::URL (info.url).createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                                             .withConnectionTimeoutMs (15000)
                                                             .withStatusCode (&status));
        if (in == nullptr || (status != 0 && status != 200)) { error = TR ("Não foi possível baixar a atualização."); return; }

        const auto total = in->getTotalLength() > 0 ? in->getTotalLength() : info.size;
        {
            juce::FileOutputStream out (target);
            if (! out.openedOk()) { error = TR ("Não foi possível baixar a atualização."); return; }
            juce::HeapBlock<char> buf (65536);
            juce::int64 got = 0;
            while (! in->isExhausted())
            {
                if (threadShouldExit()) { cancelled = true; return; }
                const int n = in->read (buf, 65536);
                if (n < 0) { error = TR ("Não foi possível baixar a atualização."); return; }
                if (n == 0) break;
                out.write (buf, (size_t) n);
                got += n;
                if (total > 0)
                {
                    setProgress ((double) got / (double) total);
                    setStatusMessage (juce::String (got / 1048576.0, 1) + " / " + juce::String (total / 1048576.0, 1) + " MB");
                }
            }
            out.flush();
        }
        setProgress (1.0);
        const auto hash = juce::SHA256 (target).toHexString().toLowerCase();
        if (hash != info.sha256)
        {
            target.deleteFile();
            error = TR ("O arquivo baixado está corrompido (SHA-256 não confere). Tente de novo mais tarde.");
        }
    }

    void threadComplete (bool userPressedCancel) override
    {
        if (userPressedCancel || cancelled) target.deleteFile();
        else if (error.isNotEmpty())
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, TR ("Erro na atualização"), error);
        else if (*alive && onDone)
            onDone (target);
        delete this;
    }

private:
    UpdateInfo info;
    std::function<void (juce::File)> onDone;
    std::shared_ptr<bool> alive;
    juce::File target;
    juce::String error;
    bool cancelled = false;
};

void Updater::download (const UpdateInfo& info)
{
    (new DownloadWindow (info, quitForUpdate, alive))->launchThread();
}
} // namespace mp
