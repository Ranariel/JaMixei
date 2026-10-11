#pragma once
#include <JuceHeader.h>
#include "../I18n.h"

namespace mp
{
namespace Theme
{
    // paleta (grafite azulado + laranja MPC)
    const juce::Colour bg        { 0xff0f1115 };
    const juce::Colour panel     { 0xff15181e };
    const juce::Colour panel2    { 0xff1b1f27 };
    const juce::Colour panel3    { 0xff242933 };
    const juce::Colour outline   { 0xff2b313c };
    const juce::Colour grid      { 0xff20252f };
    const juce::Colour gridStrong{ 0xff323946 };
    const juce::Colour text      { 0xffeef1f6 };
    const juce::Colour textDim   { 0xffa3acbd };   // mais claro = mais legivel
    const juce::Colour textFaint { 0xff737d90 };
    const juce::Colour accent    { 0xffff8a3d };   // laranja MPC
    const juce::Colour accent2   { 0xff4fc3f7 };   // ciano
    const juce::Colour good      { 0xff6fdc8c };
    const juce::Colour warn      { 0xffffd166 };
    const juce::Colour danger    { 0xffff5c5c };
    const juce::Colour rec       { 0xffff3b47 };
    const juce::Colour playhead  { 0xffffffff };

    // escala global das fontes (os tamanhos no codigo sao "pontos de desenho")
    constexpr float fontScale = 1.3f;

    // fontes embutidas: Inter (interface) e JetBrains Mono (numeros/visores)
    juce::Typeface::Ptr sansTypeface (bool bold);
    juce::Typeface::Ptr monoTypeface (bool bold);

    juce::Font font (float h, bool bold = false);
    // igual a font(), mas troca para a fonte chinesa se o texto tiver ideogramas (nomes de arquivo, idioma)
    juce::Font fontFor (const juce::String& text, float h, bool bold = false);
    juce::Typeface::Ptr cjkTypeface (bool bold);
    // mantem o tamanho, mas usa a fonte chinesa se o texto tiver ideogramas
    juce::Font withCjkFallback (juce::Font f, const juce::String& text);
    extern bool alertUsesCjk;   // janela de aviso com texto em chines (troca de idioma)
    juce::Font mono (float h, bool bold = false);

    // reduz um pouco a fonte (ate 80%) para o texto caber, em vez de espremer as letras
    inline juce::Font fitFont (juce::Font f, const juce::String& text, float width)
    {
        const float w = f.getStringWidthFloat (text);
        if (w > width && w > 0)
            f.setHeight (f.getHeight() * juce::jmax (0.8f, width / w));
        return f;
    }

    inline bool looksLikeShortcut (const juce::String& s)
    {
        for (auto k : { "Ctrl", "Shift", "Alt", "Esc", "Del", "Tab", "Enter", "F1", "Espa", "Space" })
            if (s.contains (k)) return true;
        return false;
    }

    // painel arredondado padrao (cartoes, grupos)
    void drawCard (juce::Graphics& g, juce::Rectangle<float> r, float radius = 6.0f, juce::Colour fill = panel2, bool border = true);
}

// ----------------------------------------------------------------------------
//  Animacoes suaves: um relogio de ~60 fps que aproxima valores (hover, clique,
//  ligado/desligado) do alvo e redesenha so os componentes que estao mudando.
// ----------------------------------------------------------------------------
class UiAnimator : private juce::Timer
{
public:
    static UiAnimator& get();
    // valor animado guardado nas propriedades do componente; retorna o valor atual
    float track (juce::Component& c, const juce::Identifier& key, float target, float speed = 0.22f);
    void shutdown();
private:
    UiAnimator() = default;
    void timerCallback() override;
    struct Item { juce::Component::SafePointer<juce::Component> comp; juce::Identifier key; float target, speed; };
    std::vector<Item> items;
};

class MeuPadLook : public juce::LookAndFeel_V4
{
public:
    MeuPadLook();

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return Theme::font (12.5f); }
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return Theme::font (juce::jmin (12.5f, h * 0.46f), true); }
    juce::Font getPopupMenuFont() override { return Theme::font (13.0f); }
    juce::Font getLabelFont (juce::Label&) override { return Theme::font (12.5f); }
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area, bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool hasSubMenu, const juce::String& text, const juce::String& shortcutKeyText,
                            const juce::Drawable* icon, const juce::Colour* textColour) override;
    void getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator, int standardMenuItemHeight, int& idealWidth, int& idealHeight) override;
    int getPopupMenuBorderSize() override { return 5; }
    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int w, int h, bool vertical, int thumbStart, int thumbSize, bool over, bool down) override;
    void drawTooltip (juce::Graphics&, const juce::String&, int w, int h) override;
    void fillTextEditorBackground (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void drawProgressBar (juce::Graphics&, juce::ProgressBar&, int w, int h, double progress, const juce::String& text) override;
    int getDefaultScrollbarWidth() override { return 10; }
    void drawLabel (juce::Graphics&, juce::Label&) override;
    void drawFileBrowserRow (juce::Graphics&, int w, int h, const juce::File&, const juce::String& filename, juce::Image* icon,
                             const juce::String& fileSizeDescription, const juce::String& fileTimeDescription,
                             bool isDirectory, bool isItemSelected, int itemIndex, juce::DirectoryContentsDisplayComponent&) override;
    void drawTreeviewPlusMinusBox (juce::Graphics&, const juce::Rectangle<float>& area, juce::Colour backgroundColour, bool isOpen, bool isMouseOver) override;
    juce::Font getAlertWindowTitleFont() override { return Theme::alertUsesCjk ? Theme::withCjkFallback (Theme::font (15.0f, true), juce::String::charToString (0x4e2d)) : Theme::font (15.0f, true); }
    juce::Font getAlertWindowMessageFont() override { return Theme::alertUsesCjk ? Theme::withCjkFallback (Theme::font (12.5f), juce::String::charToString (0x4e2d)) : Theme::font (12.5f); }
    juce::Font getAlertWindowFont() override { return Theme::font (12.0f); }
    void drawAlertBox (juce::Graphics&, juce::AlertWindow&, const juce::Rectangle<int>& textArea, juce::TextLayout&) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String& text, juce::Point<int> pos, juce::Rectangle<int> parentArea) override;
    void drawCornerResizer (juce::Graphics&, int, int, bool, bool) override {}
};

} // namespace mp
