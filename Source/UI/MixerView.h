#pragma once
#include <JuceHeader.h>
#include "Widgets.h"

namespace mp
{
class MixerView : public juce::Component, public Session::Listener, private juce::Timer
{
public:
    explicit MixerView (Session& s);
    ~MixerView() override;
    void sessionChanged (int what) override;
    void resized() override;
    void paint (juce::Graphics& g) override;

private:
    class Strip;
    class MasterSection;
    void timerCallback() override;
    void rebuild();

    Session& session;
    juce::Viewport viewport;
    juce::Component stripHolder;
    juce::OwnedArray<Strip> strips;
    std::unique_ptr<MasterSection> master;
};

} // namespace mp
