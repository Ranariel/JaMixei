#include "SequencerView.h"
#include "PadPanel.h"

namespace mp
{
// ============================================================================
class SequencerView::Grid : public juce::Component
{
public:
    explicit Grid (SequencerView& o) : owner (o) {}
    static constexpr int headerW = 150;

    Pattern* pat() { return owner.session.currentPattern(); }
    int numRows() const { return owner.showAllBanks ? kNumPads : kPadsPerBank; }
    int rowH() const { return owner.showAllBanks ? 26 : juce::jmax (14, getHeight() / kPadsPerBank); }
    float colW() const
    {
        auto* p = const_cast<Grid*> (this)->pat();
        const int steps = p ? p->lengthSteps : 16;
        return (getWidth() - headerW) / (float) steps;
    }
    int padForRow (int row) const { return owner.showAllBanks ? row : owner.session.currentBank * kPadsPerBank + row; }

    int playingStep = -1;

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Theme::bg);
        auto* p = pat();
        if (p == nullptr) return;
        auto& proj = owner.session.project;
        const int rh = rowH();
        const float cw = colW();
        auto clip = g.getClipBounds();
        for (int row = 0; row < numRows(); ++row)
        {
            const int y = row * rh;
            if (y + rh < clip.getY() || y > clip.getBottom()) continue;
            const int pad = padForRow (row);
            const int bank = pad / kPadsPerBank, idx = pad % kPadsPerBank;
            const auto& pd = proj.pads[(size_t) pad];
            const bool sel = owner.session.selectedPad == pad;
            // cabecalho da linha
            g.setColour (sel ? Theme::panel3 : Theme::panel2);
            g.fillRect (0, y, headerW - 2, rh - 1);
            g.setColour (pd.isEmpty() ? Theme::panel3 : pd.colour);
            g.fillRect (0, y, 4, rh - 1);
            if (bank == owner.session.currentBank)
            {
                g.setFont (Theme::mono (11.0f, true));
                g.setColour (Theme::textDim);
                g.drawText (juce::String::charToString ((juce::juce_wchar) padKeyCode (idx)), 8, y, 14, rh - 1, juce::Justification::centred);
            }
            g.setFont (Theme::font (12.0f, ! pd.isEmpty()));
            g.setColour (pd.isEmpty() ? Theme::textFaint : Theme::text);
            g.drawFittedText (juce::String::charToString ((juce::juce_wchar) ('A' + bank)) + juce::String (idx + 1) + "  " + pd.name,
                              26, y, headerW - 30, rh - 1, juce::Justification::centredLeft, 1, 1.0f);
            if (owner.showAllBanks && idx == 0 && row > 0)
            {
                g.setColour (Theme::gridStrong.brighter (0.4f));
                g.fillRect (0, y, getWidth(), 2);
            }
            // celulas vazias
            for (int st = 0; st < p->lengthSteps; ++st)
            {
                auto cell = juce::Rectangle<float> (headerW + st * cw, (float) y, cw, (float) rh).reduced (1.5f, 1.5f);
                const bool beatStart = (st / kStepsPerBeat) % 2 == 0;
                g.setColour (beatStart ? Theme::panel2 : Theme::panel);
                if (st == playingStep) g.setColour (Theme::panel3.brighter (0.15f));
                g.fillRoundedRectangle (cell, 2.5f);
            }
        }
        // linhas de compasso
        for (int st = 0; st <= p->lengthSteps; st += kStepsPerBeat)
        {
            const bool bar = st % (kStepsPerBeat * proj.beatsPerBar) == 0;
            g.setColour (bar ? Theme::gridStrong.brighter (0.2f) : Theme::gridStrong);
            g.fillRect (headerW + st * cw - 0.5f, 0.0f, bar ? 2.0f : 1.0f, (float) rh * numRows());
        }
        // notas por cima das linhas (uma barra so quando esticada)
        for (int row = 0; row < numRows(); ++row)
        {
            const int y = row * rh;
            if (y + rh < clip.getY() || y > clip.getBottom()) continue;
            const int pad = padForRow (row);
            const auto& pd = proj.pads[(size_t) pad];
            for (int st = 0; st < p->lengthSteps; ++st)
                if (const auto v = p->get (pad, st))
                {
                    const int len = juce::jmin (p->getLen (pad, st), p->lengthSteps - st);
                    auto cell = juce::Rectangle<float> (headerW + st * cw, (float) y, cw * len, (float) rh).reduced (1.5f, 1.5f);
                    auto col = pd.isEmpty() ? Theme::accent : pd.colour;
                    const float a = 0.35f + 0.65f * v / 127.0f;
                    g.setColour (col.withAlpha (a));
                    g.fillRoundedRectangle (cell, 2.5f);
                    if (len > 1)
                    {
                        // alca de esticar no fim
                        g.setColour (juce::Colours::black.withAlpha (0.35f));
                        g.fillRect (cell.getRight() - 4.0f, cell.getY() + 3.0f, 2.0f, cell.getHeight() - 6.0f);
                    }
                    // barra de velocidade
                    g.setColour (juce::Colours::white.withAlpha (0.55f));
                    g.fillRect (cell.getX() + 2, cell.getBottom() - 3, (juce::jmin (cw, cell.getWidth()) - 4) * v / 127.0f, 2.0f);
                }
        }
        if (playingStep >= 0 && ! owner.showAllBanks)
        {
            g.setColour (Theme::accent.withAlpha (0.8f));
            g.fillRect (headerW + playingStep * cw, (float) rh * numRows(), cw, 3.0f);
        }
    }

    bool cellAt (juce::Point<float> pt, int& row, int& step)
    {
        auto* p = pat();
        if (p == nullptr || pt.x < headerW) return false;
        row = (int) (pt.y / rowH());
        step = (int) ((pt.x - headerW) / colW());
        return row >= 0 && row < numRows() && step >= 0 && step < p->lengthSteps;
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        auto* p = pat();
        if (p == nullptr) return;
        const int row = (int) (e.position.y / rowH());
        stretchHead = -1;
        moved = false;
        if (e.position.x < headerW)
        {
            if (row >= 0 && row < numRows())
            {
                owner.session.padDown (padForRow (row), 120);
                auditionPad = padForRow (row);
            }
            return;
        }
        int r, st;
        if (! cellAt (e.position, r, st)) return;
        owner.session.beginEdit (TR ("Editar padrão"));
        const int pad = padForRow (r);
        const int head = p->headAt (pad, st);
        const uint8_t cur = head >= 0 ? p->get (pad, head) : 0;
        uint8_t vel = (uint8_t) owner.clickVelocity;
        if (e.mods.isShiftDown()) vel = 127;
        else if (e.mods.isAltDown()) vel = 50;
        if (e.mods.isCommandDown() && cur) { velDragPad = pad; velDragStep = head; velStart = cur; velY = e.position.y; paintValue = -1; return; }
        velDragPad = -1;
        if (e.mods.isPopupMenu())
        {
            paintValue = 0;
            if (head >= 0) p->set (pad, head, 0);
            owner.session.commit (kChPatterns);
            return;
        }
        if (head >= 0)
        {
            // nota existente: arrastar para a direita estica; clique simples desliga
            stretchPad = pad;
            stretchHead = head;
            clickStep = st;
            paintValue = -1;
            return;
        }
        paintValue = vel;
        p->set (pad, st, (uint8_t) paintValue);
        if (! owner.session.engine.isPlaying()) { owner.session.engine.padDown (pad, (uint8_t) paintValue); owner.session.engine.padUp (pad); }
        owner.session.commit (kChPatterns);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        auto* p = pat();
        if (p == nullptr || auditionPad >= 0) return;
        if (velDragPad >= 0)
        {
            const int v = juce::jlimit (1, 127, (int) (velStart + (velY - e.position.y)));
            p->set (velDragPad, velDragStep, (uint8_t) v);
            owner.session.setStatus (TR ("Velocidade: ") + juce::String (v));
            owner.session.commit (kChPatterns);
            return;
        }
        if (stretchHead >= 0)
        {
            const int st = juce::jlimit (stretchHead, p->lengthSteps - 1, (int) ((e.position.x - headerW) / colW()));
            if (st != clickStep) moved = true;
            if (! moved) return;
            const int len = st - stretchHead + 1;
            if (len != p->getLen (stretchPad, stretchHead))
            {
                // a nota esticada "engole" os passos que ficarem embaixo dela
                for (int k = stretchHead + 1; k <= st; ++k) p->set (stretchPad, k, 0);
                p->setLen (stretchPad, stretchHead, len);
                owner.session.setStatus (TR ("Duração: ") + juce::String (len) + TR (" passo(s)"));
                owner.session.commit (kChPatterns);
            }
            return;
        }
        if (paintValue < 0) return;
        int r, st;
        if (! cellAt (e.position, r, st)) return;
        const int pad = padForRow (r);
        if (paintValue == 0)
        {
            const int head = p->headAt (pad, st);
            if (head >= 0) { p->set (pad, head, 0); owner.session.commit (kChPatterns); }
            return;
        }
        if (p->headAt (pad, st) < 0)
        {
            p->set (pad, st, (uint8_t) paintValue);
            owner.session.commit (kChPatterns);
        }
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        auto* p = pat();
        if (p != nullptr && stretchHead >= 0 && ! moved && clickStep == stretchHead)
        {
            p->set (stretchPad, stretchHead, 0);   // clique simples numa nota: desliga (como antes)
            owner.session.commit (kChPatterns);
        }
        if (auditionPad >= 0) owner.session.padUp (auditionPad);
        auditionPad = -1;
        velDragPad = -1;
        stretchHead = -1;
    }

    SequencerView& owner;
    int paintValue = 0;
    int auditionPad = -1;
    int velDragPad = -1, velDragStep = 0, velStart = 0;
    int stretchPad = -1, stretchHead = -1, clickStep = -1;
    bool moved = false;
    float velY = 0;
};

// ============================================================================
SequencerView::SequencerView (Session& s) : session (s)
{
    grid = std::make_unique<Grid> (*this);
    viewport.setViewedComponent (grid.get(), false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);
    showAllBanks = session.props.getUserSettings()->getBoolValue ("seqAllBanks", false);
    styleToggle (allBanksBtn, Theme::accent2);
    allBanksBtn.setClickingTogglesState (true);
    allBanksBtn.setToggleState (showAllBanks, juce::dontSendNotification);
    allBanksBtn.setTooltip (TR ("Mostra os 64 pads (4 bancos) de uma vez, rolando: ótimo para instrumentos com várias oitavas"));
    allBanksBtn.onClick = [this]
    {
        showAllBanks = allBanksBtn.getToggleState();
        session.props.getUserSettings()->setValue ("seqAllBanks", showAllBanks);
        resized();
        grid->repaint();
    };
    addAndMakeVisible (allBanksBtn);
    for (auto* c : std::initializer_list<juce::Component*> { &patternBox, &lengthBox, &velBox, &newBtn, &dupBtn, &renBtn, &delBtn, &insertBtn, &clearBtn, &playPatBtn, &swing })
        addAndMakeVisible (c);

    patternBox.onChange = [this]
    {
        const int idx = patternBox.getSelectedItemIndex();
        if (juce::isPositiveAndBelow (idx, (int) session.project.patterns.size()))
        {
            session.project.currentPatternId = session.project.patterns[(size_t) idx].id;
            session.commit (kChPatterns);
        }
    };
    lengthBox.addItemList ({ TR ("1 compasso (16 passos)"), TR ("2 compassos (32)"), TR ("3 compassos (48)"), TR ("4 compassos (64)") }, 1);
    lengthBox.onChange = [this]
    {
        session.setPatternLength (lengthBox.getSelectedId() * 16);
    };
    velBox.addItemList ({ TR ("Clique: normal (100)"), TR ("Clique: forte (127)"), TR ("Clique: fraco (60)") }, 1);
    velBox.setSelectedId (1, juce::dontSendNotification);
    velBox.onChange = [this] { const int i = velBox.getSelectedId(); clickVelocity = i == 2 ? 127 : i == 3 ? 60 : 100; };
    velBox.setTooltip (TR ("Velocidade ao clicar. Shift+clique = forte, Alt+clique = fraco, Ctrl+arrastar numa nota = ajustar velocidade, botão direito = apagar"));

    newBtn.onClick = [this] { session.newPattern (false); };
    dupBtn.onClick = [this] { session.newPattern (true); };
    delBtn.onClick = [this]
    {
        if (auto* p = session.currentPattern())
        {
            const int id = p->id;
            juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::NoIcon, TR ("Apagar padrão"),
                TR ("Apagar \"") + p->name + TR ("\"? Os clipes deste padrão no arranjo também serao removidos."), TR ("Apagar"), TR ("Cancelar"), nullptr,
                juce::ModalCallbackFunction::create ([this, id] (int ok) { if (ok) session.deletePattern (id); }));
        }
    };
    renBtn.onClick = [this]
    {
        auto* p = session.currentPattern();
        if (p == nullptr) return;
        auto* aw = new juce::AlertWindow (TR ("Renomear padrão"), TR ("Nome:"), juce::MessageBoxIconType::NoIcon);
        aw->addTextEditor ("name", p->name);
        aw->addButton (TR ("OK"), 1, juce::KeyPress (juce::KeyPress::returnKey));
        aw->addButton (TR ("Cancelar"), 0, juce::KeyPress (juce::KeyPress::escapeKey));
        const int id = p->id;
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw, id] (int r)
        {
            if (r != 1) return;
            if (auto* pp = session.project.findPattern (id))
            {
                session.beginEdit (TR ("Renomear padrão"));
                pp->name = aw->getTextEditorContents ("name").substring (0, 30);
                for (auto& c : session.project.clips) if (c.kind == ClipKind::Pattern && c.patternId == id) c.name = pp->name;
                session.commit (kChPatterns | kChClips);
            }
        }), true);
    };
    insertBtn.setTooltip (TR ("Coloca este padrão na faixa de pads ao vivo, na posição do cursor (ou no fim)"));
    insertBtn.onClick = [this]
    {
        auto& p = session.project;
        session.beginEdit (TR ("Inserir padrão"));
        double b = std::floor (session.engine.getPositionBeats() / p.beatsPerBar) * p.beatsPerBar;
        const int id = session.addPatternClip (p.liveTrackId, p.currentPatternId, b);
        session.selectedClips = { id };
        session.commit (kChClips | kChSelection);
        session.setStatus (TR ("Padrão inserido no compasso ") + juce::String ((int) (b / p.beatsPerBar) + 1));
    };
    clearBtn.onClick = [this]
    {
        if (auto* p = session.currentPattern())
        {
            session.beginEdit (TR ("Limpar padrão"));
            for (int pad = session.currentBank * kPadsPerBank; pad < (session.currentBank + 1) * kPadsPerBank; ++pad)
                for (int st = 0; st < kMaxSteps; ++st) p->set (pad, st, 0);
            session.commit (kChPatterns);
        }
    };
    clearBtn.setTooltip (TR ("Limpa as notas do banco atual neste padrão"));
    clearBtn.setButtonText (TR ("Limpar banco"));
    playPatBtn.setTooltip (TR ("Liga o modo PADRÃO (repete este padrão, estilo MPC) e toca. Tab também alterna."));
    styleToggle (playPatBtn, Theme::accent);
    playPatBtn.onClick = [this]
    {
        const bool toPattern = session.songMode;
        session.setSongMode (! toPattern);
        if (toPattern && ! session.engine.isPlaying()) session.togglePlay();
    };
    swing.formatter = [] (double v) { return TR ("Swing ") + juce::String (v, 1) + "%"; };
    swing.onStart = [this] { session.beginEdit (TR ("Swing")); };
    swing.onChange = [this] (double v) { session.project.swing = (float) v; session.commit (kChTransport); };

    session.addListener (this);
    refresh();
    startTimerHz (60);
}

SequencerView::~SequencerView() { session.removeListener (this); }

void SequencerView::refresh()
{
    patternBox.clear (juce::dontSendNotification);
    int sel = 0;
    for (size_t i = 0; i < session.project.patterns.size(); ++i)
    {
        patternBox.addItem (session.project.patterns[i].name, (int) i + 1);
        if (session.project.patterns[i].id == session.project.currentPatternId) sel = (int) i;
    }
    patternBox.setSelectedItemIndex (sel, juce::dontSendNotification);
    if (auto* p = session.currentPattern()) lengthBox.setSelectedId (juce::jlimit (1, 4, p->lengthSteps / 16), juce::dontSendNotification);
    swing.setValue (session.project.swing);
    playPatBtn.setToggleState (! session.songMode, juce::dontSendNotification);
    grid->repaint();
}

void SequencerView::sessionChanged (int what)
{
    if (what & (kChPatterns | kChPads | kChSelection | kChTransport)) refresh();
}

void SequencerView::timerCallback()
{
    int step = -1;
    auto* p = session.currentPattern();
    if (p && session.engine.isPlaying())
    {
        const double pos = session.engine.getPositionBeats();
        if (! session.songMode) step = (int) (std::fmod (pos, p->lengthBeats()) * kStepsPerBeat);
        else
            for (auto& c : session.project.clips)
                if (c.kind == ClipKind::Pattern && c.patternId == p->id && pos >= c.startBeat && pos < session.project.clipEndBeat (c))
                {
                    step = (int) (std::fmod (pos - c.startBeat + c.patternOffsetBeats, p->lengthBeats()) * kStepsPerBeat);
                    break;
                }
    }
    if (step != grid->playingStep) { grid->playingStep = step; grid->repaint(); }
}

void SequencerView::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    auto top = r.removeFromTop (28);
    patternBox.setBounds (top.removeFromLeft (190)); top.removeFromLeft (6);
    newBtn.setBounds (top.removeFromLeft (70)); top.removeFromLeft (4);
    dupBtn.setBounds (top.removeFromLeft (80)); top.removeFromLeft (4);
    renBtn.setBounds (top.removeFromLeft (86)); top.removeFromLeft (4);
    delBtn.setBounds (top.removeFromLeft (70));
    insertBtn.setBounds (top.removeFromRight (150)); top.removeFromRight (6);
    playPatBtn.setBounds (top.removeFromRight (150));
    r.removeFromTop (6);
    auto row2 = r.removeFromTop (52);
    swing.setBounds (row2.removeFromRight (110));
    row2 = row2.withSizeKeepingCentre (row2.getWidth(), 28);
    lengthBox.setBounds (row2.removeFromLeft (190)); row2.removeFromLeft (6);
    velBox.setBounds (row2.removeFromLeft (180)); row2.removeFromLeft (6);
    clearBtn.setBounds (row2.removeFromLeft (110));
    row2.removeFromLeft (6);
    allBanksBtn.setBounds (row2.removeFromLeft (150));
    r.removeFromTop (20);
    viewport.setBounds (r);
    const int gw = r.getWidth() - (showAllBanks ? viewport.getScrollBarThickness() : 0);
    grid->setSize (gw, showAllBanks ? 26 * kNumPads : r.getHeight());
}

void SequencerView::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel);
    g.setColour (Theme::textFaint);
    g.setFont (Theme::font (11.0f));
    g.drawText (TR ("Clique liga/desliga  |  arraste numa nota para ESTICAR  |  arraste no vazio para pintar  |  botão direito apaga  |  Shift = forte, Alt = fraco, Ctrl+arrastar = velocidade"),
                grid->getBounds().withHeight (14).translated (0, -18), juce::Justification::centredLeft);
}

} // namespace mp
