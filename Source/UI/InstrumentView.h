#pragma once
#include <JuceHeader.h>
#include "Widgets.h"
#include "Icons.h"
#include <set>

namespace mp
{
// ============================================================================
//  Aba INSTRUMENTOS (estilo FL Studio): lista de instrumentos ("channel rack"),
//  piano roll do padrao atual e teclado de piano completo (88 teclas).
//  As notas ficam dentro do padrao: tocam no modo PADRAO e nos clipes do arranjo.
// ============================================================================
class InstrumentView : public juce::Component, public Session::Listener, private juce::Timer
{
public:
    explicit InstrumentView (Session& s);
    ~InstrumentView() override;

    void sessionChanged (int what) override;
    void resized() override;
    void paint (juce::Graphics& g) override;
    void visibilityChanged() override;

    // teclado do PC (estilo FL Studio): Z S X D C V... (oitava de baixo) e Q 2 W 3 E... (oitava de cima)
    static int pcKeySemitone (int keyCode);     // -1 se a tecla nao toca nota
    int pcBaseNote() const { return pcOctave * 12 + 12; }   // nota da tecla Z
    void shiftPcOctave (int delta);
    void setPcKeyHeld (int pitch, bool down);

    // atalhos da aba (chamados pela janela principal)
    void deleteSelectedNotes();
    void selectAllNotes();
    void duplicateSelectedNotes();
    void quantizeNotes();
    bool keyboardLit (int pitch) const;          // tecla soando (teclado, MIDI ou sequencia)

private:
    class ChannelList;
    class PianoRoll;
    class Keyboard;
    void timerCallback() override;
    void refreshControls();
    void addInstrumentMenu();
    void editChannel (const juce::String& undoLabel, std::function<void (InstrumentChannel&)> fn, bool undo = true);
    double gridBeats() const;

    Session& session;
    std::unique_ptr<ChannelList> channelList;
    std::unique_ptr<PianoRoll> roll;
    std::unique_ptr<Keyboard> keyboard;
    juce::ComboBox instBox, patBox, lenBox, gridBox;
    juce::Label instL, patL, lenL, gridL, pcLabel;
    Knob vol { TR ("Volume"), -40.0, 12.0, 0.0, 0.1 }, pan { TR ("Pan"), -1.0, 1.0, 0.0, 0.01 },
         attack { TR ("Ataque"), 0.0, 2000.0, 2.0, 0.1 }, release { TR ("Release"), 5.0, 4000.0, 250.0, 1.0 },
         transpose { TR ("Transpor"), -24.0, 24.0, 0.0, 1.0 };
    juce::TextButton quantBtn { TR ("Quantizar") }, clearBtn { TR ("Limpar notas") }, addBtn { TR ("+ Instrumento") }, arrBtn { TR ("No arranjo") };
    static constexpr int toolbarH = 72, channelW = 230, keyboardH = 150;
    juce::uint64 lastLo = 0, lastHi = 0;
    bool lastReady = false;
    int pcOctave = 3;                 // tecla Z = C3
    std::set<int> pcHeld;
    bool updating = false;
    friend class ChannelList;
    friend class PianoRoll;
    friend class Keyboard;
};
} // namespace mp
