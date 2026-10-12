#pragma once
#include <JuceHeader.h>

namespace mp
{
// ----------------------------------------------------------------------------
//  Configuracoes de audio do Ja Mixei (no lugar da janela generica do JUCE):
//  driver, saida, entrada, taxa de amostragem (44,1 kHz CD, 48 kHz...) e buffer.
//  Quando o driver atual nao aceita a taxa escolhida (Windows Audio compartilhado
//  so aceita a taxa configurada no Windows), troca sozinho para o DirectSound,
//  que converte qualquer taxa.
// ----------------------------------------------------------------------------
class AudioSettingsPanel : public juce::Component, private juce::ChangeListener
{
public:
    explicit AudioSettingsPanel (juce::AudioDeviceManager& dm);
    ~AudioSettingsPanel() override;
    void resized() override;
    void paint (juce::Graphics& g) override;

    static const std::vector<double>& standardRates();
    static juce::String rateLabel (double sr);
    // tenta aplicar a taxa (trocando de driver se preciso); devolve texto explicando o que aconteceu
    static juce::String applySampleRate (juce::AudioDeviceManager& dm, double sr);

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { refresh(); }
    void refresh();
    void typeChanged();
    void outputChanged();
    void inputChanged();
    void rateChanged();
    void bufferChanged();

    juce::AudioDeviceManager& dm;
    juce::ComboBox typeBox, outBox, inBox, rateBox, bufBox;
    juce::Label typeL, outL, inL, rateL, bufL, info, note;
    juce::TextButton advancedBtn, closeBtn;
    std::vector<double> rateValues;
    std::vector<int> bufValues;
    bool updating = false;
};
} // namespace mp
