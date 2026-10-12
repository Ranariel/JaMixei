#include "InstrumentView.h"
#include "../Audio/Instruments.h"

namespace mp
{
namespace
{
    bool isBlackKey (int pitch) { const int n = ((pitch % 12) + 12) % 12; return n == 1 || n == 3 || n == 6 || n == 8 || n == 10; }
    constexpr int kLowKey = 21, kHighKey = 108;     // piano de 88 teclas (A0 .. C8)
    constexpr int kRollLow = 24, kRollHigh = 108;   // faixa do piano roll (C1 .. C8)
}

// ============================================================================
//  Lista de instrumentos (channel rack)
// ============================================================================
class InstrumentView::ChannelList : public juce::Component
{
public:
    explicit ChannelList (InstrumentView& o) : owner (o) {}

    static constexpr int rowH = 50;

    void paint (juce::Graphics& g) override
    {
        auto& p = owner.session.project;
        g.fillAll (Theme::panel);
        g.setColour (Theme::outline);
        g.drawVerticalLine (getWidth() - 1, 0, (float) getHeight());
        g.setFont (Theme::font (11.0f, true));
        g.setColour (Theme::textFaint);
        g.drawText (TR ("INSTRUMENTOS DO PROJETO"), 10, 4, getWidth() - 20, 18, juce::Justification::centredLeft);
        int y = 26;
        for (auto& c : p.channels)
        {
            auto r = juce::Rectangle<float> (6.0f, (float) y, (float) getWidth() - 12.0f, (float) rowH - 6.0f);
            const bool sel = c.id == p.currentChannelId;
            g.setColour (sel ? Theme::panel3 : Theme::panel2);
            g.fillRoundedRectangle (r, 6.0f);
            if (sel)
            {
                g.setColour (c.colour.withAlpha (0.8f));
                g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.2f);
            }
            g.setColour (c.colour);
            g.fillRoundedRectangle (r.withWidth (5.0f).reduced (0.0f, 6.0f).translated (6.0f, 0.0f), 2.0f);
            auto text = r.reduced (18.0f, 5.0f).withTrimmedRight (30.0f);
            g.setColour (c.mute ? Theme::textFaint : Theme::text);
            g.setFont (Theme::fontFor (c.name, 13.0f, true));
            g.drawText (c.name, text.removeFromTop (text.getHeight() * 0.55f), juce::Justification::bottomLeft, true);
            g.setColour (Theme::textDim);
            g.setFont (Theme::font (11.0f));
            int count = 0;
            if (auto* pat = owner.session.currentPattern()) for (auto& n : pat->notes) if (n.channelId == c.id) ++count;
            juce::String sub = instruments::displayName ((instruments::Id) c.instrument);
            if (count > 0) sub << "  |  " << count << TR (" notas");
            if (owner.session.bank.isLoading ((instruments::Id) c.instrument)) sub = TR ("gerando o som...");
            g.drawText (sub, text, juce::Justification::topLeft, true);
            // botao mudo
            auto m = muteRect (y);
            g.setColour (c.mute ? Theme::warn : Theme::panel3.brighter (0.15f));
            g.fillRoundedRectangle (m, 4.0f);
            g.setColour (c.mute ? juce::Colours::black : Theme::textDim);
            g.setFont (Theme::font (11.0f, true));
            g.drawText ("M", m, juce::Justification::centred);
            y += rowH;
        }
        if (p.channels.empty())
        {
            g.setColour (Theme::textDim);
            g.setFont (Theme::font (12.5f));
            g.drawFittedText (TR ("Nenhum instrumento ainda.\nClique em \"+ Instrumento\" ou toque no teclado (começa com piano)."),
                              juce::Rectangle<int> (12, 34, getWidth() - 24, 80), juce::Justification::topLeft, 4, 1.0f);
        }
    }

    juce::Rectangle<float> muteRect (int rowY) const { return { (float) getWidth() - 36.0f, (float) rowY + 13.0f, 22.0f, 20.0f }; }

    int rowAt (int y) const
    {
        const int i = (y - 26) / rowH;
        return (y >= 26 && i < (int) owner.session.project.channels.size()) ? i : -1;
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        auto& p = owner.session.project;
        const int i = rowAt (e.y);
        if (i < 0) { if (e.mods.isPopupMenu()) owner.addInstrumentMenu(); return; }
        const int id = p.channels[(size_t) i].id;
        if (muteRect (26 + i * rowH).contains (e.position))
        {
            owner.editChannel (TR ("Silenciar instrumento"), [] (InstrumentChannel& c) { c.mute = ! c.mute; });
            return;
        }
        owner.session.selectChannel (id);
        if (e.mods.isPopupMenu()) showMenu (id);
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const int i = rowAt (e.y);
        if (i >= 0) rename (owner.session.project.channels[(size_t) i].id);
    }

    void rename (int id)
    {
        auto* c = owner.session.project.findChannel (id);
        if (c == nullptr) return;
        auto* aw = new juce::AlertWindow (TR ("Renomear instrumento"), {}, juce::MessageBoxIconType::NoIcon);
        aw->addTextEditor ("name", c->name);
        aw->addButton (TR ("OK"), 1, juce::KeyPress (juce::KeyPress::returnKey));
        aw->addButton (TR ("Cancelar"), 0, juce::KeyPress (juce::KeyPress::escapeKey));
        juce::Component::SafePointer<InstrumentView> safe (&owner);
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([safe, aw, id] (int r)
        {
            if (r != 1 || safe == nullptr) return;
            const auto name = aw->getTextEditorContents ("name").trim().substring (0, 32);
            if (name.isEmpty()) return;
            safe->session.selectChannel (id);
            safe->editChannel (TR ("Renomear instrumento"), [name] (InstrumentChannel& ch) { ch.name = name; });
        }), true);
    }

    void showMenu (int id)
    {
        juce::PopupMenu m, swap;
        for (int k = 0; k < (int) instruments::Id::count; ++k)
            swap.addItem (100 + k, instruments::displayName ((instruments::Id) k));
        m.addItem (1, TR ("Renomear..."));
        m.addSubMenu (TR ("Trocar o som por"), swap);
        m.addItem (2, TR ("Duplicar (com as notas)"));
        m.addSeparator();
        m.addItem (3, TR ("Remover instrumento"));
        juce::Component::SafePointer<InstrumentView> safe (&owner);
        m.showMenuAsync (juce::PopupMenu::Options(), [safe, id, this] (int r)
        {
            if (safe == nullptr || r == 0) return;
            auto& s = safe->session;
            if (r == 1) rename (id);
            if (r == 3) s.removeChannel (id);
            if (r >= 100) s.setChannelInstrument (id, r - 100);
            if (r == 2)
            {
                auto* src = s.project.findChannel (id);
                if (src == nullptr) return;
                const auto copy = *src;
                const int nid = s.addChannel (copy.instrument);
                if (auto* c = s.project.findChannel (nid))
                {
                    const int keepId = c->id;
                    *c = copy;
                    c->id = keepId;
                    c->name = copy.name + TR (" (cópia)");
                }
                for (auto& pat : s.project.patterns)
                {
                    std::vector<Note> add;
                    for (auto& n : pat.notes) if (n.channelId == id) { auto n2 = n; n2.channelId = nid; add.push_back (n2); }
                    pat.notes.insert (pat.notes.end(), add.begin(), add.end());
                }
                s.commit (kChPatterns);
            }
        });
    }

private:
    InstrumentView& owner;
};

// ============================================================================
//  Piano roll
// ============================================================================
class InstrumentView::PianoRoll : public juce::Component
{
public:
    explicit PianoRoll (InstrumentView& o) : owner (o) {}

    static constexpr int keysW = 58, velH = 70, headerH = 22;
    float rowH = 15.0f;
    int topPitch = 84;               // nota na primeira linha visivel (C6)
    std::vector<int> selection;      // indices em pattern->notes
    double lastLength = 0.5;     // 1/8 (muda para a ultima duracao usada)

    Pattern* pattern() { return owner.session.currentPattern(); }
    int channelId() const { return owner.session.project.currentChannelId; }

    juce::Rectangle<int> gridArea() const { return getLocalBounds().withTrimmedLeft (keysW).withTrimmedTop (headerH).withTrimmedBottom (velH); }
    juce::Rectangle<int> velArea() const { return getLocalBounds().withTrimmedLeft (keysW).removeFromBottom (velH); }
    double patLenBeats() { auto* p = pattern(); return p ? p->lengthBeats() : 4.0; }
    double pxPerBeat() { return gridArea().getWidth() / juce::jmax (0.25, patLenBeats()); }
    double xToBeat (float x) { return (x - gridArea().getX()) / pxPerBeat(); }
    float beatToX (double b) { return (float) (gridArea().getX() + b * pxPerBeat()); }
    int yToPitch (float y) const { return topPitch - (int) std::floor ((y - gridArea().getY()) / rowH); }
    float pitchToY (int pitch) const { return gridArea().getY() + (topPitch - pitch) * rowH; }
    int visibleRows() const { return (int) std::ceil (gridArea().getHeight() / rowH); }

    void clampScroll()
    {
        topPitch = juce::jlimit (kRollLow + visibleRows() - 1, kRollHigh, topPitch);
    }

    void centerOn (int pitch)
    {
        topPitch = pitch + visibleRows() / 2;
        clampScroll();
        repaint();
    }

    double snap (double beat, bool fine) const
    {
        const double g = owner.gridBeats();
        if (fine || g <= 0) return beat;
        return std::floor (beat / g + 1.0e-6) * g;
    }
    double snapRound (double beat, bool fine) const
    {
        const double g = owner.gridBeats();
        if (fine || g <= 0) return beat;
        return std::round (beat / g) * g;
    }

    void validateSelection()
    {
        auto* p = pattern();
        const int n = p ? (int) p->notes.size() : 0;
        selection.erase (std::remove_if (selection.begin(), selection.end(), [n] (int i) { return i < 0 || i >= n; }), selection.end());
    }
    bool isSelected (int i) const { return std::find (selection.begin(), selection.end(), i) != selection.end(); }

    int noteAt (juce::Point<float> pt)
    {
        auto* p = pattern();
        if (p == nullptr) return -1;
        for (int i = (int) p->notes.size() - 1; i >= 0; --i)
        {
            const auto& n = p->notes[(size_t) i];
            if (n.channelId != channelId()) continue;
            if (noteRect (n).expanded (0.0f, 0.5f).contains (pt)) return i;
        }
        return -1;
    }

    juce::Rectangle<float> noteRect (const Note& n)
    {
        const float x0 = beatToX (n.start), x1 = beatToX (n.start + n.length);
        return { x0, pitchToY (n.pitch) + 1.0f, juce::jmax (3.0f, x1 - x0 - 1.0f), rowH - 2.0f };
    }

    // ------------------------------------------------------------------ desenho
    void paint (juce::Graphics& g) override
    {
        auto grid = gridArea();
        auto* p = pattern();
        const int ch = channelId();
        const auto* chan = owner.session.project.findChannel (ch);
        const auto chanCol = chan ? chan->colour : Theme::accent2;
        g.fillAll (Theme::bg);

        // linhas das notas
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (grid);
            for (int pitch = topPitch; pitch >= topPitch - visibleRows(); --pitch)
            {
                const float y = pitchToY (pitch);
                g.setColour (isBlackKey (pitch) ? Theme::bg.darker (0.25f) : Theme::panel.brighter (0.02f));
                g.fillRect ((float) grid.getX(), y, (float) grid.getWidth(), rowH);
                if (pitch % 12 == 0)
                {
                    g.setColour (Theme::gridStrong);
                    g.fillRect ((float) grid.getX(), y + rowH - 1.0f, (float) grid.getWidth(), 1.0f);
                }
            }
            // grade vertical
            const double L = patLenBeats(), gb = owner.gridBeats() > 0 ? owner.gridBeats() : 0.25;
            const int bpb = juce::jmax (1, owner.session.project.beatsPerBar);
            for (double b = 0; b <= L + 1e-6; b += gb)
            {
                const float x = beatToX (b);
                const bool bar = std::abs (std::fmod (b, (double) bpb)) < 1e-6;
                const bool beat = std::abs (b - std::round (b)) < 1e-6;
                g.setColour (bar ? Theme::gridStrong.brighter (0.25f) : beat ? Theme::gridStrong : Theme::grid);
                g.fillRect (x, (float) grid.getY(), bar ? 1.6f : 1.0f, (float) grid.getHeight());
            }
            if (p != nullptr)
            {
                // notas dos outros instrumentos (fantasma)
                for (auto& n : p->notes)
                {
                    if (n.channelId == ch) continue;
                    const auto* oc = owner.session.project.findChannel (n.channelId);
                    g.setColour ((oc ? oc->colour : Theme::textFaint).withAlpha (0.22f));
                    g.fillRoundedRectangle (noteRect (n), 2.5f);
                }
                // notas do instrumento atual
                for (int i = 0; i < (int) p->notes.size(); ++i)
                {
                    const auto& n = p->notes[(size_t) i];
                    if (n.channelId != ch) continue;
                    auto r = noteRect (n);
                    const bool sel = isSelected (i);
                    const float a = 0.55f + 0.45f * n.vel / 127.0f;
                    g.setColour (chanCol.withMultipliedBrightness (sel ? 1.25f : 1.0f).withAlpha (a));
                    g.fillRoundedRectangle (r, 3.0f);
                    g.setColour (sel ? Theme::text : juce::Colours::black.withAlpha (0.45f));
                    g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, sel ? 1.6f : 1.0f);
                    if (r.getWidth() > 26 && rowH >= 12)
                    {
                        g.setColour (juce::Colours::black.withAlpha (0.75f));
                        g.setFont (Theme::font (9.5f, true));
                        g.drawText (instruments::noteName (n.pitch), r.reduced (3.0f, 0.0f), juce::Justification::centredLeft, false);
                    }
                }
            }
            // selecao em retangulo
            if (! rubber.isEmpty())
            {
                g.setColour (Theme::accent2.withAlpha (0.15f));
                g.fillRect (rubber);
                g.setColour (Theme::accent2.withAlpha (0.8f));
                g.drawRect (rubber, 1.0f);
            }
            // cursor de reproducao
            if (playhead >= 0)
            {
                g.setColour (Theme::playhead.withAlpha (0.85f));
                g.fillRect (beatToX (playhead), (float) grid.getY(), 1.5f, (float) grid.getHeight());
            }
        }

        // cabecalho (compassos)
        auto header = getLocalBounds().withTrimmedLeft (keysW).removeFromTop (headerH);
        g.setColour (Theme::panel2);
        g.fillRect (header);
        g.setFont (Theme::mono (11.0f));
        const int bpb = juce::jmax (1, owner.session.project.beatsPerBar);
        for (int b = 0; b < (int) std::ceil (patLenBeats()); ++b)
        {
            const float x = beatToX (b);
            g.setColour (b % bpb == 0 ? Theme::text : Theme::textFaint);
            g.drawText (b % bpb == 0 ? juce::String (b / bpb + 1) : juce::String (b / bpb + 1) + "." + juce::String (b % bpb + 1),
                        juce::Rectangle<float> (x + 4.0f, (float) header.getY(), 60.0f, (float) headerH), juce::Justification::centredLeft);
        }

        // teclas a esquerda (desenhadas como um piano de verdade)
        auto keys = getLocalBounds().withWidth (keysW).withTrimmedTop (headerH).withTrimmedBottom (velH);
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (keys);
            g.setColour (juce::Colour (0xffe9ecf2));
            g.fillRect (keys);
            for (int pitch = topPitch; pitch >= topPitch - visibleRows(); --pitch)
            {
                const float y = pitchToY (pitch);
                const int n = ((pitch % 12) + 12) % 12;
                const bool lit = owner.keyboardLit (pitch);
                if (! isBlackKey (pitch))
                {
                    if (lit) { g.setColour (chanCol); g.fillRect ((float) keys.getX(), y, (float) keysW, rowH); }
                    // divisao entre teclas brancas vizinhas (B-C e E-F)
                    if (n == 0 || n == 5)
                    {
                        g.setColour (juce::Colour (0xff9aa1ad));
                        g.fillRect ((float) keys.getX(), y + rowH - 0.8f, (float) keysW, 0.8f);
                    }
                }
                if (pitch % 12 == 0)
                {
                    g.setColour (lit ? juce::Colours::black : juce::Colour (0xff4b5563));
                    g.setFont (Theme::font (10.0f, true));
                    g.drawText (instruments::noteName (pitch), juce::Rectangle<float> ((float) keys.getX(), y, keysW - 4.0f, rowH),
                                juce::Justification::centredRight);
                }
            }
            for (int pitch = topPitch; pitch >= topPitch - visibleRows(); --pitch)
                if (isBlackKey (pitch))
                {
                    const float y = pitchToY (pitch);
                    g.setColour (owner.keyboardLit (pitch) ? chanCol.darker (0.2f) : juce::Colour (0xff1a1d23));
                    g.fillRoundedRectangle ((float) keys.getX(), y + 1.0f, keysW * 0.6f, rowH - 2.0f, 2.0f);
                }
            g.setColour (Theme::outline);
            g.drawVerticalLine (keysW - 1, (float) keys.getY(), (float) keys.getBottom());
        }
        g.setColour (Theme::panel2);
        g.fillRect (0, 0, keysW, headerH);

        // faixa de velocidade
        auto va = velArea();
        g.setColour (Theme::panel);
        g.fillRect (va);
        g.setColour (Theme::outline);
        g.drawHorizontalLine (va.getY(), (float) va.getX(), (float) va.getRight());
        g.setColour (Theme::textFaint);
        g.setFont (Theme::font (10.0f, true));
        g.drawText (TR ("VELOC."), juce::Rectangle<int> (0, va.getY(), keysW - 4, 20), juce::Justification::centredRight);
        if (p != nullptr)
            for (int i = 0; i < (int) p->notes.size(); ++i)
            {
                const auto& n = p->notes[(size_t) i];
                if (n.channelId != ch) continue;
                const float x = beatToX (n.start);
                const float h = (va.getHeight() - 8.0f) * n.vel / 127.0f;
                g.setColour (chanCol.withAlpha (isSelected (i) ? 1.0f : 0.7f));
                g.fillRect (x, va.getBottom() - 4.0f - h, 3.0f, h);
                g.fillEllipse (x - 2.5f, va.getBottom() - 4.0f - h - 3.0f, 8.0f, 6.0f);
            }

        // avisos
        if (chan == nullptr)
            overlay (g, TR ("Adicione um instrumento em \"+ Instrumento\" (ou toque no teclado lá embaixo) para escrever notas aqui."));
        else if (owner.session.bank.get ((instruments::Id) chan->instrument) == nullptr)
            overlay (g, TR ("Gerando o som de ") + instruments::displayName ((instruments::Id) chan->instrument) + "...");
    }

    void overlay (juce::Graphics& g, const juce::String& text)
    {
        auto r = gridArea().withSizeKeepingCentre (juce::jmin (620, gridArea().getWidth() - 40), 54).toFloat();
        g.setColour (Theme::panel3.withAlpha (0.94f));
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (Theme::text);
        g.setFont (Theme::font (13.0f));
        g.drawFittedText (text, r.reduced (14.0f, 4.0f).toNearestInt(), juce::Justification::centred, 2, 1.0f);
    }

    // ------------------------------------------------------------------ mouse
    enum class Drag { None, Move, Resize, Erase, Rubber, Velocity, Audition };

    void mouseDown (const juce::MouseEvent& e) override
    {
        auto* p = pattern();
        validateSelection();
        drag = Drag::None;
        changed = false;
        editPushed = false;
        auto pt = e.position;
        if (pt.x < keysW && pt.y >= headerH && pt.y < gridArea().getBottom())
        {
            auditionPitch = yToPitch (pt.y);
            owner.session.noteDown (auditionPitch, 100);
            drag = Drag::Audition;
            return;
        }
        if (p == nullptr || owner.session.project.findChannel (channelId()) == nullptr) return;
        if (velArea().contains (pt.toInt()))
        {
            drag = Drag::Velocity;
            applyVelocity (pt);
            return;
        }
        if (! gridArea().contains (pt.toInt())) return;

        const int hit = noteAt (pt);
        if (e.mods.isPopupMenu())
        {
            drag = Drag::Erase;
            if (hit >= 0) eraseNote (hit);
            return;
        }
        const bool fine = e.mods.isAltDown();
        if (hit >= 0)
        {
            if (e.mods.isShiftDown())
            {
                if (isSelected (hit)) selection.erase (std::find (selection.begin(), selection.end(), hit));
                else selection.push_back (hit);
            }
            else if (! isSelected (hit)) selection = { hit };
            const auto& n = p->notes[(size_t) hit];
            const auto r = noteRect (n);
            drag = (pt.x > r.getRight() - juce::jmin (8.0f, r.getWidth() * 0.35f)) ? Drag::Resize : Drag::Move;
            anchor = hit;
            lastLength = n.length;
            audition (n.pitch, n.vel);
        }
        else if (e.mods.isCommandDown() || e.mods.isShiftDown())
        {
            drag = Drag::Rubber;
            rubberStart = pt;
            if (! e.mods.isShiftDown()) selection.clear();
        }
        else
        {
            // nota nova: comeca na grade e usa a ultima duracao
            const double start = juce::jlimit (0.0, patLenBeats() - 1.0 / 64.0, snap (xToBeat (pt.x), fine));
            const int pitch = juce::jlimit (0, 127, yToPitch (pt.y));
            pushEdit (TR ("Adicionar nota"));
            Note n;
            n.channelId = channelId();
            n.pitch = pitch;
            n.start = start;
            n.length = juce::jmin (lastLength, patLenBeats() - start);
            n.vel = (uint8_t) lastVel;
            p->notes.push_back (n);
            const int idx = (int) p->notes.size() - 1;
            selection = { idx };
            anchor = idx;
            drag = Drag::Move;
            changed = true;
            audition (pitch, n.vel);
            apply();
        }
        originals.clear();
        for (int i : selection) originals.push_back (p->notes[(size_t) i]);
        dragStartBeat = xToBeat (pt.x);
        dragStartPitch = yToPitch (pt.y);
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        auto* p = pattern();
        auto pt = e.position;
        if (drag == Drag::Audition)
        {
            const int pitch = yToPitch (pt.y);
            if (pitch != auditionPitch)
            {
                owner.session.noteUp (auditionPitch);
                auditionPitch = pitch;
                owner.session.noteDown (pitch, 100);
            }
            return;
        }
        if (p == nullptr) return;
        const bool fine = e.mods.isAltDown();
        if (drag == Drag::Velocity) { applyVelocity (pt); return; }
        if (drag == Drag::Erase)
        {
            const int hit = noteAt (pt);
            if (hit >= 0) eraseNote (hit);
            return;
        }
        if (drag == Drag::Rubber)
        {
            rubber = juce::Rectangle<float> (rubberStart, pt);
            selection.clear();
            for (int i = 0; i < (int) p->notes.size(); ++i)
                if (p->notes[(size_t) i].channelId == channelId() && rubber.intersects (noteRect (p->notes[(size_t) i])))
                    selection.push_back (i);
            repaint();
            return;
        }
        if ((drag != Drag::Move && drag != Drag::Resize) || originals.size() != selection.size()) return;
        const double L = patLenBeats();
        const double g = owner.gridBeats() > 0 && ! fine ? owner.gridBeats() : 1.0 / 64.0;
        if (drag == Drag::Move)
        {
            double dBeat = snapRound (xToBeat (pt.x) - dragStartBeat, fine);
            int dPitch = yToPitch (pt.y) - dragStartPitch;
            // o grupo inteiro precisa caber no padrao
            for (auto& o : originals)
            {
                dBeat = juce::jlimit (-o.start, L - g - o.start, dBeat);
                dPitch = juce::jlimit (0 - o.pitch, 127 - o.pitch, dPitch);
            }
            bool any = false;
            for (size_t k = 0; k < selection.size(); ++k)
            {
                auto& n = p->notes[(size_t) selection[k]];
                const double ns = originals[k].start + dBeat;
                const int np = originals[k].pitch + dPitch;
                if (n.start != ns || n.pitch != np) any = true;
                if (n.pitch != np && (int) k == indexOf (anchor)) audition (np, n.vel);
                n.start = ns;
                n.pitch = np;
            }
            if (any) { pushEdit (TR ("Mover notas")); changed = true; apply(); }
        }
        else
        {
            const int ai = indexOf (anchor);
            if (ai < 0) return;
            const auto& o = originals[(size_t) ai];
            const double endBeat = snapRound (xToBeat (pt.x), fine);
            const double dLen = juce::jmax (g, endBeat - o.start) - o.length;
            bool any = false;
            for (size_t k = 0; k < selection.size(); ++k)
            {
                auto& n = p->notes[(size_t) selection[k]];
                const double nl = juce::jlimit (g, juce::jmax (g, L - n.start), originals[k].length + dLen);
                if (n.length != nl) any = true;
                n.length = nl;
            }
            if (any)
            {
                pushEdit (TR ("Duração da nota"));
                changed = true;
                lastLength = p->notes[(size_t) anchor].length;
                apply();
            }
        }
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (drag == Drag::Audition || auditioning >= 0)
        {
            if (drag == Drag::Audition) owner.session.noteUp (auditionPitch);
            stopAudition();
        }
        if (drag == Drag::Move || drag == Drag::Resize)
            if (auto* p = pattern(); p != nullptr && anchor >= 0 && anchor < (int) p->notes.size()) lastLength = p->notes[(size_t) anchor].length;
        if (changed) owner.session.commit (kChPatterns);
        rubber = {};
        drag = Drag::None;
        repaint();
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        // duplo clique numa nota: apaga (alternativa ao botao direito)
        const int hit = noteAt (e.position);
        if (hit >= 0 && drag == Drag::None)
        {
            changed = false;
            eraseNote (hit);
            if (changed) owner.session.commit (kChPatterns);
        }
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (e.mods.isCommandDown())
        {
            rowH = juce::jlimit (9.0f, 28.0f, rowH * (w.deltaY > 0 ? 1.12f : 0.89f));
            clampScroll();
            repaint();
            return;
        }
        topPitch += w.deltaY > 0 ? 3 : -3;
        clampScroll();
        repaint();
    }

    void resized() override { clampScroll(); }

    // ------------------------------------------------------------------ edicao
    void pushEdit (const juce::String& label)
    {
        if (editPushed) return;
        owner.session.beginEdit (label);
        editPushed = true;
    }

    void apply()
    {
        // atualiza o som na hora (sem notificar todas as telas a cada pixel)
        owner.session.rebuildSnapshot();
        repaint();
    }

    void eraseNote (int idx)
    {
        auto* p = pattern();
        if (p == nullptr || idx < 0 || idx >= (int) p->notes.size()) return;
        pushEdit (TR ("Apagar nota"));
        p->notes.erase (p->notes.begin() + idx);
        selection.clear();
        changed = true;
        apply();
    }

    void applyVelocity (juce::Point<float> pt)
    {
        auto* p = pattern();
        if (p == nullptr) return;
        auto va = velArea().toFloat();
        const int v = juce::jlimit (1, 127, (int) std::round ((va.getBottom() - 4.0f - pt.y) / (va.getHeight() - 8.0f) * 127.0f));
        // muda a nota mais proxima no X (ou as selecionadas, se o mouse estiver sobre uma delas)
        int best = -1;
        float bestD = 10.0f;
        for (int i = 0; i < (int) p->notes.size(); ++i)
        {
            const auto& n = p->notes[(size_t) i];
            if (n.channelId != channelId()) continue;
            const float d = std::abs (beatToX (n.start) - pt.x);
            if (d < bestD) { bestD = d; best = i; }
        }
        if (best < 0) return;
        std::vector<int> targets = isSelected (best) ? selection : std::vector<int> { best };
        bool any = false;
        for (int i : targets)
            if (p->notes[(size_t) i].vel != (uint8_t) v) { any = true; break; }
        if (! any) return;
        pushEdit (TR ("Velocidade da nota"));
        for (int i : targets) p->notes[(size_t) i].vel = (uint8_t) v;
        lastVel = v;
        changed = true;
        apply();
    }

    void audition (int pitch, uint8_t vel)
    {
        if (owner.session.engine.isPlaying()) return;   // tocando: nao atrapalha a musica
        stopAudition();
        auditioning = pitch;
        owner.session.noteDown (pitch, vel);
        auditionTicks = 8;
    }
    void stopAudition()
    {
        if (auditioning >= 0) owner.session.noteUp (auditioning);
        auditioning = -1;
    }
    void tick()
    {
        if (auditioning >= 0 && drag == Drag::None && --auditionTicks <= 0) stopAudition();
        // cursor de reproducao dentro do padrao atual
        double ph = -1;
        auto& s = owner.session;
        if (s.engine.isPlaying())
        {
            const double pos = s.engine.getPositionBeats();
            const double L = patLenBeats();
            if (! s.songMode) ph = std::fmod (juce::jmax (0.0, pos), L);
            else
                for (auto& c : s.project.clips)
                    if (c.kind == ClipKind::Pattern && c.patternId == s.project.currentPatternId
                        && pos >= c.startBeat && pos < s.project.clipEndBeat (c))
                    { ph = std::fmod (pos - c.startBeat + c.patternOffsetBeats, L); break; }
        }
        if (ph != playhead) { playhead = ph; repaint(); }
    }

    int indexOf (int noteIdx) const
    {
        for (size_t k = 0; k < selection.size(); ++k) if (selection[k] == noteIdx) return (int) k;
        return -1;
    }

    InstrumentView& owner;
    Drag drag = Drag::None;
    bool changed = false, editPushed = false;
    int anchor = -1, auditionPitch = -1, auditioning = -1, auditionTicks = 0, lastVel = 100;
    std::vector<Note> originals;
    double dragStartBeat = 0;
    int dragStartPitch = 60;
    juce::Point<float> rubberStart;
    juce::Rectangle<float> rubber;
    double playhead = -1;
};

// ============================================================================
//  Teclado de piano completo (88 teclas)
// ============================================================================
class InstrumentView::Keyboard : public juce::Component
{
public:
    explicit Keyboard (InstrumentView& o) : owner (o) {}

    int numWhite() const { int n = 0; for (int k = kLowKey; k <= kHighKey; ++k) if (! isBlackKey (k)) ++n; return n; }
    float whiteW() const { return getWidth() / (float) numWhite(); }

    juce::Rectangle<float> keyRect (int pitch) const
    {
        int wi = 0;
        for (int k = kLowKey; k < pitch; ++k) if (! isBlackKey (k)) ++wi;
        const float w = whiteW(), h = (float) getHeight() - 14.0f;
        if (! isBlackKey (pitch)) return { wi * w, 14.0f, w, h };
        const float bw = w * 0.62f;
        return { wi * w - bw * 0.5f, 14.0f, bw, h * 0.62f };
    }

    int keyAt (juce::Point<float> p) const
    {
        for (int k = kLowKey; k <= kHighKey; ++k) if (isBlackKey (k) && keyRect (k).contains (p)) return k;
        for (int k = kLowKey; k <= kHighKey; ++k) if (! isBlackKey (k) && keyRect (k).contains (p)) return k;
        return -1;
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Theme::panel);
        const auto* chan = owner.session.currentChannel();
        const auto lit = chan ? chan->colour : Theme::accent;
        // faixa do teclado do PC
        const int base = owner.pcBaseNote();
        auto pcR = keyRect (base).getUnion (keyRect (juce::jmin (kHighKey, base + 28)));
        g.setColour (Theme::accent.withAlpha (0.85f));
        g.fillRoundedRectangle (pcR.withY (4.0f).withHeight (5.0f), 2.0f);

        for (int pass = 0; pass < 2; ++pass)
            for (int k = kLowKey; k <= kHighKey; ++k)
            {
                const bool black = isBlackKey (k);
                if (black != (pass == 1)) continue;
                auto r = keyRect (k);
                const bool on = owner.keyboardLit (k) || k == mouseKey;
                if (! black)
                {
                    g.setColour (on ? lit : juce::Colour (0xfff2f4f8));
                    g.fillRoundedRectangle (r.reduced (0.6f, 0.0f), 3.0f);
                    g.setColour (juce::Colour (0xff9aa1ad));
                    g.drawRoundedRectangle (r.reduced (0.6f, 0.0f), 3.0f, 0.8f);
                    if (k % 12 == 0)
                    {
                        g.setColour (on ? juce::Colours::black : juce::Colour (0xff6b7280));
                        g.setFont (Theme::font (9.5f, true));
                        g.drawText (instruments::noteName (k), r.withTrimmedBottom (4.0f), juce::Justification::centredBottom);
                    }
                }
                else
                {
                    g.setColour (on ? lit.darker (0.2f) : juce::Colour (0xff15171c));
                    g.fillRoundedRectangle (r, 2.5f);
                    g.setColour (juce::Colours::white.withAlpha (0.08f));
                    g.fillRoundedRectangle (r.reduced (2.0f, 1.5f).withHeight (r.getHeight() * 0.2f), 1.5f);
                }
            }
    }

    uint8_t velocityAt (int key, juce::Point<float> p) const
    {
        auto r = keyRect (key);
        const float t = juce::jlimit (0.0f, 1.0f, (p.y - r.getY()) / r.getHeight());
        return (uint8_t) juce::jlimit (30, 127, (int) (40 + 87 * t));   // mais embaixo = mais forte
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        mouseKey = keyAt (e.position);
        if (mouseKey >= 0) owner.session.noteDown (mouseKey, velocityAt (mouseKey, e.position));
        repaint();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const int k = keyAt (e.position);
        if (k == mouseKey) return;
        if (mouseKey >= 0) owner.session.noteUp (mouseKey);
        mouseKey = k;
        if (k >= 0) owner.session.noteDown (k, velocityAt (k, e.position));   // glissando
        repaint();
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        if (mouseKey >= 0) owner.session.noteUp (mouseKey);
        mouseKey = -1;
        repaint();
    }

    InstrumentView& owner;
    int mouseKey = -1;
};

// ============================================================================
InstrumentView::InstrumentView (Session& s) : session (s)
{
    channelList = std::make_unique<ChannelList> (*this);
    roll = std::make_unique<PianoRoll> (*this);
    keyboard = std::make_unique<Keyboard> (*this);
    addAndMakeVisible (*channelList);
    addAndMakeVisible (*roll);
    addAndMakeVisible (*keyboard);

    auto label = [this] (juce::Label& l, const juce::String& t)
    {
        l.setText (t, juce::dontSendNotification);
        l.setFont (Theme::font (10.5f, true));
        l.setColour (juce::Label::textColourId, Theme::textFaint);
        addAndMakeVisible (l);
    };
    label (instL, TR ("SOM"));
    label (patL, TR ("PADRÃO"));
    label (lenL, TR ("TAMANHO"));
    label (gridL, TR ("GRADE"));
    pcLabel.setFont (Theme::font (11.5f));
    pcLabel.setColour (juce::Label::textColourId, Theme::textDim);
    pcLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (pcLabel);

    for (int k = 0; k < (int) instruments::Id::count; ++k) instBox.addItem (instruments::displayName ((instruments::Id) k), k + 1);
    instBox.onChange = [this]
    {
        if (updating) return;
        if (auto* c = session.currentChannel()) session.setChannelInstrument (c->id, instBox.getSelectedId() - 1);
        else session.addChannel (instBox.getSelectedId() - 1);
    };
    lenBox.addItemList ({ TR ("1 compasso"), TR ("2 compassos"), TR ("3 compassos"), TR ("4 compassos") }, 1);
    lenBox.onChange = [this] { if (! updating) session.setPatternLength (lenBox.getSelectedId() * 16); };
    gridBox.addItemList ({ "1/4", "1/8", "1/16", "1/32", TR ("1/8 tercina"), TR ("1/16 tercina"), TR ("Livre") }, 1);
    gridBox.setSelectedId (juce::jlimit (1, 7, session.props.getUserSettings()->getIntValue ("rollGrid", 3)), juce::dontSendNotification);
    gridBox.onChange = [this]
    {
        session.props.getUserSettings()->setValue ("rollGrid", gridBox.getSelectedId());
        session.noteQuantize = gridBeats();
        roll->repaint();
    };
    session.noteQuantize = gridBeats();
    patBox.onChange = [this]
    {
        if (updating) return;
        const int idx = patBox.getSelectedId() - 1;
        if (idx == (int) session.project.patterns.size()) { session.newPattern (false); return; }
        if (juce::isPositiveAndBelow (idx, (int) session.project.patterns.size()))
        {
            session.project.currentPatternId = session.project.patterns[(size_t) idx].id;
            roll->selection.clear();
            session.rebuildSnapshot();
            session.notify (kChPatterns);
        }
    };
    for (auto* c : { &instBox, &patBox, &lenBox, &gridBox }) addAndMakeVisible (*c);

    for (auto* k : { &vol, &pan, &attack, &release, &transpose })
    {
        addAndMakeVisible (*k);
        k->onStart = [this] { session.beginEdit (TR ("Ajustar instrumento")); };
    }
    vol.formatter = [] (double v) { return juce::String (v, 1) + " dB"; };
    attack.setSkew (60.0);
    release.setSkew (400.0);
    attack.formatter = [] (double v) { return juce::String (v, v < 10 ? 1 : 0) + " ms"; };
    release.formatter = [] (double v) { return juce::String ((int) v) + " ms"; };
    transpose.formatter = [] (double v) { return (v > 0 ? "+" : "") + juce::String ((int) v) + " st"; };
    vol.onChange = [this] (double v) { editChannel ({}, [v] (InstrumentChannel& c) { c.volumeDb = (float) v; }, false); };
    pan.onChange = [this] (double v) { editChannel ({}, [v] (InstrumentChannel& c) { c.pan = (float) v; }, false); };
    attack.onChange = [this] (double v) { editChannel ({}, [v] (InstrumentChannel& c) { c.attackMs = (float) v; }, false); };
    release.onChange = [this] (double v) { editChannel ({}, [v] (InstrumentChannel& c) { c.releaseMs = (float) v; }, false); };
    transpose.onChange = [this] (double v) { editChannel ({}, [v] (InstrumentChannel& c) { c.transpose = (int) v; }, false); };

    quantBtn.setTooltip (TR ("Alinha o início das notas na grade"));
    quantBtn.onClick = [this] { quantizeNotes(); };
    clearBtn.setTooltip (TR ("Apaga as notas deste instrumento no padrão atual"));
    clearBtn.onClick = [this]
    {
        auto* p = session.currentPattern();
        const int ch = session.project.currentChannelId;
        if (p == nullptr || std::none_of (p->notes.begin(), p->notes.end(), [ch] (const Note& n) { return n.channelId == ch; })) return;
        session.beginEdit (TR ("Limpar notas"));
        p->notes.erase (std::remove_if (p->notes.begin(), p->notes.end(), [ch] (const Note& n) { return n.channelId == ch; }), p->notes.end());
        roll->selection.clear();
        session.commit (kChPatterns);
    };
    addBtn.setColour (juce::TextButton::buttonColourId, Theme::accent.withAlpha (0.3f));
    addBtn.onClick = [this] { addInstrumentMenu(); };
    arrBtn.setTooltip (TR ("Coloca este padrão (notas + pads) na música, no compasso do cursor"));
    arrBtn.onClick = [this]
    {
        auto& p = session.project;
        session.beginEdit (TR ("Inserir padrão"));
        const double b = std::floor (session.engine.getPositionBeats() / p.beatsPerBar) * p.beatsPerBar;
        const int id = session.addPatternClip (p.liveTrackId, p.currentPatternId, b);
        session.selectedClips = { id };
        session.commit (kChClips | kChSelection);
        session.setStatus (TR ("Padrão inserido no compasso ") + juce::String ((int) (b / p.beatsPerBar) + 1)
                           + TR (" da faixa de pads (aba Arranjo)"));
    };
    for (auto* b : { &quantBtn, &clearBtn, &addBtn, &arrBtn }) addAndMakeVisible (*b);

    pcOctave = juce::jlimit (1, 6, session.props.getUserSettings()->getIntValue ("pcOctave", 3));
    session.addListener (this);
    refreshControls();
}

InstrumentView::~InstrumentView()
{
    stopTimer();
    session.removeListener (this);
}

double InstrumentView::gridBeats() const
{
    switch (gridBox.getSelectedId())
    {
        case 1: return 1.0;
        case 2: return 0.5;
        case 3: return 0.25;
        case 4: return 0.125;
        case 5: return 1.0 / 3.0;
        case 6: return 1.0 / 6.0;
        case 7: return 0.0;
        default: break;
    }
    return 0.25;
}

bool InstrumentView::keyboardLit (int pitch) const
{
    if (pcHeld.count (pitch)) return true;
    const auto lo = session.engine.soundingLo(), hi = session.engine.soundingHi();
    if (pitch < 64) return (lo >> pitch) & 1u;
    if (pitch < 128) return (hi >> (pitch - 64)) & 1u;
    return false;
}

void InstrumentView::visibilityChanged()
{
    if (isVisible()) startTimerHz (30);
    else stopTimer();
}

void InstrumentView::timerCallback()
{
    roll->tick();
    const auto lo = session.engine.soundingLo(), hi = session.engine.soundingHi();
    if (lo != lastLo || hi != lastHi) { lastLo = lo; lastHi = hi; keyboard->repaint(); roll->repaint (0, 0, PianoRoll::keysW, roll->getHeight()); }
    // instrumento ainda sendo gerado: atualiza o aviso quando ficar pronto
    if (auto* c = session.currentChannel())
    {
        const bool ready = session.bank.get ((instruments::Id) c->instrument) != nullptr;
        if (ready != lastReady) { lastReady = ready; roll->repaint(); channelList->repaint(); }
    }
}

void InstrumentView::sessionChanged (int what)
{
    if (what & (kChPatterns | kChSelection | kChTransport))
    {
        roll->validateSelection();
        refreshControls();
        roll->repaint();
        channelList->repaint();
        keyboard->repaint();
    }
}

void InstrumentView::refreshControls()
{
    const juce::ScopedValueSetter<bool> svs (updating, true);
    auto* c = session.currentChannel();
    if (c != nullptr) instBox.setSelectedId (c->instrument + 1, juce::dontSendNotification);
    else instBox.setSelectedId (0, juce::dontSendNotification);
    instBox.setTextWhenNothingSelected (TR ("Escolha um som..."));
    for (auto* k : { &vol, &pan, &attack, &release, &transpose }) k->setEnabled (c != nullptr);
    if (c != nullptr)
    {
        vol.setValue (c->volumeDb);
        pan.setValue (c->pan);
        attack.setValue (c->attackMs);
        release.setValue (c->releaseMs);
        transpose.setValue (c->transpose);
    }
    patBox.clear (juce::dontSendNotification);
    int i = 1;
    for (auto& p : session.project.patterns)
    {
        patBox.addItem (p.name, i);
        if (p.id == session.project.currentPatternId) patBox.setSelectedId (i, juce::dontSendNotification);
        ++i;
    }
    patBox.addItem (TR ("+ Novo padrão"), i);
    if (auto* p = session.currentPattern())
    {
        lenBox.setSelectedId (juce::jlimit (1, 4, (p->lengthSteps + 15) / 16), juce::dontSendNotification);
    }
    pcLabel.setText (TR ("Teclado do PC: Z S X D C... e Q 2 W 3 E...  (") + instruments::noteName (pcBaseNote()) + " - "
                     + instruments::noteName (pcBaseNote() + 28) + TR (")   |   - / = muda a oitava"), juce::dontSendNotification);
}

void InstrumentView::editChannel (const juce::String& undoLabel, std::function<void (InstrumentChannel&)> fn, bool undo)
{
    auto* c = session.currentChannel();
    if (c == nullptr) return;
    if (undo) session.beginEdit (undoLabel);
    fn (*c);
    session.rebuildSnapshot();
    if (undo) session.commit (kChPatterns);
    else channelList->repaint();
}

void InstrumentView::addInstrumentMenu()
{
    juce::PopupMenu m;
    for (int k = 0; k < (int) instruments::Id::count; ++k)
        m.addItem (k + 1, instruments::displayName ((instruments::Id) k));
    juce::Component::SafePointer<InstrumentView> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&addBtn), [safe] (int r)
    {
        if (safe == nullptr || r <= 0) return;
        safe->session.addChannel (r - 1);
        const auto& in = instruments::info ((instruments::Id) (r - 1));
        safe->roll->centerOn ((in.defaultOctave + 1) * 12 + 6);
    });
}

int InstrumentView::pcKeySemitone (int keyCode)
{
    // estilo FL Studio: fileira de baixo = oitava 1, fileira de cima = oitava 2
    static const std::pair<int, int> map[] = {
        { 'Z', 0 }, { 'S', 1 }, { 'X', 2 }, { 'D', 3 }, { 'C', 4 }, { 'V', 5 }, { 'G', 6 }, { 'B', 7 }, { 'H', 8 }, { 'N', 9 },
        { 'J', 10 }, { 'M', 11 }, { ',', 12 }, { 'L', 13 }, { '.', 14 },
        { 'Q', 12 }, { '2', 13 }, { 'W', 14 }, { '3', 15 }, { 'E', 16 }, { 'R', 17 }, { '5', 18 }, { 'T', 19 }, { '6', 20 },
        { 'Y', 21 }, { '7', 22 }, { 'U', 23 }, { 'I', 24 }, { '9', 25 }, { 'O', 26 }, { '0', 27 }, { 'P', 28 } };
    for (auto& [k, st] : map) if (k == keyCode) return st;
    return -1;
}

void InstrumentView::shiftPcOctave (int delta)
{
    pcOctave = juce::jlimit (1, 6, pcOctave + delta);
    session.props.getUserSettings()->setValue ("pcOctave", pcOctave);
    refreshControls();
    keyboard->repaint();
    roll->centerOn (pcBaseNote() + 12);
}

void InstrumentView::setPcKeyHeld (int pitch, bool down)
{
    if (down) pcHeld.insert (pitch); else pcHeld.erase (pitch);
    keyboard->repaint();
}

void InstrumentView::deleteSelectedNotes()
{
    auto* p = session.currentPattern();
    roll->validateSelection();
    if (p == nullptr || roll->selection.empty()) return;
    session.beginEdit (TR ("Apagar notas"));
    auto sel = roll->selection;
    std::sort (sel.rbegin(), sel.rend());
    for (int i : sel) p->notes.erase (p->notes.begin() + i);
    roll->selection.clear();
    session.commit (kChPatterns);
}

void InstrumentView::selectAllNotes()
{
    auto* p = session.currentPattern();
    roll->selection.clear();
    if (p != nullptr)
        for (int i = 0; i < (int) p->notes.size(); ++i)
            if (p->notes[(size_t) i].channelId == session.project.currentChannelId) roll->selection.push_back (i);
    roll->repaint();
}

void InstrumentView::duplicateSelectedNotes()
{
    auto* p = session.currentPattern();
    roll->validateSelection();
    if (p == nullptr || roll->selection.empty()) return;
    double s0 = 1e9, s1 = 0;
    for (int i : roll->selection) { s0 = juce::jmin (s0, p->notes[(size_t) i].start); s1 = juce::jmax (s1, p->notes[(size_t) i].start + p->notes[(size_t) i].length); }
    const double g = gridBeats() > 0 ? gridBeats() : 0.25;
    const double shift = juce::jmax (g, std::ceil ((s1 - s0) / g - 1e-6) * g);
    const double L = p->lengthBeats();
    std::vector<Note> add;
    for (int i : roll->selection)
    {
        auto n = p->notes[(size_t) i];
        n.start += shift;
        if (n.start < L - 1e-9) { n.length = juce::jmin (n.length, L - n.start); add.push_back (n); }
    }
    if (add.empty()) { session.setStatus (TR ("Não cabe: aumente o tamanho do padrão")); return; }
    session.beginEdit (TR ("Duplicar notas"));
    roll->selection.clear();
    for (auto& n : add) { p->notes.push_back (n); roll->selection.push_back ((int) p->notes.size() - 1); }
    session.commit (kChPatterns);
}

void InstrumentView::quantizeNotes()
{
    auto* p = session.currentPattern();
    const double g = gridBeats();
    if (p == nullptr || g <= 0) return;
    roll->validateSelection();
    const int ch = session.project.currentChannelId;
    session.beginEdit (TR ("Quantizar notas"));
    const double L = p->lengthBeats();
    for (int i = 0; i < (int) p->notes.size(); ++i)
    {
        auto& n = p->notes[(size_t) i];
        if (n.channelId != ch || (! roll->selection.empty() && ! roll->isSelected (i))) continue;
        n.start = std::round (n.start / g) * g;
        if (n.start >= L - 1e-9) n.start = juce::jmax (0.0, L - g);
        n.length = juce::jmax (g, std::round (n.length / g) * g);
    }
    session.commit (kChPatterns);
    session.setStatus (TR ("Notas quantizadas na grade"));
}

void InstrumentView::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel);
    g.setColour (Theme::outline);
    g.drawHorizontalLine (toolbarH - 1, (float) channelW, (float) getWidth());
}

void InstrumentView::resized()
{
    auto r = getLocalBounds();
    auto left = r.removeFromLeft (channelW);
    addBtn.setBounds (left.removeFromBottom (40).reduced (8, 5));
    channelList->setBounds (left);

    auto bottom = r.removeFromBottom (keyboardH);
    pcLabel.setBounds (bottom.removeFromTop (20).reduced (10, 0));
    keyboard->setBounds (bottom.reduced (6, 2));

    auto bar = r.removeFromTop (toolbarH).reduced (10, 4);
    auto labelled = [&bar] (juce::Label& l, juce::Component& c, int w)
    {
        auto a = bar.removeFromLeft (w);
        l.setBounds (a.removeFromTop (16));
        c.setBounds (a.removeFromTop (30));
        bar.removeFromLeft (10);
    };
    labelled (instL, instBox, 180);
    for (auto* k : { &vol, &pan, &attack, &release, &transpose }) k->setBounds (bar.removeFromLeft (60));
    bar.removeFromLeft (10);
    labelled (patL, patBox, 150);
    labelled (lenL, lenBox, 130);
    labelled (gridL, gridBox, 120);
    auto btns = bar.removeFromLeft (360);
    btns.removeFromTop (16);
    btns = btns.removeFromTop (30);
    quantBtn.setBounds (btns.removeFromLeft (100));
    btns.removeFromLeft (8);
    clearBtn.setBounds (btns.removeFromLeft (120));
    btns.removeFromLeft (8);
    arrBtn.setBounds (btns.removeFromLeft (120));
    roll->setBounds (r);
}
} // namespace mp
