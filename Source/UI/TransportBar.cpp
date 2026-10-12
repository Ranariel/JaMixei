#include "TransportBar.h"

namespace mp
{
// ============================================================================
class TransportBar::BpmDisplay : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit BpmDisplay (Session& s) : session (s) { setTooltip (TR ("Andamento (arraste para cima/baixo, Shift = fino, duplo clique = digitar)")); setMouseCursor (juce::MouseCursor::UpDownResizeCursor); }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        const float hv = UiAnimator::get().track (*this, "hv", isMouseOverOrDragging() ? 1.0f : 0.0f);
        if (hv > 0.01f)
        {
            g.setColour (Theme::panel3.withAlpha (0.6f * hv));
            g.fillRoundedRectangle (r.reduced (2.0f), 5.0f);
        }
        g.setColour (Theme::textFaint);
        g.setFont (Theme::font (8.5f, true));
        g.drawText ("BPM", r.reduced (8, 5).removeFromTop (12), juce::Justification::topLeft);
        g.setColour (Theme::accent);
        g.setFont (Theme::mono (17.0f, true));
        g.drawText (juce::String (session.project.bpm, 2), r.withTrimmedTop (10).withTrimmedBottom (2), juce::Justification::centred);
    }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    void mouseDown (const juce::MouseEvent&) override { start = session.project.bpm; session.beginEdit ("BPM"); }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const double sens = e.mods.isShiftDown() ? 0.01 : 0.2;
        double v = start - e.getDistanceFromDragStartY() * sens;
        v = juce::jlimit (40.0, 300.0, e.mods.isShiftDown() ? std::round (v * 100) / 100 : std::round (v * 10) / 10);
        if (v != session.project.bpm) { session.project.bpm = v; session.commit (kChTransport | kChClips); repaint(); }
    }
    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        auto* aw = new juce::AlertWindow (TR ("Andamento"), TR ("Digite o BPM (40 - 300):"), juce::MessageBoxIconType::NoIcon);
        aw->addTextEditor ("bpm", juce::String (session.project.bpm, 2));
        aw->addButton (TR ("OK"), 1, juce::KeyPress (juce::KeyPress::returnKey));
        aw->addButton (TR ("Cancelar"), 0, juce::KeyPress (juce::KeyPress::escapeKey));
        auto* sp = &session;
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([aw, sp] (int r)
        {
            if (r != 1) return;
            const double v = aw->getTextEditorContents ("bpm").replaceCharacter (',', '.').getDoubleValue();
            if (v >= 40 && v <= 300) { sp->beginEdit ("BPM"); sp->project.bpm = v; sp->commit (kChTransport | kChClips); }
        }), true);
    }
    Session& session;
    double start = 120;
};

class TransportBar::PositionDisplay : public juce::Component
{
public:
    explicit PositionDisplay (Session& s) : session (s) { setInterceptsMouseClicks (false, false); }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        auto& p = session.project;
        const double pos = session.engine.getPositionBeats();
        const int bpb = p.beatsPerBar;
        const long long sixteenth = (long long) std::floor (pos * 4.0 + 1e-6);
        const long long bar = sixteenth / (4 * bpb) + 1;
        const int beat = (int) ((sixteenth / 4) % bpb) + 1;
        const int six = (int) (sixteenth % 4) + 1;
        const double secs = p.beatsToSeconds (pos);
        g.setColour (Theme::text);
        g.setFont (Theme::mono (17.0f, true));
        auto top = r.withTrimmedBottom (r.getHeight() * 0.36f).withTrimmedTop (3.0f);
        g.drawText (juce::String (bar).paddedLeft ('0', 3) + "." + juce::String (beat) + "." + juce::String (six), top, juce::Justification::centred);
        g.setColour (Theme::textDim);
        g.setFont (Theme::mono (9.5f));
        const int mins = (int) secs / 60;
        g.drawText (juce::String (mins) + ":" + juce::String (std::fmod (secs, 60.0), 1).paddedLeft ('0', 4)
                    + (session.songMode ? "" : TR ("  PADRÃO")), r.withTrimmedTop (r.getHeight() * 0.6f).withTrimmedBottom (3.0f), juce::Justification::centred);
    }
    Session& session;
};

// ============================================================================
TransportBar::TransportBar (Session& s) : session (s)
{
    const juce::String names[] = { TR ("Arquivo"), TR ("Editar"), TR ("Ferramentas"), TR ("Ajuda") };
    for (int i = 0; i < 4; ++i)
    {
        menuBtns[i].setButtonText (names[i]);
        menuBtns[i].setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        menuBtns[i].onClick = [this, i]
        {
            if (! getMenu) return;
            auto m = getMenu (i);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&menuBtns[i]).withMinimumWidth (220),
                             [this, i] (int r) { if (r != 0 && onMenuItem) onMenuItem (i, r); });
        };
        addAndMakeVisible (menuBtns[i]);
    }
    for (auto* b : { &undoBtn, &redoBtn, &stopBtn, &playBtn, &recBtn, &micBtn, &loopBtn, &metroBtn, &settingsBtn }) addAndMakeVisible (*b);
    for (auto* b : { &undoBtn, &redoBtn, &settingsBtn, &stopBtn, &playBtn, &recBtn, &micBtn, &loopBtn, &metroBtn }) b->flat = true;
    for (auto* b : { &stopBtn, &playBtn, &recBtn, &micBtn, &loopBtn, &metroBtn }) b->iconScale = 0.62f;
    micBtn.onColour = Theme::rec;
    micBtn.onClick = [this] { if (onAudioRecord) onAudioRecord(); };
    recBtn.onColour = Theme::rec;
    playBtn.onColour = Theme::good;
    playBtn.useOnIcon = true;
    playBtn.onIcon = Icon::Pause;
    loopBtn.onColour = Theme::accent;
    metroBtn.onColour = Theme::accent2;
    undoBtn.onClick = [this] { session.undo(); };
    redoBtn.onClick = [this] { session.redo(); };
    playBtn.onClick = [this] { if (onPlay) onPlay(); else session.togglePlay(); refresh(); };
    stopBtn.onClick = [this] { if (onStop) onStop(); session.stopTransport(); refresh(); };
    recBtn.onClick = [this] { session.toggleRecord(); };
    loopBtn.onClick = [this] { session.beginEdit (TR ("Loop")); session.project.loopOn = ! session.project.loopOn; session.commit (kChTransport); };
    metroBtn.onClick = [this] { session.project.metronome = ! session.project.metronome; session.commit (kChTransport); };
    settingsBtn.onClick = [this] { if (onAudioSettings) onAudioSettings(); };

    for (auto* b : { &songBtn, &patBtn })
    {
        styleToggle (*b, Theme::accent);
        b->setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (*b);
    }
    songBtn.setTooltip (TR ("Modo MÚSICA: toca o arranjo completo (Tab alterna)"));
    patBtn.setTooltip (TR ("Modo PADRÃO: repete só o padrão atual dos pads, estilo MPC (Tab alterna)"));
    songBtn.onClick = [this] { session.setSongMode (true); };
    patBtn.onClick = [this] { session.setSongMode (false); };
    tapBtn.setTooltip (TR ("Toque no ritmo para definir o BPM"));
    tapBtn.setColour (juce::TextButton::buttonColourId, Theme::panel3);
    tapBtn.onClick = [this]
    {
        const double now = juce::Time::getMillisecondCounterHiRes();
        if (! taps.empty() && now - taps.back() > 2000) taps.clear();
        taps.push_back (now);
        if (taps.size() > 6) taps.erase (taps.begin());
        if (taps.size() >= 3)
        {
            const double avg = (taps.back() - taps.front()) / (double) (taps.size() - 1);
            const double v = juce::jlimit (40.0, 300.0, std::round (60000.0 / avg * 10.0) / 10.0);
            session.beginEdit (TR ("BPM (tap)"));
            session.project.bpm = v;
            session.commit (kChTransport | kChClips);
        }
    };
    addAndMakeVisible (tapBtn);

    bpm = std::make_unique<BpmDisplay> (session);
    position = std::make_unique<PositionDisplay> (session);
    addAndMakeVisible (*bpm);
    addAndMakeVisible (*position);
    meterL.readPeak = [this] { return session.uiMasterPeakL; };
    meterR.readPeak = [this] { return session.uiMasterPeakR; };
    addAndMakeVisible (meterL);
    addAndMakeVisible (meterR);
    session.addListener (this);
    refresh();
    startTimerHz (60);
}

TransportBar::~TransportBar() { session.removeListener (this); }

void TransportBar::refresh()
{
    playBtn.setToggleState (playingNow(), juce::dontSendNotification);
    recBtn.setToggleState (session.recording, juce::dontSendNotification);
    micBtn.setToggleState (session.isAudioRecording(), juce::dontSendNotification);
    loopBtn.setToggleState (session.project.loopOn, juce::dontSendNotification);
    metroBtn.setToggleState (session.project.metronome, juce::dontSendNotification);
    songBtn.setToggleState (session.songMode, juce::dontSendNotification);
    patBtn.setToggleState (! session.songMode, juce::dontSendNotification);
    undoBtn.setEnabled (session.history.canUndo());
    redoBtn.setEnabled (session.history.canRedo());
    undoBtn.setTooltip (session.history.canUndo() ? TR ("Desfazer: ") + session.history.undoLabel() + " (Ctrl+Z)" : TR ("Desfazer (Ctrl+Z)"));
    redoBtn.setTooltip (session.history.canRedo() ? TR ("Refazer: ") + session.history.redoLabel() + " (Ctrl+Y)" : TR ("Refazer (Ctrl+Y)"));
    bpm->repaint();
    position->repaint();
}

void TransportBar::sessionChanged (int) { refresh(); }

void TransportBar::timerCallback()
{
    // so redesenha o visor quando a posicao muda (economiza CPU/GPU parado)
    const double pos = session.engine.getPositionBeats();
    if (pos != lastPos) { lastPos = pos; position->repaint(); }
    if (micBtn.getToggleState() != session.isAudioRecording()) refresh();
    if (session.isAudioRecording()) micBtn.repaint();
    if (playBtn.getToggleState() != playingNow()) refresh();
    if (undoBtn.isEnabled() != session.history.canUndo() || redoBtn.isEnabled() != session.history.canRedo()) refresh();
    meterL.tick();
    meterR.tick();
}

void TransportBar::resized()
{
    auto r = getLocalBounds().reduced (10, 10);
    r.removeFromLeft (150);   // logo
    for (int i = 0; i < 4; ++i)
    {
        const int w = (int) Theme::font (12.5f, true).getStringWidthFloat (menuBtns[i].getButtonText()) + 26;
        menuBtns[i].setBounds (r.removeFromLeft (w).reduced (1, 3));
    }
    r.removeFromLeft (10);
    undoBtn.setBounds (r.removeFromLeft (40).reduced (1, 1));
    redoBtn.setBounds (r.removeFromLeft (40).reduced (1, 1));
    r.removeFromLeft (14);

    // grupo do transporte
    transportCard = r.removeFromLeft (6 * 42 + 2 * 9 + 8);
    {
        auto c = transportCard.reduced (4, 2);
        auto place = [&c] (juce::Button& b) { b.setBounds (c.removeFromLeft (42).reduced (1, 1)); };
        place (stopBtn); place (playBtn); place (recBtn);
        c.removeFromLeft (9);
        place (micBtn);
        c.removeFromLeft (9);
        place (loopBtn); place (metroBtn);
    }
    r.removeFromLeft (12);
    modeCard = r.removeFromLeft (176);
    {
        auto c = modeCard.reduced (3, 3);
        songBtn.setBounds (c.removeFromLeft (c.getWidth() / 2).reduced (1, 0));
        patBtn.setBounds (c.reduced (1, 0));
    }
    r.removeFromLeft (12);
    lcdCard = r.removeFromLeft (124 + 110 + 52);
    {
        auto c = lcdCard.reduced (3, 2);
        position->setBounds (c.removeFromLeft (124));
        bpm->setBounds (c.removeFromLeft (110));
        tapBtn.setBounds (c.reduced (4, 6));
    }

    settingsBtn.setBounds (r.removeFromRight (42).reduced (1, 1));
    r.removeFromRight (10);
    masterCard = r.removeFromRight (190);
    auto m = masterCard.reduced (12, 0).withTrimmedLeft (52).reduced (0, 12);
    meterL.setBounds (m.removeFromTop (m.getHeight() / 2).reduced (0, 2));
    meterR.setBounds (m.reduced (0, 2));
    if (masterCard.getX() < lcdCard.getRight() + 10) { masterCard = {}; meterL.setBounds ({}); meterR.setBounds ({}); }
}

void TransportBar::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (Theme::panel.brighter (0.04f), 0, 0, Theme::panel, 0, (float) getHeight(), false));
    g.fillAll();
    g.setColour (Theme::outline);
    g.drawHorizontalLine (getHeight() - 1, 0, (float) getWidth());

    // logo: grade de pads + nome
    {
        const float s = 30.0f, x = 14.0f, y = (getHeight() - s) * 0.5f;
        const float cell = (s - 3.0f) / 2.0f;
        const juce::Colour cols[] = { Theme::accent, Theme::accent2, juce::Colour (0xff9fd26b), Theme::warn };
        for (int i = 0; i < 4; ++i)
        {
            g.setColour (cols[i]);
            g.fillRoundedRectangle (x + (i % 2) * (cell + 3.0f), y + (i / 2) * (cell + 3.0f), cell, cell, 3.5f);
        }
        g.setColour (Theme::text);
        g.setFont (Theme::font (15.0f, true));
        g.drawText ("JA MIXEI", juce::Rectangle<float> (x + s + 10.0f, y - 2.0f, 110.0f, 20.0f), juce::Justification::centredLeft);
        g.setColour (Theme::textFaint);
        g.setFont (Theme::font (9.0f));
        g.drawText (TR ("por Ranariel"), juce::Rectangle<float> (x + s + 10.0f, y + 17.0f, 110.0f, 14.0f), juce::Justification::centredLeft);
    }

    for (auto rc : { transportCard, modeCard })
        if (! rc.isEmpty()) Theme::drawCard (g, rc.toFloat(), 9.0f, Theme::bg.interpolatedWith (Theme::panel, 0.5f));
    // separadores dentro do grupo do transporte
    g.setColour (Theme::outline);
    for (auto* b : { &micBtn, &loopBtn })
        g.fillRect (b->getX() - 6, transportCard.getY() + 10, 1, transportCard.getHeight() - 20);

    if (! lcdCard.isEmpty())
    {
        auto lc = lcdCard.toFloat();
        Theme::drawCard (g, lc, 9.0f, juce::Colour (0xff0b0d10));
        g.setColour (Theme::outline);
        g.fillRect ((float) position->getRight() + 1.0f, lc.getY() + 9.0f, 1.0f, lc.getHeight() - 18.0f);
        g.fillRect ((float) bpm->getRight() + 1.0f, lc.getY() + 9.0f, 1.0f, lc.getHeight() - 18.0f);
    }
    if (! masterCard.isEmpty())
    {
        Theme::drawCard (g, masterCard.toFloat(), 9.0f, Theme::bg.interpolatedWith (Theme::panel, 0.5f));
        g.setColour (Theme::textFaint);
        g.setFont (Theme::font (8.5f, true));
        auto lab = masterCard.reduced (12, 0).removeFromLeft (48);
        g.drawText (TR ("MASTER"), lab, juce::Justification::centredLeft);
    }
}

} // namespace mp
