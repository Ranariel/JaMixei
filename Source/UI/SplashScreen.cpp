#include "SplashScreen.h"
#include "Theme.h"

namespace mp
{
namespace
{
    juce::Font splashFont (float h, bool bold)
    {
        juce::Font f (Theme::sansTypeface (bold));
        f.setHeight (h * 1.08f);
        return f;
    }

    // batida que "toca" nos pads enquanto carrega (16 passos, indice do pad)
    const int beat[16] = { 12, 5, 9, 5, 13, 5, 10, 6, 12, 5, 9, 14, 13, 6, 3, 15 };
}

SplashScreen::SplashScreen (const juce::String& v) : version (v)
{
    status = TR ("Iniciando...");
    setOpaque (true);
    setSize (760, 428);
    addToDesktop (juce::ComponentPeer::windowHasDropShadow | juce::ComponentPeer::windowAppearsOnTaskbar);
    setName ("Ja Mixei");
    if (auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        setBounds (getLocalBounds().withCentre (d->userArea.getCentre()));
    else
        centreWithSize (getWidth(), getHeight());
    setAlwaysOnTop (true);
    setVisible (true);
    toFront (false);
    startTimerHz (40);
}

SplashScreen::~SplashScreen()
{
    stopTimer();
}

void SplashScreen::setStatus (const juce::String& text, float p)
{
    status = text;
    progress = juce::jlimit (0.0f, 1.0f, p);
    repaint();
}

void SplashScreen::fadeOutAndClose (std::function<void()> gone)
{
    onGone = std::move (gone);
    setAlwaysOnTop (false);
    fading = true;
}

void SplashScreen::timerCallback()
{
    phase += 1.0 / 40.0;
    shownProgress += (progress - shownProgress) * 0.25f;
    if (fading)
    {
        alpha -= 0.12f;
        if (alpha <= 0.0f)
        {
            stopTimer();
            setVisible (false);
            if (auto f = std::move (onGone)) f();   // pode destruir este objeto
            return;
        }
        setAlpha (alpha);
    }
    repaint();
}

void SplashScreen::drawPadGrid (juce::Graphics& g, juce::Rectangle<float> area)
{
    const float gap = 10.0f;
    const float cell = (juce::jmin (area.getWidth(), area.getHeight()) - gap * 3) / 4.0f;
    const auto origin = area.getCentre() - juce::Point<float> (2 * cell + 1.5f * gap, 2 * cell + 1.5f * gap);
    const int step = (int) (phase * 7.0) % 16;
    const float stepFrac = (float) std::fmod (phase * 7.0, 1.0);
    const int loaded = (int) std::round (shownProgress * 16.0f);

    for (int i = 0; i < 16; ++i)
    {
        const int row = i / 4, col = i % 4;
        juce::Rectangle<float> r (origin.x + col * (cell + gap), origin.y + row * (cell + gap), cell, cell);
        const bool isLoaded = i < loaded;
        float glow = 0.0f;
        if (beat[step] == i) glow = 1.0f - stepFrac * 0.35f;
        else if (beat[(step + 15) % 16] == i) glow = 0.35f * (1.0f - stepFrac);

        auto base = isLoaded ? juce::Colour (0xff2a2f3a) : juce::Colour (0xff1c2028);
        g.setGradientFill (juce::ColourGradient (base.brighter (0.15f), r.getX(), r.getY(), base.darker (0.3f), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 7.0f);

        if (glow > 0.01f)
        {
            const auto c = (i % 3 == 0) ? Theme::accent2 : Theme::accent;
            // halo
            for (int k = 3; k >= 1; --k)
            {
                g.setColour (c.withAlpha (0.10f * glow * (4 - k)));
                g.fillRoundedRectangle (r.expanded (k * 4.0f), 9.0f + k * 3.0f);
            }
            g.setGradientFill (juce::ColourGradient (c.brighter (0.45f).withAlpha (glow), r.getCentreX(), r.getY(),
                                                     c.withAlpha (0.95f * glow), r.getCentreX(), r.getBottom(), false));
            g.fillRoundedRectangle (r.reduced (1.0f), 6.0f);
        }
        g.setColour (isLoaded ? Theme::accent.withAlpha (0.55f) : juce::Colour (0xff343a46));
        g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.2f);
        // brilho no topo do pad
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.fillRoundedRectangle (r.reduced (4.0f).withHeight (cell * 0.25f), 4.0f);
    }
}

void SplashScreen::drawWave (juce::Graphics& g, juce::Rectangle<float> area)
{
    juce::Path p;
    const int n = 180;
    for (int i = 0; i <= n; ++i)
    {
        const float x = area.getX() + area.getWidth() * (float) i / (float) n;
        const float t = (float) i / (float) n;
        const float env = std::sin (t * juce::MathConstants<float>::pi);
        const float y = area.getCentreY() + env * area.getHeight() * 0.45f
                        * (0.55f * std::sin (t * 38.0f + (float) phase * 3.0f) * std::sin (t * 7.0f - (float) phase * 1.3f)
                           + 0.25f * std::sin (t * 91.0f + (float) phase * 5.0f));
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    juce::ColourGradient grad (Theme::accent.withAlpha (0.0f), area.getX(), 0, Theme::accent2.withAlpha (0.0f), area.getRight(), 0, false);
    grad.addColour (0.3, Theme::accent.withAlpha (0.45f));
    grad.addColour (0.7, Theme::accent2.withAlpha (0.35f));
    g.setGradientFill (grad);
    g.strokePath (p, juce::PathStrokeType (1.6f));
}

void SplashScreen::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    // fundo
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1b1f27), 0, 0, juce::Colour (0xff0a0b0e), b.getRight(), b.getBottom(), false));
    g.fillAll();
    // feixe de luz diagonal
    {
        juce::Path beam;
        beam.addQuadrilateral (300, 0, 470, 0, 250, b.getBottom(), 80, b.getBottom());
        g.setGradientFill (juce::ColourGradient (Theme::accent.withAlpha (0.07f), 300, 0, Theme::accent.withAlpha (0.0f), 160, b.getBottom(), false));
        g.fillPath (beam);
        juce::Path beam2;
        beam2.addQuadrilateral (520, 0, 560, 0, 400, b.getBottom(), 360, b.getBottom());
        g.setGradientFill (juce::ColourGradient (Theme::accent2.withAlpha (0.05f), 540, 0, Theme::accent2.withAlpha (0.0f), 380, b.getBottom(), false));
        g.fillPath (beam2);
    }

    // pads a direita
    drawPadGrid (g, { 452.0f, 34.0f, 270.0f, 270.0f });

    // onda decorativa
    drawWave (g, { 30.0f, 230.0f, 400.0f, 70.0f });

    // marca
    const float lx = 40.0f;
    g.setColour (Theme::accent);
    g.fillRoundedRectangle (lx, 58.0f, 46.0f, 5.0f, 2.5f);
    g.setFont (splashFont (66.0f, true));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffb27a), lx, 70, Theme::accent.darker (0.15f), lx, 140, false));
    g.drawText ("JA MIXEI", juce::Rectangle<float> (lx - 3, 66.0f, 420.0f, 76.0f), juce::Justification::centredLeft);
    g.setColour (Theme::text);
    g.setFont (splashFont (19.0f, false));
    g.drawText (juce::String (juce::CharPointer_UTF8 ("S A M P L E R   \xe2\x80\xa2   R E M I X")),
                juce::Rectangle<float> (lx, 142.0f, 400.0f, 26.0f), juce::Justification::centredLeft);
    g.setColour (Theme::textDim);
    g.setFont (splashFont (15.0f, false));
    g.drawText (TR ("Edições, montagens e remixes das suas músicas"),
                juce::Rectangle<float> (lx, 174.0f, 400.0f, 22.0f), juce::Justification::centredLeft);
    g.setColour (Theme::accent.withAlpha (0.9f));
    g.setFont (splashFont (14.0f, true));
    g.drawText (TR ("Versão ") + version, juce::Rectangle<float> (lx, 200.0f, 300.0f, 20.0f), juce::Justification::centredLeft);

    // rodape: status + progresso + creditos
    auto foot = b.withTrimmedTop (b.getHeight() - 104.0f).reduced (40.0f, 0.0f);
    g.setColour (Theme::text.withAlpha (0.9f));
    g.setFont (splashFont (15.0f, false));
    g.drawText (status, foot.removeFromTop (26.0f), juce::Justification::centredLeft, true);
    foot.removeFromTop (6.0f);
    auto bar = foot.removeFromTop (4.0f);
    g.setColour (juce::Colour (0xff262b35));
    g.fillRoundedRectangle (bar, 2.0f);
    auto fill = bar.withWidth (bar.getWidth() * shownProgress);
    g.setGradientFill (juce::ColourGradient (Theme::accent, fill.getX(), 0, juce::Colour (0xffffb27a), fill.getRight(), 0, false));
    g.fillRoundedRectangle (fill, 2.0f);
    // brilho correndo na barra
    if (shownProgress < 0.999f)
    {
        const float sx = bar.getX() + (float) std::fmod (phase * 0.6, 1.0) * fill.getWidth();
        juce::ColourGradient sh (juce::Colours::white.withAlpha (0.0f), sx - 40, 0, juce::Colours::white.withAlpha (0.0f), sx + 40, 0, false);
        sh.addColour (0.5, juce::Colours::white.withAlpha (0.55f));
        g.setGradientFill (sh);
        g.fillRect (bar.getIntersection (fill).withX (juce::jmax (fill.getX(), sx - 40)).withRight (juce::jmin (fill.getRight(), sx + 40)));
    }
    foot.removeFromTop (14.0f);
    auto credits = foot.removeFromTop (22.0f);
    const juce::String by = TR ("Criado por ");
    g.setColour (Theme::textDim);
    g.setFont (splashFont (15.0f, false));
    const float byW = g.getCurrentFont().getStringWidthFloat (by);
    g.drawText (by, credits.withWidth (byW + 2), juce::Justification::centredLeft);
    g.setColour (Theme::accent);
    g.setFont (splashFont (15.0f, true));
    g.drawText ("Ranariel", credits.withTrimmedLeft (byW), juce::Justification::centredLeft);
    g.setColour (Theme::textFaint);
    g.setFont (splashFont (13.0f, false));
    g.drawText (TR ("© 2026 Ranariel. Software livre (licença GPL v3)."), credits, juce::Justification::centredRight);

    // borda
    g.setColour (juce::Colour (0xff2f3540));
    g.drawRect (getLocalBounds(), 1);
    g.setColour (Theme::accent.withAlpha (0.8f));
    g.fillRect (0, 0, getWidth(), 2);
}

} // namespace mp
