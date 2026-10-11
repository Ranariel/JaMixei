#pragma once
#include <JuceHeader.h>
#include "Widgets.h"

namespace mp
{
class SequencerView : public juce::Component, public Session::Listener, private juce::Timer
{
public:
    explicit SequencerView (Session& s);
    ~SequencerView() override;
    void sessionChanged (int what) override;
    void resized() override;
    void paint (juce::Graphics& g) override;

private:
    class Grid;
    void timerCallback() override;
    void refresh();

    Session& session;
    std::unique_ptr<Grid> grid;
    juce::ComboBox patternBox, lengthBox, velBox;
    juce::TextButton newBtn { TR ("+ Novo") }, dupBtn { TR ("Duplicar") }, renBtn { TR ("Renomear") }, delBtn { TR ("Apagar") },
                     insertBtn { TR ("Inserir no arranjo") }, clearBtn { TR ("Limpar") }, playPatBtn { TR ("Loop do padrão") };
    Knob swing { TR ("Swing"), 50.0, 75.0, 50.0, 0.5 };
public:
    int clickVelocity = 100;
};

} // namespace mp
