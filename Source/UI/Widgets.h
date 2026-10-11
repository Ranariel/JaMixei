#pragma once
#include <JuceHeader.h>
#include "Theme.h"
#include "../Session.h"

namespace mp
{
// Desativa foco de teclado em tudo (exceto editores de texto), para que os atalhos
// dos pads e do transporte sempre cheguem a janela principal.
inline void disableFocusRecursively (juce::Component& c)
{
    if (dynamic_cast<juce::TextEditor*> (&c) == nullptr)
    {
        c.setWantsKeyboardFocus (false);
        c.setMouseClickGrabsKeyboardFocus (false);
    }
    for (auto* ch : c.getChildren()) disableFocusRecursively (*ch);
}

inline juce::String formatDb (float db) { return db <= -60.0f ? juce::String ("-inf") : (db > 0 ? "+" : "") + juce::String (db, 1) + " dB"; }

// ----------------------------------------------------------------------------
//  Knob com rotulo, ligado ao modelo via funcoes get/set
// ----------------------------------------------------------------------------
class Knob : public juce::Component
{
public:
    Knob (const juce::String& title, double min, double max, double def, double interval = 0.0)
        : name (title), defaultValue (def)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRange (min, max, interval);
        slider.setDoubleClickReturnValue (true, def);
        slider.setValue (def, juce::dontSendNotification);
        slider.setMouseDragSensitivity (180);
        slider.setPopupDisplayEnabled (false, false, nullptr);
        addAndMakeVisible (slider);
        slider.onDragStart = [this] { dragging = true; if (onStart) onStart(); };
        slider.onDragEnd = [this] { dragging = false; repaint(); };
        slider.onValueChange = [this] { if (! updating && onChange) onChange (slider.getValue()); repaint(); };
        slider.addMouseListener (this, false);
    }

    void setSkew (double mid) { slider.setSkewFactorFromMidPoint (mid); }
    void setColour (juce::Colour c) { slider.setColour (juce::Slider::rotarySliderFillColourId, c); }
    void setValue (double v) { if (dragging) return; updating = true; slider.setValue (v, juce::dontSendNotification); updating = false; repaint(); }
    double getValue() const { return slider.getValue(); }
    juce::Slider& getSlider() { return slider; }

    std::function<void()> onStart;
    std::function<void (double)> onChange;
    std::function<juce::String (double)> formatter;

    void resized() override
    {
        auto r = getLocalBounds();
        r.removeFromBottom (17);
        slider.setBounds (r.withSizeKeepingCentre (juce::jmin (r.getWidth(), r.getHeight()), juce::jmin (r.getWidth(), r.getHeight())));
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().removeFromBottom (17);
        const bool showVal = dragging || hover;
        g.setColour (showVal ? Theme::accent : Theme::textDim.brighter (0.15f));
        g.setFont (Theme::font (11.5f, showVal));
        juce::String t = showVal ? (formatter ? formatter (slider.getValue()) : juce::String (slider.getValue(), 2)) : name;
        g.drawFittedText (t, r, juce::Justification::centred, 1, 1.0f);
    }
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }

private:
    juce::Slider slider;
    juce::String name;
    double defaultValue;
    bool dragging = false, hover = false, updating = false;
};

// ----------------------------------------------------------------------------
//  Medidor de nivel (le um pico atomico e decai)
// ----------------------------------------------------------------------------
class LevelMeter : public juce::Component
{
public:
    std::function<float()> readPeak;    // deve retornar e zerar o pico
    void tick()
    {
        const float p = readPeak ? readPeak() : 0.0f;
        // chamado a ~60 quadros por segundo
        level = juce::jmax (p, level * 0.925f);
        if (p > hold) { hold = p; holdTime = 60; }
        else if (--holdTime < 0) hold *= 0.975f;
        if (std::abs (level - lastDrawnLevel) < 0.002f && std::abs (hold - lastDrawnHold) < 0.002f) return;   // nada mudou
        lastDrawnLevel = level; lastDrawnHold = hold;
        repaint();
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Theme::bg);
        g.fillRoundedRectangle (r, 2.0f);
        auto toY = [&] (float lin)
        {
            const float db = juce::Decibels::gainToDecibels (lin, -60.0f);
            return juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f);
        };
        const float v = toY (level);
        const bool vertical = r.getHeight() > r.getWidth();
        juce::ColourGradient grad = vertical
            ? juce::ColourGradient (Theme::danger, 0, r.getY(), Theme::good, 0, r.getBottom(), false)
            : juce::ColourGradient (Theme::good, r.getX(), 0, Theme::danger, r.getRight(), 0, false);
        grad.addColour (0.75, Theme::good);
        grad.addColour (0.12, Theme::warn);
        g.setGradientFill (grad);
        if (vertical) g.fillRoundedRectangle (r.withTop (r.getBottom() - r.getHeight() * v), 2.0f);
        else g.fillRoundedRectangle (r.withWidth (r.getWidth() * v), 2.0f);
        const float h = toY (hold);
        g.setColour (hold >= 0.99f ? Theme::danger : Theme::text.withAlpha (0.7f));
        if (vertical) g.fillRect (r.getX(), r.getBottom() - r.getHeight() * h, r.getWidth(), 2.0f);
        else g.fillRect (r.getX() + r.getWidth() * h - 2, r.getY(), 2.0f, r.getHeight());
    }
private:
    float level = 0, hold = 0, lastDrawnLevel = -1, lastDrawnHold = -1;
    int holdTime = 0;
};

// Botao de texto pequeno com cor ligado/desligado
inline void styleToggle (juce::TextButton& b, juce::Colour on)
{
    b.setClickingTogglesState (false);
    b.setColour (juce::TextButton::buttonOnColourId, on);
    b.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
}

// Desenha a forma de onda de uma fonte entre [startSec, endSec] dentro de r
void drawWaveform (juce::Graphics& g, const LoadedSource& src, juce::Rectangle<float> r, double startSec, double endSec,
                   juce::Colour colour, bool reverse = false, float gain = 1.0f);

} // namespace mp
