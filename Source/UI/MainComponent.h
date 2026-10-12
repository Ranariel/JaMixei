#pragma once
#include <JuceHeader.h>
#include "Widgets.h"
#include "TransportBar.h"
#include "ArrangeView.h"
#include "PadPanel.h"
#include "SequencerView.h"
#include "MixerView.h"
#include "SampleEditor.h"
#include "BrowserPanel.h"
#include "Icons.h"
#include "Updater.h"
#include "InstrumentView.h"

namespace mp
{
class MainComponent : public juce::Component,
                      public juce::DragAndDropContainer,
                      public Session::Listener,
                      private juce::Timer
{
public:
    explicit MainComponent (Session& s);
    ~MainComponent() override;

    void resized() override;
    void paint (juce::Graphics& g) override;
    bool keyPressed (const juce::KeyPress& key) override;
    bool keyStateChanged (bool isKeyDown) override;
    void sessionChanged (int what) override;
    void focusLost (FocusChangeType) override;
    void parentHierarchyChanged() override { updateTitle(); }

    void showTab (int index, bool animate = true);
    void setGpuEnabled (bool on, bool save);
    void releaseGraphics() { if (glContext.isAttached()) glContext.detach(); }
    void requestQuit (std::function<void()> quitFn);
    void openFile (const juce::File& f);

private:
    class Divider;
    class StatusBar;
    class TabBar;
    void timerCallback() override;
    juce::PopupMenu getMenu (int index);
    void menuItem (int menu, int item);

    // acoes
    void newProject();
    void openProject();
    void saveProject (bool saveAs, bool collect = false);
    void importAudio();
    void exportAudio();
    void resampleLoopToPad();
    void audioRecordClicked();
    void showAudioSettings();
    void showShortcuts();
    void showInstruments();
    void playPressed();
    void releasePcNotes();
    void confirmDiscard (std::function<void()> then);
    void changeLanguage (i18n::Lang l);
    bool skipDiscard = false;
    void installUpdate (juce::File installer);
    std::unique_ptr<Updater> updater;
    void updateTitle();
    void cutSelection();
    void selectAll();
    void nudgeCursor (double beats);

    Session& session;
    juce::TooltipWindow tooltips { this, 600 };
    std::unique_ptr<TransportBar> transport;
    std::unique_ptr<BrowserPanel> browser;
    std::unique_ptr<ArrangeView> arrange;
    std::unique_ptr<SampleEditor> editor;
    std::unique_ptr<SequencerView> sequencer;
    std::unique_ptr<MixerView> mixer;
    std::unique_ptr<InstrumentView> instView;
    std::unique_ptr<PadPanel> pads;
    std::unique_ptr<Divider> leftDivider, bottomDivider;
    std::unique_ptr<StatusBar> statusBar;
    std::unique_ptr<TabBar> tabBar;
    juce::Rectangle<int> viewArea;
    juce::OpenGLContext glContext;
    int currentTab = 0;
    int browserWidth = 380, padHeight = 300;
    std::array<bool, 16> padKeyDown {};
    std::array<int, 16> padKeyPad {};
    std::map<int, int> pcNoteKeys;          // tecla do PC -> nota tocando (aba Instrumentos)
    static constexpr int kInstrumentsTab = 3, kMixerTab = 4, kNumTabs = 5;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::uint32 lastAutosave = 0;
    bool shuttingDown = false, quitDialogOpen = false, chooserActive = false;
    void checkQuitDialog();
};

} // namespace mp
