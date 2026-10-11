#include "MainComponent.h"

namespace mp
{
// ============================================================================
class MainComponent::Divider : public juce::Component
{
public:
    Divider (bool vert, std::function<void (int)> onDrag) : vertical (vert), dragFn (std::move (onDrag))
    {
        setMouseCursor (vertical ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::UpDownResizeCursor);
    }
    void paint (juce::Graphics& g) override
    {
        const float hv = UiAnimator::get().track (*this, "hv", isMouseOverOrDragging() ? 1.0f : 0.0f);
        g.fillAll (Theme::bg);
        auto r = getLocalBounds().toFloat();
        auto line = vertical ? r.withSizeKeepingCentre (1.0f + 2.0f * hv, r.getHeight()) : r.withSizeKeepingCentre (r.getWidth(), 1.0f + 2.0f * hv);
        g.setColour (Theme::outline.interpolatedWith (Theme::accent, hv));
        g.fillRect (line);
    }
    void mouseDown (const juce::MouseEvent& e) override { last = vertical ? e.getScreenX() : e.getScreenY(); }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const int now = vertical ? e.getScreenX() : e.getScreenY();
        if (dragFn) dragFn (now - last);
        last = now;
    }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    bool vertical;
    std::function<void (int)> dragFn;
    int last = 0;
};

class MainComponent::StatusBar : public juce::Component
{
public:
    explicit StatusBar (Session& s) : session (s) {}
    void paint (juce::Graphics& g) override
    {
        g.fillAll (Theme::panel);
        g.setColour (Theme::outline);
        g.drawHorizontalLine (0, 0, (float) getWidth());
        auto r = getLocalBounds().reduced (10, 0);
        g.setFont (Theme::font (10.5f));

        // indicador de renderizacao (GPU / CPU)
        {
            auto chip = r.removeFromRight (gpu ? 92 : 70).reduced (0, 4).toFloat();
            const auto col = gpu ? Theme::good : Theme::textFaint;
            g.setColour (col.withAlpha (0.14f));
            g.fillRoundedRectangle (chip, chip.getHeight() * 0.5f);
            Icons::draw (g, Icon::Bolt, chip.removeFromLeft (chip.getHeight()).reduced (3.0f).translated (4.0f, 0.0f), col);
            g.setColour (col);
            g.drawText (gpu ? TR ("GPU ativa") : "CPU", chip.withTrimmedLeft (4.0f), juce::Justification::centredLeft);
            r.removeFromRight (10);
        }
        auto right = r.removeFromRight (480);
        juce::String info;
        if (auto* dev = session.engine.devices().getCurrentAudioDevice())
            info << dev->getTypeName() << "  " << juce::String ((int) dev->getCurrentSampleRate()) << " Hz  "
                 << dev->getCurrentBufferSizeSamples() << TR (" amostras (")
                 << juce::String (dev->getCurrentBufferSizeSamples() * 1000.0 / juce::jmax (1.0, dev->getCurrentSampleRate()), 1) << " ms)";
        else info << TR ("Sem dispositivo de áudio!");
        info << "   CPU " << juce::String ((int) (session.engine.getCpu() * 100)) << "%";
        g.setColour (Theme::textDim);
        g.drawText (info, right, juce::Justification::centredRight);

        auto mid = r.removeFromRight (260);
        juce::String work;
        if (session.isLoading()) work = TR ("Carregando áudio...");
        if (session.stretch->hasPending())
            work = "Time-stretch: " + session.stretch->currentJobName() + " " + juce::String ((int) (session.stretch->progress() * 100)) + "%";
        if (work.isNotEmpty())
        {
            g.setColour (Theme::warn);
            g.drawText (work, mid, juce::Justification::centredRight);
        }
        g.setColour (session.status.isNotEmpty() ? Theme::text : Theme::textFaint);
        g.drawText (session.status.isNotEmpty() ? session.status
                    : juce::String (TR ("Espaço: tocar/parar   Ctrl+E: cortar   Ctrl+D: duplicar   Del: apagar   Ctrl+Z: desfazer   Tab: música/padrão   Ajuda > Atalhos")),
                    r, juce::Justification::centredLeft);
    }
    Session& session;
    bool gpu = false;
};

// ============================================================================
//  Barra de abas com icones e indicador deslizante
// ============================================================================
class MainComponent::TabBar : public juce::Component
{
public:
    TabBar()
    {
        names = { TR ("ARRANJO"), TR ("EDITOR / CHOP"), TR ("SEQUENCIADOR"), TR ("MIXER") };
        icons = { Icon::Arrange, Icon::Scissors, Icon::Grid, Icon::Mixer };
        setRepaintsOnMouseActivity (true);
    }
    std::function<void (int)> onSelect;
    void setCurrent (int i) { current = i; repaint(); }
    juce::Rectangle<float> tabRect (int i) const
    {
        float x = 8.0f;
        for (int k = 0; k < i; ++k) x += tabWidth (k) + 4.0f;
        return { x, 4.0f, tabWidth (i), (float) getHeight() - 8.0f };
    }
    float tabWidth (int i) const { return Theme::font (11.5f, true).getStringWidthFloat (names[(size_t) i]) + 52.0f; }
    int tabAt (juce::Point<int> p) const
    {
        for (int i = 0; i < 4; ++i) if (tabRect (i).contains (p.toFloat())) return i;
        return -1;
    }
    void paint (juce::Graphics& g) override
    {
        g.fillAll (Theme::panel);
        g.setColour (Theme::outline);
        g.drawHorizontalLine (getHeight() - 1, 0, (float) getWidth());
        auto& anim = UiAnimator::get();
        auto target = tabRect (current);
        const float ix = anim.track (*this, "ix", target.getX(), 0.28f);
        const float iw = anim.track (*this, "iw", target.getWidth(), 0.28f);
        auto pill = juce::Rectangle<float> (ix, target.getY(), iw, target.getHeight());
        g.setColour (Theme::panel3);
        g.fillRoundedRectangle (pill, 7.0f);
        g.setColour (Theme::accent);
        g.fillRoundedRectangle (pill.withTop (pill.getBottom() - 2.5f).reduced (10.0f, 0.0f), 1.25f);

        const int hover = isMouseOver() ? tabAt (getMouseXYRelative()) : -1;
        for (int i = 0; i < 4; ++i)
        {
            auto r = tabRect (i);
            const bool act = i == current;
            const float hv = anim.track (*this, juce::Identifier ("h" + juce::String (i)), (hover == i && ! act) ? 1.0f : 0.0f);
            if (hv > 0.01f)
            {
                g.setColour (Theme::panel2.withAlpha (hv));
                g.fillRoundedRectangle (r, 7.0f);
            }
            const auto col = act ? Theme::text : Theme::textDim.interpolatedWith (Theme::text, hv);
            auto inner = r.reduced (14.0f, 0.0f);
            Icons::draw (g, icons[(size_t) i], inner.removeFromLeft (16.0f).withSizeKeepingCentre (16.0f, 16.0f), act ? Theme::accent : col);
            inner.removeFromLeft (8.0f);
            g.setColour (col);
            g.setFont (Theme::font (11.5f, true));
            g.drawText (names[(size_t) i], inner, juce::Justification::centredLeft, false);
        }
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        const int i = tabAt (e.getPosition());
        if (i >= 0 && onSelect) onSelect (i);
    }
    void mouseMove (const juce::MouseEvent&) override { repaint(); }
    juce::StringArray names;
    std::vector<Icon> icons;
    int current = 0;
};

// ============================================================================
//  Exportacao em segundo plano com janela de progresso
// ============================================================================
class ExportJob : public juce::ThreadWithProgressWindow
{
public:
    ExportJob (Session& s, ExportSettings es, std::function<void (bool, juce::String)> done)
        : juce::ThreadWithProgressWindow (TR ("Exportando áudio..."), true, true), session (s), settings (std::move (es)), onDone (std::move (done))
    {
        setStatusMessage (TR ("Preparando..."));
    }
    void run() override
    {
        ok = session.renderToFile (settings, [this] (float p)
        {
            if (p < 0) { setStatusMessage (TR ("Processando time-stretch das músicas...")); setProgress (-1.0); }
            else { setStatusMessage (TR ("Renderizando ") + settings.file.getFileName()); setProgress (p); }
            return ! threadShouldExit();
        }, error);
    }
    void threadComplete (bool cancelled) override
    {
        if (cancelled && error.isEmpty()) error = TR ("Cancelado");
        if (onDone) onDone (ok && ! cancelled, error);
        delete this;
    }
private:
    Session& session;
    ExportSettings settings;
    std::function<void (bool, juce::String)> onDone;
    bool ok = false;
    juce::String error;
};

// ============================================================================
MainComponent::MainComponent (Session& s) : session (s)
{
    transport = std::make_unique<TransportBar> (session);
    browser = std::make_unique<BrowserPanel> (session);
    arrange = std::make_unique<ArrangeView> (session);
    editor = std::make_unique<SampleEditor> (session);
    sequencer = std::make_unique<SequencerView> (session);
    mixer = std::make_unique<MixerView> (session);
    pads = std::make_unique<PadPanel> (session);
    statusBar = std::make_unique<StatusBar> (session);
    leftDivider = std::make_unique<Divider> (true, [this] (int d) { browserWidth = juce::jlimit (0, 600, browserWidth + d); resized(); });
    bottomDivider = std::make_unique<Divider> (false, [this] (int d) { padHeight = juce::jlimit (0, 520, padHeight - d); resized(); });

    for (auto* c : std::initializer_list<juce::Component*> { transport.get(), browser.get(), arrange.get(), editor.get(), sequencer.get(),
                                                              mixer.get(), pads.get(), statusBar.get(), leftDivider.get(), bottomDivider.get() })
        addAndMakeVisible (c);

    tabBar = std::make_unique<TabBar>();
    tabBar->onSelect = [this] (int i) { showTab (i); };
    addAndMakeVisible (*tabBar);

    auto openEditor = [this] (int sourceId, double a, double b) { editor->open (sourceId, a, b); showTab (1); };
    arrange->onOpenInEditor = openEditor;
    pads->onOpenInEditor = openEditor;
    browser->onOpenInEditor = openEditor;
    arrange->onShowSequencer = [this] { showTab (2); };

    transport->getMenu = [this] (int i) { return getMenu (i); };
    transport->onMenuItem = [this] (int m, int i) { menuItem (m, i); };
    transport->onAudioSettings = [this] { showAudioSettings(); };
    transport->onAudioRecord = [this] { audioRecordClicked(); };

    browserWidth = session.props.getUserSettings()->getIntValue ("browserWidth3", 380);
    padHeight = session.props.getUserSettings()->getIntValue ("padHeight", 300);

    session.addListener (this);
    disableFocusRecursively (*this);
    setWantsKeyboardFocus (true);
    setMouseClickGrabsKeyboardFocus (true);
    showTab (0, false);
    setSize (1600, 960);
    updateTitle();
    startTimerHz (10);

    // desenho da interface pela placa de video (OpenGL), quando disponivel
    auto* settings = session.props.getUserSettings();
    bool wantGpu = settings->getBoolValue ("gpu", true) && juce::SystemStats::getEnvironmentVariable ("MEUPAD_NOGPU", {}).isEmpty();
    if (wantGpu && settings->getBoolValue ("gpuGuard", false))
    {
        // a ultima abertura com GPU nao terminou bem: usa o modo seguro (CPU)
        wantGpu = false;
        settings->setValue ("gpu", false);
        session.setStatus (TR ("Aceleração por GPU desligada (problema na última abertura). Religue em Ferramentas."));
    }
    setGpuEnabled (wantGpu, false);

    // atualizacao automatica: procura uma versao nova alguns segundos depois de abrir
    updater = std::make_unique<Updater> (*settings, [this] (juce::File installer) { installUpdate (installer); });
    if (updater->autoCheckEnabled() && juce::SystemStats::getEnvironmentVariable ("MEUPAD_NOUPDATE", {}).isEmpty())
        juce::Timer::callAfterDelay (6000, [safe = juce::Component::SafePointer<MainComponent> (this)]
        {
            if (safe != nullptr && safe->updater != nullptr) safe->updater->checkInBackground (false);
        });
}

void MainComponent::setGpuEnabled (bool on, bool save)
{
    auto* settings = session.props.getUserSettings();
    if (save) settings->setValue ("gpu", on);
    if (on == glContext.isAttached()) { statusBar->gpu = on; return; }
    if (on)
    {
        settings->setValue ("gpuGuard", true);
        settings->saveIfNeeded();
        glContext.setComponentPaintingEnabled (true);
        glContext.setContinuousRepainting (false);
        glContext.setSwapInterval (1);
        glContext.attachTo (*this);
        // se em alguns segundos tudo estiver bem, libera a trava de seguranca
        juce::Timer::callAfterDelay (4000, [safe = juce::Component::SafePointer<MainComponent> (this)]
        {
            if (safe == nullptr) return;
            safe->session.props.getUserSettings()->setValue ("gpuGuard", false);
            safe->session.props.getUserSettings()->saveIfNeeded();
        });
    }
    else
    {
        glContext.detach();
        settings->setValue ("gpuGuard", false);
    }
    statusBar->gpu = on;
    statusBar->repaint();
}

MainComponent::~MainComponent()
{
    glContext.detach();
    session.props.getUserSettings()->setValue ("gpuGuard", false);
    session.props.getUserSettings()->setValue ("browserWidth3", browserWidth);
    session.props.getUserSettings()->setValue ("padHeight", padHeight);
    session.removeListener (this);
    stopTimer();
    chooser.reset();
    removeAllChildren();
    shuttingDown = true;   // impede resized() de usar componentes ja destruidos
    pads.reset(); arrange.reset(); editor.reset(); sequencer.reset(); mixer.reset(); browser.reset(); transport.reset(); statusBar.reset();
    leftDivider.reset(); bottomDivider.reset(); tabBar.reset();
}

void MainComponent::showTab (int index, bool animate)
{
    const int prev = currentTab;
    currentTab = juce::jlimit (0, 3, index);
    juce::Component* views[] = { arrange.get(), editor.get(), sequencer.get(), mixer.get() };
    for (int i = 0; i < 4; ++i) views[i]->setVisible (i == currentTab);
    if (tabBar != nullptr) tabBar->setCurrent (currentTab);
    // transicao: a nova tela entra deslizando de leve e aparecendo (fade)
    auto* v = views[currentTab];
    if (animate && prev != currentTab && ! viewArea.isEmpty())
    {
        auto& animator = juce::Desktop::getInstance().getAnimator();
        const int dir = currentTab > prev ? 1 : -1;
        v->setBounds (viewArea.translated (dir * 24, 0));
        v->setAlpha (0.0f);
        animator.animateComponent (v, viewArea, 1.0f, 200, false, 0.2, 0.0);
    }
    else if (! viewArea.isEmpty()) { v->setAlpha (1.0f); v->setBounds (viewArea); }
}

void MainComponent::resized()
{
    if (shuttingDown || transport == nullptr || statusBar == nullptr || pads == nullptr || browser == nullptr) return;
    auto r = getLocalBounds();
    transport->setBounds (r.removeFromTop (TransportBar::preferredHeight));
    statusBar->setBounds (r.removeFromBottom (26));
    const int ph = juce::jmin (padHeight, r.getHeight() - 250);
    if (ph > 40)
    {
        pads->setVisible (true);
        pads->setBounds (r.removeFromBottom (ph));
    }
    else pads->setVisible (false);
    bottomDivider->setBounds (r.removeFromBottom (5));
    const int bw = juce::jmin (browserWidth, r.getWidth() / 2);
    if (bw > 60)
    {
        browser->setVisible (true);
        browser->setBounds (r.removeFromLeft (bw));
    }
    else browser->setVisible (false);
    leftDivider->setBounds (r.removeFromLeft (5));
    tabBar->setBounds (r.removeFromTop (40));
    viewArea = r;
    auto& animator = juce::Desktop::getInstance().getAnimator();
    for (auto* c : std::initializer_list<juce::Component*> { arrange.get(), editor.get(), sequencer.get(), mixer.get() })
    {
        animator.cancelAnimation (c, false);
        c->setAlpha (1.0f);
        c->setBounds (r);
    }
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (Theme::bg);
}

void MainComponent::sessionChanged (int what)
{
    juce::ignoreUnused (what);
    updateTitle();
}

void MainComponent::updateTitle()
{
    if (auto* w = findParentComponentOfClass<juce::DocumentWindow>())
    {
        auto name = session.project.projectFile == juce::File() ? juce::String (TR ("Sem título")) : session.project.projectFile.getFileNameWithoutExtension();
        w->setName ((session.isDirty() ? "* " : "") + name + "  -  Ja Mixei " + juce::JUCEApplication::getInstance()->getApplicationVersion());
    }
}

void MainComponent::focusLost (FocusChangeType)
{
    // solta pads presos se a janela perder o foco
    for (int i = 0; i < 16; ++i)
        if (padKeyDown[(size_t) i]) { padKeyDown[(size_t) i] = false; session.padUp (padKeyPad[(size_t) i]); }
}

void MainComponent::timerCallback()
{
    statusBar->repaint();
    // garante que os atalhos sempre funcionem
    auto* focused = juce::Component::getCurrentlyFocusedComponent();
    if ((focused == nullptr || focused == getTopLevelComponent()) && isShowing()
        && juce::ModalComponentManager::getInstance()->getNumModalComponents() == 0
        && juce::Process::isForegroundProcess())
        grabKeyboardFocus();
    // auto-salvamento a cada 2 minutos
    const auto now = juce::Time::getMillisecondCounter();
    if (session.isDirty() && now - lastAutosave > 120000)
    {
        lastAutosave = now;
        auto dir = Session::dataFolder().getChildFile ("Autosave");
        dir.createDirectory();
        dir.getChildFile ("autosave.mpad").replaceWithText (session.project.toJson());
    }
    static bool lastDirty = false;
    if (lastDirty != session.isDirty()) { lastDirty = session.isDirty(); updateTitle(); }
}

// ----------------------------------------------------------------------------
bool MainComponent::keyStateChanged (bool)
{

    if (dynamic_cast<juce::TextEditor*> (juce::Component::getCurrentlyFocusedComponent()) != nullptr) return false;
    const auto mods = juce::ModifierKeys::getCurrentModifiersRealtime();
    const bool blocked = mods.isCommandDown() || mods.isAltDown() || mods.isCtrlDown();
    bool any = false;
    for (int i = 0; i < 16; ++i)
    {
        const bool down = juce::KeyPress::isKeyCurrentlyDown (padKeyCode (i));
        if (down && ! padKeyDown[(size_t) i] && ! blocked)
        {
            padKeyDown[(size_t) i] = true;
            padKeyPad[(size_t) i] = session.currentBank * kPadsPerBank + i;
            session.padDown (padKeyPad[(size_t) i], 120);
            any = true;
        }
        else if (! down && padKeyDown[(size_t) i])
        {
            padKeyDown[(size_t) i] = false;
            session.padUp (padKeyPad[(size_t) i]);
            any = true;
        }
    }
    return any;
}

bool MainComponent::keyPressed (const juce::KeyPress& key)
{

    if (dynamic_cast<juce::TextEditor*> (juce::Component::getCurrentlyFocusedComponent()) != nullptr) return false;
    const auto mods = key.getModifiers();
    const bool cmd = mods.isCommandDown();
    const bool shift = mods.isShiftDown();
    const int code = key.getKeyCode();
    juce::juce_wchar upper = (code > 0 && code < 128) ? juce::CharacterFunctions::toUpperCase ((juce::juce_wchar) code)
                                                      : juce::CharacterFunctions::toUpperCase (key.getTextCharacter());

    if (! cmd && ! mods.isAltDown())
        for (int i = 0; i < 16; ++i)
            if (code == padKeyCode (i) || upper == (juce::juce_wchar) padKeyCode (i)) return true;   // tratado em keyStateChanged

    if (key == juce::KeyPress::spaceKey) { session.togglePlay(); return true; }
    if (key == juce::KeyPress::returnKey) { if (session.engine.isPlaying()) session.engine.stop(); session.stopTransport(); return true; }
    if (key == juce::KeyPress::homeKey) { session.seek (0); return true; }
    if (key == juce::KeyPress::escapeKey) { session.engine.stopPreview(); session.engine.stopFilePreview(); session.engine.panic(); return true; }
    if (key == juce::KeyPress::tabKey) { session.setSongMode (! session.songMode); return true; }
    const bool arrangeTab = currentTab == 0;
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey) { if (arrangeTab) session.deleteSelectedClips(); return true; }
    if (key == juce::KeyPress::leftKey) { nudgeCursor (cmd ? -session.project.beatsPerBar : -1.0); return true; }
    if (key == juce::KeyPress::rightKey) { nudgeCursor (cmd ? session.project.beatsPerBar : 1.0); return true; }
    if (code >= juce::KeyPress::F1Key && code <= juce::KeyPress::F4Key)
    {
        session.currentBank = code - juce::KeyPress::F1Key;
        session.selectedPad = session.currentBank * kPadsPerBank + session.selectedPad % kPadsPerBank;
        session.notify (kChPads | kChSelection);
        return true;
    }
    if (! cmd && (upper == '=' || upper == '+')) { arrange->zoomBy (1.25, arrange->getWidth() * 0.4f); return true; }
    if (! cmd && upper == '-') { arrange->zoomBy (0.8, arrange->getWidth() * 0.4f); return true; }

    if (cmd)
    {
        switch (upper)
        {
            case 'Z': if (shift) session.redo(); else session.undo(); return true;
            case 'Y': session.redo(); return true;
            case 'S': saveProject (shift); return true;
            case 'O': openProject(); return true;
            case 'N': newProject(); return true;
            case 'I': importAudio(); return true;
            case 'E': if (shift) exportAudio(); else if (arrangeTab) session.splitAt (session.engine.getPositionBeats()); return true;
            case 'D': if (arrangeTab) session.duplicateSelectedClips(); return true;
            case 'C': if (arrangeTab) session.copySelection(); return true;
            case 'X': if (arrangeTab) cutSelection(); return true;
            case 'V': if (arrangeTab) session.paste (session.engine.getPositionBeats(), session.selectedTrackId); return true;
            case 'A': if (arrangeTab) selectAll(); return true;
            case 'R': if (shift) audioRecordClicked(); else session.toggleRecord(); return true;
            case 'L': session.beginEdit (TR ("Loop")); session.project.loopOn = ! session.project.loopOn; session.commit (kChTransport); return true;
            case 'M': session.project.metronome = ! session.project.metronome; session.commit (kChTransport); return true;
            case '1': case '2': case '3': case '4': showTab (upper - '1'); return true;
            default: break;
        }
    }
    return false;
}

void MainComponent::nudgeCursor (double beats)
{
    const double pos = session.engine.getPositionBeats();
    double target = std::round ((pos + beats) / std::abs (beats)) * std::abs (beats);
    session.seek (juce::jmax (0.0, target));
}

void MainComponent::cutSelection()
{
    session.copySelection();
    session.deleteSelectedClips();
}

void MainComponent::selectAll()
{
    session.selectedClips.clear();
    for (auto& c : session.project.clips) session.selectedClips.insert (c.id);
    session.notify (kChSelection);
}

// ----------------------------------------------------------------------------
juce::PopupMenu MainComponent::getMenu (int index)
{
    juce::PopupMenu m;
    if (index == 0)
    {
        m.addItem (1, TR ("Novo projeto  (Ctrl+N)"));
        m.addItem (2, TR ("Abrir projeto...  (Ctrl+O)"));
        juce::PopupMenu recent;
        auto files = session.recentFiles();
        for (int i = 0; i < files.size(); ++i) recent.addItem (100 + i, files[i]);
        m.addSubMenu (TR ("Abrir recente"), recent, files.size() > 0);
        m.addSeparator();
        m.addItem (3, TR ("Salvar  (Ctrl+S)"));
        m.addItem (4, TR ("Salvar como...  (Ctrl+Shift+S)"));
        m.addItem (5, TR ("Salvar copiando os áudios para a pasta do projeto..."));
        m.addSeparator();
        m.addItem (6, TR ("Importar áudio...  (Ctrl+I)"));
        m.addItem (7, TR ("Exportar mixagem (MP3/WAV/FLAC/OGG)...  (Ctrl+Shift+E)"));
        m.addItem (10, TR ("Gravar áudio (microfone ou mix ao vivo)...  (Ctrl+Shift+R)"));
        m.addItem (11, TR ("Abrir pasta de gravações"));
        m.addSeparator();
        m.addItem (8, TR ("Abrir pasta de auto-salvamento"));
        m.addItem (9, TR ("Sair"));
    }
    else if (index == 1)
    {
        m.addItem (1, TR ("Desfazer ") + session.history.undoLabel() + "  (Ctrl+Z)", session.history.canUndo());
        m.addItem (2, TR ("Refazer ") + session.history.redoLabel() + "  (Ctrl+Y)", session.history.canRedo());
        m.addSeparator();
        m.addItem (3, TR ("Cortar clipe no cursor  (Ctrl+E)"));
        m.addItem (4, TR ("Duplicar seleção  (Ctrl+D)"), ! session.selectedClips.empty());
        m.addItem (5, TR ("Copiar  (Ctrl+C)"), ! session.selectedClips.empty());
        m.addItem (6, TR ("Recortar  (Ctrl+X)"), ! session.selectedClips.empty());
        m.addItem (7, TR ("Colar no cursor  (Ctrl+V)"), session.hasClipboard());
        m.addItem (8, TR ("Apagar  (Del)"), ! session.selectedClips.empty());
        m.addItem (9, TR ("Selecionar todos os clipes  (Ctrl+A)"));
        m.addSeparator();
        m.addItem (10, TR ("Nova faixa de áudio"));
        m.addItem (11, TR ("Nova faixa de padrões"));
    }
    else if (index == 2)
    {
        m.addItem (1, TR ("Reamostrar o loop para o pad selecionado (estilo MPC)"));
        m.addItem (2, TR ("Exportar só o loop..."));
        m.addSeparator();
        m.addItem (3, TR ("Ajustar zoom do arranjo  (ver tudo)"));
        m.addItem (4, TR ("Metrônomo"), true, session.project.metronome);
        m.addSeparator();
        juce::PopupMenu sig;
        for (int b : { 2, 3, 4, 5, 6, 7 }) sig.addItem (200 + b, juce::String (b) + "/4", true, session.project.beatsPerBar == b);
        m.addSubMenu (TR ("Compasso"), sig);
        juce::PopupMenu langs;
        for (int l = 0; l < 4; ++l)
            langs.addItem (300 + l, i18n::nativeName ((i18n::Lang) l), true, (int) i18n::language() == l);
        m.addSubMenu (TR ("Idioma") + (i18n::language() == i18n::Lang::En ? juce::String() : juce::String (" / Language")), langs);
        m.addSeparator();
        m.addItem (5, TR ("Configurações de áudio..."));
        m.addItem (7, TR ("Usar a placa de vídeo (GPU) para desenhar a tela"), true, glContext.isAttached());
        m.addItem (6, TR ("Parar todos os sons (pânico)  (Esc)"));
    }
    else
    {
        m.addItem (1, TR ("Atalhos de teclado e dicas"));
        m.addItem (2, TR ("Sobre o Ja Mixei"));
        m.addSeparator();
        m.addItem (3, TR ("Procurar atualizações..."));
        m.addItem (4, TR ("Procurar atualizações automaticamente"), true, updater != nullptr && updater->autoCheckEnabled());
    }
    return m;
}

void MainComponent::menuItem (int menu, int item)
{
    if (menu == 0)
    {
        if (item == 1) newProject();
        if (item == 2) openProject();
        if (item == 3) saveProject (false);
        if (item == 4) saveProject (true);
        if (item == 5) saveProject (true, true);
        if (item == 6) importAudio();
        if (item == 7) exportAudio();
        if (item == 10) audioRecordClicked();
        if (item == 11) { auto d = Session::recordingsFolder(); d.createDirectory(); d.startAsProcess(); }
        if (item == 8)
        {
            auto dir = Session::dataFolder().getChildFile ("Autosave");
            dir.createDirectory();
            dir.startAsProcess();
        }
        if (item == 9) juce::JUCEApplication::getInstance()->systemRequestedQuit();
        if (item >= 100)
        {
            auto files = session.recentFiles();
            if (item - 100 < files.size())
            {
                juce::File f (files[item - 100]);
                confirmDiscard ([this, f] { openFile (f); });
            }
        }
    }
    else if (menu == 1)
    {
        switch (item)
        {
            case 1: session.undo(); break;
            case 2: session.redo(); break;
            case 3: session.splitAt (session.engine.getPositionBeats()); break;
            case 4: session.duplicateSelectedClips(); break;
            case 5: session.copySelection(); break;
            case 6: cutSelection(); break;
            case 7: session.paste (session.engine.getPositionBeats(), session.selectedTrackId); break;
            case 8: session.deleteSelectedClips(); break;
            case 9: selectAll(); break;
            case 10: session.addTrack (TrackType::Audio); break;
            case 11: session.addTrack (TrackType::Pattern); break;
            default: break;
        }
    }
    else if (menu == 2)
    {
        if (item == 1) resampleLoopToPad();
        if (item == 2)
        {
            if (! session.project.loopOn) { session.setStatus (TR ("Ligue o loop (Ctrl+L) e defina a região na regua primeiro")); return; }
            ExportSettings es;
            es.onlyLoop = true;
            es.tailSeconds = 0;
            chooser = std::make_unique<juce::FileChooser> (TR ("Exportar loop"), session.lastFolder().getChildFile ("loop.wav"), "*.wav");
            chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                                  [this, es] (const juce::FileChooser& fc) mutable
                                  {
                                      if (fc.getResult() == juce::File()) return;
                                      es.file = fc.getResult().withFileExtension (".wav");
                                      (new ExportJob (session, es, [this] (bool ok, juce::String err)
                                      { session.setStatus (ok ? TR ("Loop exportado") : TR ("Exportação: ") + err); }))->launchThread();
                                  });
        }
        if (item == 3) arrange->zoomToFit();
        if (item == 4) { session.project.metronome = ! session.project.metronome; session.commit (kChTransport); }
        if (item == 5) showAudioSettings();
        if (item == 6) session.engine.panic();
        if (item == 7)
        {
            setGpuEnabled (! glContext.isAttached(), true);
            session.setStatus (glContext.isAttached() ? TR ("GPU ligada: a tela é desenhada pela placa de vídeo (OpenGL)")
                                                      : TR ("GPU desligada: a tela é desenhada pelo processador"));
        }
        if (item >= 300 && item < 304) { changeLanguage ((i18n::Lang) (item - 300)); return; }
        if (item >= 200 && item < 300) { session.beginEdit (TR ("Compasso")); session.project.beatsPerBar = item - 200; session.commit (kChAll); }
    }
    else
    {
        if (item == 1) showShortcuts();
        if (item == 3 && updater != nullptr) { session.setStatus (TR ("Procurando atualizações...")); updater->checkInBackground (true); }
        if (item == 4 && updater != nullptr) updater->setAutoCheck (! updater->autoCheckEnabled());
        if (item == 2)
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, "Ja Mixei " + juce::JUCEApplication::getInstance()->getApplicationVersion(),
                (TR ("Criado por Ranariel.\n\nSampler / remixador estilo MPC.\n\nMotor de áudio em C++ (JUCE), time-stretch Rubber Band de alta qualidade, ") + TR ("detecção de BPM e transientes, 64 pads em 4 bancos, sequenciador de passos, arranjo com crossfades, automação, ") + TR ("mixer com EQ/filtro DJ/reverb/delay, gravação de áudio e exportação MP3/WAV/FLAC/OGG.\n\n© 2026 Ranariel. Software livre (licença GPL v3).")));
    }
}

// ----------------------------------------------------------------------------
void MainComponent::changeLanguage (i18n::Lang l)
{
    if (l == i18n::language()) return;
    session.props.getUserSettings()->setValue ("language", i18n::code (l));
    session.props.saveIfNeeded();
    // a mensagem aparece ja no idioma escolhido
    const auto old = i18n::language();
    i18n::setLanguage (l);
    const auto title = TR ("Idioma alterado"), msg = TR ("Reinicie o Ja Mixei para aplicar o novo idioma."),
               now = TR ("Reiniciar agora"), later = TR ("Depois");
    Theme::alertUsesCjk = (l == i18n::Lang::Zh);   // a mensagem em chines precisa da fonte chinesa
    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::NoIcon, title, msg, now, later, nullptr,
        juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<MainComponent> (this)] (int r)
        {
            Theme::alertUsesCjk = false;
            if (r != 1 || safe == nullptr) return;
            safe->confirmDiscard ([safe]
            {
                if (safe == nullptr) return;
                juce::File::getSpecialLocation (juce::File::currentExecutableFile).startAsProcess ("--restart");
                safe->skipDiscard = true;
                juce::JUCEApplication::getInstance()->systemRequestedQuit();
            });
        }));
    i18n::setLanguage (old);
}

// instalador ja baixado e conferido: pergunta se salva o projeto, abre o instalador e fecha
void MainComponent::installUpdate (juce::File installer)
{
    confirmDiscard ([safe = juce::Component::SafePointer<MainComponent> (this), installer]
    {
        if (safe == nullptr) return;
        // /S = silencioso, /RELAUNCH = o instalador reabre o Ja Mixei no final
        if (! installer.startAsProcess ("/S /RELAUNCH"))
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, TR ("Erro na atualização"),
                TR ("O instalador não foi aberto (permissão de administrador negada?). Tente de novo pelo menu Ajuda."));
            return;
        }
        safe->skipDiscard = true;
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    });
}

void MainComponent::confirmDiscard (std::function<void()> then)
{
    if (! session.isDirty() || skipDiscard) { then(); return; }
    juce::AlertWindow::showYesNoCancelBox (juce::MessageBoxIconType::NoIcon, TR ("Projeto não salvo"),
        TR ("Deseja salvar as alterações antes de continuar?"), TR ("Salvar"), TR ("Descartar"), TR ("Cancelar"), nullptr,
        juce::ModalCallbackFunction::create ([this, then] (int r)
        {
            if (r == 1)
            {
                if (session.project.projectFile != juce::File())
                {
                    if (session.saveProject (session.project.projectFile, false)) then();
                }
                else
                {
                    chooser = std::make_unique<juce::FileChooser> (TR ("Salvar projeto"), session.lastFolder().getChildFile ("Meu remix.mpad"), "*.mpad");
                    chooserActive = true;
                    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                                          [this, then] (const juce::FileChooser& fc)
                                          {
                                              chooserActive = false;
                                              if (fc.getResult() != juce::File() && session.saveProject (fc.getResult(), false)) then();
                                          });
                }
            }
            else if (r == 2) then();
        }));
}

void MainComponent::requestQuit (std::function<void()> quitFn)
{
    if (quitDialogOpen) return;                 // evita varias janelas "Projeto nao salvo"
    // para tudo que esta tocando/gravando antes de perguntar
    if (session.isAudioRecording()) session.stopAudioRecording();
    if (session.recording) session.toggleRecord();
    session.stopTransport();
    session.engine.stopPreview();
    session.engine.stopFilePreview();
    session.engine.panic();
    for (int i = 0; i < 16; ++i)
        if (padKeyDown[(size_t) i]) { padKeyDown[(size_t) i] = false; session.padUp (padKeyPad[(size_t) i]); }

    if (! session.isDirty() || skipDiscard) { quitFn(); return; }
    quitDialogOpen = true;
    confirmDiscard ([this, quitFn] { quitDialogOpen = false; quitFn(); });
    // se o usuario cancelar, libera para perguntar de novo
    juce::Timer::callAfterDelay (300, [safe = juce::Component::SafePointer<MainComponent> (this)]
    {
        if (safe != nullptr) safe->checkQuitDialog();
    });
}

void MainComponent::checkQuitDialog()
{
    if (! quitDialogOpen) return;
    if (juce::ModalComponentManager::getInstance()->getNumModalComponents() == 0 && (chooser == nullptr || ! chooserActive))
        quitDialogOpen = false;
    else
        juce::Timer::callAfterDelay (300, [safe = juce::Component::SafePointer<MainComponent> (this)] { if (safe != nullptr) safe->checkQuitDialog(); });
}

void MainComponent::newProject()
{
    confirmDiscard ([this] { session.newProject(); updateTitle(); });
}

void MainComponent::openFile (const juce::File& f)
{
    if (session.openProject (f)) { arrange->zoomToFit(); updateTitle(); }
    else juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, TR ("Erro"), TR ("Não foi possível abrir ") + f.getFileName());
}

void MainComponent::openProject()
{
    confirmDiscard ([this]
    {
        chooser = std::make_unique<juce::FileChooser> (TR ("Abrir projeto"), session.lastFolder(), "*.mpad");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc) { if (fc.getResult().existsAsFile()) openFile (fc.getResult()); });
    });
}

void MainComponent::saveProject (bool saveAs, bool collect)
{
    if (! saveAs && session.project.projectFile != juce::File())
    {
        session.saveProject (session.project.projectFile, false);
        updateTitle();
        return;
    }
    auto def = session.project.projectFile != juce::File() ? session.project.projectFile : session.lastFolder().getChildFile ("Meu remix.mpad");
    chooser = std::make_unique<juce::FileChooser> (collect ? TR ("Salvar projeto e copiar áudios") : TR ("Salvar projeto"), def, "*.mpad");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this, collect] (const juce::FileChooser& fc)
                          {
                              if (fc.getResult() == juce::File()) return;
                              session.saveProject (fc.getResult(), collect);
                              updateTitle();
                          });
}

void MainComponent::importAudio()
{
    chooser = std::make_unique<juce::FileChooser> (TR ("Importar áudio"), session.lastFolder(), session.sources.wildcard());
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems,
                          [this] (const juce::FileChooser& fc)
                          {
                              for (auto& f : fc.getResults())
                              {
                                  session.setLastFolder (f.getParentDirectory());
                                  juce::Component::SafePointer<MainComponent> safe (this);
                                  session.importFile (f, [safe] (int id) { if (safe) safe->editor->open (id, -1, -1); });
                              }
                          });
}

void MainComponent::exportAudio()
{
    auto* aw = new juce::AlertWindow (TR ("Exportar mixagem"), TR ("Renderiza o arranjo (modo MÚSICA) com todos os efeitos."), juce::MessageBoxIconType::NoIcon);
    aw->addComboBox ("fmt", { TR ("MP3 320 kbps (melhor qualidade)"), "MP3 256 kbps", "MP3 192 kbps", TR ("MP3 128 kbps (arquivo menor)"),
                              TR ("WAV 24 bits (sem perdas)"), TR ("WAV 16 bits (qualidade de CD)"), TR ("FLAC 24 bits (sem perdas, compactado)"), "OGG Vorbis" }, TR ("Formato"));
    aw->addComboBox ("sr", { "44100 Hz", "48000 Hz" }, TR ("Taxa de amostragem"));
    aw->addComboBox ("range", { TR ("Música inteira"), TR ("Somente a região de loop") }, TR ("Trecho"));
    aw->addButton (TR ("Escolher arquivo..."), 1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton (TR ("Cancelar"), 0, juce::KeyPress (juce::KeyPress::escapeKey));
    aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw] (int r)
    {
        if (r != 1) return;
        ExportSettings es;
        const int fmt = aw->getComboBoxComponent ("fmt")->getSelectedItemIndex();
        if (fmt <= 3) { es.format = 3; es.mp3Quality = fmt; }
        else es.format = fmt == 6 ? 1 : fmt == 7 ? 2 : 0;
        es.bitDepth = fmt == 5 ? 16 : 24;
        es.sampleRate = aw->getComboBoxComponent ("sr")->getSelectedItemIndex() == 1 ? 48000.0 : 44100.0;
        es.onlyLoop = aw->getComboBoxComponent ("range")->getSelectedItemIndex() == 1;
        const juce::String ext = es.format == 1 ? ".flac" : es.format == 2 ? ".ogg" : es.format == 3 ? ".mp3" : ".wav";
        auto name = session.project.projectFile == juce::File() ? juce::String (TR ("Meu remix")) : session.project.projectFile.getFileNameWithoutExtension();
        chooser = std::make_unique<juce::FileChooser> (TR ("Exportar como"), session.lastFolder().getChildFile (name + ext), "*" + ext);
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this, es, ext] (const juce::FileChooser& fc) mutable
                              {
                                  if (fc.getResult() == juce::File()) return;
                                  es.file = fc.getResult().withFileExtension (ext);
                                  session.engine.stop();
                                  auto file = es.file;
                                  (new ExportJob (session, es, [this, file] (bool ok, juce::String err)
                                  {
                                      if (ok)
                                      {
                                          session.setStatus (TR ("Exportado: ") + file.getFullPathName());
                                          juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, TR ("Exportação concluída"),
                                                                                  TR ("Arquivo salvo em:\n") + file.getFullPathName());
                                      }
                                      else if (err != TR ("Cancelado"))
                                          juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, TR ("Erro na exportação"), err);
                                  }))->launchThread();
                              });
    }), true);
}

void MainComponent::audioRecordClicked()
{
    if (session.isAudioRecording()) { session.stopAudioRecording(); return; }
    auto* aw = new juce::AlertWindow (TR ("Gravar áudio"),
        TR ("A música toca e tudo é gravado a partir do cursor. Pare com Espaço ou no botão Parar."),
        juce::MessageBoxIconType::NoIcon);
    aw->addComboBox ("src", { (TR ("Tudo que está tocando: música + pads tocados ao vivo (mix)")),
                              TR ("Microfone / entrada de áudio (voz, instrumento)") }, TR ("Fonte"));
    aw->addComboBox ("fmt", { TR ("MP3 320 kbps"), TR ("WAV 24 bits (sem perdas)") }, TR ("Formato (mix)"));
    aw->addComboBox ("mon", { TR ("Não ouvir a entrada"), TR ("Ouvir a entrada enquanto grava (use fone de ouvido!)") },
                     TR ("Monitoração (microfone)"));
    aw->addTextBlock ((TR ("Microfone: a gravação entra numa faixa de áudio do arranjo, alinhada com a música.\n") + TR ("Mix: gera um arquivo com tudo que você ouvir (ótimo para gravar os pads por cima da música).\n") + TR ("Arquivos em Documentos\\Ja Mixei\\Gravações.")));
    aw->addButton (TR ("● Gravar"), 1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton (TR ("Cancelar"), 0, juce::KeyPress (juce::KeyPress::escapeKey));
    aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw] (int r)
    {
        if (r != 1) return;
        Session::AudioRecOptions o;
        o.fromInput = aw->getComboBoxComponent ("src")->getSelectedItemIndex() == 1;
        o.mp3 = aw->getComboBoxComponent ("fmt")->getSelectedItemIndex() == 0;
        o.monitor = aw->getComboBoxComponent ("mon")->getSelectedItemIndex() == 1;
        juce::String err;
        if (! session.startAudioRecording (o, err))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, TR ("Não foi possível gravar"),
                err + TR ("\n\nConfira o microfone em Configurações de áudio (ícone no canto superior direito)."));
    }), true);
}

void MainComponent::resampleLoopToPad()
{
    auto& p = session.project;
    if (! p.loopOn || p.loopEnd <= p.loopStart)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, TR ("Reamostrar"),
            TR ("Ligue o loop (Ctrl+L) e arraste na parte de baixo da regua para marcar o trecho que sera gravado no pad."));
        return;
    }
    auto dir = Session::dataFolder().getChildFile ("Reamostragens");
    dir.createDirectory();
    ExportSettings es;
    es.onlyLoop = true;
    es.tailSeconds = 0;
    es.sampleRate = 44100;
    es.file = dir.getNonexistentChildFile (TR ("Reamostragem ") + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M-%S"), ".wav");
    const int pad = session.selectedPad;
    auto file = es.file;
    session.engine.stop();
    (new ExportJob (session, es, [this, file, pad] (bool ok, juce::String err)
    {
        if (! ok) { session.setStatus (TR ("Reamostragem: ") + err); return; }
        session.importFile (file, [this, pad, file] (int id)
        {
            if (auto src = session.getSource (id))
            {
                session.assignPad (pad, id, 0, src->lengthSec, "Resample");
                session.setStatus (TR ("Loop reamostrado no pad ") + session.project.pads[(size_t) pad].name + "  (" + file.getFileName() + ")");
            }
        });
    }))->launchThread();
}

void MainComponent::showAudioSettings()
{
    auto* sel = new juce::AudioDeviceSelectorComponent (session.engine.devices(), 0, 0, 2, 2, false, false, true, false);
    sel->setSize (620, 460);
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (sel);
    o.dialogTitle = TR ("Configurações de áudio");
    o.dialogBackgroundColour = Theme::panel;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = false;
    o.launchAsync();
}

void MainComponent::showShortcuts()
{
    const juce::String text =
        (TR ("TRANSPORTE\n") + TR ("  Espaço  tocar / pausar          Enter  parar e voltar\n") + TR ("  Tab  alternar MÚSICA / PADRÃO    Ctrl+R  gravar pads no padrão\n") + TR ("  Ctrl+Shift+R  gravar áudio (mix ao vivo ou microfone)\n") + TR ("  Ctrl+L  loop    Ctrl+M  metrônomo    Setas  mover cursor (Ctrl = compasso)\n\n") + TR ("PADS (teclado, estilo MPC)\n") + TR ("  1 2 3 4 / Q W E R / A S D F / Z X C V     F1-F4  bancos A-D\n\n") + TR ("EDIÇÃO NO ARRANJO\n") + TR ("  Ctrl+E  cortar no cursor    Ctrl+D  duplicar    Del  apagar\n") + TR ("  Ctrl+C / Ctrl+X / Ctrl+V  copiar / recortar / colar no cursor\n") + TR ("  Ctrl+Z / Ctrl+Y  desfazer / refazer    Ctrl+A  selecionar tudo\n") + TR ("  Arrastar borda = aparar   Bolinha no topo = fade   Alt+arrastar = copiar\n") + TR ("  Shift ao arrastar = sem grade    Ctrl+roda = zoom    + / -  zoom\n") + TR ("  Clipes sobrepostos na mesma faixa fazem crossfade automático\n") + TR ("  Botão A da faixa: automação de volume / filtro (transições de DJ)\n\n") + TR ("ARQUIVO\n") + TR ("  Ctrl+N novo   Ctrl+O abrir   Ctrl+S salvar   Ctrl+I importar   Ctrl+Shift+E exportar\n\n") + TR ("DICA DE REMIX: a primeira música define o BPM. As seguintes são sincronizadas (Warp)\n") + TR ("automaticamente. Use o Editor para fatiar trechos e tocar nos pads."));
    auto* lbl = new juce::Label ({}, text);
    lbl->setFont (Theme::mono (13.0f));
    lbl->setJustificationType (juce::Justification::topLeft);
    lbl->setColour (juce::Label::textColourId, Theme::text);
    lbl->setSize (920, 580);
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (lbl);
    o.dialogTitle = TR ("Atalhos e dicas");
    o.dialogBackgroundColour = Theme::panel;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.launchAsync();
}

} // namespace mp
