#pragma once
#include <JuceHeader.h>

namespace mp
{
// Tela de inicializacao (estilo dos editores profissionais): logo, versao,
// creditos e o progresso do carregamento das ferramentas.
class SplashScreen : public juce::Component, private juce::Timer
{
public:
    explicit SplashScreen (const juce::String& version);
    ~SplashScreen() override;

    void setStatus (const juce::String& text, float progress);
    void fadeOutAndClose (std::function<void()> onGone);   // some suavemente e chama onGone

    void paint (juce::Graphics& g) override;

private:
    void timerCallback() override;
    void drawPadGrid (juce::Graphics& g, juce::Rectangle<float> area);
    void drawWave (juce::Graphics& g, juce::Rectangle<float> area);

    juce::String version, status;
    float progress = 0.0f, shownProgress = 0.0f;
    double phase = 0.0;
    float alpha = 1.0f;
    bool fading = false;
    std::function<void()> onGone;
};

} // namespace mp
