#include "MixerView.h"

namespace mp
{
// ============================================================================
class MixerView::Strip : public juce::Component
{
public:
    Strip (MixerView& o, int id, int slot) : owner (o), trackId (id), trackSlot (slot)
    {
        auto& s = owner.session;
        auto bind = [this, &s] (Knob& k, std::function<void (Track&, float)> fn)
        {
            addAndMakeVisible (k);
            k.onStart = [&s] { s.beginEdit (TR ("Mixer")); };
            k.onChange = [this, fn, &s] (double v) { if (auto* t = track()) { fn (*t, (float) v); s.commit (kChMixer); } };
        };
        hi.formatter = mid.formatter = lo.formatter = [] (double v) { return (v > 0 ? "+" : "") + juce::String (v, 1) + " dB"; };
        filter.formatter = [] (double v) { return std::abs (v) < 0.005 ? juce::String (TR ("desl.")) : v < 0 ? "LP " + juce::String ((int) (-v * 100)) + "%" : "HP " + juce::String ((int) (v * 100)) + "%"; };
        res.formatter = rev.formatter = dly.formatter = [] (double v) { return juce::String ((int) (v * 100)) + "%"; };
        pan.formatter = [] (double v) { return std::abs (v) < 0.01 ? juce::String ("C") : (v < 0 ? "L" : "R") + juce::String ((int) std::round (std::abs (v) * 100)); };
        filter.setColour (Theme::accent2);
        rev.setColour (Theme::good);
        dly.setColour (Theme::good);
        bind (hi, [] (Track& t, float v) { t.eqHigh = v; });
        bind (mid, [] (Track& t, float v) { t.eqMid = v; });
        bind (lo, [] (Track& t, float v) { t.eqLow = v; });
        bind (filter, [] (Track& t, float v) { t.filter = v; });
        bind (res, [] (Track& t, float v) { t.filterRes = v; });
        bind (rev, [] (Track& t, float v) { t.reverbSend = v; });
        bind (dly, [] (Track& t, float v) { t.delaySend = v; });
        bind (pan, [] (Track& t, float v) { t.pan = v; });
        filter.getSlider().setTooltip (TR ("Filtro DJ: para a esquerda corta agudos (LP), para a direita corta graves (HP)"));

        fader.setSliderStyle (juce::Slider::LinearVertical);
        fader.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        fader.setRange (-60.0, 6.0, 0.1);
        fader.setSkewFactorFromMidPoint (-12.0);
        fader.setDoubleClickReturnValue (true, 0.0);
        fader.onDragStart = [this] { owner.session.beginEdit (TR ("Volume")); };
        fader.onValueChange = [this] { if (! updating) if (auto* t = track()) { t->volumeDb = (float) fader.getValue(); owner.session.commit (kChMixer); } };
        addAndMakeVisible (fader);
        meter.readPeak = [this] { return owner.session.engine.getRenderer().trackPeak[(size_t) juce::jlimit (0, kMaxTracks - 1, trackSlot)].exchange (0.0f); };
        addAndMakeVisible (meter);
        for (auto* b : { &muteBtn, &soloBtn }) addAndMakeVisible (*b);
        styleToggle (muteBtn, Theme::warn);
        styleToggle (soloBtn, Theme::accent2);
        muteBtn.onClick = [this] { if (auto* t = track()) { owner.session.beginEdit (TR ("Mute")); t->mute = ! t->mute; owner.session.commit (kChMixer | kChTracks); } };
        soloBtn.onClick = [this] { if (auto* t = track()) { owner.session.beginEdit (TR ("Solo")); t->solo = ! t->solo; owner.session.commit (kChMixer | kChTracks); } };
        refresh();
    }

    Track* track() { return owner.session.project.findTrack (trackId); }

    void refresh()
    {
        auto* t = track();
        if (t == nullptr) return;
        updating = true;
        hi.setValue (t->eqHigh); mid.setValue (t->eqMid); lo.setValue (t->eqLow);
        filter.setValue (t->filter); res.setValue (t->filterRes);
        rev.setValue (t->reverbSend); dly.setValue (t->delaySend); pan.setValue (t->pan);
        fader.setValue (t->volumeDb, juce::dontSendNotification);
        fader.setColour (juce::Slider::trackColourId, t->colour);
        muteBtn.setToggleState (t->mute, juce::dontSendNotification);
        soloBtn.setToggleState (t->solo, juce::dontSendNotification);
        updating = false;
        repaint();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (5);
        r.removeFromTop (24);
        auto kh = 50;
        auto row = [&] (Knob& a, Knob& b) { auto rr = r.removeFromTop (kh); a.setBounds (rr.removeFromLeft (rr.getWidth() / 2)); b.setBounds (rr); };
        auto single = [&] (Knob& a) { a.setBounds (r.removeFromTop (kh).withSizeKeepingCentre (r.getWidth() / 2 + 6, kh)); };
        single (hi);
        row (mid, lo);
        row (filter, res);
        row (rev, dly);
        single (pan);
        r.removeFromTop (2);
        auto btns = r.removeFromBottom (22);
        muteBtn.setBounds (btns.removeFromLeft (btns.getWidth() / 2).reduced (2, 0));
        soloBtn.setBounds (btns.reduced (2, 0));
        r.removeFromBottom (18);
        auto fr = r.reduced (4, 4);
        meter.setBounds (fr.removeFromRight (8));
        fader.setBounds (fr);
    }

    void paint (juce::Graphics& g) override
    {
        auto* t = track();
        if (t == nullptr) return;
        auto r = getLocalBounds().reduced (2).toFloat();
        g.setColour (Theme::panel2);
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (t->colour);
        g.fillRoundedRectangle (r.withHeight (4.0f), 2.0f);
        g.setColour (Theme::text);
        g.setFont (Theme::font (12.0f, true));
        g.drawFittedText (t->name, getLocalBounds().reduced (6, 5).removeFromTop (20), juce::Justification::centred, 1, 1.0f);
        g.setColour (Theme::textDim);
        g.setFont (Theme::font (10.5f));
        g.drawText (formatDb (t->volumeDb), getLocalBounds().reduced (6).withTrimmedBottom (24).removeFromBottom (16), juce::Justification::centred);
    }

    MixerView& owner;
    const int trackId;
    int trackSlot;
    Knob hi { TR ("Agudos"), -15, 15, 0, 0.1 }, mid { TR ("Médios"), -15, 15, 0, 0.1 }, lo { TR ("Graves"), -15, 15, 0, 0.1 },
         filter { TR ("Filtro"), -1, 1, 0, 0.005 }, res { TR ("Resson."), 0, 1, 0.2, 0.01 },
         rev { TR ("Reverb"), 0, 1, 0, 0.01 }, dly { TR ("Delay"), 0, 1, 0, 0.01 }, pan { TR ("Pan"), -1, 1, 0, 0.01 };
    juce::Slider fader;
    LevelMeter meter;
    juce::TextButton muteBtn { "M" }, soloBtn { "S" };
    bool updating = false;
};

// ============================================================================
class MixerView::MasterSection : public juce::Component
{
public:
    explicit MasterSection (MixerView& o) : owner (o)
    {
        auto& s = owner.session;
        size.formatter = damp.formatter = width.formatter = fb.formatter = tone.formatter = [] (double v) { return juce::String ((int) (v * 100)) + "%"; };
        auto bind = [this, &s] (Knob& k, std::function<void (Project&, float)> fn)
        {
            addAndMakeVisible (k);
            k.setColour (Theme::good);
            k.onStart = [&s] { s.beginEdit (TR ("Efeitos")); };
            k.onChange = [fn, &s] (double v) { fn (s.project, (float) v); s.commit (kChMixer); };
        };
        bind (size, [] (Project& p, float v) { p.reverbSize = v; });
        bind (damp, [] (Project& p, float v) { p.reverbDamp = v; });
        bind (width, [] (Project& p, float v) { p.reverbWidth = v; });
        bind (fb, [] (Project& p, float v) { p.delayFeedback = v; });
        bind (tone, [] (Project& p, float v) { p.delayTone = v; });
        delayTime.addItemList ({ "1/16", "1/8", TR ("1/8 pontuada"), "1/4", TR ("1/4 pontuada"), "1/2", TR ("1 compasso"), TR ("1/8 tercina"), TR ("1/4 tercina") }, 1);
        delayTime.onChange = [this]
        {
            static const double vals[] = { 0.25, 0.5, 0.75, 1.0, 1.5, 2.0, 4.0, 1.0 / 3.0, 2.0 / 3.0 };
            owner.session.beginEdit (TR ("Tempo do delay"));
            owner.session.project.delayBeats = vals[juce::jlimit (0, 8, delayTime.getSelectedId() - 1)];
            owner.session.commit (kChMixer);
        };
        addAndMakeVisible (delayTime);

        fader.setSliderStyle (juce::Slider::LinearVertical);
        fader.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        fader.setRange (-60.0, 6.0, 0.1);
        fader.setSkewFactorFromMidPoint (-12.0);
        fader.setDoubleClickReturnValue (true, 0.0);
        fader.setColour (juce::Slider::trackColourId, Theme::accent);
        fader.onDragStart = [this] { owner.session.beginEdit (TR ("Volume master")); };
        fader.onValueChange = [this] { if (! updating) { owner.session.project.masterVolDb = (float) fader.getValue(); owner.session.commit (kChMixer); } };
        addAndMakeVisible (fader);
        meterL.readPeak = [this] { return owner.session.uiMasterPeakL; };
        meterR.readPeak = [this] { return owner.session.uiMasterPeakR; };
        addAndMakeVisible (meterL);
        addAndMakeVisible (meterR);
        refresh();
    }

    void refresh()
    {
        auto& p = owner.session.project;
        updating = true;
        size.setValue (p.reverbSize); damp.setValue (p.reverbDamp); width.setValue (p.reverbWidth);
        fb.setValue (p.delayFeedback); tone.setValue (p.delayTone);
        static const double vals[] = { 0.25, 0.5, 0.75, 1.0, 1.5, 2.0, 4.0, 1.0 / 3.0, 2.0 / 3.0 };
        int best = 3;
        for (int i = 0; i < 9; ++i) if (std::abs (vals[i] - p.delayBeats) < 1e-3) best = i;
        delayTime.setSelectedId (best + 1, juce::dontSendNotification);
        fader.setValue (p.masterVolDb, juce::dontSendNotification);
        updating = false;
        repaint();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8);
        auto masterCol = r.removeFromRight (90);
        r.removeFromRight (10);
        auto fx = r;
        fx.removeFromTop (22);
        auto revRow = fx.removeFromTop (62);
        size.setBounds (revRow.removeFromLeft (70)); damp.setBounds (revRow.removeFromLeft (70)); width.setBounds (revRow.removeFromLeft (70));
        fx.removeFromTop (30);
        delayTime.setBounds (fx.removeFromTop (26).withWidth (190));
        fx.removeFromTop (6);
        auto dRow = fx.removeFromTop (62);
        fb.setBounds (dRow.removeFromLeft (80)); tone.setBounds (dRow.removeFromLeft (70));

        masterCol.removeFromTop (26);
        masterCol.removeFromBottom (22);
        meterR.setBounds (masterCol.removeFromRight (9));
        masterCol.removeFromRight (3);
        meterL.setBounds (masterCol.removeFromRight (9));
        fader.setBounds (masterCol);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().reduced (2).toFloat();
        g.setColour (Theme::panel2);
        g.fillRoundedRectangle (r, 5.0f);
        auto inner = getLocalBounds().reduced (10, 6);
        auto masterCol = inner.removeFromRight (88);
        g.setColour (Theme::accent);
        g.setFont (Theme::font (13.0f, true));
        g.drawText (TR ("MASTER"), masterCol.removeFromTop (20), juce::Justification::centred);
        g.setColour (Theme::textDim);
        g.setFont (Theme::font (11.0f));
        g.drawText (formatDb (owner.session.project.masterVolDb), getLocalBounds().reduced (10, 6).removeFromRight (88).removeFromBottom (18), juce::Justification::centred);
        g.setColour (Theme::good);
        g.setFont (Theme::font (12.0f, true));
        g.drawText (TR ("REVERB (envio)"), inner.removeFromTop (18), juce::Justification::centredLeft);
        inner.removeFromTop (72);
        g.drawText (TR ("DELAY PING-PONG (sincronizado)"), inner.removeFromTop (20), juce::Justification::centredLeft);
        g.setColour (Theme::textFaint);
        g.setFont (Theme::font (11.0f));
        g.drawFittedText (TR ("O master tem um limitador para nunca distorcer. Use os envios Reverb/Delay de cada faixa para mandar som aos efeitos."),
                          getLocalBounds().reduced (10).removeFromBottom (80).withTrimmedRight (100), juce::Justification::bottomLeft, 5, 1.0f);
    }

    MixerView& owner;
    Knob size { TR ("Tamanho"), 0, 1, 0.6, 0.01 }, damp { TR ("Abafar"), 0, 1, 0.4, 0.01 }, width { TR ("Largura"), 0, 1, 1, 0.01 },
         fb { TR ("Repetições"), 0, 0.95, 0.35, 0.01 }, tone { TR ("Tom"), 0, 1, 0.6, 0.01 };
    juce::ComboBox delayTime;
    juce::Slider fader;
    LevelMeter meterL, meterR;
    bool updating = false;
};

// ============================================================================
MixerView::MixerView (Session& s) : session (s)
{
    viewport.setViewedComponent (&stripHolder, false);
    viewport.setScrollBarsShown (false, true);
    addAndMakeVisible (viewport);
    master = std::make_unique<MasterSection> (*this);
    addAndMakeVisible (*master);
    session.addListener (this);
    rebuild();
    startTimerHz (60);
}

MixerView::~MixerView() { session.removeListener (this); }

void MixerView::rebuild()
{
    strips.clear();
    int slot = 0;
    for (auto& t : session.project.tracks)
    {
        auto* st = strips.add (new Strip (*this, t.id, slot++));
        stripHolder.addAndMakeVisible (st);
    }
    disableFocusRecursively (*this);
    resized();
}

void MixerView::sessionChanged (int what)
{
    if (what & kChTracks)
    {
        bool same = strips.size() == (int) session.project.tracks.size();
        for (int i = 0; same && i < strips.size(); ++i) same = strips[i]->trackId == session.project.tracks[(size_t) i].id;
        if (! same) { rebuild(); return; }
    }
    if (what & (kChMixer | kChTracks))
    {
        for (auto* st : strips) st->refresh();
        master->refresh();
    }
}

void MixerView::timerCallback()
{
    if (! isShowing()) return;
    for (auto* st : strips) st->meter.tick();
    master->meterL.tick();
    master->meterR.tick();
}

void MixerView::resized()
{
    auto r = getLocalBounds().reduced (8);
    master->setBounds (r.removeFromRight (360));
    r.removeFromRight (8);
    viewport.setBounds (r);
    const int w = 112;
    const int h = r.getHeight() - (strips.size() * w > r.getWidth() ? 12 : 0);
    stripHolder.setSize (juce::jmax (r.getWidth(), strips.size() * w), h);
    for (int i = 0; i < strips.size(); ++i) strips[i]->setBounds (i * w, 0, w, h);
}

void MixerView::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel);
}

} // namespace mp
