#include "Theme.h"
#include "Icons.h"
#include <MeuPadData.h>

namespace mp
{
// ============================================================================
//  Fontes embutidas
// ============================================================================
juce::Typeface::Ptr Theme::sansTypeface (bool bold)
{
    if (i18n::language() == i18n::Lang::Zh)
        return cjkTypeface (bold);   // chines: Noto Sans SC (tem os ideogramas e tambem o alfabeto latino)
    static juce::Typeface::Ptr med  = juce::Typeface::createSystemTypefaceFor (MeuPadData::InterMedium_ttf, (size_t) MeuPadData::InterMedium_ttfSize);
    static juce::Typeface::Ptr bld  = juce::Typeface::createSystemTypefaceFor (MeuPadData::InterBold_ttf, (size_t) MeuPadData::InterBold_ttfSize);
    return bold ? bld : med;
}

juce::Typeface::Ptr Theme::cjkTypeface (bool bold)
{
    static juce::Typeface::Ptr zmed = juce::Typeface::createSystemTypefaceFor (MeuPadData::NotoSansSCMedium_ttf, (size_t) MeuPadData::NotoSansSCMedium_ttfSize);
    static juce::Typeface::Ptr zbld = juce::Typeface::createSystemTypefaceFor (MeuPadData::NotoSansSCBold_ttf, (size_t) MeuPadData::NotoSansSCBold_ttfSize);
    return bold ? zbld : zmed;
}

juce::Font Theme::fontFor (const juce::String& text, float h, bool bold)
{
    if (i18n::language() != i18n::Lang::Zh)
        for (auto p = text.getCharPointer(); ! p.isEmpty(); ++p)
            if (*p >= 0x2E80)
            {
                juce::Font f (cjkTypeface (bold));
                f.setHeight (h * fontScale * 1.1f);
                return f;
            }
    return font (h, bold);
}

bool Theme::alertUsesCjk = false;

juce::Font Theme::withCjkFallback (juce::Font f, const juce::String& text)
{
    for (auto p = text.getCharPointer(); ! p.isEmpty(); ++p)
        if (*p >= 0x2E80)
        {
            if (f.getTypefaceName() == cjkTypeface (false)->getName() || f.getTypefaceName() == cjkTypeface (true)->getName()) return f;
            const float h = f.getHeight();
            juce::Font c (cjkTypeface (f.isBold() || f.getTypefaceName().containsIgnoreCase ("bold")));
            c.setHeight (h * 1.1f);
            return c;
        }
    return f;
}

juce::Typeface::Ptr Theme::monoTypeface (bool bold)
{
    static juce::Typeface::Ptr med = juce::Typeface::createSystemTypefaceFor (MeuPadData::JetBrainsMonoMedium_ttf, (size_t) MeuPadData::JetBrainsMonoMedium_ttfSize);
    static juce::Typeface::Ptr bld = juce::Typeface::createSystemTypefaceFor (MeuPadData::JetBrainsMonoBold_ttf, (size_t) MeuPadData::JetBrainsMonoBold_ttfSize);
    return bold ? bld : med;
}

juce::Font Theme::font (float h, bool bold)
{
    juce::Font f (sansTypeface (bold));
    // a fonte chinesa tem margens maiores: compensa para ficar do mesmo tamanho visual
    f.setHeight (h * fontScale * (i18n::language() == i18n::Lang::Zh ? 1.1f : 1.0f));
    return f;
}

juce::Font Theme::mono (float h, bool bold)
{
    juce::Font f (monoTypeface (bold));
    f.setHeight (h * fontScale);
    return f;
}

void Theme::drawCard (juce::Graphics& g, juce::Rectangle<float> r, float radius, juce::Colour fill, bool border)
{
    g.setColour (fill);
    g.fillRoundedRectangle (r, radius);
    if (border)
    {
        g.setColour (outline);
        g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.025f));
        g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + radius, r.getRight() - radius);
    }
}

// ============================================================================
//  Animador
// ============================================================================
UiAnimator& UiAnimator::get()
{
    static UiAnimator a;
    return a;
}

float UiAnimator::track (juce::Component& c, const juce::Identifier& key, float target, float speed)
{
    auto& props = c.getProperties();
    if (! props.contains (key)) { props.set (key, target); return target; }   // primeiro desenho: sem animar
    const float cur = (float) props[key];
    if (std::abs (cur - target) > 0.002f)
    {
        bool found = false;
        for (auto& it : items)
            if (it.comp.getComponent() == &c && it.key == key) { it.target = target; it.speed = speed; found = true; break; }
        if (! found) items.push_back ({ &c, key, target, speed });
        if (! isTimerRunning()) startTimerHz (60);
    }
    return cur;
}

void UiAnimator::shutdown()
{
    stopTimer();
    items.clear();
}

void UiAnimator::timerCallback()
{
    for (auto it = items.begin(); it != items.end();)
    {
        auto* c = it->comp.getComponent();
        if (c == nullptr) { it = items.erase (it); continue; }
        auto& props = c->getProperties();
        float cur = (float) props[it->key];
        cur += (it->target - cur) * it->speed;
        const bool done = std::abs (cur - it->target) < 0.004f;
        if (done) cur = it->target;
        props.set (it->key, cur);
        c->repaint();
        if (done) it = items.erase (it); else ++it;
    }
    if (items.empty()) stopTimer();
}

// ============================================================================
//  Visual
// ============================================================================
MeuPadLook::MeuPadLook()
{
    using namespace Theme;
    auto scheme = juce::LookAndFeel_V4::ColourScheme (
        bg, panel, panel2, outline, text, accent, juce::Colours::black, panel3, text);
    setColourScheme (scheme);

    setColour (juce::ResizableWindow::backgroundColourId, bg);
    setColour (juce::DocumentWindow::textColourId, text);
    setColour (juce::Label::textColourId, text);
    setColour (juce::TextButton::buttonColourId, panel3);
    setColour (juce::TextButton::buttonOnColourId, accent);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, juce::Colour (0xff121419));
    setColour (juce::ComboBox::backgroundColourId, panel2);
    setColour (juce::ComboBox::outlineColourId, outline);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::arrowColourId, textDim);
    setColour (juce::PopupMenu::backgroundColourId, panel2);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.18f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::PopupMenu::headerTextColourId, textDim);
    setColour (juce::Slider::thumbColourId, accent);
    setColour (juce::Slider::trackColourId, accent);
    setColour (juce::Slider::rotarySliderFillColourId, accent);
    setColour (juce::Slider::rotarySliderOutlineColourId, panel3);
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxBackgroundColourId, panel2);
    setColour (juce::Slider::textBoxOutlineColourId, outline);
    setColour (juce::TextEditor::backgroundColourId, bg);
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::outlineColourId, outline);
    setColour (juce::TextEditor::focusedOutlineColourId, accent);
    setColour (juce::TextEditor::highlightColourId, accent.withAlpha (0.4f));
    setColour (juce::CaretComponent::caretColourId, accent);
    setColour (juce::ListBox::backgroundColourId, panel);
    setColour (juce::ListBox::textColourId, text);
    setColour (juce::ScrollBar::thumbColourId, panel3.brighter (0.2f));
    setColour (juce::TreeView::backgroundColourId, panel);
    setColour (juce::TreeView::selectedItemBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::TreeView::linesColourId, juce::Colours::transparentBlack);
    setColour (juce::DirectoryContentsDisplayComponent::highlightColourId, accent.withAlpha (0.22f));
    setColour (juce::DirectoryContentsDisplayComponent::textColourId, text);
    setColour (juce::DirectoryContentsDisplayComponent::highlightedTextColourId, text);
    setColour (juce::FileTreeComponent::linesColourId, juce::Colours::transparentBlack);
    setColour (juce::AlertWindow::backgroundColourId, panel2);
    setColour (juce::AlertWindow::textColourId, text);
    setColour (juce::AlertWindow::outlineColourId, outline);
    setColour (juce::ProgressBar::backgroundColourId, panel3);
    setColour (juce::ProgressBar::foregroundColourId, accent);
    setColour (juce::TooltipWindow::backgroundColourId, panel3);
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::ToggleButton::textColourId, text);
    setColour (juce::ToggleButton::tickColourId, accent);
    setColour (juce::GroupComponent::outlineColourId, outline);
    setColour (juce::GroupComponent::textColourId, textDim);
}

juce::Typeface::Ptr MeuPadLook::getTypefaceForFont (const juce::Font& f)
{
    // qualquer fonte padrao (menus, janelas, campos de texto) usa as fontes embutidas
    if (f.getTypefaceName() == juce::Font::getDefaultSansSerifFontName())
        return Theme::sansTypeface (f.isBold());
    if (f.getTypefaceName() == juce::Font::getDefaultMonospacedFontName())
        return Theme::monoTypeface (f.isBold());
    // fontes embutidas procuradas pelo nome (ex.: texto ja formatado em janelas de aviso)
    for (bool b : { false, true })
    {
        if (f.getTypefaceName() == Theme::cjkTypeface (b)->getName()) return Theme::cjkTypeface (b);
        if (f.getTypefaceName() == Theme::sansTypeface (b)->getName()) return Theme::sansTypeface (b);
    }
    return LookAndFeel_V4::getTypefaceForFont (f);
}

void MeuPadLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
    const float r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float lineW = juce::jmax (2.5f, r * 0.16f);
    const float arcR = r - lineW * 0.5f;
    const float angle = start + pos * (end - start);
    auto fill = s.findColour (juce::Slider::rotarySliderFillColourId);
    if (! s.isEnabled()) fill = fill.withSaturation (0.0f).withAlpha (0.5f);
    const float hv = UiAnimator::get().track (s, "hv", (s.isMouseOverOrDragging() && s.isEnabled()) ? 1.0f : 0.0f);

    juce::Path bgArc;
    bgArc.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, start, end, true);
    g.setColour (Theme::panel3.brighter (0.05f + 0.1f * hv));
    g.strokePath (bgArc, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const bool bipolar = s.getMinimum() < 0 && s.getMaximum() > 0;
    float from = start;
    if (bipolar) from = start + (float) ((0.0 - s.getMinimum()) / (s.getMaximum() - s.getMinimum())) * (end - start);
    juce::Path valArc;
    valArc.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
    if (hv > 0.01f)
    {
        g.setColour (fill.withAlpha (0.18f * hv));
        g.strokePath (valArc, juce::PathStrokeType (lineW + 4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.setColour (fill.brighter (0.15f * hv));
    g.strokePath (valArc, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float knobR = arcR - lineW * 1.15f;
    g.setGradientFill (juce::ColourGradient (Theme::panel3.brighter (0.3f), c.x, c.y - knobR,
                                             Theme::panel2.darker (0.1f), c.x, c.y + knobR, false));
    g.fillEllipse (c.x - knobR, c.y - knobR, knobR * 2, knobR * 2);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawEllipse (c.x - knobR, c.y - knobR, knobR * 2, knobR * 2, 1.0f);

    juce::Path tick;
    tick.addRoundedRectangle (-1.3f, -knobR + 2.5f, 2.6f, knobR * 0.5f, 1.3f);
    g.setColour (Theme::text);
    g.fillPath (tick, juce::AffineTransform::rotation (angle).translated (c.x, c.y));
}

void MeuPadLook::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                   juce::Slider::SliderStyle style, juce::Slider& s)
{
    auto fill = s.findColour (juce::Slider::trackColourId);
    const float hv = UiAnimator::get().track (s, "hv", s.isMouseOverOrDragging() ? 1.0f : 0.0f);
    if (style == juce::Slider::LinearVertical)
    {
        const float cx = x + w * 0.5f;
        auto track = juce::Rectangle<float> (cx - 2.5f, (float) y, 5.0f, (float) h);
        g.setColour (Theme::bg);
        g.fillRoundedRectangle (track, 2.5f);
        g.setGradientFill (juce::ColourGradient (fill.brighter (0.2f), cx, pos, fill.darker (0.3f), cx, (float) (y + h), false));
        g.fillRoundedRectangle (track.withTop (pos), 2.5f);
        g.setColour (Theme::textFaint.withAlpha (0.6f));
        for (int i = 0; i <= 10; ++i)
        {
            const float yy = (float) y + h * i / 10.0f;
            g.drawHorizontalLine ((int) yy, cx - 11, cx - 7);
            g.drawHorizontalLine ((int) yy, cx + 7, cx + 11);
        }
        auto thumb = juce::Rectangle<float> (cx - 14, pos - 8, 28, 16);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (thumb.translated (0, 1.5f), 4.0f);
        g.setGradientFill (juce::ColourGradient (Theme::panel3.brighter (0.55f + 0.15f * hv), thumb.getX(), thumb.getY(),
                                                 Theme::panel3.brighter (0.05f), thumb.getX(), thumb.getBottom(), false));
        g.fillRoundedRectangle (thumb, 4.0f);
        g.setColour (fill.interpolatedWith (Theme::text, 0.3f));
        g.fillRoundedRectangle (thumb.getX() + 5, pos - 1.0f, thumb.getWidth() - 10, 2.0f, 1.0f);
        return;
    }
    if (style == juce::Slider::LinearHorizontal || style == juce::Slider::LinearBar)
    {
        const float cy = y + h * 0.5f;
        auto track = juce::Rectangle<float> ((float) x, cy - 2.0f, (float) w, 4.0f);
        g.setColour (Theme::bg);
        g.fillRoundedRectangle (track, 2.0f);
        g.setColour (fill);
        g.fillRoundedRectangle (track.withRight (pos), 2.0f);
        const float tr = 6.0f + 1.5f * hv;
        g.setColour (fill.withAlpha (0.25f * hv));
        g.fillEllipse (pos - tr - 3, cy - tr - 3, (tr + 3) * 2, (tr + 3) * 2);
        g.setColour (Theme::text);
        g.fillEllipse (pos - tr, cy - tr, tr * 2, tr * 2);
        return;
    }
    LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, 0, 0, style, s);
}

void MeuPadLook::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bgCol, bool over, bool down)
{
    auto& anim = UiAnimator::get();
    const bool on = b.getToggleState();
    const float hv = anim.track (b, "hv", (over && b.isEnabled()) ? 1.0f : 0.0f);
    const float pv = anim.track (b, "pv", down ? 1.0f : 0.0f, 0.45f);
    const float ov = anim.track (b, "ov", on ? 1.0f : 0.0f, 0.25f);

    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const float radius = juce::jmin (6.0f, r.getHeight() * 0.3f);
    const bool flat = bgCol.getAlpha() == 0;            // botao "plano" (menus): so aparece no hover
    auto offCol = flat ? Theme::panel3.withAlpha (hv) : bgCol.interpolatedWith (bgCol.brighter (0.12f), hv);
    auto base = offCol.interpolatedWith (b.findColour (juce::TextButton::buttonOnColourId), ov);
    base = base.brighter (0.14f * pv);
    if (! b.isEnabled()) base = base.withMultipliedAlpha (0.45f);
    g.setColour (base);
    g.fillRoundedRectangle (r, radius);
    if (! flat)
    {
        g.setColour (Theme::outline.brighter (0.25f * hv).withMultipliedAlpha (1.0f - 0.8f * ov));
        g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.03f + 0.07f * ov));
        g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + radius, r.getRight() - radius);
    }
}

void MeuPadLook::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    auto area = b.getLocalBounds().reduced (6, 1);
    g.setFont (Theme::fitFont (Theme::withCjkFallback (getTextButtonFont (b, b.getHeight()), b.getButtonText()), b.getButtonText(), (float) area.getWidth()));
    const float ov = b.getProperties().contains ("ov") ? (float) b.getProperties()["ov"] : (b.getToggleState() ? 1.0f : 0.0f);
    auto col = b.findColour (juce::TextButton::textColourOffId).interpolatedWith (b.findColour (juce::TextButton::textColourOnId), ov);
    if (! b.isEnabled()) col = col.withAlpha (0.4f);
    g.setColour (col);
    g.drawFittedText (b.getButtonText(), area, juce::Justification::centred, 1, 1.0f);
}

void MeuPadLook::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    const float ov = UiAnimator::get().track (b, "ov", b.getToggleState() ? 1.0f : 0.0f, 0.3f);
    const float hv = UiAnimator::get().track (b, "hv", over ? 1.0f : 0.0f);
    auto r = b.getLocalBounds().toFloat();
    auto box = r.removeFromLeft (juce::jmin (r.getHeight(), 22.0f)).withSizeKeepingCentre (16.0f, 16.0f);
    g.setColour (Theme::panel3.interpolatedWith (Theme::accent, ov));
    g.fillRoundedRectangle (box, 4.0f);
    g.setColour (Theme::outline.brighter (0.3f * hv).withMultipliedAlpha (1.0f - ov));
    g.drawRoundedRectangle (box.reduced (0.5f), 4.0f, 1.0f);
    if (ov > 0.02f)
        Icons::draw (g, Icon::Check, box.reduced (1.5f), juce::Colour (0xff121419).withAlpha (ov));
    g.setColour (b.isEnabled() ? Theme::text : Theme::textFaint);
    g.setFont (Theme::font (12.5f));
    g.drawFittedText (b.getButtonText(), r.toNearestInt().withTrimmedLeft (6), juce::Justification::centredLeft, 1, 1.0f);
}

void MeuPadLook::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    const float hv = UiAnimator::get().track (box, "hv", box.isMouseOver (true) ? 1.0f : 0.0f);
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId).interpolatedWith (Theme::panel3, hv * 0.6f));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (box.hasKeyboardFocus (true) || box.isPopupActive() ? Theme::accent : Theme::outline.brighter (0.2f * hv));
    g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
    Icons::draw (g, Icon::ChevronDown, { w - 22.0f, h * 0.5f - 7.0f, 14.0f, 14.0f }, Theme::textDim.interpolatedWith (Theme::text, hv));
}

void MeuPadLook::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (4, 1, box.getWidth() - 28, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

void MeuPadLook::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (Theme::panel2);
    g.setColour (Theme::outline.brighter (0.15f));
    g.drawRect (0, 0, w, h);
}

void MeuPadLook::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive, bool isHighlighted,
                                    bool isTicked, bool hasSubMenu, const juce::String& text, const juce::String& shortcutKeyText,
                                    const juce::Drawable*, const juce::Colour* textColour)
{
    if (isSeparator)
    {
        g.setColour (Theme::outline);
        g.fillRect (area.reduced (10, 0).withSizeKeepingCentre (area.getWidth() - 20, 1));
        return;
    }
    auto r = area.toFloat().reduced (4.0f, 1.0f);
    if (isHighlighted && isActive)
    {
        g.setColour (Theme::accent.withAlpha (0.2f));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (Theme::accent);
        g.fillRoundedRectangle (r.withWidth (3.0f).reduced (0.0f, 5.0f), 1.5f);
    }
    // "Texto  (Ctrl+X)" -> atalho alinhado a direita, mais discreto
    juce::String label = text, shortcut = shortcutKeyText;
    if (shortcut.isEmpty() && label.endsWithChar (')'))
    {
        const int i = label.lastIndexOf ("  (");
        const auto sc = label.substring (i + 3, label.length() - 1);
        if (i > 0 && Theme::looksLikeShortcut (sc)) { shortcut = sc; label = label.substring (0, i); }
    }
    auto textCol = textColour != nullptr ? *textColour : Theme::text;
    if (! isActive) textCol = Theme::textFaint;
    auto inner = r.reduced (12.0f, 0.0f);
    auto tickArea = inner.removeFromLeft (18.0f);
    if (isTicked)
        Icons::draw (g, Icon::Check, tickArea.withSizeKeepingCentre (15.0f, 15.0f), Theme::accent);
    inner.removeFromLeft (4.0f);
    if (hasSubMenu)
        Icons::draw (g, Icon::ChevronRight, inner.removeFromRight (14.0f).withSizeKeepingCentre (13.0f, 13.0f), Theme::textDim);
    g.setFont (getPopupMenuFont());
    if (shortcut.isNotEmpty())
    {
        g.setColour (isHighlighted && isActive ? Theme::textDim : Theme::textFaint);
        g.setFont (Theme::font (11.0f));
        const float sw = g.getCurrentFont().getStringWidthFloat (shortcut) + 4.0f;
        g.drawText (shortcut, inner.removeFromRight (sw), juce::Justification::centredRight, false);
        inner.removeFromRight (16.0f);
        g.setFont (getPopupMenuFont());
    }
    g.setColour (isHighlighted && isActive ? juce::Colours::white : textCol);
    g.setFont (Theme::fontFor (label, 13.0f));
    g.drawFittedText (label, inner.toNearestInt(), juce::Justification::centredLeft, 1, 1.0f);
}

void MeuPadLook::getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator, int, int& idealWidth, int& idealHeight)
{
    if (isSeparator) { idealWidth = 50; idealHeight = 9; return; }
    auto f = getPopupMenuFont();
    idealHeight = juce::roundToInt (f.getHeight() * 1.75f);
    juce::String label = text, shortcut;
    const int i = label.lastIndexOf ("  (");
    if (label.endsWithChar (')') && i > 0 && Theme::looksLikeShortcut (label.substring (i + 3, label.length() - 1)))
    { shortcut = label.substring (i + 3, label.length() - 1); label = label.substring (0, i); }
    idealWidth = f.getStringWidth (label) + 80 + (shortcut.isNotEmpty() ? Theme::font (11.0f).getStringWidth (shortcut) + 24 : 0);
}

void MeuPadLook::drawScrollbar (juce::Graphics& g, juce::ScrollBar& sb, int x, int y, int w, int h, bool vertical,
                                int thumbStart, int thumbSize, bool over, bool down)
{
    const float hv = UiAnimator::get().track (sb, "hv", (over || down) ? 1.0f : 0.0f);
    g.setColour (Theme::panel);
    g.fillRect (x, y, w, h);
    const float thick = 4.0f + 3.0f * hv;
    auto thumb = vertical ? juce::Rectangle<float> ((float) x + (w - thick) * 0.5f, (float) thumbStart + 2, thick, (float) thumbSize - 4)
                          : juce::Rectangle<float> ((float) thumbStart + 2, (float) y + (h - thick) * 0.5f, (float) thumbSize - 4, thick);
    g.setColour (Theme::panel3.brighter (0.25f + 0.25f * hv + (down ? 0.15f : 0.0f)));
    g.fillRoundedRectangle (thumb, thick * 0.5f);
}

void MeuPadLook::drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h)
{
    g.fillAll (Theme::panel3);
    g.setColour (Theme::outline.brighter (0.35f));
    g.drawRect (0, 0, w, h);
    g.setColour (Theme::text);
    g.setFont (Theme::font (12.0f));
    g.drawFittedText (text, 10, 4, w - 20, h - 8, juce::Justification::centredLeft, 6, 1.0f);
}

juce::Rectangle<int> MeuPadLook::getTooltipBounds (const juce::String& text, juce::Point<int> pos, juce::Rectangle<int> parentArea)
{
    const auto f = Theme::font (12.0f);
    const int maxW = 460;
    juce::GlyphArrangement ga;
    ga.addJustifiedText (f, text, 0, 0, (float) maxW, juce::Justification::left);
    auto bb = ga.getBoundingBox (0, -1, true);
    const int widest = (int) std::ceil (bb.getWidth());
    const int lines = juce::jmax (1, (int) std::ceil (bb.getHeight() / f.getHeight()));
    const int w = juce::jmin (maxW, widest) + 22;
    const int h = (int) std::ceil (lines * f.getHeight()) + 12;
    return juce::Rectangle<int> (pos.x > parentArea.getCentreX() ? pos.x - (w + 12) : pos.x + 24,
                                 pos.y > parentArea.getCentreY() ? pos.y - (h + 6) : pos.y + 6, w, h)
        .constrainedWithin (parentArea);
}

void MeuPadLook::fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor& e)
{
    g.setColour (e.findColour (juce::TextEditor::backgroundColourId));
    g.fillRoundedRectangle (0, 0, (float) w, (float) h, 5.0f);
}

void MeuPadLook::drawTextEditorOutline (juce::Graphics& g, int w, int h, juce::TextEditor& e)
{
    g.setColour (e.hasKeyboardFocus (true) ? Theme::accent : Theme::outline.brighter (0.15f));
    g.drawRoundedRectangle (0.5f, 0.5f, w - 1.0f, h - 1.0f, 5.0f, 1.0f);
}

void MeuPadLook::drawProgressBar (juce::Graphics& g, juce::ProgressBar&, int w, int h, double progress, const juce::String& text)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
    g.setColour (Theme::panel3);
    g.fillRoundedRectangle (r, h * 0.5f);
    if (progress >= 0 && progress <= 1.0)
    {
        g.setGradientFill (juce::ColourGradient (Theme::accent, 0, 0, Theme::accent.brighter (0.4f), (float) w, 0, false));
        g.fillRoundedRectangle (r.withWidth ((float) (w * progress)), h * 0.5f);
    }
    else
    {
        const float t = (float) (juce::Time::getMillisecondCounter() % 1200) / 1200.0f;
        g.setColour (Theme::accent.withAlpha (0.7f));
        g.fillRoundedRectangle (r.withX (t * w - w * 0.2f).withWidth (w * 0.3f).getIntersection (r), h * 0.5f);
    }
    g.setColour (Theme::text);
    g.setFont (Theme::font (12.0f, true));
    g.drawText (text, r, juce::Justification::centred);
}

void MeuPadLook::drawLabel (juce::Graphics& g, juce::Label& label)
{
    g.fillAll (label.findColour (juce::Label::backgroundColourId));
    if (! label.isBeingEdited())
    {
        const float alpha = label.isEnabled() ? 1.0f : 0.5f;
        auto area = getLabelBorderSize (label).subtractedFrom (label.getLocalBounds());
        g.setColour (label.findColour (juce::Label::textColourId).withMultipliedAlpha (alpha));
        g.setFont (Theme::fitFont (Theme::withCjkFallback (getLabelFont (label), label.getText()), label.getText(), (float) area.getWidth()));
        g.drawFittedText (label.getText(), area, label.getJustificationType(),
                          juce::jmax (1, (int) ((float) area.getHeight() / g.getCurrentFont().getHeight())), 1.0f);
        g.setColour (label.findColour (juce::Label::outlineColourId).withMultipliedAlpha (alpha));
    }
    else if (label.isEnabled())
        g.setColour (label.findColour (juce::Label::outlineColourId));
    g.drawRect (label.getLocalBounds());
}

void MeuPadLook::drawFileBrowserRow (juce::Graphics& g, int w, int h, const juce::File& file, const juce::String& filename, juce::Image*,
                                     const juce::String&, const juce::String&, bool isDirectory, bool isSelected, int, juce::DirectoryContentsDisplayComponent&)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (2.0f, 1.0f);
    if (isSelected)
    {
        g.setColour (Theme::accent.withAlpha (0.2f));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (Theme::accent);
        g.fillRoundedRectangle (r.withWidth (3.0f).reduced (0.0f, 4.0f), 1.5f);
    }
    const auto ext = file.getFileExtension().toLowerCase();
    const bool video = ext == ".mp4" || ext == ".webm" || ext == ".mkv" || ext == ".mov" || ext == ".avi";
    const Icon icon = isDirectory ? Icon::Folder : video ? Icon::FileVideo : Icon::FileAudio;
    const auto iconCol = isDirectory ? Theme::warn.withSaturation (0.55f) : video ? juce::Colour (0xffb39dfa) : Theme::accent2;
    const float iconS = juce::jmin (18.0f, h - 6.0f);
    Icons::draw (g, icon, { 8.0f, (h - iconS) * 0.5f, iconS, iconS }, iconCol);
    const float x = 8.0f + iconS + 8.0f;
    g.setColour (isSelected ? juce::Colours::white : Theme::text);
    g.setFont (Theme::fontFor (filename, 12.0f, isDirectory));
    // nome inteiro quando cabe; senao corta com "..." (sem espremer as letras)
    g.drawText (filename, juce::Rectangle<float> (x, 0.0f, w - x - 6.0f, (float) h), juce::Justification::centredLeft, true);
}

void MeuPadLook::drawTreeviewPlusMinusBox (juce::Graphics& g, const juce::Rectangle<float>& area, juce::Colour, bool isOpen, bool isMouseOver)
{
    Icons::draw (g, isOpen ? Icon::ChevronDown : Icon::ChevronRight, area.withSizeKeepingCentre (14.0f, 14.0f),
                 isMouseOver ? Theme::text : Theme::textDim);
}

void MeuPadLook::drawAlertBox (juce::Graphics& g, juce::AlertWindow& alert, const juce::Rectangle<int>& textArea, juce::TextLayout& textLayout)
{
    auto r = alert.getLocalBounds().toFloat();
    g.fillAll (Theme::panel2);
    g.setColour (Theme::accent);
    g.fillRect (r.withHeight (3.0f));
    g.setColour (Theme::outline.brighter (0.2f));
    g.drawRect (r, 1.0f);
    textLayout.draw (g, textArea.toFloat().withTrimmedTop (6.0f));
}

} // namespace mp
