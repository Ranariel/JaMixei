#pragma once
#include <JuceHeader.h>
#include "Widgets.h"

namespace mp
{
class SampleEditor : public juce::Component, public Session::Listener, private juce::Timer
{
public:
    explicit SampleEditor (Session& s);
    ~SampleEditor() override;

    void sessionChanged (int what) override;
    void resized() override;
    void paint (juce::Graphics& g) override;

    void open (int sourceId, double selStart, double selEnd);
    void auditionSelection();
    void togglePlay();      // Play/Espaco na aba Editor: toca (ou para) a selecao/o sample
    bool hasSource() const;

private:
    class Overview;
    class Wave;
    void timerCallback() override;
    void refreshSources();
    void refreshInfo();
    void setView (double a, double b);
    std::vector<double>& markers();
    void doChop();
    void sendSlicesToPads();
    void selectionToPad();
    void selectionToArrange();
    void exportSelection();
    bool rangeForAction (double& a, double& b) const;

    Session& session;
    std::unique_ptr<Overview> overview;
    std::unique_ptr<Wave> wave;
    std::map<int, std::vector<double>> markerMap;

    double viewStart = 0, viewEnd = 10;
    double selA = -1, selB = -1;

    juce::ComboBox sourceBox, chopMode, divBox, beatBox, firstPadBox;
    juce::TextButton importBtn { TR ("Importar...") }, detectBtn { TR ("Detectar BPM") }, x2Btn { "x2" }, halfBtn { "/2" },
                     gridBtn { TR ("Grade") }, zoomInBtn { "+" }, zoomOutBtn { "-" }, zoomAllBtn { TR ("Tudo") }, zoomSelBtn { TR ("Seleção") },
                     chopBtn { TR ("Fatiar") }, clearBtn { TR ("Limpar marcadores") },
                     toPadsBtn { TR ("Fatias -> Pads") }, selPadBtn { TR ("Seleção -> Pad atual") }, selArrBtn { TR ("Seleção -> Arranjo") },
                     playSelBtn { TR ("Ouvir seleção") }, exportBtn { TR ("Exportar seleção...") };
    juce::ToggleButton loopToggle { TR ("Repetir") };
    juce::ToggleButton chokeToggle { TR ("Fatias se cortam (estilo MPC)") };
    Knob sens { TR ("Sensib."), 0.0, 1.0, 0.5, 0.01 };
    juce::Label bpmLabel;
    std::unique_ptr<juce::FileChooser> chooser;
    friend class Wave;
    friend class Overview;
};

} // namespace mp
