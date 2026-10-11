#include "BrowserPanel.h"

namespace mp
{
BrowserPanel::BrowserPanel (Session& s)
    : session (s),
      filter (s.sources.wildcard(), "*", TR ("Áudio")),
      dirList (&filter, scanThread),
      tree (dirList)
{
    scanThread.startThread (juce::Thread::Priority::low);
    tree.addListener (this);
    tree.setDragAndDropDescription ("browserfile");
    tree.setColour (juce::FileTreeComponent::backgroundColourId, Theme::panel);
    tree.setRootItemVisible (false);
    tree.setItemHeight (24);
    addAndMakeVisible (tree);

    sourceList.setRowHeight (48);
    sourceList.setColour (juce::ListBox::backgroundColourId, Theme::panel);
    sourceList.setMultipleSelectionEnabled (false);
    addAndMakeVisible (sourceList);

    auto docs = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
    musicBtn.onClick = [this] { setRoot (juce::File::getSpecialLocation (juce::File::userMusicDirectory)); };
    downBtn.onClick = [this, docs] { setRoot (docs.getChildFile ("Downloads")); };
    deskBtn.onClick = [this] { setRoot (juce::File::getSpecialLocation (juce::File::userDesktopDirectory)); };
    upBtn.onClick = [this] { if (root.getParentDirectory() != root) setRoot (root.getParentDirectory()); };
    upBtn.setTooltip (TR ("Pasta acima"));
    folderBtn.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> (TR ("Escolher pasta de músicas"), root);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                              [this] (const juce::FileChooser& fc) { if (fc.getResult().isDirectory()) setRoot (fc.getResult()); });
    };
    for (auto* b : std::initializer_list<juce::Button*> { &musicBtn, &downBtn, &deskBtn, &folderBtn, &upBtn })
        addAndMakeVisible (*b);
    upBtn.flat = true;
    musicBtn.setTooltip (TR ("Pasta Músicas"));
    downBtn.setTooltip (TR ("Pasta Downloads"));
    deskBtn.setTooltip (TR ("Área de Trabalho"));
    folderBtn.setTooltip (TR ("Escolher outra pasta"));
    autoPreview.setToggleState (true, juce::dontSendNotification);
    autoPreview.setTooltip (TR ("Ao clicar num arquivo, toca uma prévia (Esc para parar)"));
    autoPreview.onClick = [this] { if (! autoPreview.getToggleState()) session.engine.stopFilePreview(); };
    addAndMakeVisible (autoPreview);
    pathLabel.setFont (Theme::font (11.0f));
    pathLabel.setColour (juce::Label::textColourId, Theme::textDim);
    pathLabel.setMinimumHorizontalScale (1.0f);
    addAndMakeVisible (pathLabel);

    auto saved = juce::File (session.props.getUserSettings()->getValue ("browserRoot"));
    if (! saved.isDirectory()) saved = docs.getChildFile ("Downloads");
    if (! saved.isDirectory()) saved = juce::File::getSpecialLocation (juce::File::userMusicDirectory);
    setRoot (saved);
    session.addListener (this);
}

BrowserPanel::~BrowserPanel()
{
    session.removeListener (this);
    tree.removeListener (this);
    scanThread.stopThread (2000);
}

void BrowserPanel::setRoot (const juce::File& dir)
{
    if (! dir.isDirectory()) return;
    root = dir;
    dirList.setDirectory (dir, true, true);
    tree.refresh();
    pathLabel.setText (dir.getFullPathName(), juce::dontSendNotification);
    session.props.getUserSettings()->setValue ("browserRoot", dir.getFullPathName());
}

void BrowserPanel::selectionChanged()
{
    auto f = tree.getSelectedFile();
    if (f.existsAsFile() && autoPreview.getToggleState())
        if (! session.engine.previewFile (f, session.sources.formats()))
            session.setStatus (TR ("Pré-escuta indisponível para ") + f.getFileExtension() + TR (" - de duplo clique para importar"));
}

void BrowserPanel::fileDoubleClicked (const juce::File& f)
{
    if (f.isDirectory()) { setRoot (f); return; }
    session.engine.stopFilePreview();
    juce::Component::SafePointer<BrowserPanel> safe (this);
    session.importFile (f, [safe] (int id)
    {
        if (safe && safe->onOpenInEditor) safe->onOpenInEditor (id, -1, -1);
    });
}

int BrowserPanel::getNumRows() { return (int) session.project.sources.size(); }

void BrowserPanel::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (! juce::isPositiveAndBelow (row, (int) session.project.sources.size())) return;
    auto& si = session.project.sources[(size_t) row];
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (4.0f, 3.0f);
    g.setColour (selected ? Theme::panel3 : Theme::panel2);
    g.fillRoundedRectangle (r, 6.0f);
    if (selected)
    {
        g.setColour (Theme::accent2.withAlpha (0.6f));
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    }
    const bool loading = session.sources.isLoading (si.id);
    auto src = session.getSource (si.id);
    const bool ok = src != nullptr;
    // mini forma de onda ao fundo
    if (ok)
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (r.reduced (2.0f).toNearestInt());
        drawWaveform (g, *src, r.withTrimmedLeft (r.getWidth() * 0.42f).reduced (4.0f, 5.0f), 0.0, src->lengthSec,
                      Theme::accent2.withAlpha (selected ? 0.45f : 0.28f));
    }
    g.setColour (ok ? Theme::accent2 : loading ? Theme::warn : Theme::danger);
    g.fillRoundedRectangle (r.removeFromLeft (4.0f).reduced (0.0f, 6.0f), 2.0f);
    r.removeFromLeft (8.0f);
    g.setColour (Theme::text);
    g.setFont (Theme::fontFor (si.name, 12.0f, true));
    g.drawText (si.name, r.removeFromTop (r.getHeight() * 0.55f).withTrimmedRight (6.0f), juce::Justification::bottomLeft, true);
    g.setColour (Theme::textDim);
    g.setFont (Theme::font (10.5f));
    juce::String info = loading ? TR ("carregando...") : ! ok ? TR ("arquivo ausente") : "";
    if (ok)
    {
        const int secs = (int) src->lengthSec;
        info << juce::String (secs / 60) << ":" << juce::String (secs % 60).paddedLeft ('0', 2) << "   " << juce::String (si.bpm, 1) << " BPM";
    }
    g.drawText (info, r.withTrimmedTop (2.0f), juce::Justification::topLeft, true);
}

void BrowserPanel::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    if (juce::isPositiveAndBelow (row, (int) session.project.sources.size()) && onOpenInEditor)
        onOpenInEditor (session.project.sources[(size_t) row].id, -1, -1);
}

void BrowserPanel::listBoxItemClicked (int row, const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu() || ! juce::isPositiveAndBelow (row, (int) session.project.sources.size())) return;
    const int id = session.project.sources[(size_t) row].id;
    bool used = false;
    for (auto& c : session.project.clips) used |= c.sourceId == id;
    for (auto& p : session.project.pads) used |= p.sourceId == id;
    juce::PopupMenu m;
    m.addItem (1, TR ("Abrir no editor (fatiar)"));
    m.addItem (2, TR ("Inserir no arranjo (no cursor)"));
    m.addItem (3, TR ("Carregar inteiro no pad selecionado"));
    m.addItem (4, TR ("Mostrar no Explorer"));
    m.addSeparator();
    m.addItem (5, used ? TR ("Remover do projeto (em uso)") : TR ("Remover do projeto"), ! used);
    m.showMenuAsync (juce::PopupMenu::Options(), [this, id] (int r)
    {
        auto* si = session.project.findSource (id);
        if (si == nullptr) return;
        if (r == 1 && onOpenInEditor) onOpenInEditor (id, -1, -1);
        if (r == 2)
        {
            int tid = -1;
            if (auto* t = session.project.findTrack (session.selectedTrackId)) if (t->type == TrackType::Audio) tid = t->id;
            if (tid < 0) for (auto& t : session.project.tracks) if (t.type == TrackType::Audio) { tid = t.id; break; }
            if (tid >= 0)
            {
                session.beginEdit (TR ("Inserir áudio"));
                const int cid = session.addAudioClip (tid, id, session.engine.getPositionBeats());
                if (cid >= 0) session.selectedClips = { cid };
                session.commit (kChAll);
            }
        }
        if (r == 3)
            if (auto src = session.getSource (id))
                session.assignPad (session.selectedPad, id, 0, src->lengthSec, si->name.substring (0, 24));
        if (r == 4) juce::File (si->path).revealToUser();
        if (r == 5)
        {
            session.beginEdit (TR ("Remover áudio"));
            session.project.sources.erase (std::remove_if (session.project.sources.begin(), session.project.sources.end(),
                                                           [id] (auto& x) { return x.id == id; }), session.project.sources.end());
            session.sources.remove (id);
            session.commit (kChSources);
        }
    });
}

juce::var BrowserPanel::getDragSourceDescription (const juce::SparseSet<int>& rows)
{
    if (rows.isEmpty()) return {};
    const int row = rows[0];
    if (! juce::isPositiveAndBelow (row, (int) session.project.sources.size())) return {};
    return "source:" + juce::String (session.project.sources[(size_t) row].id);
}

void BrowserPanel::sessionChanged (int what)
{
    if (what & kChSources)
    {
        sourceList.updateContent();
        sourceList.repaint();
        repaint();
    }
}

void BrowserPanel::resized()
{
    auto r = getLocalBounds().reduced (8, 6);
    r.removeFromTop (18);
    auto row = r.removeFromTop (30);
    {
        // larguras proporcionais ao texto de cada botao (nada espremido)
        IconTextButton* btns[] = { &musicBtn, &downBtn, &deskBtn, &folderBtn };
        float tw[4], total = 0;
        for (int i = 0; i < 4; ++i)
        {
            tw[i] = Theme::font (12.0f, true).getStringWidthFloat (btns[i]->getButtonText()) + 40.0f;
            total += tw[i];
        }
        const float avail = (float) (row.getWidth() - 3 * 4);
        const float extra = (avail - total) / 4.0f;
        for (int i = 0; i < 4; ++i)
        {
            const int w = i == 3 ? row.getWidth() : juce::roundToInt (tw[i] + extra);
            btns[i]->setBounds (row.removeFromLeft (w));
            row.removeFromLeft (4);
        }
    }
    r.removeFromTop (5);
    auto row2 = r.removeFromTop (24);
    upBtn.setBounds (row2.removeFromLeft (28).reduced (0, -1));
    row2.removeFromLeft (6);
    autoPreview.setBounds (row2.removeFromRight (126));
    pathLabel.setBounds (row2);
    r.removeFromTop (4);
    const int listH = juce::jmax (120, (int) (r.getHeight() * 0.38f));
    auto bottom = r.removeFromBottom (listH);
    tree.setBounds (r);
    bottom.removeFromTop (22);
    sourceList.setBounds (bottom);
}

void BrowserPanel::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel);
    g.setColour (Theme::outline);
    g.drawVerticalLine (getWidth() - 1, 0, (float) getHeight());
    g.setColour (Theme::textFaint);
    g.setFont (Theme::font (11.0f, true));
    g.drawText (TR ("NAVEGADOR  (arraste para o arranjo ou pads)"), getLocalBounds().reduced (10, 6).removeFromTop (14), juce::Justification::centredLeft);
    auto listTop = sourceList.getY() - 20;
    g.drawText (TR ("ÁUDIOS NO PROJETO  (") + juce::String ((int) session.project.sources.size()) + ")", 10, listTop, getWidth() - 20, 16, juce::Justification::centredLeft);
}

} // namespace mp
