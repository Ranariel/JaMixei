#pragma once
#include <JuceHeader.h>

namespace mp
{
// Conjunto unico de icones vetoriais (grade 24x24, traco arredondado), para que
// todos os simbolos do programa tenham o mesmo estilo e espessura.
enum class Icon
{
    Play, Pause, Stop, Record, Mic, Loop, Metronome, Undo, Redo, Sliders, Gear,
    Folder, FileAudio, FileVideo, ArrowUp, Music, Download, Desktop, Plus,
    Arrange, Waveform, Grid, Mixer, Headphones, Scissors, Magnet, Fit, Follow, Pads,
    Tap, Trash, ChevronRight, ChevronDown, Check, Bolt, Piano
};

namespace Icons
{
    // desenha o icone centralizado em r (quadrado), na cor indicada
    void draw (juce::Graphics& g, Icon icon, juce::Rectangle<float> r, juce::Colour colour);
    // caminho ja pronto para preenchimento, na grade 0..24
    const juce::Path& path (Icon icon);
}

// Botao so com icone (barra de transporte, ferramentas). Anima hover/clique/ligado.
class IconButton : public juce::Button
{
public:
    IconButton (Icon i, const juce::String& tip) : juce::Button (tip), icon (i) { setTooltip (tip); }
    juce::Colour onColour { juce::Colour (0xffff8a3d) };
    bool flat = false;          // sem fundo quando desligado (estilo barra de ferramentas)
    bool tintIconOnly = false;  // ligado = so o icone fica colorido (sem preencher o botao)
    Icon icon;
    Icon onIcon { Icon::Pause };
    bool useOnIcon = false;     // ex.: Play vira Pause quando ligado
    juce::Colour iconColour;    // cor fixa do icone quando desligado (transparente = padrao)
    float iconScale = 0.56f;    // tamanho do icone em relacao ao botao
    void paintButton (juce::Graphics& g, bool over, bool down) override;
};

// Botao com icone + texto (botoes de atalho, abas)
class IconTextButton : public juce::Button
{
public:
    IconTextButton (Icon i, const juce::String& text) : juce::Button (text), icon (i) {}
    Icon icon;
    juce::Colour onColour { juce::Colour (0xffff8a3d) };
    void paintButton (juce::Graphics& g, bool over, bool down) override;
};

} // namespace mp
