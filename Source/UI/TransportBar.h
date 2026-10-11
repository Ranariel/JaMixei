#pragma once
#include <JuceHeader.h>
#include "Widgets.h"
#include "Icons.h"

namespace mp
{
class TransportBar : public juce::Component, public Session::Listener, private juce::Timer
{
public:
    explicit TransportBar (Session& s);
    ~TransportBar() override;
    void sessionChanged (int what) override;
    void resized() override;
    void paint (juce::Graphics& g) override;

    std::function<juce::PopupMenu (int index)> getMenu;      // 0 Arquivo, 1 Editar, 2 Ferramentas, 3 Ajuda
    std::function<void (int menu, int item)> onMenuItem;
    std::function<void()> onAudioSettings;
    std::function<void()> onAudioRecord;

    static constexpr int preferredHeight = 64;

private:
    class BpmDisplay;
    class PositionDisplay;
    void timerCallback() override;
    void refresh();

    Session& session;
    juce::TextButton menuBtns[4];
    IconButton undoBtn { Icon::Undo, TR ("Desfazer (Ctrl+Z)") }, redoBtn { Icon::Redo, TR ("Refazer (Ctrl+Y)") };
    IconButton stopBtn { Icon::Stop, TR ("Parar / voltar ao início (Enter)") },
               playBtn { Icon::Play, TR ("Tocar / pausar (Espaço)") },
               recBtn { Icon::Record, TR ("Gravar pads no padrão (Ctrl+R)") },
               loopBtn { Icon::Loop, TR ("Loop (Ctrl+L)") },
               micBtn { Icon::Mic, TR ("Gravar áudio: microfone ou o mix com os pads ao vivo (Ctrl+Shift+R)") },
               metroBtn { Icon::Metronome, TR ("Metrônomo (Ctrl+M)") },
               settingsBtn { Icon::Sliders, TR ("Configurações de áudio") };
    juce::TextButton songBtn { TR ("MÚSICA") }, patBtn { TR ("PADRÃO") }, tapBtn { "TAP" };
    std::unique_ptr<BpmDisplay> bpm;
    std::unique_ptr<PositionDisplay> position;
    LevelMeter meterL, meterR;
    std::vector<double> taps;
    juce::Rectangle<int> transportCard, modeCard, lcdCard, masterCard;
    double lastPos = -1;
};

} // namespace mp
