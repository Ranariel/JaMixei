#include "ArrangeView.h"

namespace mp
{
// ============================================================================
//  Linha do cursor de reproducao (componente fino sobreposto, repaint barato)
// ============================================================================
class ArrangeView::PlayheadLine : public juce::Component
{
public:
    PlayheadLine() { setInterceptsMouseClicks (false, false); }
    void paint (juce::Graphics& g) override
    {
        g.setColour (Theme::playhead.withAlpha (0.9f));
        g.fillRect (getWidth() / 2, 0, 1, getHeight());
        g.setColour (Theme::accent.withAlpha (0.35f));
        g.fillRect (getWidth() / 2 - 1, 0, 1, getHeight());
        g.fillRect (getWidth() / 2 + 1, 0, 1, getHeight());
    }
};

// ============================================================================
//  Regua: compassos, cursor e regiao de loop
// ============================================================================
class ArrangeView::Ruler : public juce::Component
{
public:
    explicit Ruler (ArrangeView& o) : owner (o) {}

    void paint (juce::Graphics& g) override
    {
        auto& p = owner.session.project;
        g.fillAll (Theme::panel);
        const int h = getHeight();
        const int loopTop = h / 2;
        // loop
        if (p.loopEnd > p.loopStart)
        {
            const float x0 = owner.beatToX (p.loopStart), x1 = owner.beatToX (p.loopEnd);
            g.setColour ((p.loopOn ? Theme::accent : Theme::textFaint).withAlpha (p.loopOn ? 0.35f : 0.18f));
            g.fillRect (juce::Rectangle<float> (x0, (float) loopTop + 2, x1 - x0, (float) h - loopTop - 4));
            g.setColour (p.loopOn ? Theme::accent : Theme::textFaint);
            g.fillRect (juce::Rectangle<float> (x0, (float) loopTop + 2, 2.0f, (float) h - loopTop - 4));
            g.fillRect (juce::Rectangle<float> (x1 - 2, (float) loopTop + 2, 2.0f, (float) h - loopTop - 4));
        }
        // marcas
        const int bpb = p.beatsPerBar;
        const double barPx = owner.pxPerBeat * bpb;
        int barStep = 1;
        while (barPx * barStep < 46) barStep *= 2;
        const double firstBeat = std::floor (owner.scrollBeats / bpb) * bpb;
        g.setFont (Theme::font (11.5f, true));
        for (double b = firstBeat; ; b += 1.0)
        {
            const float x = owner.beatToX (b);
            if (x > getWidth()) break;
            const int bar = (int) std::llround (b / bpb);
            const bool isBar = std::fmod (b, bpb) == 0;
            if (isBar && bar % barStep == 0)
            {
                g.setColour (Theme::textDim);
                g.fillRect (x, 2.0f, 1.0f, (float) h - 4);
                g.setColour (Theme::text);
                g.drawText (juce::String (bar + 1), (int) x + 4, 1, 40, loopTop, juce::Justification::centredLeft);
            }
            else if (owner.pxPerBeat >= 10)
            {
                g.setColour (Theme::textFaint);
                g.fillRect (x, (float) h - 8, 1.0f, 6.0f);
            }
        }
        // cursor
        const float px = owner.beatToX (owner.session.engine.getPositionBeats());
        juce::Path tri;
        tri.addTriangle (px - 6, 0, px + 6, 0, px, 9);
        g.setColour (Theme::playhead);
        g.fillPath (tri);
        g.setColour (Theme::outline);
        g.drawHorizontalLine (h - 1, 0, (float) getWidth());
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        auto& p = owner.session.project;
        const double b = owner.xToBeat ((float) e.x);
        if (e.mods.isPopupMenu())
        {
            juce::PopupMenu m;
            m.addItem (1, p.loopOn ? TR ("Desligar loop") : TR ("Ligar loop"));
            m.addItem (2, TR ("Loop nos clipes selecionados"), ! owner.session.selectedClips.empty());
            m.addItem (3, TR ("Mover cursor para o início do loop"));
            m.showMenuAsync (juce::PopupMenu::Options(), [this] (int r)
            {
                auto& s = owner.session;
                auto& pr = s.project;
                if (r == 1) { s.beginEdit (TR ("Loop")); pr.loopOn = ! pr.loopOn; s.commit (kChTransport); }
                if (r == 2)
                {
                    double a = 1e18, z = -1e18;
                    for (auto& c : pr.clips) if (s.selectedClips.count (c.id)) { a = std::min (a, c.startBeat); z = std::max (z, pr.clipEndBeat (c)); }
                    s.beginEdit (TR ("Loop")); pr.loopStart = a; pr.loopEnd = z; pr.loopOn = true; s.commit (kChTransport);
                }
                if (r == 3) s.seek (pr.loopStart);
                repaint();
            });
            return;
        }
        mode = 0;
        if (e.y >= getHeight() / 2)
        {
            const float x0 = owner.beatToX (p.loopStart), x1 = owner.beatToX (p.loopEnd);
            owner.session.beginEdit (TR ("Loop"));
            if (std::abs (e.x - x0) < 7) mode = 2;
            else if (std::abs (e.x - x1) < 7) mode = 3;
            else { mode = 4; anchor = snapLoop (b); p.loopStart = anchor; p.loopEnd = anchor; }
        }
        else
        {
            mode = 1;
            owner.session.seek (juce::jmax (0.0, owner.snap (b, false)));
        }
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        auto& p = owner.session.project;
        const double b = juce::jmax (0.0, owner.xToBeat ((float) e.x));
        if (mode == 1) owner.session.seek (e.mods.isShiftDown() ? b : owner.snap (b));
        else if (mode == 2) p.loopStart = juce::jmin (snapLoop (b), p.loopEnd - 0.25);
        else if (mode == 3) p.loopEnd = juce::jmax (snapLoop (b), p.loopStart + 0.25);
        else if (mode == 4)
        {
            const double s = snapLoop (b);
            p.loopStart = juce::jmin (anchor, s);
            p.loopEnd = juce::jmax (anchor, s);
        }
        if (mode >= 2) { p.loopOn = true; owner.session.commit (kChTransport); }
        repaint();
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        auto& p = owner.session.project;
        if (mode == 4 && p.loopEnd - p.loopStart < 0.24)
        {
            p.loopEnd = p.loopStart + p.beatsPerBar;
            owner.session.commit (kChTransport);
        }
        mode = 0;
    }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (e.y >= getHeight() / 2)
        {
            auto& s = owner.session;
            s.beginEdit (TR ("Loop"));
            s.project.loopOn = ! s.project.loopOn;
            s.commit (kChTransport);
        }
    }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        owner.zoomBy (w.deltaY > 0 ? 1.25 : 0.8, (float) e.x);
    }

private:
    double snapLoop (double b) const
    {
        const double g = owner.session.snapBeats > 0 ? juce::jmax (0.25, owner.session.snapBeats) : 0.25;
        return std::round (b / g) * g;
    }
    ArrangeView& owner;
    int mode = 0;
    double anchor = 0;
};

// ============================================================================
//  Cabecalho de faixa
// ============================================================================
class ArrangeView::TrackHeader : public juce::Component
{
public:
    TrackHeader (ArrangeView& o, int id) : owner (o), trackId (id)
    {
        for (auto* b : { &muteBtn, &soloBtn, &autoBtn })
        {
            addAndMakeVisible (*b);
            b->setClickingTogglesState (false);
        }
        styleToggle (muteBtn, Theme::warn);
        styleToggle (soloBtn, Theme::accent2);
        styleToggle (autoBtn, Theme::good);
        muteBtn.setTooltip (TR ("Silenciar faixa"));
        soloBtn.setTooltip (TR ("Solo"));
        autoBtn.setTooltip (TR ("Automação: desligada / volume / filtro. Clique na faixa para criar pontos; duplo clique apaga."));
        muteBtn.onClick = [this] { edit (TR ("Mute"), [] (Track& t) { t.mute = ! t.mute; }); };
        soloBtn.onClick = [this] { edit (TR ("Solo"), [] (Track& t) { t.solo = ! t.solo; }); };
        autoBtn.onClick = [this]
        {
            if (auto* t = track())
            {
                t->autoView = t->autoView == AutoParam::None ? AutoParam::Volume
                            : t->autoView == AutoParam::Volume ? AutoParam::Filter : AutoParam::None;
                owner.session.commit (kChTracks);
            }
        };
        vol.setSliderStyle (juce::Slider::LinearHorizontal);
        vol.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        vol.setRange (-60.0, 6.0, 0.1);
        vol.setSkewFactorFromMidPoint (-12.0);
        vol.setDoubleClickReturnValue (true, 0.0);
        vol.onDragStart = [this] { owner.session.beginEdit (TR ("Volume da faixa")); };
        vol.onValueChange = [this] { if (! updating) if (auto* t = track()) { t->volumeDb = (float) vol.getValue(); owner.session.commit (kChMixer); } };
        vol.setTooltip (TR ("Volume da faixa"));
        addAndMakeVisible (vol);
        refresh();
    }

    Track* track() { return owner.session.project.findTrack (trackId); }

    void edit (const juce::String& label, std::function<void (Track&)> fn)
    {
        if (auto* t = track())
        {
            owner.session.beginEdit (label);
            fn (*t);
            owner.session.commit (kChTracks | kChMixer);
        }
    }

    void refresh()
    {
        auto* t = track();
        if (t == nullptr) return;
        updating = true;
        muteBtn.setToggleState (t->mute, juce::dontSendNotification);
        soloBtn.setToggleState (t->solo, juce::dontSendNotification);
        autoBtn.setToggleState (t->autoView != AutoParam::None, juce::dontSendNotification);
        autoBtn.setButtonText (t->autoView == AutoParam::Volume ? "VOL" : t->autoView == AutoParam::Filter ? "FLT" : "A");
        vol.setValue (t->volumeDb, juce::dontSendNotification);
        vol.setColour (juce::Slider::trackColourId, t->colour);
        updating = false;
        repaint();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (6, 4).withTrimmedLeft (6);
        auto top = r.removeFromTop (20);
        juce::ignoreUnused (top);
        r.removeFromTop (2);
        auto row = r.removeFromTop (20);
        muteBtn.setBounds (row.removeFromLeft (24)); row.removeFromLeft (3);
        soloBtn.setBounds (row.removeFromLeft (24)); row.removeFromLeft (3);
        autoBtn.setBounds (row.removeFromLeft (34)); row.removeFromLeft (6);
        vol.setBounds (row);
    }

    void paint (juce::Graphics& g) override
    {
        auto* t = track();
        if (t == nullptr) return;
        const bool sel = owner.session.selectedTrackId == trackId;
        g.setColour (sel ? Theme::panel3 : Theme::panel2);
        g.fillRect (getLocalBounds().withTrimmedBottom (1));
        g.setColour (t->colour);
        g.fillRect (0, 0, 5, getHeight() - 1);
        g.setColour (Theme::outline);
        g.drawHorizontalLine (getHeight() - 1, 0, (float) getWidth());
        g.drawVerticalLine (getWidth() - 1, 0, (float) getHeight());

        auto r = getLocalBounds().reduced (6, 4).withTrimmedLeft (6).removeFromTop (20);
        const bool isPat = t->type == TrackType::Pattern;
        auto badge = r.removeFromRight (isPat ? 46 : 44).toFloat().reduced (0, 3);
        g.setColour (isPat ? Theme::accent.withAlpha (0.2f) : Theme::accent2.withAlpha (0.15f));
        g.fillRoundedRectangle (badge, 3.0f);
        g.setColour (isPat ? Theme::accent : Theme::accent2);
        g.setFont (Theme::font (10.0f, true));
        g.drawText (isPat ? TR ("PADS") : TR ("ÁUDIO"), badge, juce::Justification::centred);
        if (isPat && owner.session.project.liveTrackId == trackId)
        {
            auto live = r.removeFromRight (16).toFloat().withSizeKeepingCentre (8, 8);
            g.setColour (Theme::rec);
            g.fillEllipse (live);
        }
        g.setColour (Theme::text);
        g.setFont (Theme::fontFor (t->name, 13.5f, true));
        g.drawFittedText (t->name, r.withTrimmedRight (4), juce::Justification::centredLeft, 1, 1.0f);

        if (t->volumeDb != 0.0f && getHeight() > 60)
        {
            g.setColour (Theme::textFaint);
            g.setFont (Theme::font (10.5f));
            g.drawText (formatDb (t->volumeDb), getLocalBounds().reduced (12, 4).removeFromBottom (14), juce::Justification::bottomRight);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        owner.session.selectedTrackId = trackId;
        owner.session.notify (kChSelection);
        if (e.mods.isPopupMenu()) showMenu();
    }
    void mouseDoubleClick (const juce::MouseEvent&) override { rename(); }

    void rename()
    {
        auto* t = track();
        if (t == nullptr) return;
        auto* aw = new juce::AlertWindow (TR ("Renomear faixa"), TR ("Novo nome:"), juce::MessageBoxIconType::NoIcon);
        aw->addTextEditor ("name", t->name);
        aw->addButton (TR ("OK"), 1, juce::KeyPress (juce::KeyPress::returnKey));
        aw->addButton (TR ("Cancelar"), 0, juce::KeyPress (juce::KeyPress::escapeKey));
        const int id = trackId;
        auto* ownerPtr = &owner;
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([aw, id, ownerPtr] (int r)
        {
            if (r == 1)
                if (auto* tr = ownerPtr->session.project.findTrack (id))
                {
                    ownerPtr->session.beginEdit (TR ("Renomear faixa"));
                    tr->name = aw->getTextEditorContents ("name").substring (0, 40);
                    ownerPtr->session.commit (kChTracks | kChMixer);
                }
        }), true);
    }

    void showMenu()
    {
        auto* t = track();
        if (t == nullptr) return;
        juce::PopupMenu m, colours, heights;
        m.addItem (1, TR ("Renomear..."));
        for (int i = 0; i < 12; ++i)
            colours.addColouredItem (100 + i, TR ("Cor ") + juce::String (i + 1), Project::trackColour (i), true, t->colour == Project::trackColour (i));
        m.addSubMenu (TR ("Cor"), colours);
        heights.addItem (200, TR ("Pequena"), true, t->height <= 56);
        heights.addItem (201, TR ("Média"), true, t->height > 56 && t->height <= 100);
        heights.addItem (202, TR ("Grande"), true, t->height > 100);
        m.addSubMenu (TR ("Altura"), heights);
        if (t->type == TrackType::Pattern)
            m.addItem (5, TR ("Receber pads tocados ao vivo"), true, owner.session.project.liveTrackId == trackId);
        m.addSeparator();
        m.addItem (6, TR ("Limpar automação de volume"), ! t->volAuto.empty());
        m.addItem (7, TR ("Limpar automação de filtro"), ! t->filterAuto.empty());
        m.addSeparator();
        m.addItem (2, TR ("Mover para cima"));
        m.addItem (3, TR ("Mover para baixo"));
        m.addSeparator();
        m.addItem (4, TR ("Apagar faixa"));
        const int id = trackId;
        auto* ownerPtr = &owner;
        m.showMenuAsync (juce::PopupMenu::Options(), [this, id, ownerPtr] (int r)
        {
            auto& s = ownerPtr->session;
            auto* tr = s.project.findTrack (id);
            if (tr == nullptr || r == 0) return;
            if (r == 1) rename();
            else if (r == 2) s.moveTrack (id, -1);
            else if (r == 3) s.moveTrack (id, 1);
            else if (r == 4)
            {
                juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::NoIcon, TR ("Apagar faixa"),
                    TR ("Apagar \"") + tr->name + TR ("\" e todos os clipes dela?"), TR ("Apagar"), TR ("Cancelar"), nullptr,
                    juce::ModalCallbackFunction::create ([ownerPtr, id] (int ok) { if (ok) ownerPtr->session.deleteTrack (id); }));
            }
            else if (r == 5) { s.beginEdit (TR ("Faixa ao vivo")); s.project.liveTrackId = id; s.commit (kChTracks); }
            else if (r == 6) { s.beginEdit (TR ("Limpar automação")); tr->volAuto.clear(); s.commit (kChTracks); }
            else if (r == 7) { s.beginEdit (TR ("Limpar automação")); tr->filterAuto.clear(); s.commit (kChTracks); }
            else if (r >= 100 && r < 112)
            {
                s.beginEdit (TR ("Cor da faixa"));
                tr->colour = Project::trackColour (r - 100);
                for (auto& c : s.project.clips) if (c.trackId == id) c.colour = tr->colour;
                s.commit (kChTracks | kChClips | kChMixer);
            }
            else if (r >= 200 && r <= 202)
            {
                tr->height = r == 200 ? 52.0f : r == 201 ? 84.0f : 140.0f;
                s.commit (kChTracks);
            }
        });
    }

    ArrangeView& owner;
    const int trackId;
    juce::TextButton muteBtn { "M" }, soloBtn { "S" }, autoBtn { "A" };
    juce::Slider vol;
    bool updating = false;
};

// ============================================================================
//  Area das faixas: clipes, automacao, selecao
// ============================================================================
class ArrangeView::Lanes : public juce::Component
{
public:
    explicit Lanes (ArrangeView& o) : owner (o) {}

    enum class Zone { None, Body, TrimL, TrimR, FadeIn, FadeOut };
    struct Hit { int clipId = -1; Zone zone = Zone::None; };

    Session& session() { return owner.session; }
    Project& project() { return owner.session.project; }

    juce::Rectangle<float> clipRect (const Clip& c) const
    {
        auto& p = owner.session.project;
        const int ti = p.trackIndex (c.trackId);
        if (ti < 0) return {};
        const float x0 = owner.beatToX (c.startBeat);
        const float x1 = owner.beatToX (p.clipEndBeat (c));
        const int top = owner.trackTop (ti);
        return { x0, (float) top + 1, juce::jmax (2.0f, x1 - x0), p.tracks[(size_t) ti].height - 3.0f };
    }

    Hit hitTest (juce::Point<float> pt)
    {
        auto& p = project();
        for (int i = (int) p.clips.size() - 1; i >= 0; --i)
        {
            auto& c = p.clips[(size_t) i];
            auto r = clipRect (c);
            if (! r.expanded (2, 0).contains (pt)) continue;
            Hit h; h.clipId = c.id; h.zone = Zone::Body;
            if (c.kind == ClipKind::Audio && pt.y < r.getY() + 16)
            {
                const float fi = r.getX() + (float) (c.fadeInBeats * owner.pxPerBeat);
                const float fo = r.getRight() - (float) (c.fadeOutBeats * owner.pxPerBeat);
                if (std::abs (pt.x - fi) < 7) { h.zone = Zone::FadeIn; return h; }
                if (std::abs (pt.x - fo) < 7) { h.zone = Zone::FadeOut; return h; }
            }
            const float edge = juce::jmin (7.0f, r.getWidth() * 0.25f);
            if (pt.x < r.getX() + edge) h.zone = Zone::TrimL;
            else if (pt.x > r.getRight() - edge) h.zone = Zone::TrimR;
            return h;
        }
        return {};
    }

    void paint (juce::Graphics& g) override
    {
        auto& p = project();
        g.fillAll (Theme::bg);
        const int bpb = p.beatsPerBar;

        // fundo das faixas
        for (int i = 0; i < (int) p.tracks.size(); ++i)
        {
            const int top = owner.trackTop (i);
            const int h = (int) p.tracks[(size_t) i].height;
            if (top > getHeight() || top + h < 0) continue;
            g.setColour (i % 2 == 0 ? Theme::panel : Theme::panel.darker (0.12f));
            if (p.tracks[(size_t) i].id == session().selectedTrackId) g.setColour (Theme::panel2);
            g.fillRect (0, top, getWidth(), h);
            g.setColour (Theme::bg);
            g.drawHorizontalLine (top + h - 1, 0, (float) getWidth());
        }

        // grade
        double sub = owner.session.snapBeats > 0 ? owner.session.snapBeats : 0.25;
        while (sub * owner.pxPerBeat < 8) sub *= 2;
        const double first = std::floor (owner.scrollBeats / sub) * sub;
        for (double b = first; ; b += sub)
        {
            const float x = owner.beatToX (b);
            if (x > getWidth()) break;
            const bool bar = std::fmod (b + 1e-9, bpb) < 1e-6;
            const bool beat = std::fmod (b + 1e-9, 1.0) < 1e-6;
            g.setColour (bar ? Theme::gridStrong : beat ? Theme::grid : Theme::grid.withAlpha (0.45f));
            g.fillRect (x, 0.0f, 1.0f, (float) getHeight());
        }

        // loop
        if (p.loopOn)
        {
            const float x0 = owner.beatToX (p.loopStart), x1 = owner.beatToX (p.loopEnd);
            g.setColour (Theme::accent.withAlpha (0.05f));
            g.fillRect (juce::Rectangle<float> (x0, 0, x1 - x0, (float) getHeight()));
            g.setColour (Theme::accent.withAlpha (0.35f));
            g.fillRect (x0, 0.0f, 1.0f, (float) getHeight());
            g.fillRect (x1, 0.0f, 1.0f, (float) getHeight());
        }

        // clipes
        for (auto& c : p.clips) drawClip (g, c);
        drawCrossfades (g);

        // automacao
        for (int i = 0; i < (int) p.tracks.size(); ++i)
            if (p.tracks[(size_t) i].autoView != AutoParam::None) drawAutomation (g, i);

        // selecao por retangulo
        if (! rubber.isEmpty())
        {
            g.setColour (Theme::accent2.withAlpha (0.12f));
            g.fillRect (rubber);
            g.setColour (Theme::accent2.withAlpha (0.7f));
            g.drawRect (rubber, 1.0f);
        }
        if (! owner.dropHighlight.isEmpty())
        {
            g.setColour (Theme::accent.withAlpha (0.25f));
            g.fillRect (owner.dropHighlight);
            g.setColour (Theme::accent);
            g.drawRect (owner.dropHighlight, 2);
        }
        if (p.clips.empty())
        {
            g.setColour (Theme::textFaint);
            g.setFont (Theme::font (16.0f));
            g.drawFittedText ((TR ("Arraste músicas (MP3, WAV, FLAC...) do navegador a esquerda ou do Windows Explorer para uma faixa de áudio.\n") + TR ("Duplo clique numa faixa de PADS insere o padrão atual.")),
                              getLocalBounds().reduced (40).withTrimmedTop (owner.totalTracksHeight() - owner.scrollY),
                              juce::Justification::centredTop, 3, 1.0f);
        }
    }

    void drawCrossfades (juce::Graphics& g)
    {
        auto& p = project();
        for (int ti = 0; ti < (int) p.tracks.size(); ++ti)
        {
            std::vector<const Clip*> list;
            for (auto& c : p.clips)
                if (c.trackId == p.tracks[(size_t) ti].id && c.kind == ClipKind::Audio && ! c.muted) list.push_back (&c);
            std::sort (list.begin(), list.end(), [] (auto* a, auto* b) { return a->startBeat < b->startBeat; });
            const int top = owner.trackTop (ti) + 17;
            const float h = p.tracks[(size_t) ti].height - 20.0f;
            for (size_t i = 0; i + 1 < list.size(); ++i)
                for (size_t j = i + 1; j < list.size(); ++j)
                {
                    auto* a = list[i]; auto* b = list[j];
                    const double aEnd = p.clipEndBeat (*a);
                    if (b->startBeat >= aEnd) break;
                    if (p.clipEndBeat (*b) <= aEnd) continue;
                    const float x0 = owner.beatToX (b->startBeat), x1 = owner.beatToX (aEnd);
                    if (x1 < 0 || x0 > getWidth() || x1 - x0 < 2) continue;
                    auto area = juce::Rectangle<float> (x0, (float) top, x1 - x0, h);
                    g.setColour (juce::Colours::black.withAlpha (0.35f));
                    g.fillRect (area);
                    juce::Path outP, inP;
                    for (int k = 0; k <= 24; ++k)
                    {
                        const float t = k / 24.0f;
                        const float x = area.getX() + area.getWidth() * t;
                        const float yo = area.getBottom() - area.getHeight() * std::cos (t * juce::MathConstants<float>::halfPi);
                        const float yi = area.getBottom() - area.getHeight() * std::sin (t * juce::MathConstants<float>::halfPi);
                        if (k == 0) { outP.startNewSubPath (x, yo); inP.startNewSubPath (x, yi); }
                        else { outP.lineTo (x, yo); inP.lineTo (x, yi); }
                    }
                    g.setColour (a->colour.brighter (0.4f));
                    g.strokePath (outP, juce::PathStrokeType (2.0f));
                    g.setColour (b->colour.brighter (0.4f));
                    g.strokePath (inP, juce::PathStrokeType (2.0f));
                    if (area.getWidth() > 70)
                    {
                        g.setColour (Theme::text);
                        g.setFont (Theme::font (10.5f, true));
                        g.drawText ("CROSSFADE " + juce::String (p.beatsToSeconds (aEnd - b->startBeat), 1) + "s",
                                    area.reduced (4, 2).toNearestInt(), juce::Justification::topLeft);
                    }
                }
        }
    }

    void drawClip (juce::Graphics& g, const Clip& c)
    {
        auto r = clipRect (c);
        if (r.isEmpty() || r.getRight() < 0 || r.getX() > getWidth() || r.getBottom() < 0 || r.getY() > getHeight()) return;
        auto& p = project();
        const bool sel = session().selectedClips.count (c.id) > 0;
        auto col = c.colour;
        if (c.muted) col = col.withSaturation (0.1f).withMultipliedBrightness (0.6f);

        g.setColour (col.withAlpha (sel ? 0.42f : 0.28f));
        g.fillRoundedRectangle (r, 4.0f);
        auto header = r.withHeight (juce::jmin (16.0f, r.getHeight()));
        g.setColour (col.withAlpha (sel ? 0.95f : 0.75f));
        g.fillRoundedRectangle (header, 4.0f);
        g.fillRect (header.withTrimmedTop (4));

        const auto body = r.withTrimmedTop (header.getHeight());
        if (c.kind == ClipKind::Audio)
        {
            if (auto src = session().getSource (c.sourceId))
            {
                g.saveState();
                g.reduceClipRegion (body.toNearestInt());
                const float gain = juce::Decibels::decibelsToGain (c.gainDb);
                drawWaveform (g, *src, body.reduced (0, 2), c.sourceOffsetSec, c.sourceOffsetSec + c.sourceLenSec,
                              col.brighter (0.5f).withAlpha (0.9f), c.reverse, gain);
                g.restoreState();
            }
            else
            {
                g.setColour (Theme::textDim);
                g.setFont (Theme::font (12.0f));
                g.drawText (session().sources.isLoading (c.sourceId) ? TR ("carregando...") : TR ("áudio ausente"), body, juce::Justification::centred);
            }
            // fades
            const float fi = (float) (c.fadeInBeats * owner.pxPerBeat);
            const float fo = (float) (c.fadeOutBeats * owner.pxPerBeat);
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            if (fi > 1)
            {
                juce::Path pth;
                pth.startNewSubPath (r.getX(), body.getY());
                for (int k = 0; k <= 16; ++k)
                {
                    const float t = k / 16.0f;
                    pth.lineTo (r.getX() + fi * t, body.getBottom() - body.getHeight() * std::sin (t * juce::MathConstants<float>::halfPi));
                }
                pth.lineTo (r.getX(), body.getBottom());
                pth.closeSubPath();
                g.fillPath (pth);
            }
            if (fo > 1)
            {
                juce::Path pth;
                pth.startNewSubPath (r.getRight(), body.getY());
                for (int k = 0; k <= 16; ++k)
                {
                    const float t = k / 16.0f;
                    pth.lineTo (r.getRight() - fo * t, body.getBottom() - body.getHeight() * std::sin (t * juce::MathConstants<float>::halfPi));
                }
                pth.lineTo (r.getRight(), body.getBottom());
                pth.closeSubPath();
                g.fillPath (pth);
            }
            // pegadores de fade
            g.setColour (Theme::text);
            g.fillEllipse (r.getX() + fi - 3.5f, r.getY() + 4.5f, 7, 7);
            g.fillEllipse (r.getRight() - fo - 3.5f, r.getY() + 4.5f, 7, 7);
        }
        else if (auto* pat = p.findPattern (c.patternId))
        {
            // mini visualizacao dos passos
            std::vector<int> used;
            for (int pad = 0; pad < kNumPads; ++pad)
                for (int s = 0; s < pat->lengthSteps; ++s) if (pat->get (pad, s)) { used.push_back (pad); break; }
            if (! used.empty())
            {
                const float rowH = juce::jmin (6.0f, (body.getHeight() - 4) / (float) used.size());
                const double stepPx = owner.pxPerBeat / kStepsPerBeat;
                const int totalSteps = (int) std::ceil (c.lengthBeatsPattern * kStepsPerBeat);
                g.setColour (col.brighter (0.6f));
                const int firstStep = juce::jmax (0, (int) ((0 - r.getX()) / stepPx) - 1);
                for (int k = firstStep; k < totalSteps; ++k)
                {
                    const float x = r.getX() + (float) (k * stepPx);
                    if (x > getWidth()) break;
                    const int s = (int) (((long long) std::llround (k + c.patternOffsetBeats * kStepsPerBeat)) % pat->lengthSteps);
                    for (size_t u = 0; u < used.size(); ++u)
                        if (auto v = pat->get (used[u], s))
                            g.fillRect (x + 0.5f, body.getY() + 2 + rowH * (float) u, juce::jmax (1.5f, (float) stepPx - 1.0f), juce::jmax (1.5f, rowH - 1.0f) * (0.4f + 0.6f * v / 127.0f));
                }
            }
            // divisorias de repeticao
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            const double plen = pat->lengthBeats();
            for (double b = plen - std::fmod (c.patternOffsetBeats, plen); b < c.lengthBeatsPattern - 1e-6; b += plen)
                g.fillRect (r.getX() + (float) (b * owner.pxPerBeat), body.getY(), 1.0f, body.getHeight());
        }

        // titulo
        g.setColour (juce::Colours::black.withAlpha (0.85f));
        g.setFont (Theme::fontFor (c.name, 11.5f, true));
        juce::String title = c.name;
        if (c.kind == ClipKind::Audio)
        {
            if (c.warp) title << "  |  " << juce::String (c.sourceBpm, 1) << " BPM";
            if (std::abs (c.semitones) > 0.001f) title << "  " << (c.semitones > 0 ? "+" : "") << juce::String (c.semitones, 1) << "st";
            if (c.reverse) title << "  REV";
            if (c.gainDb != 0) title << "  " << formatDb (c.gainDb);
        }
        auto tr = header.reduced (5, 0).toNearestInt();
        if (tr.getX() < 2 && tr.getRight() > 60) tr.setLeft (2);
        g.drawText (title, tr, juce::Justification::centredLeft, true);

        g.setColour (sel ? Theme::text : col.darker (0.3f));
        g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, sel ? 1.6f : 1.0f);
    }

    // --------------------- automacao ---------------------
    std::vector<AutoPoint>* autoPoints (int ti)
    {
        auto& t = project().tracks[(size_t) ti];
        if (t.autoView == AutoParam::Volume) return &t.volAuto;
        if (t.autoView == AutoParam::Filter) return &t.filterAuto;
        return nullptr;
    }
    juce::Rectangle<float> autoArea (int ti) const
    {
        auto& t = owner.session.project.tracks[(size_t) ti];
        return juce::Rectangle<float> (0, (float) owner.trackTop (ti) + 6, (float) getWidth(), t.height - 13.0f);
    }
    float valueToY (int ti, float v) const
    {
        auto a = autoArea (ti);
        const bool vol = owner.session.project.tracks[(size_t) ti].autoView == AutoParam::Volume;
        const float norm = vol ? v : (v + 1.0f) * 0.5f;
        return a.getBottom() - norm * a.getHeight();
    }
    float yToValue (int ti, float y) const
    {
        auto a = autoArea (ti);
        const float norm = juce::jlimit (0.0f, 1.0f, (a.getBottom() - y) / a.getHeight());
        const bool vol = owner.session.project.tracks[(size_t) ti].autoView == AutoParam::Volume;
        return vol ? norm : norm * 2.0f - 1.0f;
    }

    void drawAutomation (juce::Graphics& g, int ti)
    {
        auto& t = project().tracks[(size_t) ti];
        auto* pts = autoPoints (ti);
        auto area = autoArea (ti);
        const bool vol = t.autoView == AutoParam::Volume;
        auto col = vol ? Theme::good : Theme::accent2;
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.fillRect (area.expanded (0, 5));
        g.setColour (col.withAlpha (0.6f));
        g.setFont (Theme::font (11.0f, true));
        g.drawText (vol ? TR ("AUTOMAÇÃO: VOLUME") : TR ("AUTOMAÇÃO: FILTRO  (cima = corta graves | meio = neutro | baixo = corta agudos)"),
                    area.withTrimmedTop (area.getHeight() - 12).translated (6, 4).toNearestInt(), juce::Justification::centredLeft);

        std::vector<AutoPoint> sorted = *pts;
        std::sort (sorted.begin(), sorted.end(), [] (auto& a, auto& b) { return a.beat < b.beat; });
        if (sorted.empty())
        {
            const float y = valueToY (ti, vol ? 1.0f : t.filter);
            g.setColour (col.withAlpha (0.5f));
            const float dashes[] = { 4.0f, 4.0f };
            g.drawDashedLine (juce::Line<float> (0, y, (float) getWidth(), y), dashes, 2, 1.5f);
            return;
        }
        juce::Path path;
        path.startNewSubPath (0, valueToY (ti, sorted.front().value));
        for (auto& pt : sorted) path.lineTo (owner.beatToX (pt.beat), valueToY (ti, pt.value));
        path.lineTo ((float) getWidth(), valueToY (ti, sorted.back().value));
        g.setColour (col);
        g.strokePath (path, juce::PathStrokeType (2.0f));
        for (auto& pt : sorted)
        {
            const float x = owner.beatToX (pt.beat), y = valueToY (ti, pt.value);
            g.setColour (Theme::bg);
            g.fillEllipse (x - 5, y - 5, 10, 10);
            g.setColour (col);
            g.drawEllipse (x - 5, y - 5, 10, 10, 2.0f);
        }
    }

    int autoPointAt (int ti, juce::Point<float> pt)
    {
        auto* pts = autoPoints (ti);
        if (pts == nullptr) return -1;
        for (int i = 0; i < (int) pts->size(); ++i)
        {
            const float x = owner.beatToX ((*pts)[(size_t) i].beat), y = valueToY (ti, (*pts)[(size_t) i].value);
            if (pt.getDistanceFrom ({ x, y }) < 8) return i;
        }
        return -1;
    }

    // --------------------- mouse ---------------------
    enum class Drag { None, Move, TrimL, TrimR, FadeIn, FadeOut, Rubber, Auto, Pending };

    void mouseMove (const juce::MouseEvent& e) override
    {
        const int ti = owner.trackAtY (e.y);
        if (ti >= 0 && project().tracks[(size_t) ti].autoView != AutoParam::None)
        {
            setMouseCursor (autoPointAt (ti, e.position) >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::CrosshairCursor);
            return;
        }
        auto h = hitTest (e.position);
        switch (h.zone)
        {
            case Zone::TrimL: case Zone::TrimR: setMouseCursor (juce::MouseCursor::LeftRightResizeCursor); break;
            case Zone::FadeIn: case Zone::FadeOut: setMouseCursor (juce::MouseCursor::PointingHandCursor); break;
            case Zone::Body: setMouseCursor (juce::MouseCursor::DraggingHandCursor); break;
            case Zone::None: default: setMouseCursor (juce::MouseCursor::NormalCursor); break;
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        auto& s = session();
        auto& p = project();
        drag = Drag::None;
        editStarted = false;
        downPos = e.position;
        downBeat = owner.xToBeat (e.position.x);
        const int ti = owner.trackAtY (e.y);
        if (ti >= 0 && s.selectedTrackId != p.tracks[(size_t) ti].id)
        {
            s.selectedTrackId = p.tracks[(size_t) ti].id;
            s.notify (kChSelection);
        }

        // modo automacao
        if (ti >= 0 && p.tracks[(size_t) ti].autoView != AutoParam::None)
        {
            if (e.mods.isPopupMenu()) { showAutoMenu (ti); return; }
            autoTrack = ti;
            autoIndex = autoPointAt (ti, e.position);
            if (autoIndex < 0)
            {
                s.beginEdit (TR ("Ponto de automação"));
                editStarted = true;
                auto* pts = autoPoints (ti);
                pts->push_back ({ juce::jmax (0.0, e.mods.isShiftDown() ? downBeat : owner.snap (downBeat, true)), yToValue (ti, e.position.y) });
                autoIndex = (int) pts->size() - 1;
                s.commit (kChTracks);
            }
            drag = Drag::Auto;
            return;
        }

        auto h = hitTest (e.position);
        if (h.clipId < 0)
        {
            if (e.mods.isPopupMenu()) { showEmptyMenu (ti, downBeat); return; }
            if (! e.mods.isShiftDown() && ! e.mods.isCommandDown())
            {
                if (! s.selectedClips.empty()) { s.selectedClips.clear(); s.notify (kChSelection); }
            }
            rubberBase = s.selectedClips;
            drag = Drag::Rubber;
            return;
        }

        // clique em clipe
        if (e.mods.isShiftDown() || e.mods.isCommandDown())
        {
            if (s.selectedClips.count (h.clipId)) s.selectedClips.erase (h.clipId);
            else s.selectedClips.insert (h.clipId);
        }
        else if (! s.selectedClips.count (h.clipId))
        {
            s.selectedClips = { h.clipId };
        }
        s.notify (kChSelection);
        if (e.mods.isPopupMenu()) { showClipMenu (h.clipId); return; }

        primaryClip = h.clipId;
        originals.clear();
        for (auto& c : p.clips) if (s.selectedClips.count (c.id)) originals[c.id] = c;
        downTrack = ti;
        altCopy = e.mods.isAltDown();
        switch (h.zone)
        {
            case Zone::TrimL: drag = Drag::TrimL; originals = { { h.clipId, *p.findClip (h.clipId) } }; break;
            case Zone::TrimR: drag = Drag::TrimR; originals = { { h.clipId, *p.findClip (h.clipId) } }; break;
            case Zone::FadeIn: drag = Drag::FadeIn; originals = { { h.clipId, *p.findClip (h.clipId) } }; break;
            case Zone::FadeOut: drag = Drag::FadeOut; originals = { { h.clipId, *p.findClip (h.clipId) } }; break;
            default: drag = Drag::Move; break;
        }
    }

    void startEditOnce (const juce::String& label)
    {
        if (editStarted) return;
        session().beginEdit (label);
        editStarted = true;
        if (drag == Drag::Move && altCopy)
        {
            // Alt+arrastar: duplica e move a copia
            auto& p = project();
            std::map<int, Clip> copies;
            session().selectedClips.clear();
            for (auto& [id, c] : originals)
            {
                Clip n = c; n.id = p.newId();
                p.clips.push_back (n);
                copies[n.id] = n;
                session().selectedClips.insert (n.id);
                if (id == primaryClip) primaryClip = n.id;
            }
            originals = copies;
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        auto& p = project();
        auto& s = session();
        const bool free = e.mods.isShiftDown();
        const double beat = owner.xToBeat (e.position.x);
        if (e.getDistanceFromDragStart() < 3 && ! editStarted && drag != Drag::Auto) return;

        if (drag == Drag::Auto)
        {
            auto* pts = autoPoints (autoTrack);
            if (pts == nullptr || autoIndex < 0 || autoIndex >= (int) pts->size()) return;
            if (! editStarted) { s.beginEdit (TR ("Mover automação")); editStarted = true; }
            auto& pt = (*pts)[(size_t) autoIndex];
            pt.beat = juce::jmax (0.0, free ? beat : owner.snap (beat, true));
            pt.value = yToValue (autoTrack, e.position.y);
            s.commit (kChTracks);
            repaint();
            return;
        }
        if (drag == Drag::Rubber)
        {
            rubber = juce::Rectangle<float> (downPos, e.position);
            s.selectedClips = rubberBase;
            for (auto& c : p.clips)
                if (clipRect (c).intersects (rubber)) s.selectedClips.insert (c.id);
            s.notify (kChSelection);
            repaint();
            return;
        }
        if (originals.empty()) return;

        if (drag == Drag::Move)
        {
            startEditOnce (altCopy ? TR ("Copiar clipes") : TR ("Mover clipes"));
            auto& prim = originals[primaryClip];
            double newStart = prim.startBeat + (beat - downBeat);
            if (! free) newStart = owner.snap (newStart);
            double delta = newStart - prim.startBeat;
            double minStart = 1e18;
            for (auto& [id, c] : originals) minStart = std::min (minStart, c.startBeat);
            if (minStart + delta < 0) delta = -minStart;
            const int ti = juce::jlimit (0, (int) p.tracks.size() - 1, owner.trackAtY (juce::jlimit (0, getHeight() - 1, e.y)));
            int dt = (downTrack >= 0 && ti >= 0) ? ti - downTrack : 0;
            // valida a troca de faixa para todos
            for (auto& [id, c] : originals)
            {
                const int from = p.trackIndex (c.trackId);
                const int to = from + dt;
                if (to < 0 || to >= (int) p.tracks.size()
                    || (p.tracks[(size_t) to].type == TrackType::Audio) != (c.kind == ClipKind::Audio)) { dt = 0; break; }
            }
            for (auto& [id, c] : originals)
                if (auto* cl = p.findClip (id))
                {
                    cl->startBeat = c.startBeat + delta;
                    cl->trackId = p.tracks[(size_t) (p.trackIndex (c.trackId) + dt)].id;
                }
            s.commit (kChClips);
            return;
        }

        auto it = originals.begin();
        const Clip& o = it->second;
        auto* cl = p.findClip (it->first);
        if (cl == nullptr) return;
        const double oEnd = p.clipEndBeat (o);
        const double b = free ? beat : owner.snap (beat);

        if (drag == Drag::TrimL)
        {
            startEditOnce (TR ("Aparar clipe"));
            double ns = juce::jlimit (0.0, oEnd - 0.0625, b);
            if (o.kind == ClipKind::Audio)
            {
                const double rate = p.clipRate (o);
                auto src = s.getSource (o.sourceId);
                const double srcLen = src ? src->lengthSec : o.sourceOffsetSec + o.sourceLenSec;
                double dsec = p.beatsToSeconds (ns - o.startBeat) * rate;   // >0 encurta
                if (! o.reverse) dsec = juce::jmax (dsec, -o.sourceOffsetSec);
                else dsec = juce::jmax (dsec, -(srcLen - o.sourceOffsetSec - o.sourceLenSec));
                dsec = juce::jmin (dsec, o.sourceLenSec - 0.005);
                ns = o.startBeat + p.secondsToBeats (dsec / rate);
                cl->startBeat = ns;
                cl->sourceLenSec = o.sourceLenSec - dsec;
                if (! o.reverse) cl->sourceOffsetSec = o.sourceOffsetSec + dsec;
                cl->fadeInBeats = juce::jmin (o.fadeInBeats, p.clipLengthBeats (*cl));
            }
            else
            {
                cl->startBeat = ns;
                cl->lengthBeatsPattern = oEnd - ns;
                cl->patternOffsetBeats = o.patternOffsetBeats + (ns - o.startBeat);
                if (cl->patternOffsetBeats < 0)
                    if (auto* pat = p.findPattern (o.patternId))
                        cl->patternOffsetBeats = std::fmod (cl->patternOffsetBeats, pat->lengthBeats()) + pat->lengthBeats();
            }
        }
        else if (drag == Drag::TrimR)
        {
            startEditOnce (TR ("Aparar clipe"));
            double ne = juce::jmax (o.startBeat + 0.0625, b);
            if (o.kind == ClipKind::Audio)
            {
                const double rate = p.clipRate (o);
                auto src = s.getSource (o.sourceId);
                const double srcLen = src ? src->lengthSec : o.sourceOffsetSec + o.sourceLenSec;
                double newLen = p.beatsToSeconds (ne - o.startBeat) * rate;
                if (! o.reverse)
                {
                    newLen = juce::jmin (newLen, srcLen - o.sourceOffsetSec);
                    cl->sourceLenSec = newLen;
                }
                else
                {
                    newLen = juce::jmin (newLen, o.sourceOffsetSec + o.sourceLenSec);
                    cl->sourceOffsetSec = o.sourceOffsetSec + o.sourceLenSec - newLen;
                    cl->sourceLenSec = newLen;
                }
                cl->fadeOutBeats = juce::jmin (o.fadeOutBeats, p.clipLengthBeats (*cl));
            }
            else cl->lengthBeatsPattern = ne - o.startBeat;
        }
        else if (drag == Drag::FadeIn)
        {
            startEditOnce (TR ("Fade"));
            const double len = p.clipLengthBeats (o);
            cl->fadeInBeats = juce::jlimit (0.0, len - o.fadeOutBeats, (free ? beat : owner.snap (beat, true)) - o.startBeat);
        }
        else if (drag == Drag::FadeOut)
        {
            startEditOnce (TR ("Fade"));
            const double len = p.clipLengthBeats (o);
            cl->fadeOutBeats = juce::jlimit (0.0, len - o.fadeInBeats, oEnd - (free ? beat : owner.snap (beat, true)));
        }
        s.commit (kChClips);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        auto& s = session();
        if (drag == Drag::Auto)
        {
            if (auto* pts = autoPoints (autoTrack))
                std::sort (pts->begin(), pts->end(), [] (auto& a, auto& b) { return a.beat < b.beat; });
            s.commit (kChTracks);
        }
        if (drag == Drag::Rubber && rubber.isEmpty() && e.getDistanceFromDragStart() < 3 && ! e.mods.isPopupMenu())
            s.seek (juce::jmax (0.0, owner.snap (downBeat)));   // clique simples no vazio: move o cursor
        rubber = {};
        drag = Drag::None;
        originals.clear();
        repaint();
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        auto& s = session();
        auto& p = project();
        const int ti = owner.trackAtY (e.y);
        if (ti >= 0 && p.tracks[(size_t) ti].autoView != AutoParam::None)
        {
            const int idx = autoPointAt (ti, e.position);
            if (idx >= 0)
            {
                // o primeiro clique ja pode ter criado um ponto: remove este
                s.beginEdit (TR ("Apagar ponto"));
                auto* pts = autoPoints (ti);
                pts->erase (pts->begin() + idx);
                s.commit (kChTracks);
            }
            return;
        }
        auto h = hitTest (e.position);
        if (h.clipId >= 0)
        {
            auto* c = p.findClip (h.clipId);
            if (c && c->kind == ClipKind::Audio && owner.onOpenInEditor)
                owner.onOpenInEditor (c->sourceId, c->sourceOffsetSec, c->sourceOffsetSec + c->sourceLenSec);
            else if (c && c->kind == ClipKind::Pattern)
            {
                p.currentPatternId = c->patternId;
                s.commit (kChPatterns);
                if (owner.onShowSequencer) owner.onShowSequencer();
            }
            return;
        }
        if (ti >= 0 && p.tracks[(size_t) ti].type == TrackType::Pattern)
        {
            s.beginEdit (TR ("Inserir padrão"));
            const double b = juce::jmax (0.0, std::floor (owner.xToBeat (e.position.x) / p.beatsPerBar) * p.beatsPerBar);
            const int id = s.addPatternClip (p.tracks[(size_t) ti].id, p.currentPatternId, b);
            s.selectedClips = { id };
            s.commit (kChClips | kChSelection);
        }
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (e.mods.isCommandDown() || e.mods.isAltDown())
            owner.zoomBy (w.deltaY > 0 ? 1.2 : 1.0 / 1.2, e.position.x);
        else if (e.mods.isShiftDown() || std::abs (w.deltaX) > std::abs (w.deltaY))
        {
            const float d = std::abs (w.deltaX) > std::abs (w.deltaY) ? w.deltaX : w.deltaY;
            owner.scrollTarget = -1;
            owner.scrollBeats = juce::jmax (0.0, owner.scrollBeats - d * 300.0 / owner.pxPerBeat);
            owner.followPlayhead = false;
            owner.updateScrollbars();
            owner.repaint();
        }
        else
        {
            owner.scrollY = juce::jlimit (0, juce::jmax (0, owner.totalTracksHeight() - getHeight() + 40), owner.scrollY - (int) (w.deltaY * 200));
            owner.updateScrollbars();
            owner.layoutHeaders();
            repaint();
        }
    }

    // --------------------- menus ---------------------
    void showAutoMenu (int ti)
    {
        juce::PopupMenu m;
        m.addItem (1, TR ("Limpar esta automação"));
        m.addItem (2, TR ("Ocultar automação"));
        auto id = project().tracks[(size_t) ti].id;
        m.showMenuAsync (juce::PopupMenu::Options(), [this, id] (int r)
        {
            auto* t = project().findTrack (id);
            if (t == nullptr) return;
            if (r == 1) { session().beginEdit (TR ("Limpar automação")); (t->autoView == AutoParam::Volume ? t->volAuto : t->filterAuto).clear(); session().commit (kChTracks); }
            if (r == 2) { t->autoView = AutoParam::None; session().commit (kChTracks); }
        });
    }

    void showEmptyMenu (int ti, double beat)
    {
        auto& p = project();
        juce::PopupMenu m;
        const bool isPat = ti >= 0 && p.tracks[(size_t) ti].type == TrackType::Pattern;
        m.addItem (1, TR ("Colar aqui (Ctrl+V)"), session().hasClipboard());
        if (isPat) m.addItem (2, TR ("Inserir padrão atual aqui"));
        m.addItem (3, TR ("Mover cursor para cá"));
        m.addSeparator();
        m.addItem (4, TR ("+ Faixa de áudio"));
        m.addItem (5, TR ("+ Faixa de padrões"));
        const int tid = ti >= 0 ? p.tracks[(size_t) ti].id : -1;
        m.showMenuAsync (juce::PopupMenu::Options(), [this, tid, beat] (int r)
        {
            auto& s = session();
            const double b = juce::jmax (0.0, owner.snap (beat));
            if (r == 1) s.paste (b, tid);
            if (r == 2) { s.beginEdit (TR ("Inserir padrão")); s.selectedClips = { s.addPatternClip (tid, s.project.currentPatternId, b) }; s.commit (kChClips); }
            if (r == 3) s.seek (b);
            if (r == 4) s.addTrack (TrackType::Audio);
            if (r == 5) s.addTrack (TrackType::Pattern);
        });
    }

    void showClipMenu (int clipId)
    {
        auto& p = project();
        auto* c = p.findClip (clipId);
        if (c == nullptr) return;
        const bool audio = c->kind == ClipKind::Audio;
        juce::PopupMenu m, colours;
        m.addItem (1, TR ("Cortar no cursor  (Ctrl+E)"));
        m.addItem (2, TR ("Duplicar  (Ctrl+D)"));
        m.addItem (3, TR ("Copiar  (Ctrl+C)"));
        m.addItem (4, TR ("Apagar  (Del)"));
        m.addItem (5, c->muted ? TR ("Ativar clipe") : TR ("Silenciar clipe"));
        m.addItem (6, TR ("Renomear..."));
        for (int i = 0; i < 12; ++i) colours.addColouredItem (100 + i, TR ("Cor ") + juce::String (i + 1), Project::trackColour (i), true);
        m.addSubMenu (TR ("Cor"), colours);
        m.addSeparator();
        if (audio)
        {
            m.addItem (10, TR ("Abrir no Editor (fatiar / pads)"));
            m.addItem (11, TR ("Mandar trecho para o pad selecionado"));
            m.addItem (12, TR ("Normalizar volume"));
            m.addItem (13, TR ("Reverso"), true, c->reverse);
            m.addItem (14, TR ("Sincronizar ao BPM (warp)"), true, c->warp);
            m.addItem (15, TR ("Manter tom ao mudar velocidade"), true, c->keepPitch);
            m.addItem (16, TR ("BPM original x2"));
            m.addItem (17, TR ("BPM original / 2"));
            m.addItem (18, TR ("Fade in/out suave (1 batida)"));
            m.addItem (19, TR ("Definir loop neste clipe"));
            m.addItem (20, TR ("Alinhar início na primeira batida (pula a introdução)"));
        }
        else
        {
            m.addItem (30, TR ("Editar padrão no sequenciador"));
            m.addItem (31, TR ("Tornar padrão único (cópia)"));
        }
        m.showMenuAsync (juce::PopupMenu::Options(), [this, clipId] (int r) { clipMenuResult (clipId, r); });
    }

    void clipMenuResult (int clipId, int r)
    {
        auto& s = session();
        auto& p = project();
        auto* c = p.findClip (clipId);
        if (c == nullptr || r == 0) return;
        switch (r)
        {
            case 1: s.splitAt (s.engine.getPositionBeats()); break;
            case 2: s.duplicateSelectedClips(); break;
            case 3: s.copySelection(); break;
            case 4: s.deleteSelectedClips(); break;
            case 5: s.beginEdit (TR ("Silenciar clipe")); c->muted = ! c->muted; s.commit (kChClips); break;
            case 6:
            {
                auto* aw = new juce::AlertWindow (TR ("Renomear clipe"), TR ("Nome:"), juce::MessageBoxIconType::NoIcon);
                aw->addTextEditor ("name", c->name);
                aw->addButton (TR ("OK"), 1, juce::KeyPress (juce::KeyPress::returnKey));
                aw->addButton (TR ("Cancelar"), 0, juce::KeyPress (juce::KeyPress::escapeKey));
                aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw, clipId] (int res)
                {
                    if (res == 1)
                        if (auto* cl = project().findClip (clipId))
                        {
                            session().beginEdit (TR ("Renomear clipe"));
                            cl->name = aw->getTextEditorContents ("name");
                            session().commit (kChClips);
                        }
                }), true);
                break;
            }
            case 10: if (owner.onOpenInEditor) owner.onOpenInEditor (c->sourceId, c->sourceOffsetSec, c->sourceOffsetSec + c->sourceLenSec); break;
            case 11:
            {
                s.assignPad (s.selectedPad, c->sourceId, c->sourceOffsetSec, c->sourceOffsetSec + c->sourceLenSec, c->name.substring (0, 14));
                if (auto* pd = &p.pads[(size_t) s.selectedPad]) { pd->reverse = c->reverse; s.commit (kChPads); }
                s.setStatus (TR ("Trecho enviado para o pad ") + p.pads[(size_t) s.selectedPad].name);
                break;
            }
            case 12: s.normaliseClip (clipId); break;
            case 13: s.beginEdit (TR ("Reverso")); c->reverse = ! c->reverse; s.commit (kChClips); break;
            case 14: s.beginEdit (TR ("Warp")); c->warp = ! c->warp; s.commit (kChClips); break;
            case 15: s.beginEdit (TR ("Manter tom")); c->keepPitch = ! c->keepPitch; s.commit (kChClips); break;
            case 16: s.beginEdit (TR ("BPM original")); c->sourceBpm *= 2; s.commit (kChClips); break;
            case 17: s.beginEdit (TR ("BPM original")); c->sourceBpm *= 0.5; s.commit (kChClips); break;
            case 18:
            {
                s.beginEdit (TR ("Fades"));
                const double len = p.clipLengthBeats (*c);
                c->fadeInBeats = juce::jmin (1.0, len * 0.3);
                c->fadeOutBeats = juce::jmin (1.0, len * 0.3);
                s.commit (kChClips);
                break;
            }
            case 19: s.beginEdit (TR ("Loop")); p.loopStart = c->startBeat; p.loopEnd = p.clipEndBeat (*c); p.loopOn = true; s.commit (kChTransport); break;
            case 20:
            {
                if (auto src = s.getSource (c->sourceId))
                {
                    const double spb = 60.0 / juce::jmax (20.0, c->sourceBpm);
                    double t = src->beatOffsetSec;
                    while (t < c->sourceOffsetSec - 1e-6) t += spb;
                    const double cut = t - c->sourceOffsetSec;
                    if (cut > 0.001 && cut < c->sourceLenSec - 0.05)
                    {
                        s.beginEdit (TR ("Alinhar na batida"));
                        c->sourceOffsetSec += cut;
                        c->sourceLenSec -= cut;
                        s.commit (kChClips);
                        s.setStatus (TR ("Início alinhado na batida (") + juce::String (cut, 3) + TR (" s removidos)"));
                    }
                }
                break;
            }
            case 30: p.currentPatternId = c->patternId; s.commit (kChPatterns); if (owner.onShowSequencer) owner.onShowSequencer(); break;
            case 31:
            {
                if (auto* pat = p.findPattern (c->patternId))
                {
                    s.beginEdit (TR ("Padrão único"));
                    Pattern n = *pat;
                    n.id = p.newId();
                    n.name = pat->name + TR (" (cópia)");
                    p.patterns.push_back (n);
                    if (auto* cc = p.findClip (clipId)) { cc->patternId = n.id; cc->name = n.name; }
                    p.currentPatternId = n.id;
                    s.commit (kChPatterns | kChClips);
                }
                break;
            }
            default:
                if (r >= 100 && r < 112)
                {
                    s.beginEdit (TR ("Cor"));
                    for (auto& cl : p.clips) if (s.selectedClips.count (cl.id)) cl.colour = Project::trackColour (r - 100);
                    s.commit (kChClips);
                }
                break;
        }
    }

    ArrangeView& owner;
    Drag drag = Drag::None;
    bool editStarted = false, altCopy = false;
    juce::Point<float> downPos;
    double downBeat = 0;
    int downTrack = -1, primaryClip = -1;
    std::map<int, Clip> originals;
    juce::Rectangle<float> rubber;
    std::set<int> rubberBase;
    int autoTrack = -1, autoIndex = -1;
};

// ============================================================================
//  Inspetor do clipe selecionado
// ============================================================================
class ArrangeView::Inspector : public juce::Component
{
public:
    explicit Inspector (ArrangeView& o) : owner (o)
    {
        gain.formatter = [] (double v) { return formatDb ((float) v); };
        fadeIn.formatter = fadeOut.formatter = [] (double v) { return juce::String (v, 2) + " bt"; };
        pitch.formatter = [] (double v) { return (v > 0 ? "+" : "") + juce::String (v, 2) + " st"; };
        fadeIn.setSkew (2.0); fadeOut.setSkew (2.0);
        for (auto* k : { &gain, &fadeIn, &fadeOut, &pitch })
        {
            addAndMakeVisible (*k);
            k->onStart = [this] { owner.session.beginEdit (TR ("Editar clipe")); };
        }
        gain.onChange = [this] (double v) { apply ([v] (Clip& c) { c.gainDb = (float) v; }); };
        fadeIn.onChange = [this] (double v) { apply ([this, v] (Clip& c) { c.fadeInBeats = juce::jmin (v, owner.session.project.clipLengthBeats (c) - c.fadeOutBeats); }); };
        fadeOut.onChange = [this] (double v) { apply ([this, v] (Clip& c) { c.fadeOutBeats = juce::jmin (v, owner.session.project.clipLengthBeats (c) - c.fadeInBeats); }); };
        pitch.onChange = [this] (double v) { apply ([v] (Clip& c) { c.semitones = (float) v; }); };

        for (auto* b : { &warpBtn, &keepBtn, &revBtn, &x2Btn, &half, &normBtn, &editBtn, &tapAlign })
            addAndMakeVisible (*b);
        styleToggle (warpBtn, Theme::accent2);
        styleToggle (keepBtn, Theme::accent2);
        styleToggle (revBtn, Theme::accent);
        warpBtn.setTooltip (TR ("Sincroniza o clipe com o BPM do projeto (time-stretch de alta qualidade)"));
        keepBtn.setTooltip (TR ("Ligado: muda a velocidade sem mudar o tom.  Desligado: estilo vinil (velocidade e tom juntos)"));
        revBtn.setTooltip (TR ("Toca o trecho de tras para frente"));
        tapAlign.setTooltip (TR ("Usa o BPM atual do projeto como BPM original deste clipe"));
        warpBtn.onClick = [this] { toggle (TR ("Warp"), [] (Clip& c) { c.warp = ! c.warp; }); };
        keepBtn.onClick = [this] { toggle (TR ("Manter tom"), [] (Clip& c) { c.keepPitch = ! c.keepPitch; }); };
        revBtn.onClick = [this] { toggle (TR ("Reverso"), [] (Clip& c) { c.reverse = ! c.reverse; }); };
        x2Btn.onClick = [this] { toggle (TR ("BPM original"), [] (Clip& c) { c.sourceBpm *= 2; }); };
        half.onClick = [this] { toggle (TR ("BPM original"), [] (Clip& c) { c.sourceBpm *= 0.5; }); };
        tapAlign.onClick = [this] { toggle (TR ("BPM original"), [this] (Clip& c) { c.sourceBpm = owner.session.project.bpm; }); };
        normBtn.onClick = [this] { if (auto* c = clip()) owner.session.normaliseClip (c->id); };
        editBtn.onClick = [this]
        {
            if (auto* c = clip())
            {
                if (c->kind == ClipKind::Audio && owner.onOpenInEditor)
                    owner.onOpenInEditor (c->sourceId, c->sourceOffsetSec, c->sourceOffsetSec + c->sourceLenSec);
                else if (c->kind == ClipKind::Pattern)
                {
                    owner.session.project.currentPatternId = c->patternId;
                    owner.session.commit (kChPatterns);
                    if (owner.onShowSequencer) owner.onShowSequencer();
                }
            }
        };

        bpmLabel.setEditable (false, true, false);
        bpmLabel.setJustificationType (juce::Justification::centred);
        bpmLabel.setColour (juce::Label::backgroundColourId, Theme::panel2);
        bpmLabel.setColour (juce::Label::outlineColourId, Theme::outline);
        bpmLabel.setFont (Theme::mono (14.0f, true));
        bpmLabel.setTooltip (TR ("BPM original do trecho (duplo clique para digitar)"));
        bpmLabel.onTextChange = [this]
        {
            const double v = bpmLabel.getText().retainCharacters ("0123456789.,").replaceCharacter (',', '.').getDoubleValue();
            if (v >= 30 && v <= 300) toggle (TR ("BPM original"), [v] (Clip& c) { c.sourceBpm = v; });
            else refresh();
        };
        addAndMakeVisible (bpmLabel);
        refresh();
    }

    Clip* clip()
    {
        auto& s = owner.session;
        if (s.selectedClips.size() != 1) return nullptr;
        return s.project.findClip (*s.selectedClips.begin());
    }

    void apply (std::function<void (Clip&)> fn)
    {
        auto& s = owner.session;
        for (auto& c : s.project.clips) if (s.selectedClips.count (c.id) && c.kind == ClipKind::Audio) fn (c);
        s.commit (kChClips);
    }
    void toggle (const juce::String& label, std::function<void (Clip&)> fn)
    {
        owner.session.beginEdit (label);
        apply (fn);
        refresh();
    }

    void refresh()
    {
        auto& s = owner.session;
        Clip* c = nullptr;
        for (auto& cl : s.project.clips) if (s.selectedClips.count (cl.id) && cl.kind == ClipKind::Audio) { c = &cl; break; }
        const bool hasAudio = c != nullptr;
        for (juce::Component* k : std::initializer_list<juce::Component*> { &gain, &fadeIn, &fadeOut, &pitch, &warpBtn, &keepBtn, &revBtn, &x2Btn, &half, &normBtn, &bpmLabel, &tapAlign })
            k->setVisible (hasAudio);
        auto* single = clip();
        editBtn.setVisible (single != nullptr);
        editBtn.setButtonText (single && single->kind == ClipKind::Pattern ? TR ("Editar padrão") : TR ("Editor / Fatiar"));
        if (hasAudio)
        {
            gain.setValue (c->gainDb);
            fadeIn.setValue (c->fadeInBeats);
            fadeOut.setValue (c->fadeOutBeats);
            pitch.setValue (c->semitones);
            warpBtn.setToggleState (c->warp, juce::dontSendNotification);
            keepBtn.setToggleState (c->keepPitch, juce::dontSendNotification);
            revBtn.setToggleState (c->reverse, juce::dontSendNotification);
            bpmLabel.setText (juce::String (c->sourceBpm, 2), juce::dontSendNotification);
        }
        repaint();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8, 2);
        r.removeFromLeft (256);
        auto knobW = 58;
        for (auto* k : { &gain, &fadeIn, &fadeOut, &pitch }) { k->setBounds (r.removeFromLeft (knobW)); r.removeFromLeft (2); }
        r.removeFromLeft (8);
        auto col1 = r.removeFromLeft (170);
        auto top = col1.removeFromTop (col1.getHeight() / 2).reduced (0, 2);
        auto bot = col1.reduced (0, 2);
        warpBtn.setBounds (top.removeFromLeft (60)); top.removeFromLeft (4);
        bpmLabel.setBounds (top);
        x2Btn.setBounds (bot.removeFromLeft (34)); bot.removeFromLeft (3);
        half.setBounds (bot.removeFromLeft (34)); bot.removeFromLeft (3);
        tapAlign.setBounds (bot);
        r.removeFromLeft (8);
        auto col2 = r.removeFromLeft (124);
        keepBtn.setBounds (col2.removeFromTop (col2.getHeight() / 2).reduced (0, 2));
        revBtn.setBounds (col2.reduced (0, 2));
        r.removeFromLeft (8);
        auto col3 = r.removeFromLeft (136);
        normBtn.setBounds (col3.removeFromTop (col3.getHeight() / 2).reduced (0, 2));
        editBtn.setBounds (col3.reduced (0, 2));
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Theme::panel);
        g.setColour (Theme::outline);
        g.drawHorizontalLine (getHeight() - 1, 0, (float) getWidth());
        auto r = getLocalBounds().reduced (12, 6).removeFromLeft (244);
        auto& s = owner.session;
        g.setFont (Theme::font (11.0f, true));
        g.setColour (Theme::textFaint);
        g.drawText (TR ("INSPETOR DE CLIPE"), r.removeFromTop (16), juce::Justification::centredLeft);
        g.setFont (Theme::font (14.0f, true));
        if (s.selectedClips.empty())
        {
            g.setColour (Theme::textDim);
            g.setFont (Theme::font (12.0f));
            g.drawFittedText (TR ("Selecione um clipe para editar.   Arraste as bordas para aparar, as bolinhas do topo para fades.   Alt+arrastar copia."),
                              r.withWidth (getWidth() - 24).withTrimmedTop (4), juce::Justification::topLeft, 2, 1.0f);
            return;
        }
        if (auto* c = clip())
        {
            g.setColour (c->colour);
            g.setFont (Theme::fontFor (c->name, 14.0f, true));
            g.drawFittedText (c->name, r.removeFromTop (18), juce::Justification::centredLeft, 1, 1.0f);
            g.setColour (Theme::textDim);
            g.setFont (Theme::font (11.5f));
            auto& p = s.project;
            juce::String info;
            const double lenB = p.clipLengthBeats (*c);
            info << juce::String (lenB / p.beatsPerBar, 2) << TR (" compassos  |  ") << juce::String (p.beatsToSeconds (lenB), 2) << " s";
            if (c->kind == ClipKind::Audio && c->warp) info << "  |  x" << juce::String (p.clipRate (*c), 3);
            g.drawText (info, r.removeFromTop (16), juce::Justification::centredLeft);
        }
        else
        {
            g.setColour (Theme::text);
            g.drawText (juce::String ((int) s.selectedClips.size()) + TR (" clipes selecionados"), r.removeFromTop (18), juce::Justification::centredLeft);
        }
    }

    ArrangeView& owner;
    Knob gain { TR ("Ganho"), -30.0, 12.0, 0.0, 0.1 };
    Knob fadeIn { TR ("Fade in"), 0.0, 32.0, 0.0, 0.0 };
    Knob fadeOut { TR ("Fade out"), 0.0, 32.0, 0.0, 0.0 };
    Knob pitch { TR ("Tom"), -12.0, 12.0, 0.0, 0.01 };
    juce::TextButton warpBtn { TR ("Warp") }, keepBtn { TR ("Manter tom") }, revBtn { TR ("Reverso") }, x2Btn { "x2" }, half { "/2" },
                     normBtn { TR ("Normalizar") }, editBtn { TR ("Editor / Fatiar") }, tapAlign { TR ("= Projeto") };
    juce::Label bpmLabel;
};

// ============================================================================
ArrangeView::ArrangeView (Session& s) : session (s)
{
    ruler = std::make_unique<Ruler> (*this);
    lanes = std::make_unique<Lanes> (*this);
    inspector = std::make_unique<Inspector> (*this);
    playheadLine = std::make_unique<PlayheadLine>();
    addAndMakeVisible (*inspector);
    addAndMakeVisible (*ruler);
    addAndMakeVisible (headerHolder);
    addAndMakeVisible (*lanes);
    lanes->addAndMakeVisible (*playheadLine);
    addAndMakeVisible (hScroll);
    addAndMakeVisible (vScroll);
    hScroll.addListener (this);
    vScroll.addListener (this);
    hScroll.setAutoHide (false);
    vScroll.setAutoHide (false);
    addAndMakeVisible (addAudioBtn);
    addAndMakeVisible (addPatBtn);
    addAudioBtn.onClick = [this] { session.addTrack (TrackType::Audio); };
    addPatBtn.onClick = [this] { session.addTrack (TrackType::Pattern); };
    addAudioBtn.setTooltip (TR ("Nova faixa de áudio (músicas, trechos, vocais)"));
    addPatBtn.setTooltip (TR ("Nova faixa de padrões (batidas feitas com os pads)"));
    snapBox.addItemList ({ TR ("Livre"), "1/32", "1/16", "1/8", TR ("1 tempo"), TR ("2 tempos"), TR ("Compasso") }, 1);
    snapBox.setSelectedId (3, juce::dontSendNotification);
    snapBox.setTooltip (TR ("Grade de encaixe (segure Shift ao arrastar para mover livremente)"));
    snapBox.onChange = [this]
    {
        static const double vals[] = { 0.0, 0.125, 0.25, 0.5, 1.0, 2.0, 4.0 };
        const int i = snapBox.getSelectedId() - 1;
        session.snapBeats = (i == 6) ? (double) session.project.beatsPerBar : vals[juce::jlimit (0, 6, i)];
        lanes->repaint();
    };
    fitBtn.setTooltip (TR ("Ajustar o zoom para ver a música inteira"));
    fitBtn.onClick = [this] { zoomToFit(); };
    followBtn.setTooltip (TR ("Rolar a tela acompanhando o cursor durante a reprodução"));
    followBtn.onColour = Theme::accent2;
    fitBtn.flat = followBtn.flat = true;
    followBtn.setToggleState (true, juce::dontSendNotification);
    followBtn.onClick = [this] { followPlayhead = ! followPlayhead; followBtn.setToggleState (followPlayhead, juce::dontSendNotification); };
    for (auto* c : std::initializer_list<juce::Component*> { &snapBox, &fitBtn, &followBtn }) addAndMakeVisible (c);

    session.addListener (this);
    rebuildHeaders();
    startTimerHz (60);
}

ArrangeView::~ArrangeView()
{
    session.removeListener (this);
}

double ArrangeView::xToBeat (float x) const { return scrollBeats + x / pxPerBeat; }
float ArrangeView::beatToX (double b) const { return (float) ((b - scrollBeats) * pxPerBeat); }

double ArrangeView::snap (double beat, bool force) const
{
    double g = session.snapBeats;
    if (g <= 0) return force ? std::round (beat * 16.0) / 16.0 : beat;
    return std::round (beat / g) * g;
}

int ArrangeView::totalTracksHeight() const
{
    int h = 0;
    for (auto& t : session.project.tracks) h += (int) t.height;
    return h;
}

int ArrangeView::trackTop (int index) const
{
    int y = -scrollY;
    for (int i = 0; i < index && i < (int) session.project.tracks.size(); ++i) y += (int) session.project.tracks[(size_t) i].height;
    return y;
}

int ArrangeView::trackAtY (int y) const
{
    int top = -scrollY;
    for (int i = 0; i < (int) session.project.tracks.size(); ++i)
    {
        const int h = (int) session.project.tracks[(size_t) i].height;
        if (y >= top && y < top + h) return i;
        top += h;
    }
    return -1;
}

void ArrangeView::zoomBy (double factor, float anchorX)
{
    scrollTarget = -1; zoomTarget = -1;
    const double anchorBeat = xToBeat (anchorX);
    pxPerBeat = juce::jlimit (1.5, 600.0, pxPerBeat * factor);
    scrollBeats = juce::jmax (0.0, anchorBeat - anchorX / pxPerBeat);
    updateScrollbars();
    repaint();
}

void ArrangeView::zoomToFit()
{
    const double end = juce::jmax (16.0, session.project.songEndBeat() + 4.0);
    const double target = juce::jlimit (1.5, 600.0, (lanes->getWidth() - 20) / end);
    if (! isShowing() || lanes->getWidth() <= 20)
    {
        pxPerBeat = target; scrollBeats = 0;
        updateScrollbars();
        repaint();
        return;
    }
    // zoom animado (transicao suave)
    zoomTarget = target;
    scrollTarget = 0.0;
}

void ArrangeView::rebuildHeaders()
{
    headers.clear();
    for (auto& t : session.project.tracks)
    {
        auto* h = headers.add (new TrackHeader (*this, t.id));
        headerHolder.addAndMakeVisible (h);
    }
    disableFocusRecursively (headerHolder);
    layoutHeaders();
}

void ArrangeView::layoutHeaders()
{
    auto& p = session.project;
    for (int i = 0; i < headers.size() && i < (int) p.tracks.size(); ++i)
        headers[i]->setBounds (0, trackTop (i), headerW, (int) p.tracks[(size_t) i].height);
}

void ArrangeView::updateScrollbars()
{
    const double end = juce::jmax (session.project.songEndBeat() + 32.0, scrollBeats + lanes->getWidth() / pxPerBeat);
    hScroll.setRangeLimits (0.0, end, juce::dontSendNotification);
    hScroll.setCurrentRange (scrollBeats, lanes->getWidth() / pxPerBeat, juce::dontSendNotification);
    const int total = totalTracksHeight() + 40;
    vScroll.setRangeLimits (0.0, (double) juce::jmax (total, lanes->getHeight()), juce::dontSendNotification);
    vScroll.setCurrentRange (scrollY, lanes->getHeight(), juce::dontSendNotification);
}

void ArrangeView::scrollBarMoved (juce::ScrollBar* sb, double start)
{
    if (sb == &hScroll) { scrollTarget = -1; scrollBeats = start; followPlayhead = false; followBtn.setToggleState (false, juce::dontSendNotification); }
    else { scrollY = (int) start; layoutHeaders(); }
    repaint();
}

void ArrangeView::resized()
{
    auto r = getLocalBounds();
    inspector->setBounds (r.removeFromTop (inspectorH));
    auto top = r.removeFromTop (rulerH);
    {
        auto corner = top.removeFromLeft (headerW).reduced (4, 3);
        followBtn.setBounds (corner.removeFromRight (corner.getHeight() + 4));
        corner.removeFromRight (2);
        fitBtn.setBounds (corner.removeFromRight (corner.getHeight() + 4));
        corner.removeFromRight (4);
        snapBox.setBounds (corner);
    }
    top.removeFromRight (10);
    ruler->setBounds (top);
    auto bottom = r.removeFromBottom (30);
    auto btns = bottom.removeFromLeft (headerW).reduced (3, 2);
    addAudioBtn.setBounds (btns.removeFromLeft (btns.getWidth() / 2).reduced (1, 0));
    addPatBtn.setBounds (btns.reduced (1, 0));
    hScroll.setBounds (bottom.withTrimmedRight (10).reduced (0, 7));
    vScroll.setBounds (r.removeFromRight (10));
    headerHolder.setBounds (r.removeFromLeft (headerW));
    lanes->setBounds (r);
    playheadLine->setSize (5, lanes->getHeight());
    layoutHeaders();
    updateScrollbars();
}

void ArrangeView::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel);
    auto corner = juce::Rectangle<int> (0, inspectorH, headerW, rulerH);
    g.setColour (Theme::outline);
    g.drawHorizontalLine (corner.getBottom() - 1, 0, (float) headerW);
}

void ArrangeView::sessionChanged (int what)
{
    if (what & kChTracks)
    {
        if (headers.size() != (int) session.project.tracks.size()) rebuildHeaders();
        else
        {
            bool same = true;
            for (int i = 0; i < headers.size(); ++i) same &= headers[i]->trackId == session.project.tracks[(size_t) i].id;
            if (! same) rebuildHeaders();
        }
        layoutHeaders();
    }
    if (what & (kChTracks | kChMixer | kChSelection))
        for (auto* h : headers) h->refresh();
    if (what & (kChClips | kChSelection | kChTracks | kChPatterns))
        inspector->refresh();
    updateScrollbars();
    lanes->repaint();
    ruler->repaint();
}

void ArrangeView::timerCallback()
{
    if (! isShowing()) return;
    const double pos = session.engine.getPositionBeats();
    const bool playing = session.engine.isPlaying();
    if (followPlayhead && playing && session.songMode && scrollTarget < 0)
    {
        const float x = beatToX (pos);
        const float w = (float) lanes->getWidth();
        if (x > w - 60 || x < 0)
            scrollTarget = juce::jmax (0.0, pos - 2.0);   // vira a "pagina" com animacao
    }
    if (zoomTarget > 0)
    {
        const double ratio = zoomTarget / pxPerBeat;
        if (std::abs (ratio - 1.0) < 0.004) { pxPerBeat = zoomTarget; zoomTarget = -1; }
        else pxPerBeat *= std::pow (ratio, 0.25);
        updateScrollbars();
        lanes->repaint();
        ruler->repaint();
    }
    if (scrollTarget >= 0)
    {
        // rolagem suave ate o alvo (~200 ms)
        const double d = scrollTarget - scrollBeats;
        if (std::abs (d) * pxPerBeat < 0.75) { scrollBeats = scrollTarget; scrollTarget = -1; }
        else scrollBeats += d * 0.22;
        updateScrollbars();
        lanes->repaint();
        ruler->repaint();
    }
    const int x = (int) std::round (beatToX (pos));
    playheadLine->setVisible (session.songMode);
    if (x != lastPlayheadX || playheadLine->getHeight() != lanes->getHeight())
    {
        // o cursor e um componente proprio: so a faixinha dele e redesenhada
        playheadLine->setBounds (x - 2, 0, 5, lanes->getHeight());
        if (lastPlayheadX != x) ruler->repaint (juce::Rectangle<int> (juce::jmin (x, lastPlayheadX) - 10, 0, std::abs (x - lastPlayheadX) + 20, ruler->getHeight()));
        lastPlayheadX = x;
    }
}

// --------------------------- arrastar e soltar ------------------------------
void ArrangeView::fileDragMove (const juce::StringArray&, int x, int y)
{
    auto p = lanes->getLocalPoint (this, juce::Point<int> (x, y));
    const int ti = trackAtY (p.y);
    if (ti >= 0)
    {
        const double b = juce::jmax (0.0, snap (xToBeat ((float) p.x)));
        dropHighlight = { (int) beatToX (b), trackTop (ti), 160, (int) session.project.tracks[(size_t) ti].height };
    }
    else dropHighlight = {};
    lanes->repaint();
}
void ArrangeView::fileDragExit (const juce::StringArray&) { dropHighlight = {}; lanes->repaint(); }

void ArrangeView::filesDropped (const juce::StringArray& files, int x, int y)
{
    dropHighlight = {};
    int offsetTrack = 0;
    for (auto& f : files)
    {
        juce::File file (f);
        if (! file.existsAsFile()) continue;
        dropFileAt (file, { x, y + offsetTrack });
        offsetTrack += 0;
    }
    lanes->repaint();
}

void ArrangeView::itemDragMove (const SourceDetails& d) { fileDragMove ({}, d.localPosition.x, d.localPosition.y); }
void ArrangeView::itemDragExit (const SourceDetails&) { dropHighlight = {}; lanes->repaint(); }

void ArrangeView::itemDropped (const SourceDetails& d)
{
    dropHighlight = {};
    const auto desc = d.description.toString();
    if (desc.startsWith ("file:")) dropFileAt (juce::File (desc.fromFirstOccurrenceOf ("file:", false, false)), d.localPosition);
    else if (desc.startsWith ("source:")) dropSourceAt (desc.fromFirstOccurrenceOf ("source:", false, false).getIntValue(), d.localPosition);
    else if (desc.startsWith ("pattern:")) dropPatternAt (desc.fromFirstOccurrenceOf ("pattern:", false, false).getIntValue(), d.localPosition);
    else if (desc.startsWith ("range:"))
    {
        auto parts = juce::StringArray::fromTokens (desc.fromFirstOccurrenceOf ("range:", false, false), ";", "");
        if (parts.size() >= 3)
        {
            auto p = lanes->getLocalPoint (this, d.localPosition);
            auto& pr = session.project;
            const int ti = trackAtY (p.y);
            int trackId = (ti >= 0 && pr.tracks[(size_t) ti].type == TrackType::Audio) ? pr.tracks[(size_t) ti].id : -1;
            if (trackId < 0) for (auto& t : pr.tracks) if (t.type == TrackType::Audio) { trackId = t.id; break; }
            if (trackId >= 0)
            {
                session.beginEdit (TR ("Inserir trecho"));
                const double a = parts[1].getDoubleValue(), b = parts[2].getDoubleValue();
                const int id = session.addAudioClip (trackId, parts[0].getIntValue(), juce::jmax (0.0, snap (xToBeat ((float) p.x))), a, b - a);
                if (id >= 0) session.selectedClips = { id };
                session.commit (kChClips | kChSelection);
            }
        }
    }
    else if (auto* tree = dynamic_cast<juce::FileTreeComponent*> (d.sourceComponent.get()))
    {
        for (int i = 0; i < tree->getNumSelectedFiles(); ++i)
            if (tree->getSelectedFile (i).existsAsFile()) dropFileAt (tree->getSelectedFile (i), d.localPosition);
    }
    lanes->repaint();
}

void ArrangeView::dropFileAt (const juce::File& f, juce::Point<int> posInThis)
{
    juce::Component::SafePointer<ArrangeView> safe (this);
    session.importFile (f, [safe, posInThis] (int sourceId)
    {
        if (safe != nullptr) safe->dropSourceAt (sourceId, posInThis);
    });
}

void ArrangeView::dropSourceAt (int sourceId, juce::Point<int> posInThis)
{
    auto p = lanes->getLocalPoint (this, posInThis);
    auto& pr = session.project;
    int ti = trackAtY (p.y);
    int trackId = -1;
    if (ti >= 0 && pr.tracks[(size_t) ti].type == TrackType::Audio) trackId = pr.tracks[(size_t) ti].id;
    if (trackId < 0)
    {
        // procura uma faixa de audio livre, ou cria uma
        for (auto& t : pr.tracks) if (t.type == TrackType::Audio) { trackId = t.id; break; }
    }
    session.beginEdit (TR ("Inserir áudio"));
    if (trackId < 0) trackId = pr.addTrack (TrackType::Audio, TR ("Faixa ") + juce::String ((int) pr.tracks.size())).id;
    const double b = juce::jmax (0.0, snap (xToBeat ((float) p.x)));
    const int id = session.addAudioClip (trackId, sourceId, b);
    if (id >= 0) session.selectedClips = { id };
    session.commit (kChAll);
}

void ArrangeView::dropPatternAt (int patternId, juce::Point<int> posInThis)
{
    auto p = lanes->getLocalPoint (this, posInThis);
    auto& pr = session.project;
    int ti = trackAtY (p.y);
    int trackId = (ti >= 0 && pr.tracks[(size_t) ti].type == TrackType::Pattern) ? pr.tracks[(size_t) ti].id : pr.liveTrackId;
    session.beginEdit (TR ("Inserir padrão"));
    const double b = juce::jmax (0.0, std::floor (xToBeat ((float) p.x) / pr.beatsPerBar) * pr.beatsPerBar);
    const int id = session.addPatternClip (trackId, patternId, b);
    if (id >= 0) session.selectedClips = { id };
    session.commit (kChClips | kChSelection);
}

} // namespace mp
