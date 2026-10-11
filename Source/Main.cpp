#include <JuceHeader.h>
#include "Session.h"
#include "UI/MainComponent.h"
#include "UI/SplashScreen.h"

#ifndef MEUPAD_VERSION
 #define MEUPAD_VERSION "1.4.0"
#endif

class MeuPadApplication : public juce::JUCEApplication, private juce::Timer
{
public:
    const juce::String getApplicationName() override { return "Ja Mixei"; }
    const juce::String getApplicationVersion() override { return MEUPAD_VERSION; }
    // "--restart": nova instancia aberta pelo proprio programa ao trocar de idioma
    bool moreThanOneInstanceAllowed() override { return getCommandLineParameters().contains ("--restart"); }

    void initialise (const juce::String& commandLine) override
    {
        if (juce::SystemStats::getEnvironmentVariable ("MEUPAD_DEBUG", {}).isNotEmpty())
        {
            logger.reset (juce::FileLogger::createDefaultAppLogger ("MeuPadStudio", "debug.txt", "MeuPad debug"));
            juce::Logger::setCurrentLogger (logger.get());
        }
        mp::i18n::setLanguage (initialLanguage());
        // o visual pertence ao aplicativo: vive mais que todas as janelas
        juce::LookAndFeel::setDefaultLookAndFeel (&look);

        pendingCommandLine = commandLine;
        startTime = juce::Time::getMillisecondCounterHiRes();
        if (juce::SystemStats::getEnvironmentVariable ("MEUPAD_NOSPLASH", {}).isEmpty())
            splash = std::make_unique<mp::SplashScreen> (MEUPAD_VERSION);
        loadStep = 0;
        startTimer (60);
    }

    void shutdown() override
    {
        stopTimer();
        quitting = true;
        if (window != nullptr)
        {
            if (window->main != nullptr) window->main->releaseGraphics();
            window->setVisible (false);   // a janela some na hora
        }
        splash.reset();
        window.reset();
        session.reset();
        mp::UiAnimator::get().shutdown();
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
        juce::Logger::setCurrentLogger (nullptr);
    }

    void systemRequestedQuit() override
    {
        if (quitting) return;
        if (window != nullptr && window->main != nullptr)
            window->main->requestQuit ([this]
            {
                quitting = true;
                if (window != nullptr)
                {
                    window->main->releaseGraphics();   // solta a GPU antes de esconder a janela
                    window->setVisible (false);
                }
                quit();
            });
        else
        {
            quitting = true;
            quit();
        }
    }

    void anotherInstanceStarted (const juce::String& commandLine) override
    {
        auto f = juce::File (commandLine.unquoted().trim());
        if (window != nullptr && window->main != nullptr && f.existsAsFile() && f.hasFileExtension ("mpad"))
            window->main->requestQuit ([this, f] { if (window != nullptr) window->main->openFile (f); });
        else if (window != nullptr)
            window->toFront (true);
    }

    class MainWindow : public juce::DocumentWindow
    {
    public:
        explicit MainWindow (mp::Session& s)
            : DocumentWindow ("Ja Mixei", mp::Theme::bg, DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            main = new mp::MainComponent (s);
            setContentOwned (main, true);
            setResizable (true, false);
            setResizeLimits (1100, 700, 10000, 10000);
            centreWithSize (juce::jmin (1700, getParentMonitorArea().getWidth() - 40),
                            juce::jmin (1000, getParentMonitorArea().getHeight() - 60));
            setVisible (false);   // so aparece quando o carregamento terminar
        }
        void showNow()
        {
            setVisible (true);
            if (getParentMonitorArea().getWidth() <= 1920) setFullScreen (true);  // maximizado em telas comuns
            toFront (true);
            main->grabKeyboardFocus();
        }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
        mp::MainComponent* main = nullptr;
    };

    // idioma salvo nas preferencias; na primeira vez, o escolhido no instalador
    static mp::i18n::Lang initialLanguage()
    {
        juce::PropertiesFile::Options o;
        o.applicationName = "MeuPadStudio";
        o.filenameSuffix = ".settings";
        o.folderName = "MeuPadStudio";
        o.osxLibrarySubFolder = "Application Support";
        juce::PropertiesFile pf (o);
        juce::String c = pf.getValue ("language");
       #if JUCE_WINDOWS
        // idioma escolhido no instalador: vale quando o programa e (re)instalado com outro idioma
        auto reg = juce::WindowsRegistry::getValue ("HKEY_LOCAL_MACHINE\\Software\\JaMixei\\Language");
        if (reg.isNotEmpty() && reg != pf.getValue ("installerLanguage"))
        {
            c = reg;
            pf.setValue ("language", reg);
            pf.setValue ("installerLanguage", reg);
            pf.saveIfNeeded();
        }
       #endif
        if (c.isEmpty()) c = "pt";
        return mp::i18n::fromCode (c);
    }

private:
    // carregamento em etapas, para a tela de inicializacao mostrar o progresso
    void timerCallback() override
    {
        auto say = [this] (const juce::String& text, float p)
        {
            if (splash != nullptr) splash->setStatus (text, p);
        };
        stopTimer();
        int next = 220;
        switch (loadStep++)
        {
            case 0: say (TR ("Iniciando o motor de áudio..."), 0.08f); break;
            case 1:
                session = std::make_unique<mp::Session>();
                say (TR ("Carregando o time-stretch profissional (Rubber Band)..."), 0.30f);
                break;
            case 2: say (TR ("Carregando decodificadores (MP3, WAV, FLAC, OGG, M4A)..."), 0.42f); break;
            case 3: say (TR ("Carregando o codificador MP3 (LAME 3.100)..."), 0.54f); break;
            case 4:
            {
                juce::String dev = TR ("nenhum");
                if (auto* d = session->engine.devices().getCurrentAudioDevice())
                    dev = d->getName() + "  (" + juce::String ((int) d->getCurrentSampleRate()) + " Hz)";
                if (splash != nullptr) splash->setStatus (TR ("Dispositivo de áudio: ") + dev, 0.64f);
                next = 300;
                break;
            }
            case 5: say (TR ("Carregando efeitos: EQ, filtro, reverb, delay, limitador..."), 0.74f); break;
            case 6: say (TR ("Montando a interface e a aceleração gráfica (GPU)..."), 0.88f); break;
            case 7: say (TR ("Carregando atalhos do teclado e preferências..."), 0.96f); break;
            case 8:
            {
                say (TR ("Pronto!"), 1.0f);
                const double elapsed = juce::Time::getMillisecondCounterHiRes() - startTime;
                next = (int) juce::jmax (150.0, 2600.0 - elapsed);   // a tela fica pelo menos ~2,6 s
                break;
            }
            case 9:
            {
                window = std::make_unique<MainWindow> (*session);
                window->showNow();
                auto f = juce::File (pendingCommandLine.unquoted().trim());
                if (pendingCommandLine.isNotEmpty() && f.existsAsFile() && f.hasFileExtension ("mpad"))
                    window->main->openFile (f);
                if (splash != nullptr)
                    splash->fadeOutAndClose ([this] { juce::MessageManager::callAsync ([this] { splash.reset(); }); });
                return;
            }
            default: return;
        }
        startTimer (next);
    }

    mp::MeuPadLook look;
    std::unique_ptr<juce::FileLogger> logger;
    std::unique_ptr<mp::SplashScreen> splash;
    std::unique_ptr<mp::Session> session;
    std::unique_ptr<MainWindow> window;
    juce::String pendingCommandLine;
    double startTime = 0;
    int loadStep = 0;
    bool quitting = false;
};

START_JUCE_APPLICATION (MeuPadApplication)
