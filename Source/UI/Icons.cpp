#include "Icons.h"
#include "Theme.h"

namespace mp
{
namespace
{
    constexpr float kStroke = 1.9f;
    constexpr float pi = juce::MathConstants<float>::pi;

    struct Builder
    {
        juce::Path out;
        void stroke (const juce::Path& p, float w = kStroke)
        {
            juce::Path s;
            juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded).createStrokedPath (s, p);
            out.addPath (s);
        }
        void fill (const juce::Path& p) { out.addPath (p); }
        void line (float x1, float y1, float x2, float y2) { juce::Path p; p.startNewSubPath (x1, y1); p.lineTo (x2, y2); stroke (p); }
        void poly (std::initializer_list<juce::Point<float>> pts, bool closed = false)
        {
            juce::Path p;
            bool first = true;
            for (auto& pt : pts) { if (first) p.startNewSubPath (pt); else p.lineTo (pt); first = false; }
            if (closed) p.closeSubPath();
            stroke (p);
        }
        void rrect (float x, float y, float w, float h, float r, bool filled = false)
        {
            juce::Path p; p.addRoundedRectangle (x, y, w, h, r);
            if (filled) fill (p); else stroke (p);
        }
        void circle (float cx, float cy, float r, bool filled = false)
        {
            juce::Path p; p.addEllipse (cx - r, cy - r, r * 2, r * 2);
            if (filled) fill (p); else stroke (p);
        }
        void arc (float cx, float cy, float r, float a0, float a1)
        {
            juce::Path p; p.addCentredArc (cx, cy, r, r, 0.0f, a0, a1, true);
            stroke (p);
        }
        void filledRound (std::initializer_list<juce::Point<float>> pts, float roundness = 1.2f)
        {
            juce::Path p;
            bool first = true;
            for (auto& pt : pts) { if (first) p.startNewSubPath (pt); else p.lineTo (pt); first = false; }
            p.closeSubPath();
            fill (p);
            stroke (p, roundness * 2.0f);   // cantos arredondados
        }
    };

    juce::Path build (Icon icon)
    {
        Builder b;
        switch (icon)
        {
            case Icon::Play:    b.filledRound ({ { 7.5f, 4.5f }, { 7.5f, 19.5f }, { 19.5f, 12.0f } }); break;
            case Icon::Pause:   b.rrect (6.5f, 5.0f, 4.0f, 14.0f, 1.3f, true); b.rrect (13.5f, 5.0f, 4.0f, 14.0f, 1.3f, true); break;
            case Icon::Stop:    b.rrect (6.0f, 6.0f, 12.0f, 12.0f, 2.2f, true); break;
            case Icon::Record:  b.circle (12.0f, 12.0f, 6.5f, true); break;
            case Icon::Mic:
                b.rrect (9.0f, 3.0f, 6.0f, 11.0f, 3.0f);
                b.arc (12.0f, 10.5f, 6.5f, pi * 0.5f, pi * 1.5f);
                b.line (12.0f, 17.0f, 12.0f, 20.5f);
                b.line (8.5f, 20.5f, 15.5f, 20.5f);
                break;
            case Icon::Loop:
            {
                juce::Path p;
                p.startNewSubPath (17.0f, 7.0f); p.lineTo (8.0f, 7.0f); p.quadraticTo (4.0f, 7.0f, 4.0f, 11.0f); p.lineTo (4.0f, 12.0f);
                b.stroke (p);
                b.poly ({ { 14.5f, 4.0f }, { 17.5f, 7.0f }, { 14.5f, 10.0f } });
                juce::Path q;
                q.startNewSubPath (7.0f, 17.0f); q.lineTo (16.0f, 17.0f); q.quadraticTo (20.0f, 17.0f, 20.0f, 13.0f); q.lineTo (20.0f, 12.0f);
                b.stroke (q);
                b.poly ({ { 9.5f, 14.0f }, { 6.5f, 17.0f }, { 9.5f, 20.0f } });
                break;
            }
            case Icon::Metronome:
                b.poly ({ { 9.0f, 3.5f }, { 15.0f, 3.5f }, { 19.0f, 20.5f }, { 5.0f, 20.5f } }, true);
                b.line (12.0f, 16.0f, 17.5f, 6.0f);
                b.line (7.0f, 16.0f, 17.0f, 16.0f);
                break;
            case Icon::Undo:
            case Icon::Redo:
            {
                const bool undo = icon == Icon::Undo;
                auto X = [undo] (float x) { return undo ? x : 24.0f - x; };
                juce::Path p;
                p.startNewSubPath (X (4.5f), 9.0f); p.lineTo (X (14.5f), 9.0f);
                p.cubicTo (X (17.8f), 9.0f, X (20.0f), 11.5f, X (20.0f), 14.5f);
                p.cubicTo (X (20.0f), 17.5f, X (17.8f), 20.0f, X (14.5f), 20.0f);
                p.lineTo (X (10.0f), 20.0f);
                b.stroke (p);
                b.poly ({ { X (9.0f), 4.5f }, { X (4.5f), 9.0f }, { X (9.0f), 13.5f } });
                break;
            }
            case Icon::Sliders:
            case Icon::Gear:
                b.line (4.0f, 6.5f, 12.6f, 6.5f);  b.circle (15.0f, 6.5f, 2.4f);  b.line (17.4f, 6.5f, 20.0f, 6.5f);
                b.line (4.0f, 12.0f, 6.6f, 12.0f); b.circle (9.0f, 12.0f, 2.4f);  b.line (11.4f, 12.0f, 20.0f, 12.0f);
                b.line (4.0f, 17.5f, 10.6f, 17.5f); b.circle (13.0f, 17.5f, 2.4f); b.line (15.4f, 17.5f, 20.0f, 17.5f);
                break;
            case Icon::Folder:
            {
                juce::Path p;
                p.startNewSubPath (3.5f, 8.0f);
                p.quadraticTo (3.5f, 5.5f, 6.0f, 5.5f); p.lineTo (9.5f, 5.5f); p.lineTo (11.5f, 7.5f); p.lineTo (18.0f, 7.5f);
                p.quadraticTo (20.5f, 7.5f, 20.5f, 10.0f); p.lineTo (20.5f, 16.5f);
                p.quadraticTo (20.5f, 19.0f, 18.0f, 19.0f); p.lineTo (6.0f, 19.0f);
                p.quadraticTo (3.5f, 19.0f, 3.5f, 16.5f); p.closeSubPath();
                b.stroke (p);
                b.line (3.5f, 10.5f, 20.5f, 10.5f);
                break;
            }
            case Icon::FileAudio:
            case Icon::FileVideo:
            {
                juce::Path p;
                p.startNewSubPath (13.5f, 3.0f); p.lineTo (7.0f, 3.0f); p.quadraticTo (5.0f, 3.0f, 5.0f, 5.0f);
                p.lineTo (5.0f, 19.0f); p.quadraticTo (5.0f, 21.0f, 7.0f, 21.0f); p.lineTo (17.0f, 21.0f);
                p.quadraticTo (19.0f, 21.0f, 19.0f, 19.0f); p.lineTo (19.0f, 8.5f); p.closeSubPath();
                b.stroke (p);
                b.poly ({ { 13.5f, 3.0f }, { 13.5f, 8.5f }, { 19.0f, 8.5f } });
                if (icon == Icon::FileAudio)
                {
                    b.circle (10.3f, 16.6f, 1.8f, true);
                    b.poly ({ { 12.0f, 16.6f }, { 12.0f, 11.0f }, { 15.0f, 12.2f } });
                }
                else b.filledRound ({ { 10.0f, 11.5f }, { 10.0f, 17.5f }, { 15.0f, 14.5f } }, 0.6f);
                break;
            }
            case Icon::ArrowUp:
                b.line (12.0f, 19.0f, 12.0f, 5.5f);
                b.poly ({ { 6.5f, 11.0f }, { 12.0f, 5.5f }, { 17.5f, 11.0f } });
                break;
            case Icon::Music:
                b.poly ({ { 9.0f, 17.5f }, { 9.0f, 5.5f }, { 19.0f, 3.5f }, { 19.0f, 15.5f } });
                b.circle (6.5f, 17.5f, 2.5f);
                b.circle (16.5f, 15.5f, 2.5f);
                break;
            case Icon::Download:
                b.line (12.0f, 4.0f, 12.0f, 14.5f);
                b.poly ({ { 7.5f, 10.0f }, { 12.0f, 14.5f }, { 16.5f, 10.0f } });
                b.poly ({ { 4.5f, 15.5f }, { 4.5f, 19.5f }, { 19.5f, 19.5f }, { 19.5f, 15.5f } });
                break;
            case Icon::Desktop:
                b.rrect (3.5f, 4.0f, 17.0f, 12.0f, 2.0f);
                b.line (12.0f, 16.0f, 12.0f, 19.5f);
                b.line (8.0f, 19.5f, 16.0f, 19.5f);
                break;
            case Icon::Plus:
                b.line (12.0f, 5.0f, 12.0f, 19.0f);
                b.line (5.0f, 12.0f, 19.0f, 12.0f);
                break;
            case Icon::Arrange:
                b.rrect (3.5f, 4.5f, 10.0f, 4.0f, 1.6f);
                b.rrect (8.5f, 10.0f, 12.0f, 4.0f, 1.6f);
                b.rrect (5.0f, 15.5f, 9.0f, 4.0f, 1.6f);
                break;
            case Icon::Waveform:
            {
                const float xs[] = { 4.0f, 7.2f, 10.4f, 13.6f, 16.8f, 20.0f };
                const float hs[] = { 4.0f, 10.0f, 16.0f, 8.0f, 13.0f, 5.0f };
                for (int i = 0; i < 6; ++i) b.line (xs[i], 12.0f - hs[i] * 0.5f, xs[i], 12.0f + hs[i] * 0.5f);
                break;
            }
            case Icon::Grid:
                for (int r = 0; r < 3; ++r)
                    for (int c = 0; c < 3; ++c)
                        b.rrect (3.5f + c * 6.0f, 3.5f + r * 6.0f, 5.0f, 5.0f, 1.2f, (r + c) % 2 == 0);
                break;
            case Icon::Mixer:
                b.line (6.0f, 4.0f, 6.0f, 20.0f); b.line (12.0f, 4.0f, 12.0f, 20.0f); b.line (18.0f, 4.0f, 18.0f, 20.0f);
                b.rrect (3.5f, 12.5f, 5.0f, 3.0f, 1.0f, true);
                b.rrect (9.5f, 6.5f, 5.0f, 3.0f, 1.0f, true);
                b.rrect (15.5f, 10.0f, 5.0f, 3.0f, 1.0f, true);
                break;
            case Icon::Headphones:
                b.arc (12.0f, 13.0f, 8.0f, pi * 1.5f, pi * 2.5f);
                b.rrect (3.5f, 13.0f, 4.0f, 7.0f, 1.6f);
                b.rrect (16.5f, 13.0f, 4.0f, 7.0f, 1.6f);
                break;
            case Icon::Scissors:
                b.circle (6.5f, 6.5f, 2.6f); b.circle (6.5f, 17.5f, 2.6f);
                b.line (8.7f, 8.0f, 20.0f, 17.5f);
                b.line (8.7f, 16.0f, 20.0f, 6.5f);
                break;
            case Icon::Magnet:
            {
                juce::Path p;
                p.startNewSubPath (6.0f, 4.0f); p.lineTo (6.0f, 12.0f);
                p.cubicTo (6.0f, 15.5f, 8.7f, 18.5f, 12.0f, 18.5f);
                p.cubicTo (15.3f, 18.5f, 18.0f, 15.5f, 18.0f, 12.0f);
                p.lineTo (18.0f, 4.0f);
                b.stroke (p);
                b.line (4.0f, 8.0f, 8.0f, 8.0f); b.line (16.0f, 8.0f, 20.0f, 8.0f);
                break;
            }
            case Icon::Fit:
                b.line (4.0f, 12.0f, 20.0f, 12.0f);
                b.poly ({ { 7.5f, 8.5f }, { 4.0f, 12.0f }, { 7.5f, 15.5f } });
                b.poly ({ { 16.5f, 8.5f }, { 20.0f, 12.0f }, { 16.5f, 15.5f } });
                b.line (2.0f, 5.0f, 2.0f, 19.0f); b.line (22.0f, 5.0f, 22.0f, 19.0f);
                break;
            case Icon::Follow:
                b.line (9.0f, 5.0f, 9.0f, 20.5f);
                b.filledRound ({ { 5.5f, 3.5f }, { 12.5f, 3.5f }, { 9.0f, 7.5f } }, 0.6f);
                b.line (12.5f, 14.0f, 20.0f, 14.0f);
                b.poly ({ { 17.0f, 11.0f }, { 20.0f, 14.0f }, { 17.0f, 17.0f } });
                break;
            case Icon::Pads:
                b.rrect (3.5f, 3.5f, 7.5f, 7.5f, 1.8f, true);
                b.rrect (13.0f, 3.5f, 7.5f, 7.5f, 1.8f);
                b.rrect (3.5f, 13.0f, 7.5f, 7.5f, 1.8f);
                b.rrect (13.0f, 13.0f, 7.5f, 7.5f, 1.8f);
                break;
            case Icon::Tap:
                b.circle (12.0f, 12.0f, 2.6f, true);
                b.circle (12.0f, 12.0f, 6.0f);
                b.arc (12.0f, 12.0f, 9.5f, -0.9f, 0.9f);
                b.arc (12.0f, 12.0f, 9.5f, pi - 0.9f, pi + 0.9f);
                break;
            case Icon::Trash:
                b.line (4.5f, 7.0f, 19.5f, 7.0f);
                b.poly ({ { 9.5f, 7.0f }, { 9.5f, 4.5f }, { 14.5f, 4.5f }, { 14.5f, 7.0f } });
                b.poly ({ { 6.5f, 7.0f }, { 7.5f, 20.0f }, { 16.5f, 20.0f }, { 17.5f, 7.0f } });
                b.line (10.0f, 11.0f, 10.0f, 16.5f); b.line (14.0f, 11.0f, 14.0f, 16.5f);
                break;
            case Icon::ChevronRight: b.poly ({ { 9.5f, 6.0f }, { 15.5f, 12.0f }, { 9.5f, 18.0f } }); break;
            case Icon::ChevronDown:  b.poly ({ { 6.0f, 9.5f }, { 12.0f, 15.5f }, { 18.0f, 9.5f } }); break;
            case Icon::Check:        b.poly ({ { 5.0f, 12.5f }, { 10.0f, 17.5f }, { 19.0f, 7.0f } }); break;
            case Icon::Piano:
                b.rrect (3.5f, 5.0f, 17.0f, 14.0f, 2.0f);
                b.line (9.0f, 13.0f, 9.0f, 19.0f); b.line (15.0f, 13.0f, 15.0f, 19.0f);
                b.rrect (7.6f, 5.0f, 2.8f, 8.0f, 0.6f, true);
                b.rrect (13.6f, 5.0f, 2.8f, 8.0f, 0.6f, true);
                break;
            case Icon::Bolt:
                b.filledRound ({ { 13.0f, 3.0f }, { 5.5f, 13.5f }, { 11.5f, 13.5f }, { 10.5f, 21.0f }, { 18.5f, 10.0f }, { 12.5f, 10.0f } }, 0.7f);
                break;
        }
        return b.out;
    }
}

const juce::Path& Icons::path (Icon icon)
{
    static std::map<int, juce::Path> cache;
    auto it = cache.find ((int) icon);
    if (it == cache.end()) it = cache.emplace ((int) icon, build (icon)).first;
    return it->second;
}

void Icons::draw (juce::Graphics& g, Icon icon, juce::Rectangle<float> r, juce::Colour colour)
{
    const float s = juce::jmin (r.getWidth(), r.getHeight());
    auto box = r.withSizeKeepingCentre (s, s);
    g.setColour (colour);
    g.fillPath (path (icon), juce::AffineTransform::scale (s / 24.0f).translated (box.getX(), box.getY()));
}

// ----------------------------------------------------------------------------
void IconButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto& anim = UiAnimator::get();
    const bool on = getToggleState();
    const float hv = anim.track (*this, "hv", (over && isEnabled()) ? 1.0f : 0.0f);
    const float pv = anim.track (*this, "pv", down ? 1.0f : 0.0f, 0.45f);
    const float ov = anim.track (*this, "ov", on ? 1.0f : 0.0f, 0.25f);

    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const float radius = juce::jmin (8.0f, r.getHeight() * 0.28f);
    auto offBg = flat ? Theme::panel3.withAlpha (0.0f) : Theme::panel3;
    auto bg = offBg.interpolatedWith (Theme::panel3, flat ? hv * 0.9f : 0.0f);
    if (! tintIconOnly) bg = bg.interpolatedWith (onColour, ov * 0.92f);
    bg = bg.brighter (0.08f * hv + 0.14f * pv);
    if (! isEnabled()) bg = bg.withMultipliedAlpha (0.5f);
    g.setColour (bg);
    g.fillRoundedRectangle (r, radius);
    if (! flat || hv > 0.01f)
    {
        g.setColour (Theme::outline.brighter (0.15f * hv).withMultipliedAlpha (flat ? hv : 1.0f - ov * 0.7f));
        g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
    }
    // brilho suave quando ligado
    if (ov > 0.01f && ! tintIconOnly)
    {
        g.setColour (juce::Colours::white.withAlpha (0.10f * ov));
        g.fillRoundedRectangle (r.reduced (2.0f).withHeight (r.getHeight() * 0.42f), radius - 1.0f);
    }

    juce::Colour fg = iconColour.isTransparent() ? Theme::text : iconColour;
    if (icon == Icon::Record) fg = Theme::rec;
    if (icon == Icon::Mic) fg = Theme::rec.brighter (0.25f);
    const auto onFg = tintIconOnly ? onColour : juce::Colour (0xff121419);
    fg = fg.interpolatedWith (onFg, ov);
    if (icon == Icon::Record && ! tintIconOnly) fg = fg.interpolatedWith (juce::Colours::white, ov);
    if (! isEnabled()) fg = Theme::textFaint.withAlpha (0.6f);
    if (on && icon == Icon::Mic)   // pisca devagar enquanto grava
        fg = fg.withAlpha (0.55f + 0.45f * (0.5f + 0.5f * std::sin ((float) juce::Time::getMillisecondCounter() * 0.006f)));

    const float s = juce::jmin (r.getWidth(), r.getHeight()) * iconScale;
    const Icon ic = (useOnIcon && on) ? onIcon : icon;
    Icons::draw (g, ic, juce::Rectangle<float> (s, s).withCentre (r.getCentre().translated (0.0f, pv * 0.6f)), fg);
}

void IconTextButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto& anim = UiAnimator::get();
    const float hv = anim.track (*this, "hv", over ? 1.0f : 0.0f);
    const float pv = anim.track (*this, "pv", down ? 1.0f : 0.0f, 0.45f);
    const float ov = anim.track (*this, "ov", getToggleState() ? 1.0f : 0.0f, 0.25f);
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    auto bg = Theme::panel2.interpolatedWith (Theme::panel3, hv).interpolatedWith (onColour.withAlpha (0.22f), ov).brighter (0.12f * pv);
    g.setColour (bg);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (Theme::outline.brighter (0.2f * hv).interpolatedWith (onColour, ov * 0.8f));
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);

    auto f = Theme::font (12.0f, true);
    const float iconS = juce::jmin (16.0f, r.getHeight() * 0.56f);
    if (r.getWidth() < iconS + 6.0f + 16.0f + Theme::font (12.0f, true).getStringWidthFloat (getButtonText()) * 0.8f)
    {
        // pouco espaco: so o icone (o texto aparece na dica)
        const auto fgc = Theme::text.interpolatedWith (onColour.brighter (0.3f), ov);
        Icons::draw (g, icon, r.withSizeKeepingCentre (iconS + 2.0f, iconS + 2.0f), fgc);
        return;
    }
    const float textW = f.getStringWidthFloat (getButtonText());
    const float total = iconS + 6.0f + textW;
    auto inner = r.reduced (8.0f, 0.0f);
    if (total > inner.getWidth()) f = Theme::fitFont (f, getButtonText(), inner.getWidth() - iconS - 6.0f);
    const float tw = juce::jmin (f.getStringWidthFloat (getButtonText()), inner.getWidth() - iconS - 6.0f);
    const float x0 = inner.getCentreX() - (iconS + 6.0f + tw) * 0.5f;
    const auto fg = Theme::text.interpolatedWith (onColour.brighter (0.3f), ov);
    Icons::draw (g, icon, { x0, r.getCentreY() - iconS * 0.5f, iconS, iconS }, Theme::textDim.interpolatedWith (fg, 0.5f + 0.5f * juce::jmax (hv, ov)));
    g.setColour (fg);
    g.setFont (f);
    g.drawText (getButtonText(), juce::Rectangle<float> (x0 + iconS + 6.0f, r.getY(), tw + 2.0f, r.getHeight()), juce::Justification::centredLeft, true);
}

} // namespace mp
