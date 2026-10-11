#pragma once
#include <JuceHeader.h>
#include "Widgets.h"

namespace mp
{
// Mapa de teclas estilo MPC (pad 1 = canto inferior esquerdo)
//   1 2 3 4    -> pads 13-16
//   Q W E R    -> pads 9-12
//   A S D F    -> pads 5-8
//   Z X C V    -> pads 1-4
inline int padKeyCode (int indexInBank)
{
    static const char keys[16] = { 'Z','X','C','V','A','S','D','F','Q','W','E','R','1','2','3','4' };
    return keys[juce::jlimit (0, 15, indexInBank)];
}

class PadPanel : public juce::Component,
                 public Session::Listener,
                 public juce::FileDragAndDropTarget,
                 public juce::DragAndDropTarget,
                 private juce::Timer
{
public:
    explicit PadPanel (Session& s);
    ~PadPanel() override;

    void sessionChanged (int what) override;
    void resized() override;
    void paint (juce::Graphics& g) override;

    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void filesDropped (const juce::StringArray& files, int x, int y) override;
    void fileDragMove (const juce::StringArray&, int x, int y) override { dragOverPad = padAt ({ x, y }); repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragOverPad = -1; repaint(); }
    bool isInterestedInDragSource (const SourceDetails&) override { return true; }
    void itemDragMove (const SourceDetails& d) override { dragOverPad = padAt (d.localPosition); repaint(); }
    void itemDragExit (const SourceDetails&) override { dragOverPad = -1; repaint(); }
    void itemDropped (const SourceDetails& d) override;

    void loadFileIntoPad (const juce::File& f, int pad);
    void refreshEditor();

private:
    class PadButton;
    class PadWave;
    void timerCallback() override;
    int padAt (juce::Point<int> p) const;
    void editPad (const juce::String& label, std::function<void (Pad&)> fn, bool undo = true);

    Session& session;
    juce::OwnedArray<PadButton> pads;
    juce::TextButton bankBtns[kNumBanks];
    std::unique_ptr<PadWave> wave;
    Knob vol { TR ("Volume"), -40.0, 12.0, 0.0, 0.1 }, pan { TR ("Pan"), -1.0, 1.0, 0.0, 0.01 },
         tune { TR ("Tom"), -24.0, 24.0, 0.0, 1.0 }, fine { TR ("Fino"), -100.0, 100.0, 0.0, 1.0 },
         attack { TR ("Ataque"), 0.0, 500.0, 1.0, 0.1 }, release { TR ("Release"), 1.0, 3000.0, 40.0, 1.0 },
         cutoff { TR ("Filtro"), 40.0, 20000.0, 20000.0, 1.0 }, reso { TR ("Resson."), 0.0, 1.0, 0.1, 0.01 };
    juce::ComboBox modeBox, chokeBox;
    juce::TextButton revBtn { TR ("Reverso") }, loadBtn { TR ("Carregar...") }, clearBtn { TR ("Limpar") }, editBtn { TR ("Abrir no editor") };
    int dragOverPad = -1;
    std::array<int, kNumPads> lastActivity {};
    std::array<float, kNumPads> flash {};
    juce::Rectangle<int> padArea, editorArea;
    std::unique_ptr<juce::FileChooser> chooser;

public:
    std::function<void (int sourceId, double s, double e)> onOpenInEditor;
};

} // namespace mp
