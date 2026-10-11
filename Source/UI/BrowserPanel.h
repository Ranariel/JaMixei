#pragma once
#include <JuceHeader.h>
#include "Widgets.h"
#include "Icons.h"

namespace mp
{
class BrowserPanel : public juce::Component,
                     public Session::Listener,
                     private juce::FileBrowserListener,
                     private juce::ListBoxModel
{
public:
    explicit BrowserPanel (Session& s);
    ~BrowserPanel() override;

    void sessionChanged (int what) override;
    void resized() override;
    void paint (juce::Graphics& g) override;
    void setRoot (const juce::File& dir);

    std::function<void (int sourceId, double s, double e)> onOpenInEditor;

private:
    // FileBrowserListener
    void selectionChanged() override;
    void fileClicked (const juce::File&, const juce::MouseEvent&) override {}
    void fileDoubleClicked (const juce::File& f) override;
    void browserRootChanged (const juce::File&) override {}
    // ListBoxModel
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    void listBoxItemClicked (int row, const juce::MouseEvent& e) override;
    juce::var getDragSourceDescription (const juce::SparseSet<int>& rows) override;

    Session& session;
    juce::TimeSliceThread scanThread { "MeuPad scan" };
    juce::WildcardFileFilter filter;
    juce::DirectoryContentsList dirList;
    juce::FileTreeComponent tree;
    juce::ListBox sourceList { "fontes", this };
    IconTextButton musicBtn { Icon::Music, TR ("Músicas") }, downBtn { Icon::Download, TR ("Downloads") },
                   deskBtn { Icon::Desktop, TR ("Desktop") }, folderBtn { Icon::Folder, TR ("Pasta...") };
    IconButton upBtn { Icon::ArrowUp, TR ("Pasta acima") };
    juce::ToggleButton autoPreview { TR ("Pré-escuta") };
    juce::Label pathLabel;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::File root;
};

} // namespace mp
