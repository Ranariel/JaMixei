#pragma once
#include <JuceHeader.h>
#include "Widgets.h"
#include "Icons.h"

namespace mp
{
class ArrangeView : public juce::Component,
                    public Session::Listener,
                    public juce::FileDragAndDropTarget,
                    public juce::DragAndDropTarget,
                    private juce::Timer,
                    private juce::ScrollBar::Listener
{
public:
    explicit ArrangeView (Session& s);
    ~ArrangeView() override;

    void sessionChanged (int what) override;
    void resized() override;
    void paint (juce::Graphics& g) override;

    // zoom / rolagem
    double pxPerBeat = 24.0;
    double scrollBeats = 0.0;
    int scrollY = 0;
    bool followPlayhead = true;
    void zoomBy (double factor, float anchorX);
    void zoomToFit();
    double xToBeat (float x) const;     // x relativo a area das faixas
    float beatToX (double b) const;
    double snap (double beat, bool force = false) const;

    std::function<void (int sourceId, double startSec, double endSec)> onOpenInEditor;
    std::function<void()> onShowSequencer;

    // arquivos arrastados do Windows Explorer
    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void fileDragMove (const juce::StringArray&, int x, int y) override;
    void fileDragExit (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;
    // arrastar do navegador interno
    bool isInterestedInDragSource (const SourceDetails&) override { return true; }
    void itemDragMove (const SourceDetails& d) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails& d) override;

    void dropFileAt (const juce::File& f, juce::Point<int> posInThis);
    void dropSourceAt (int sourceId, juce::Point<int> posInThis);
    void dropPatternAt (int patternId, juce::Point<int> posInThis);

    double scrollTarget = -1;   // rolagem animada (-1 = nenhuma)
    double zoomTarget = -1;     // zoom animado
    int lastPlayheadX = -100000;
    static constexpr int headerW = 196;
    static constexpr int rulerH = 30;
    static constexpr int inspectorH = 64;

    int trackAtY (int yInLanes) const;   // indice
    int trackTop (int index) const;      // y relativo as lanes (ja com rolagem)
    int totalTracksHeight() const;

    Session& session;

private:
    class Ruler;
    class Lanes;
    class TrackHeader;
    class Inspector;
    class PlayheadLine;

    void timerCallback() override;
    void scrollBarMoved (juce::ScrollBar*, double) override;
    void rebuildHeaders();
    void updateScrollbars();
    void layoutHeaders();

    std::unique_ptr<Ruler> ruler;
    std::unique_ptr<Lanes> lanes;
    std::unique_ptr<Inspector> inspector;
    std::unique_ptr<PlayheadLine> playheadLine;
    juce::OwnedArray<TrackHeader> headers;
    juce::Component headerHolder;
    juce::ScrollBar hScroll { false }, vScroll { true };
    IconTextButton addAudioBtn { Icon::Plus, TR ("Áudio") }, addPatBtn { Icon::Plus, TR ("Padrões") };
    juce::ComboBox snapBox;
    IconButton fitBtn { Icon::Fit, TR ("Ajustar o zoom para ver a música inteira") },
               followBtn { Icon::Follow, TR ("Seguir o cursor durante a reprodução") };
    juce::Rectangle<int> dropHighlight;
    friend class Lanes;
};

} // namespace mp
