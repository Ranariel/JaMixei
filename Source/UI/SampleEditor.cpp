#include "SampleEditor.h"
#include "../Audio/Analysis.h"

namespace mp
{
// ============================================================================
//  Visao geral (musica inteira) - arraste para navegar
// ============================================================================
class SampleEditor::Overview : public juce::Component
{
public:
    explicit Overview (SampleEditor& o) : owner (o) {}
    void paint (juce::Graphics& g) override
    {
        g.fillAll (Theme::bg);
        auto src = owner.session.getSource (owner.session.editorSourceId);
        if (src == nullptr) return;
        auto r = getLocalBounds().toFloat();
        drawWaveform (g, *src, r.reduced (0, 2), 0.0, src->lengthSec, Theme::textDim);
        const float x0 = (float) (owner.viewStart / src->lengthSec * getWidth());
        const float x1 = (float) (owner.viewEnd / src->lengthSec * getWidth());
        g.setColour (Theme::accent.withAlpha (0.15f));
        g.fillRect (juce::Rectangle<float> (x0, 0, juce::jmax (2.0f, x1 - x0), r.getHeight()));
        g.setColour (Theme::accent);
        g.drawRect (juce::Rectangle<float> (x0, 0, juce::jmax (2.0f, x1 - x0), r.getHeight()), 1.5f);
        if (owner.selB > owner.selA)
        {
            g.setColour (Theme::accent2.withAlpha (0.35f));
            g.fillRect (juce::Rectangle<float> ((float) (owner.selA / src->lengthSec * getWidth()), r.getHeight() - 4,
                                                (float) ((owner.selB - owner.selA) / src->lengthSec * getWidth()), 4.0f));
        }
    }
    void mouseDown (const juce::MouseEvent& e) override { moveTo (e.position.x); }
    void mouseDrag (const juce::MouseEvent& e) override { moveTo (e.position.x); }
    void moveTo (float x)
    {
        auto src = owner.session.getSource (owner.session.editorSourceId);
        if (src == nullptr) return;
        const double span = owner.viewEnd - owner.viewStart;
        const double centre = x / getWidth() * src->lengthSec;
        owner.setView (centre - span / 2, centre + span / 2);
    }
    SampleEditor& owner;
};

// ============================================================================
//  Forma de onda principal: selecao, marcadores, grade
// ============================================================================
class SampleEditor::Wave : public juce::Component
{
public:
    explicit Wave (SampleEditor& o) : owner (o) {}

    double xToSec (float x) const { return owner.viewStart + x / getWidth() * (owner.viewEnd - owner.viewStart); }
    float secToX (double s) const { return (float) ((s - owner.viewStart) / (owner.viewEnd - owner.viewStart) * getWidth()); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Theme::bg);
        auto src = owner.session.getSource (owner.session.editorSourceId);
        auto r = getLocalBounds().toFloat();
        if (src == nullptr)
        {
            g.setColour (Theme::textFaint);
            g.setFont (Theme::font (16.0f));
            g.drawFittedText (owner.session.project.sources.empty()
                                  ? TR ("Importe uma música (botão Importar ou arraste do navegador) para fatiar e mandar para os pads.")
                                  : TR ("Escolha um áudio na lista acima."),
                              getLocalBounds().reduced (30), juce::Justification::centred, 3, 1.0f);
            return;
        }
        // grade de batidas
        if (owner.gridBtn.getToggleState())
            if (auto* info = owner.session.project.findSource (owner.session.editorSourceId))
            {
                const double spb = 60.0 / juce::jmax (20.0, info->bpm);
                double first = src->beatOffsetSec;
                while (first > owner.viewStart) first -= spb;
                const double pxPerBeat = spb / (owner.viewEnd - owner.viewStart) * getWidth();
                if (pxPerBeat > 4)
                {
                    int k = (int) std::floor ((owner.viewStart - src->beatOffsetSec) / spb);
                    for (double t = src->beatOffsetSec + k * spb; t < owner.viewEnd; t += spb, ++k)
                    {
                        const float x = secToX (t);
                        const bool bar = ((k % 4) + 4) % 4 == 0;
                        g.setColour (bar ? Theme::gridStrong.brighter (0.15f) : Theme::grid);
                        g.fillRect (x, 0.0f, 1.0f, r.getHeight());
                    }
                }
            }
        g.setColour (Theme::grid);
        g.drawHorizontalLine ((int) r.getCentreY(), 0, r.getWidth());

        drawWaveform (g, *src, r.reduced (0, 8), owner.viewStart, owner.viewEnd, Theme::accent2.withAlpha (0.85f));

        // selecao
        if (owner.selB > owner.selA)
        {
            const float x0 = secToX (owner.selA), x1 = secToX (owner.selB);
            g.setColour (Theme::accent.withAlpha (0.16f));
            g.fillRect (juce::Rectangle<float> (x0, 0, x1 - x0, r.getHeight()));
            g.setColour (Theme::accent);
            g.fillRect (x0, 0.0f, 1.5f, r.getHeight());
            g.fillRect (x1 - 1.5f, 0.0f, 1.5f, r.getHeight());
        }
        // marcadores
        auto& m = owner.markers();
        g.setFont (Theme::font (10.5f, true));
        float lastTag = -1000.0f;
        for (size_t i = 0; i < m.size(); ++i)
        {
            const float x = secToX (m[i]);
            if (x < -20 || x > getWidth() + 20) continue;
            g.setColour (Theme::warn);
            g.fillRect (x - 0.5f, 0.0f, 1.5f, r.getHeight());
            if (x - lastTag < 26.0f) continue;
            lastTag = x;
            auto tag = juce::Rectangle<float> (x, 0, 22, 14);
            g.fillRoundedRectangle (tag, 2.0f);
            g.setColour (juce::Colours::black);
            g.drawText (juce::String ((int) i + 1), tag, juce::Justification::centred);
        }
        // pre-escuta
        const double pp = owner.session.engine.getPreviewPositionSec();
        if (pp >= 0)
        {
            g.setColour (Theme::playhead);
            g.fillRect (secToX (pp), 0.0f, 1.5f, r.getHeight());
        }
        // tempo
        g.setColour (Theme::textDim);
        g.setFont (Theme::mono (11.0f));
        auto fmt = [] (double s) { return juce::String ((int) s / 60) + ":" + juce::String (std::fmod (s, 60.0), 3).paddedLeft ('0', 6); };
        g.drawText (fmt (owner.viewStart), 6, getHeight() - 16, 100, 14, juce::Justification::left);
        g.drawText (fmt (owner.viewEnd), getWidth() - 106, getHeight() - 16, 100, 14, juce::Justification::right);
        if (owner.selB > owner.selA)
            g.drawText (TR ("seleção ") + fmt (owner.selA) + " - " + fmt (owner.selB) + "  (" + juce::String (owner.selB - owner.selA, 3) + " s)",
                        getLocalBounds().withHeight (16).withY (getHeight() - 16), juce::Justification::centred);
    }

    int markerAt (float x)
    {
        auto& m = owner.markers();
        for (int i = 0; i < (int) m.size(); ++i) if (std::abs (secToX (m[(size_t) i]) - x) < 6) return i;
        return -1;
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        setMouseCursor (markerAt (e.position.x) >= 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::IBeamCursor);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        auto src = owner.session.getSource (owner.session.editorSourceId);
        if (src == nullptr) return;
        dragMarker = markerAt (e.position.x);
        moved = false;
        if (dragMarker >= 0 && e.mods.isPopupMenu())
        {
            auto& m = owner.markers();
            m.erase (m.begin() + dragMarker);
            dragMarker = -1;
            owner.refreshInfo();
            repaint();
            return;
        }
        if (e.mods.isPopupMenu()) { showMenu (xToSec (e.position.x)); return; }
        if (e.mods.isCommandDown() && owner.selB > owner.selA)
        {
            // Ctrl + arrastar: leva a selecao para o arranjo ou para um pad
            if (auto* dc = juce::DragAndDropContainer::findParentDragContainerFor (this))
            {
                dc->startDragging ("range:" + juce::String (owner.session.editorSourceId) + ";" + juce::String (owner.selA, 6) + ";" + juce::String (owner.selB, 6),
                                   this, juce::ScaledImage(), true);
                return;
            }
        }
        anchor = juce::jlimit (0.0, src->lengthSec, xToSec (e.position.x));
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        auto src = owner.session.getSource (owner.session.editorSourceId);
        if (src == nullptr || e.mods.isPopupMenu()) return;
        if (e.getDistanceFromDragStart() < 3 && ! moved) return;
        moved = true;
        const double t = juce::jlimit (0.0, src->lengthSec, xToSec (e.position.x));
        if (dragMarker >= 0)
        {
            owner.markers()[(size_t) dragMarker] = t;
        }
        else
        {
            owner.selA = juce::jmin (anchor, t);
            owner.selB = juce::jmax (anchor, t);
        }
        repaint();
        owner.overview->repaint();
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        auto src = owner.session.getSource (owner.session.editorSourceId);
        if (src == nullptr || e.mods.isPopupMenu()) return;
        auto& m = owner.markers();
        if (dragMarker >= 0)
        {
            m[(size_t) dragMarker] = Analysis::snapToZeroCrossing (*src->buffer, src->sampleRate, m[(size_t) dragMarker]);
            std::sort (m.begin(), m.end());
        }
        else if (moved)
        {
            owner.selA = Analysis::snapToZeroCrossing (*src->buffer, src->sampleRate, owner.selA);
            owner.selB = Analysis::snapToZeroCrossing (*src->buffer, src->sampleRate, owner.selB);
        }
        else
        {
            // clique: toca a fatia sob o mouse (entre marcadores)
            const double t = xToSec (e.position.x);
            double a = 0, b = src->lengthSec;
            if (! m.empty())
            {
                for (double mk : m) { if (mk <= t) a = mk; else { b = mk; break; } }
            }
            else
            {
                a = t;
                b = juce::jmin (src->lengthSec, t + 8.0);
            }
            if (! m.empty() || ! (owner.selB > owner.selA && t >= owner.selA && t <= owner.selB))
            {
                if (! m.empty()) { owner.selA = a; owner.selB = b; }
                owner.session.engine.previewBuffer (src->buffer, src->sampleRate, a, b);
            }
            else owner.session.engine.previewBuffer (src->buffer, src->sampleRate, t, owner.selB);
        }
        dragMarker = -1;
        owner.refreshInfo();
        repaint();
        owner.overview->repaint();
    }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        auto src = owner.session.getSource (owner.session.editorSourceId);
        if (src == nullptr) return;
        auto& m = owner.markers();
        m.push_back (Analysis::snapToZeroCrossing (*src->buffer, src->sampleRate, juce::jlimit (0.0, src->lengthSec, xToSec (e.position.x))));
        std::sort (m.begin(), m.end());
        owner.refreshInfo();
        repaint();
    }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        const double span = owner.viewEnd - owner.viewStart;
        if (e.mods.isShiftDown() || std::abs (w.deltaX) > std::abs (w.deltaY))
        {
            const float d = std::abs (w.deltaX) > std::abs (w.deltaY) ? w.deltaX : w.deltaY;
            owner.setView (owner.viewStart - d * span * 0.5, owner.viewEnd - d * span * 0.5);
        }
        else
        {
            const double anchorT = xToSec (e.position.x);
            const double f = w.deltaY > 0 ? 0.8 : 1.25;
            owner.setView (anchorT - (anchorT - owner.viewStart) * f, anchorT + (owner.viewEnd - anchorT) * f);
        }
    }

    void showMenu (double t)
    {
        juce::PopupMenu m;
        m.addItem (1, TR ("Adicionar marcador aqui"));
        m.addItem (2, TR ("Ouvir seleção"), owner.selB > owner.selA);
        m.addItem (3, TR ("Selecionar tudo"));
        m.addItem (4, TR ("Limpar seleção"));
        m.showMenuAsync (juce::PopupMenu::Options(), [this, t] (int r)
        {
            auto src = owner.session.getSource (owner.session.editorSourceId);
            if (src == nullptr) return;
            if (r == 1) { owner.markers().push_back (t); std::sort (owner.markers().begin(), owner.markers().end()); }
            if (r == 2) owner.auditionSelection();
            if (r == 3) { owner.selA = 0; owner.selB = src->lengthSec; }
            if (r == 4) { owner.selA = owner.selB = -1; }
            owner.refreshInfo();
            repaint();
        });
    }

    SampleEditor& owner;
    int dragMarker = -1;
    bool moved = false;
    double anchor = 0;
};

// ============================================================================
SampleEditor::SampleEditor (Session& s) : session (s)
{
    overview = std::make_unique<Overview> (*this);
    wave = std::make_unique<Wave> (*this);
    addAndMakeVisible (*overview);
    addAndMakeVisible (*wave);

    for (auto* c : std::initializer_list<juce::Component*> { &sourceBox, &chopMode, &divBox, &beatBox, &firstPadBox, &importBtn, &detectBtn,
                                                              &x2Btn, &halfBtn, &gridBtn, &zoomInBtn, &zoomOutBtn, &zoomAllBtn, &zoomSelBtn, &chopBtn,
                                                              &clearBtn, &toPadsBtn, &selPadBtn, &selArrBtn, &playSelBtn, &exportBtn, &chokeToggle, &sens, &bpmLabel })
        addAndMakeVisible (c);

    sourceBox.setTextWhenNothingSelected (TR ("(nenhum áudio no projeto)"));
    sourceBox.onChange = [this]
    {
        const int idx = sourceBox.getSelectedItemIndex();
        if (juce::isPositiveAndBelow (idx, (int) session.project.sources.size()))
        {
            const int id = session.project.sources[(size_t) idx].id;
            if (id != session.editorSourceId) open (id, -1, -1);
        }
    };
    importBtn.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> (TR ("Importar áudio"), session.lastFolder(), session.sources.wildcard());
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  auto f = fc.getResult();
                                  if (! f.existsAsFile()) return;
                                  session.setLastFolder (f.getParentDirectory());
                                  juce::Component::SafePointer<SampleEditor> safe (this);
                                  session.importFile (f, [safe] (int id) { if (safe) safe->open (id, -1, -1); });
                              });
    };

    bpmLabel.setEditable (false, true, false);
    bpmLabel.setJustificationType (juce::Justification::centred);
    bpmLabel.setColour (juce::Label::backgroundColourId, Theme::panel2);
    bpmLabel.setColour (juce::Label::outlineColourId, Theme::outline);
    bpmLabel.setFont (Theme::mono (14.0f, true));
    bpmLabel.setTooltip (TR ("BPM deste áudio (duplo clique para digitar). Usado no warp e na grade."));
    bpmLabel.onTextChange = [this]
    {
        const double v = bpmLabel.getText().retainCharacters ("0123456789.,").replaceCharacter (',', '.').getDoubleValue();
        if (auto* info = session.project.findSource (session.editorSourceId))
            if (v >= 30 && v <= 300)
            {
                session.beginEdit (TR ("BPM do áudio"));
                info->bpm = v; info->bpmDetected = true;
                for (auto& c : session.project.clips) if (c.sourceId == info->id) c.sourceBpm = v;
                session.commit (kChSources | kChClips);
            }
        refreshInfo();
    };
    auto scaleBpm = [this] (double f)
    {
        if (auto* info = session.project.findSource (session.editorSourceId))
        {
            session.beginEdit (TR ("BPM do áudio"));
            info->bpm *= f;
            for (auto& c : session.project.clips) if (c.sourceId == info->id) c.sourceBpm *= f;
            session.commit (kChSources | kChClips);
            refreshInfo();
        }
    };
    x2Btn.onClick = [scaleBpm] { scaleBpm (2.0); };
    halfBtn.onClick = [scaleBpm] { scaleBpm (0.5); };
    detectBtn.onClick = [this]
    {
        auto src = session.getSource (session.editorSourceId);
        auto* info = session.project.findSource (session.editorSourceId);
        if (src == nullptr || info == nullptr) return;
        const double bpm = Analysis::detectBpm (*src->buffer, src->sampleRate);
        if (bpm > 0)
        {
            session.beginEdit (TR ("BPM do áudio"));
            info->bpm = bpm; info->bpmDetected = true;
            for (auto& c : session.project.clips) if (c.sourceId == info->id) c.sourceBpm = bpm;
            session.commit (kChSources | kChClips);
            session.setStatus (TR ("BPM detectado: ") + juce::String (bpm, 2));
        }
        else session.setStatus (TR ("Não foi possível detectar o BPM deste áudio"));
        refreshInfo();
    };
    styleToggle (gridBtn, Theme::accent2);
    gridBtn.setToggleState (true, juce::dontSendNotification);
    gridBtn.setTooltip (TR ("Mostra a grade de batidas (BPM)"));
    gridBtn.onClick = [this] { gridBtn.setToggleState (! gridBtn.getToggleState(), juce::dontSendNotification); wave->repaint(); };
    zoomInBtn.onClick = [this] { const double c = (viewStart + viewEnd) / 2, s = (viewEnd - viewStart) * 0.35; setView (c - s, c + s); };
    zoomOutBtn.onClick = [this] { const double c = (viewStart + viewEnd) / 2, s = (viewEnd - viewStart) * 1.4; setView (c - s, c + s); };
    zoomAllBtn.onClick = [this] { if (auto src = session.getSource (session.editorSourceId)) setView (0, src->lengthSec); };
    zoomSelBtn.onClick = [this] { if (selB > selA) { const double m = (selB - selA) * 0.05; setView (selA - m, selB + m); } };

    chopMode.addItemList ({ TR ("Fatiar por transientes (batidas)"), TR ("Fatiar em partes iguais"), TR ("Fatiar pela grade (BPM)") }, 1);
    chopMode.setSelectedId (1, juce::dontSendNotification);
    chopMode.onChange = [this] { resized(); };
    divBox.addItemList ({ TR ("2 partes"), TR ("4 partes"), TR ("8 partes"), TR ("16 partes"), TR ("32 partes"), TR ("64 partes") }, 1);
    divBox.setSelectedId (4, juce::dontSendNotification);
    beatBox.addItemList ({ TR ("a cada 1/4 de tempo"), TR ("a cada 1/2 tempo"), TR ("a cada tempo"), TR ("a cada 2 tempos"), TR ("a cada compasso") }, 1);
    beatBox.setSelectedId (3, juce::dontSendNotification);
    sens.formatter = [] (double v) { return juce::String ((int) (v * 100)) + "%"; };
    chopBtn.setColour (juce::TextButton::buttonColourId, Theme::accent.withAlpha (0.35f));
    chopBtn.onClick = [this] { doChop(); };
    chopBtn.setTooltip (TR ("Cria marcadores dentro da seleção (ou no áudio todo). Duplo clique adiciona marcador manual; botão direito num marcador apaga."));
    clearBtn.onClick = [this] { markers().clear(); refreshInfo(); wave->repaint(); };

    for (int i = 0; i < kNumPads; ++i)
        firstPadBox.addItem (TR ("começando no pad ") + juce::String::charToString ((juce::juce_wchar) ('A' + i / kPadsPerBank)) + juce::String (i % kPadsPerBank + 1), i + 1);
    firstPadBox.setSelectedId (1, juce::dontSendNotification);
    chokeToggle.setToggleState (true, juce::dontSendNotification);
    toPadsBtn.setColour (juce::TextButton::buttonColourId, Theme::accent.withAlpha (0.35f));
    toPadsBtn.onClick = [this] { sendSlicesToPads(); };
    selPadBtn.onClick = [this] { selectionToPad(); };
    selArrBtn.onClick = [this] { selectionToArrange(); };
    selArrBtn.setTooltip (TR ("Coloca a seleção como novo clipe no arranjo, na posição do cursor (Ctrl+arrastar a seleção também funciona)"));
    playSelBtn.onClick = [this] { auditionSelection(); };
    exportBtn.onClick = [this] { exportSelection(); };

    session.addListener (this);
    refreshSources();
    startTimerHz (60);
}

SampleEditor::~SampleEditor() { session.removeListener (this); }

std::vector<double>& SampleEditor::markers() { return markerMap[session.editorSourceId]; }

void SampleEditor::open (int sourceId, double s, double e)
{
    session.editorSourceId = sourceId;
    auto src = session.getSource (sourceId);
    if (s >= 0 && e > s)
    {
        selA = s; selB = e;
        const double m = (e - s) * 0.08;
        setView (s - m, e + m);
    }
    else
    {
        selA = selB = -1;
        if (src) setView (0, src->lengthSec);
    }
    refreshSources();
}

void SampleEditor::setView (double a, double b)
{
    auto src = session.getSource (session.editorSourceId);
    const double len = src ? src->lengthSec : 10.0;
    double span = juce::jlimit (0.005, juce::jmax (0.01, len), b - a);
    a = juce::jlimit (0.0, juce::jmax (0.0, len - span), a);
    viewStart = a;
    viewEnd = a + span;
    wave->repaint();
    overview->repaint();
}

void SampleEditor::refreshSources()
{
    sourceBox.clear (juce::dontSendNotification);
    int sel = -1;
    for (size_t i = 0; i < session.project.sources.size(); ++i)
    {
        auto& si = session.project.sources[i];
        sourceBox.addItem (si.name, (int) i + 1);
        if (si.id == session.editorSourceId) sel = (int) i;
    }
    if (sel < 0 && ! session.project.sources.empty() && session.getSource (session.project.sources.back().id))
    {
        sel = (int) session.project.sources.size() - 1;
        session.editorSourceId = session.project.sources.back().id;
        if (auto src = session.getSource (session.editorSourceId)) { viewStart = 0; viewEnd = src->lengthSec; }
    }
    sourceBox.setSelectedItemIndex (sel, juce::dontSendNotification);
    refreshInfo();
    wave->repaint();
    overview->repaint();
}

void SampleEditor::refreshInfo()
{
    if (auto* info = session.project.findSource (session.editorSourceId))
        bpmLabel.setText (juce::String (info->bpm, 2), juce::dontSendNotification);
    else bpmLabel.setText ("-", juce::dontSendNotification);
    const bool hasSel = selB > selA;
    for (auto* b : { &selPadBtn, &selArrBtn, &playSelBtn, &exportBtn, &zoomSelBtn }) b->setEnabled (hasSel);
    int count = 0;
    {
        double a, b;
        if (rangeForAction (a, b))
        {
            for (double mk : markers()) if (mk > a + 0.005 && mk < b - 0.005) ++count;
            count += 1;
        }
    }
    toPadsBtn.setButtonText (markers().empty() ? TR ("Fatias -> Pads") : TR ("Fatias (") + juce::String (juce::jmin (count, kNumPads)) + TR (") -> Pads"));
    repaint();
}

void SampleEditor::sessionChanged (int what)
{
    if (what & kChSources)
    {
        // audio recem carregado: abre automaticamente se nada estiver aberto
        if (session.getSource (session.editorSourceId) == nullptr) session.editorSourceId = -1;
        refreshSources();
    }
}

void SampleEditor::timerCallback()
{
    static double lastPP = -2;
    static int lastX = -100000;
    const double pp = session.engine.getPreviewPositionSec();
    if (pp == lastPP || ! isShowing()) return;
    lastPP = pp;
    // redesenha so a faixa por onde o cursor da pre-escuta passou (bem mais leve)
    const int x = pp >= 0 ? (int) wave->secToX (pp) : -100000;
    if (x == lastX) return;
    if (lastX > -100000) wave->repaint (lastX - 3, 0, 7, wave->getHeight());
    if (x > -100000) wave->repaint (x - 3, 0, 7, wave->getHeight());
    lastX = x;
}

bool SampleEditor::rangeForAction (double& a, double& b) const
{
    auto src = session.getSource (session.editorSourceId);
    if (src == nullptr) return false;
    if (selB > selA) { a = selA; b = selB; }
    else { a = 0; b = src->lengthSec; }
    return b > a;
}

void SampleEditor::doChop()
{
    auto src = session.getSource (session.editorSourceId);
    double a, b;
    if (src == nullptr || ! rangeForAction (a, b)) return;
    auto& m = markers();
    // remove marcadores dentro da faixa
    m.erase (std::remove_if (m.begin(), m.end(), [a, b] (double x) { return x >= a - 1e-6 && x <= b + 1e-6; }), m.end());
    std::vector<double> pts;
    const int mode = chopMode.getSelectedId();
    if (mode == 1)
    {
        pts = Analysis::detectTransients (*src->buffer, src->sampleRate, a, b, (float) sens.getValue());
        if (pts.size() > 64)
        {
            session.setStatus (TR ("Muitos transientes (") + juce::String ((int) pts.size()) + TR ("): reduza a sensibilidade ou selecione um trecho menor"));
            pts.resize (64);
        }
    }
    else if (mode == 2)
    {
        static const int divs[] = { 2, 4, 8, 16, 32, 64 };
        const int n = divs[juce::jlimit (0, 5, divBox.getSelectedId() - 1)];
        for (int i = 0; i < n; ++i) pts.push_back (a + (b - a) * i / n);
    }
    else
    {
        static const double beats[] = { 0.25, 0.5, 1.0, 2.0, 4.0 };
        const double step = beats[juce::jlimit (0, 4, beatBox.getSelectedId() - 1)];
        auto* info = session.project.findSource (session.editorSourceId);
        const double spb = 60.0 / juce::jmax (20.0, info ? info->bpm : 120.0);
        for (double t = a; t < b - 0.01 && pts.size() < 256; t += spb * step) pts.push_back (t);
    }
    for (auto& t : pts) t = Analysis::snapToZeroCrossing (*src->buffer, src->sampleRate, t);
    if (pts.empty() || pts.front() > a + 0.01) pts.insert (pts.begin(), a);
    m.insert (m.end(), pts.begin(), pts.end());
    std::sort (m.begin(), m.end());
    m.erase (std::unique (m.begin(), m.end(), [] (double x, double y) { return std::abs (x - y) < 0.005; }), m.end());
    if (selA < 0) { selA = a; selB = b; }
    session.setStatus (juce::String ((int) pts.size()) + TR (" fatias criadas. Clique numa fatia para ouvir; depois 'Fatias -> Pads'."));
    refreshInfo();
    wave->repaint();
}

void SampleEditor::sendSlicesToPads()
{
    auto src = session.getSource (session.editorSourceId);
    double a, b;
    if (src == nullptr || ! rangeForAction (a, b)) return;
    std::vector<double> pts;
    for (double m : markers()) if (m >= a - 1e-6 && m < b - 0.005) pts.push_back (m);
    if (pts.empty() || pts.front() > a + 0.005) pts.insert (pts.begin(), a);
    const int first = firstPadBox.getSelectedId() - 1;
    const int n = session.chopToPads (session.editorSourceId, pts, b, first);
    if (chokeToggle.getToggleState() && n > 0)
    {
        for (int i = first; i < first + n && i < kNumPads; ++i) session.project.pads[(size_t) i].chokeGroup = 1;
        session.commit (kChPads);
    }
    session.currentBank = first / kPadsPerBank;
    session.selectedPad = first;
    session.notify (kChSelection | kChPads);
    session.setStatus (juce::String (n) + TR (" fatias enviadas para os pads (teclas Z X C V / A S D F / Q W E R / 1 2 3 4)"));
}

void SampleEditor::selectionToPad()
{
    if (! (selB > selA)) return;
    session.assignPad (session.selectedPad, session.editorSourceId, selA, selB, TR ("Trecho"));
    session.setStatus (TR ("Seleção enviada para o pad ") + session.project.pads[(size_t) session.selectedPad].name);
}

void SampleEditor::selectionToArrange()
{
    if (! (selB > selA)) return;
    auto& p = session.project;
    int trackId = -1;
    if (auto* t = p.findTrack (session.selectedTrackId)) if (t->type == TrackType::Audio) trackId = t->id;
    if (trackId < 0) for (auto& t : p.tracks) if (t.type == TrackType::Audio) { trackId = t.id; break; }
    session.beginEdit (TR ("Seleção para o arranjo"));
    if (trackId < 0) trackId = p.addTrack (TrackType::Audio, TR ("Faixa")).id;
    const int id = session.addAudioClip (trackId, session.editorSourceId, session.engine.getPositionBeats(), selA, selB - selA);
    if (id >= 0) session.selectedClips = { id };
    session.commit (kChAll);
    session.setStatus (TR ("Trecho adicionado ao arranjo no cursor"));
}

void SampleEditor::auditionSelection()
{
    auto src = session.getSource (session.editorSourceId);
    if (src == nullptr || ! (selB > selA)) return;
    session.engine.previewBuffer (src->buffer, src->sampleRate, selA, selB);
}

void SampleEditor::exportSelection()
{
    auto src = session.getSource (session.editorSourceId);
    if (src == nullptr || ! (selB > selA)) return;
    auto def = session.lastFolder().getChildFile (session.sourceName (session.editorSourceId) + TR (" - trecho.wav"));
    chooser = std::make_unique<juce::FileChooser> (TR ("Exportar seleção como WAV"), def, "*.wav");
    const double a = selA, b = selB;
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this, src, a, b] (const juce::FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (f == juce::File()) return;
                              f = f.withFileExtension (".wav");
                              f.deleteFile();
                              juce::WavAudioFormat wav;
                              std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), src->sampleRate,
                                                                          (unsigned int) src->buffer->getNumChannels(), 24, {}, 0));
                              if (w == nullptr) { session.setStatus (TR ("Erro ao exportar")); return; }
                              const int s0 = (int) (a * src->sampleRate), n = (int) ((b - a) * src->sampleRate);
                              w->writeFromAudioSampleBuffer (*src->buffer, s0, juce::jmin (n, src->buffer->getNumSamples() - s0));
                              session.setStatus (TR ("Seleção exportada: ") + f.getFileName());
                          });
}

void SampleEditor::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    auto top = r.removeFromTop (28);
    sourceBox.setBounds (top.removeFromLeft (260)); top.removeFromLeft (6);
    importBtn.setBounds (top.removeFromLeft (92)); top.removeFromLeft (44);
    bpmLabel.setBounds (top.removeFromLeft (74)); top.removeFromLeft (4);
    detectBtn.setBounds (top.removeFromLeft (104)); top.removeFromLeft (4);
    x2Btn.setBounds (top.removeFromLeft (32)); top.removeFromLeft (3);
    halfBtn.setBounds (top.removeFromLeft (32)); top.removeFromLeft (3);
    gridBtn.setBounds (top.removeFromLeft (58));
    zoomSelBtn.setBounds (top.removeFromRight (74)); top.removeFromRight (3);
    zoomAllBtn.setBounds (top.removeFromRight (50)); top.removeFromRight (3);
    zoomOutBtn.setBounds (top.removeFromRight (30)); top.removeFromRight (3);
    zoomInBtn.setBounds (top.removeFromRight (30));

    r.removeFromTop (8);
    auto bottom2 = r.removeFromBottom (30);
    r.removeFromBottom (6);
    auto bottom1 = r.removeFromBottom (54);
    r.removeFromBottom (8);
    overview->setBounds (r.removeFromTop (44));
    r.removeFromTop (18);
    wave->setBounds (r);

    auto b1 = bottom1;
    chopMode.setBounds (b1.removeFromLeft (250).withSizeKeepingCentre (250, 28)); b1.removeFromLeft (8);
    const int mode = chopMode.getSelectedId();
    sens.setVisible (mode == 1);
    divBox.setVisible (mode == 2);
    beatBox.setVisible (mode == 3);
    sens.setBounds (b1.removeFromLeft (56));
    if (mode != 1) { b1 = b1.withX (sens.getX()); }
    divBox.setBounds (b1.withWidth (160).withSizeKeepingCentre (160, 28));
    beatBox.setBounds (b1.withWidth (180).withSizeKeepingCentre (180, 28));
    b1.removeFromLeft (mode == 1 ? 6 : 190);
    chopBtn.setBounds (b1.removeFromLeft (90).withSizeKeepingCentre (90, 28)); b1.removeFromLeft (6);
    clearBtn.setBounds (b1.removeFromLeft (140).withSizeKeepingCentre (140, 28));

    auto b2 = bottom2;
    toPadsBtn.setBounds (b2.removeFromLeft (150)); b2.removeFromLeft (6);
    firstPadBox.setBounds (b2.removeFromLeft (200)); b2.removeFromLeft (6);
    chokeToggle.setBounds (b2.removeFromLeft (230)); b2.removeFromLeft (14);
    selPadBtn.setBounds (b2.removeFromLeft (160)); b2.removeFromLeft (6);
    selArrBtn.setBounds (b2.removeFromLeft (150)); b2.removeFromLeft (6);
    playSelBtn.setBounds (b2.removeFromLeft (110)); b2.removeFromLeft (6);
    exportBtn.setBounds (b2.removeFromLeft (150));
}

void SampleEditor::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel);
    g.setColour (Theme::textFaint);
    g.setFont (Theme::font (11.0f));
    g.drawText ("BPM:", juce::Rectangle<int> (bpmLabel.getX() - 34, bpmLabel.getY(), 32, bpmLabel.getHeight()), juce::Justification::centredRight);
    g.drawText (TR ("Arraste para selecionar  |  clique para ouvir a fatia  |  duplo clique = marcador  |  direito no marcador = apagar  |  Ctrl+arrastar a seleção = levar ao arranjo/pad  |  roda = zoom"),
                wave->getBounds().withHeight (14).translated (0, -14), juce::Justification::centredLeft);
}

} // namespace mp
