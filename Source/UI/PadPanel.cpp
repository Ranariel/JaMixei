#include "PadPanel.h"
#include "../Audio/Analysis.h"

namespace mp
{
// ============================================================================
class PadPanel::PadButton : public juce::Component
{
public:
    PadButton (PadPanel& o, int idx) : owner (o), index (idx) {}

    int padIndex() const { return owner.session.currentBank * kPadsPerBank + index; }

    void paint (juce::Graphics& g) override
    {
        const int pi = padIndex();
        const auto& pad = owner.session.project.pads[(size_t) pi];
        const float hv = UiAnimator::get().track (*this, "hv", isMouseOver() ? 1.0f : 0.0f);
        const float pv = UiAnimator::get().track (*this, "pv", held ? 1.0f : 0.0f, 0.5f);
        auto r = getLocalBounds().toFloat().reduced (3.0f + 1.5f * pv);
        const bool sel = owner.session.selectedPad == pi;
        const bool empty = pad.isEmpty();
        const float fl = owner.flash[(size_t) pi];
        // brilho em volta quando o pad toca
        if (fl > 0.01f && ! empty)
        {
            for (int k = 3; k >= 1; --k)
            {
                g.setColour (pad.colour.withAlpha (0.10f * fl * (float) (4 - k)));
                g.drawRoundedRectangle (r.expanded ((float) k * 1.2f), 7.0f + k, 2.0f);
            }
        }
        auto base = empty ? Theme::panel2.interpolatedWith (Theme::panel3, hv * 0.7f)
                          : pad.colour.withMultipliedBrightness (0.35f + 0.08f * hv).withMultipliedSaturation (0.8f);
        base = base.interpolatedWith (pad.colour.brighter (0.3f), fl * 0.85f);
        g.setGradientFill (juce::ColourGradient (base.brighter (0.12f), r.getX(), r.getY(), base.darker (0.15f), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 6.0f);
        if (! empty)
        {
            g.setColour (pad.colour.withAlpha (0.9f));
            g.fillRoundedRectangle (r.withHeight (3.0f).reduced (8.0f, 0).translated (0, 4), 1.5f);
        }
        g.setColour (owner.dragOverPad == pi ? Theme::accent : sel ? Theme::text : Theme::outline.brighter (0.15f));
        g.drawRoundedRectangle (r, 6.0f, sel || owner.dragOverPad == pi ? 2.0f : 1.0f);

        auto inner = r.reduced (7.0f, 6.0f);
        g.setFont (Theme::font (11.0f, true));
        g.setColour (Theme::textDim);
        g.drawText (juce::String::charToString ((juce::juce_wchar) ('A' + owner.session.currentBank)) + juce::String (index + 1),
                    inner.removeFromTop (14), juce::Justification::topLeft);
        // tecla
        auto keyR = juce::Rectangle<float> (r.getRight() - 22, r.getY() + 6, 16, 16);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (keyR, 3.0f);
        g.setColour (Theme::text.withAlpha (0.85f));
        g.setFont (Theme::mono (11.0f, true));
        g.drawText (juce::String::charToString ((juce::juce_wchar) padKeyCode (index)), keyR, juce::Justification::centred);

        g.setFont (Theme::font (12.0f, ! empty));
        g.setColour (empty ? Theme::textFaint : Theme::text);
        g.drawFittedText (empty ? TR ("vazio") : pad.name, inner.toNearestInt(), juce::Justification::bottomLeft, 2, 1.0f);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int pi = padIndex();
        if (e.mods.isPopupMenu()) { showMenu(); return; }
        const float y = juce::jlimit (0.0f, 1.0f, e.position.y / (float) getHeight());
        const uint8_t vel = (uint8_t) juce::jlimit (30, 127, (int) (127 - y * 70));
        owner.session.padDown (pi, vel);
        held = true;
        repaint();
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        if (held) owner.session.padUp (padIndex());
        held = false;
        repaint();
    }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

    void showMenu()
    {
        const int pi = padIndex();
        owner.session.selectedPad = pi;
        owner.session.notify (kChSelection);
        juce::PopupMenu m;
        m.addItem (1, TR ("Carregar arquivo de áudio..."));
        m.addItem (2, TR ("Abrir no editor"));
        m.addItem (3, TR ("Renomear..."));
        m.addItem (4, TR ("Copiar pad"));
        m.addItem (5, TR ("Colar pad"), owner.session.project.pads.size() > 0 && clipboardValid);
        m.addSeparator();
        m.addItem (6, TR ("Limpar pad"));
        m.showMenuAsync (juce::PopupMenu::Options(), [this, pi] (int r)
        {
            auto& s = owner.session;
            if (r == 1) owner.loadBtn.triggerClick();
            if (r == 2 && owner.onOpenInEditor && ! s.project.pads[(size_t) pi].isEmpty())
            {
                auto& p = s.project.pads[(size_t) pi];
                owner.onOpenInEditor (p.sourceId, p.startSec, p.endSec);
            }
            if (r == 3)
            {
                auto* aw = new juce::AlertWindow (TR ("Renomear pad"), TR ("Nome:"), juce::MessageBoxIconType::NoIcon);
                aw->addTextEditor ("name", s.project.pads[(size_t) pi].name);
                aw->addButton (TR ("OK"), 1, juce::KeyPress (juce::KeyPress::returnKey));
                aw->addButton (TR ("Cancelar"), 0, juce::KeyPress (juce::KeyPress::escapeKey));
                auto* ownerPtr = &owner;
                aw->enterModalState (true, juce::ModalCallbackFunction::create ([aw, pi, ownerPtr] (int res)
                {
                    if (res == 1) ownerPtr->editPad (TR ("Renomear pad"), [aw] (Pad& p) { p.name = aw->getTextEditorContents ("name").substring (0, 24); });
                    juce::ignoreUnused (pi);
                }), true);
            }
            if (r == 4) { clipboard = s.project.pads[(size_t) pi]; clipboardValid = true; }
            if (r == 5 && clipboardValid) { s.beginEdit (TR ("Colar pad")); s.project.pads[(size_t) pi] = clipboard; s.commit (kChPads); }
            if (r == 6) s.clearPad (pi);
        });
    }

    PadPanel& owner;
    const int index;
    bool held = false;
    static Pad clipboard;
    static bool clipboardValid;
};
Pad PadPanel::PadButton::clipboard;
bool PadPanel::PadButton::clipboardValid = false;

// ============================================================================
//  Forma de onda do pad com marcadores de inicio/fim arrastaveis
// ============================================================================
class PadPanel::PadWave : public juce::Component
{
public:
    explicit PadWave (PadPanel& o) : owner (o) {}

    void range (double& a, double& b) const
    {
        const auto& p = owner.session.project.pads[(size_t) owner.session.selectedPad];
        auto src = owner.session.getSource (p.sourceId);
        const double len = juce::jmax (0.01, p.endSec - p.startSec);
        a = juce::jmax (0.0, p.startSec - len * 0.2);
        b = p.endSec + len * 0.2;
        if (src) b = juce::jmin (src->lengthSec, b);
        if (dragging) { a = viewA; b = viewB; }
    }
    float secToX (double s) const { double a, b; range (a, b); return (float) ((s - a) / (b - a) * getWidth()); }
    double xToSec (float x) const { double a, b; range (a, b); return a + (x / (double) getWidth()) * (b - a); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Theme::bg);
        g.fillRoundedRectangle (r, 4.0f);
        const auto& p = owner.session.project.pads[(size_t) owner.session.selectedPad];
        auto src = owner.session.getSource (p.sourceId);
        if (p.isEmpty() || src == nullptr)
        {
            g.setColour (Theme::textFaint);
            g.setFont (Theme::font (12.0f));
            g.drawFittedText (TR ("Pad vazio. Arraste um áudio aqui ou para um pad, ou use o Editor para fatiar uma música."),
                              getLocalBounds().reduced (10), juce::Justification::centred, 3, 1.0f);
            return;
        }
        double a, b; range (a, b);
        drawWaveform (g, *src, r.reduced (0, 3), a, b, Theme::textFaint);
        const float x0 = secToX (p.startSec), x1 = secToX (p.endSec);
        g.saveState();
        g.reduceClipRegion (juce::Rectangle<float> (x0, 0, x1 - x0, r.getHeight()).toNearestInt());
        drawWaveform (g, *src, r.reduced (0, 3), a, b, p.colour.brighter (0.3f), false);
        g.restoreState();
        g.setColour (p.colour.withAlpha (0.12f));
        g.fillRect (juce::Rectangle<float> (x0, 0, x1 - x0, r.getHeight()));
        for (float x : { x0, x1 })
        {
            g.setColour (Theme::accent);
            g.fillRect (x - 1, 0.0f, 2.0f, r.getHeight());
            g.fillRect (x - 5, 0.0f, 10.0f, 8.0f);
        }
        g.setColour (Theme::textDim);
        g.setFont (Theme::font (10.5f));
        g.drawText (juce::String (p.endSec - p.startSec, 3) + " s" + (p.reverse ? TR ("  (reverso)") : ""),
                    getLocalBounds().reduced (6, 2), juce::Justification::topRight);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto& p = owner.session.project.pads[(size_t) owner.session.selectedPad];
        if (p.isEmpty()) return;
        range (viewA, viewB);
        handle = 0;
        if (std::abs (e.position.x - secToX (p.startSec)) < 8) handle = 1;
        else if (std::abs (e.position.x - secToX (p.endSec)) < 8) handle = 2;
        if (handle)
        {
            dragging = true;
            owner.session.beginEdit (TR ("Ajustar pad"));
        }
        else
        {
            owner.session.padDown (owner.session.selectedPad, 120);
            auditioning = true;
        }
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! handle) return;
        auto& p = owner.session.project.pads[(size_t) owner.session.selectedPad];
        auto src = owner.session.getSource (p.sourceId);
        const double maxLen = src ? src->lengthSec : 1e9;
        const double t = juce::jlimit (0.0, maxLen, xToSec (e.position.x));
        if (handle == 1) p.startSec = juce::jmin (t, p.endSec - 0.005);
        else p.endSec = juce::jmax (t, p.startSec + 0.005);
        owner.session.commit (kChPads);
        repaint();
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        if (auditioning) owner.session.padUp (owner.session.selectedPad);
        auditioning = false;
        if (handle)
        {
            auto& p = owner.session.project.pads[(size_t) owner.session.selectedPad];
            if (auto src = owner.session.getSource (p.sourceId))
            {
                if (handle == 1) p.startSec = Analysis::snapToZeroCrossing (*src->buffer, src->sampleRate, p.startSec);
                else p.endSec = Analysis::snapToZeroCrossing (*src->buffer, src->sampleRate, p.endSec);
            }
            owner.session.commit (kChPads);
        }
        handle = 0;
        dragging = false;
        repaint();
    }
    void mouseMove (const juce::MouseEvent& e) override
    {
        const auto& p = owner.session.project.pads[(size_t) owner.session.selectedPad];
        const bool near = ! p.isEmpty() && (std::abs (e.position.x - secToX (p.startSec)) < 8 || std::abs (e.position.x - secToX (p.endSec)) < 8);
        setMouseCursor (near ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::PointingHandCursor);
    }

    PadPanel& owner;
    int handle = 0;
    bool dragging = false, auditioning = false;
    double viewA = 0, viewB = 1;
};

// ============================================================================
PadPanel::PadPanel (Session& s) : session (s)
{
    for (int i = 0; i < kPadsPerBank; ++i)
    {
        auto* p = pads.add (new PadButton (*this, i));
        addAndMakeVisible (p);
    }
    for (int b = 0; b < kNumBanks; ++b)
    {
        auto& btn = bankBtns[b];
        btn.setButtonText (juce::String::charToString ((juce::juce_wchar) ('A' + b)));
        styleToggle (btn, Theme::accent);
        btn.setTooltip (TR ("Banco ") + btn.getButtonText() + " (F" + juce::String (b + 1) + ")");
        btn.onClick = [this, b] { session.currentBank = b; session.selectedPad = b * kPadsPerBank + session.selectedPad % kPadsPerBank; session.notify (kChPads | kChSelection); };
        addAndMakeVisible (btn);
    }
    wave = std::make_unique<PadWave> (*this);
    addAndMakeVisible (*wave);

    vol.formatter = [] (double v) { return formatDb ((float) v); };
    pan.formatter = [] (double v) { return std::abs (v) < 0.01 ? juce::String ("C") : (v < 0 ? "L" : "R") + juce::String ((int) std::round (std::abs (v) * 100)); };
    tune.formatter = [] (double v) { return (v > 0 ? "+" : "") + juce::String ((int) v) + " st"; };
    fine.formatter = [] (double v) { return (v > 0 ? "+" : "") + juce::String ((int) v) + " ct"; };
    attack.formatter = [] (double v) { return juce::String (v, 1) + " ms"; };
    release.formatter = [] (double v) { return juce::String ((int) v) + " ms"; };
    cutoff.formatter = [] (double v) { return v >= 19500 ? juce::String (TR ("aberto")) : v >= 1000 ? juce::String (v / 1000.0, 1) + " kHz" : juce::String ((int) v) + " Hz"; };
    reso.formatter = [] (double v) { return juce::String ((int) (v * 100)) + "%"; };
    attack.setSkew (20.0); release.setSkew (200.0); cutoff.setSkew (1200.0);
    for (auto* k : { &vol, &pan, &tune, &fine, &attack, &release, &cutoff, &reso })
    {
        addAndMakeVisible (*k);
        k->onStart = [this] { session.beginEdit (TR ("Ajustar pad")); };
    }
    vol.onChange = [this] (double v) { editPad ({}, [v] (Pad& p) { p.gainDb = (float) v; }, false); };
    pan.onChange = [this] (double v) { editPad ({}, [v] (Pad& p) { p.pan = (float) v; }, false); };
    tune.onChange = [this] (double v) { editPad ({}, [v] (Pad& p) { p.semitones = (float) v; }, false); };
    fine.onChange = [this] (double v) { editPad ({}, [v] (Pad& p) { p.fineCents = (float) v; }, false); };
    attack.onChange = [this] (double v) { editPad ({}, [v] (Pad& p) { p.attackMs = (float) v; }, false); };
    release.onChange = [this] (double v) { editPad ({}, [v] (Pad& p) { p.releaseMs = (float) v; }, false); };
    cutoff.onChange = [this] (double v) { editPad ({}, [v] (Pad& p) { p.cutoff = (float) v; }, false); };
    reso.onChange = [this] (double v) { editPad ({}, [v] (Pad& p) { p.resonance = (float) v; }, false); };

    modeBox.addItemList ({ TR ("One-shot (toca até o fim)"), TR ("Gate (enquanto segura)"), TR ("Loop (enquanto segura)") }, 1);
    modeBox.onChange = [this] { const int m = modeBox.getSelectedId() - 1; editPad (TR ("Modo do pad"), [m] (Pad& p) { p.mode = (PadMode) m; }); };
    chokeBox.addItem (TR ("Sem grupo de corte"), 1);
    for (int i = 1; i <= 8; ++i) chokeBox.addItem (TR ("Grupo de corte ") + juce::String (i), i + 1);
    chokeBox.setTooltip (TR ("Pads no mesmo grupo se cortam (ex: chimbal aberto/fechado, fatias de uma música)"));
    chokeBox.onChange = [this] { const int c = chokeBox.getSelectedId() - 1; editPad (TR ("Grupo de corte"), [c] (Pad& p) { p.chokeGroup = c; }); };
    addAndMakeVisible (modeBox);
    addAndMakeVisible (chokeBox);

    styleToggle (revBtn, Theme::accent);
    revBtn.onClick = [this] { editPad (TR ("Reverso"), [] (Pad& p) { p.reverse = ! p.reverse; }); };
    loadBtn.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> (TR ("Carregar áudio no pad"), session.lastFolder(), session.sources.wildcard());
        const int pad = session.selectedPad;
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this, pad] (const juce::FileChooser& fc)
                              {
                                  auto f = fc.getResult();
                                  if (f.existsAsFile()) { session.setLastFolder (f.getParentDirectory()); loadFileIntoPad (f, pad); }
                              });
    };
    clearBtn.onClick = [this] { session.clearPad (session.selectedPad); };
    editBtn.onClick = [this]
    {
        auto& p = session.project.pads[(size_t) session.selectedPad];
        if (! p.isEmpty() && onOpenInEditor) onOpenInEditor (p.sourceId, p.startSec, p.endSec);
    };
    for (auto* b : { &revBtn, &loadBtn, &clearBtn, &editBtn }) addAndMakeVisible (*b);

    session.addListener (this);
    refreshEditor();
    startTimerHz (60);
}

PadPanel::~PadPanel()
{
    session.removeListener (this);
}

void PadPanel::editPad (const juce::String& label, std::function<void (Pad&)> fn, bool undo)
{
    if (undo) session.beginEdit (label);
    fn (session.project.pads[(size_t) session.selectedPad]);
    session.commit (kChPads);
}

void PadPanel::refreshEditor()
{
    const auto& p = session.project.pads[(size_t) session.selectedPad];
    vol.setValue (p.gainDb);
    pan.setValue (p.pan);
    tune.setValue (p.semitones);
    fine.setValue (p.fineCents);
    attack.setValue (p.attackMs);
    release.setValue (p.releaseMs);
    cutoff.setValue (p.cutoff);
    reso.setValue (p.resonance);
    modeBox.setSelectedId ((int) p.mode + 1, juce::dontSendNotification);
    chokeBox.setSelectedId (p.chokeGroup + 1, juce::dontSendNotification);
    revBtn.setToggleState (p.reverse, juce::dontSendNotification);
    for (int b = 0; b < kNumBanks; ++b) bankBtns[b].setToggleState (session.currentBank == b, juce::dontSendNotification);
    wave->repaint();
    repaint();
}

void PadPanel::sessionChanged (int what)
{
    if (what & (kChPads | kChSelection | kChSources)) refreshEditor();
}

void PadPanel::timerCallback()
{
    auto& r = session.engine.getRenderer();
    const int bankStart = session.currentBank * kPadsPerBank;
    for (int i = 0; i < kNumPads; ++i)
    {
        const int a = r.padActivity[(size_t) i].load();
        bool changed = false;
        if (a != lastActivity[(size_t) i]) { lastActivity[(size_t) i] = a; flash[(size_t) i] = 1.0f; changed = true; }
        else if (flash[(size_t) i] > 0)
        {
            // decaimento suave (exponencial) em vez de degraus
            flash[(size_t) i] = flash[(size_t) i] < 0.02f ? 0.0f : flash[(size_t) i] * 0.88f;
            changed = true;
        }
        // so redesenha os pads do banco visivel que mudaram
        if (changed && i >= bankStart && i < bankStart + kPadsPerBank && isShowing())
            for (auto* p : pads) if (p->padIndex() == i) p->repaint();
    }
}

int PadPanel::padAt (juce::Point<int> pt) const
{
    for (auto* p : pads)
        if (p->getBounds().contains (pt)) return p->padIndex();
    return -1;
}

void PadPanel::loadFileIntoPad (const juce::File& f, int pad)
{
    juce::Component::SafePointer<PadPanel> safe (this);
    session.importFile (f, [safe, pad, name = f.getFileNameWithoutExtension()] (int sourceId)
    {
        if (safe == nullptr) return;
        auto src = safe->session.getSource (sourceId);
        if (src == nullptr) return;
        safe->session.assignPad (pad, sourceId, 0.0, src->lengthSec, name.substring (0, 24));
        safe->session.selectedPad = pad;
        safe->session.notify (kChSelection);
    });
}

void PadPanel::filesDropped (const juce::StringArray& files, int x, int y)
{
    int pad = padAt ({ x, y });
    if (pad < 0) pad = session.selectedPad;
    for (auto& f : files)
    {
        juce::File file (f);
        if (file.existsAsFile() && pad < kNumPads) loadFileIntoPad (file, pad++);
    }
    dragOverPad = -1;
    repaint();
}

void PadPanel::itemDropped (const SourceDetails& d)
{
    int pad = padAt (d.localPosition);
    if (pad < 0) pad = session.selectedPad;
    dragOverPad = -1;
    const auto desc = d.description.toString();
    if (desc.startsWith ("file:")) loadFileIntoPad (juce::File (desc.fromFirstOccurrenceOf ("file:", false, false)), pad);
    else if (desc.startsWith ("source:"))
    {
        const int id = desc.fromFirstOccurrenceOf ("source:", false, false).getIntValue();
        if (auto src = session.getSource (id))
            session.assignPad (pad, id, 0.0, src->lengthSec, session.sourceName (id).substring (0, 24));
    }
    else if (auto* tree = dynamic_cast<juce::FileTreeComponent*> (d.sourceComponent.get()))
    {
        if (tree->getSelectedFile().existsAsFile()) loadFileIntoPad (tree->getSelectedFile(), pad);
    }
    else if (desc.startsWith ("range:"))
    {
        auto parts = juce::StringArray::fromTokens (desc.fromFirstOccurrenceOf ("range:", false, false), ";", "");
        if (parts.size() >= 3)
            session.assignPad (pad, parts[0].getIntValue(), parts[1].getDoubleValue(), parts[2].getDoubleValue(), TR ("Trecho"));
    }
    repaint();
}

void PadPanel::resized()
{
    auto r = getLocalBounds().reduced (8, 6);
    r.removeFromTop (18);
    auto banks = r.removeFromLeft (30);
    for (int b = 0; b < kNumBanks; ++b)
        bankBtns[b].setBounds (banks.removeFromTop (banks.getHeight() / (kNumBanks - b)).reduced (0, 2));
    r.removeFromLeft (6);
    const int padSize = juce::jmin ((r.getHeight()) / 4, 74);
    padArea = r.removeFromLeft (padSize * 4 + 4).withHeight (padSize * 4);
    for (int i = 0; i < kPadsPerBank; ++i)
    {
        const int row = 3 - i / 4, col = i % 4;
        pads[i]->setBounds (padArea.getX() + col * padSize, padArea.getY() + row * padSize, padSize, padSize);
    }
    r.removeFromLeft (14);
    editorArea = r;
    auto e = r;
    e.removeFromTop (2);
    auto waveR = e.removeFromTop (juce::jmax (60, e.getHeight() - 130));
    wave->setBounds (waveR);
    e.removeFromTop (6);
    auto knobs = e.removeFromTop (70);
    const int kw = juce::jmin (64, knobs.getWidth() / 8);
    for (auto* k : { &vol, &pan, &tune, &fine, &attack, &release, &cutoff, &reso }) k->setBounds (knobs.removeFromLeft (kw));
    e.removeFromTop (6);
    auto row = e.removeFromTop (26);
    modeBox.setBounds (row.removeFromLeft (190)); row.removeFromLeft (6);
    chokeBox.setBounds (row.removeFromLeft (160)); row.removeFromLeft (6);
    revBtn.setBounds (row.removeFromLeft (76)); row.removeFromLeft (6);
    loadBtn.setBounds (row.removeFromLeft (92)); row.removeFromLeft (6);
    editBtn.setBounds (row.removeFromLeft (118)); row.removeFromLeft (6);
    clearBtn.setBounds (row.removeFromLeft (66));
}

void PadPanel::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel);
    g.setColour (Theme::outline);
    g.drawHorizontalLine (0, 0, (float) getWidth());
    auto r = getLocalBounds().reduced (8, 4);
    auto title = r.removeFromTop (16);
    g.setFont (Theme::font (11.0f, true));
    g.setColour (Theme::textFaint);
    g.drawText (TR ("PADS   (F1-F4 = bancos)"), title.withWidth (editorArea.getX() - title.getX() - 8), juce::Justification::centredLeft);
    const auto& p = session.project.pads[(size_t) session.selectedPad];
    g.setColour (Theme::text);
    g.setFont (Theme::font (12.5f, true));
    auto ed = juce::Rectangle<int> (editorArea.getX(), title.getY(), editorArea.getWidth(), 16);
    g.drawText ("Pad " + juce::String::charToString ((juce::juce_wchar) ('A' + session.selectedPad / kPadsPerBank))
                + juce::String (session.selectedPad % kPadsPerBank + 1) + "  -  " + p.name
                + (p.isEmpty() ? juce::String() : "   (" + session.sourceName (p.sourceId) + ")"),
                ed, juce::Justification::centredLeft);
}

} // namespace mp
