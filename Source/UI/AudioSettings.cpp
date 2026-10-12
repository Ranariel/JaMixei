#include "AudioSettings.h"
#include "Theme.h"
#include "../I18n.h"

namespace mp
{
namespace
{
    bool sameRate (double a, double b) { return std::abs (a - b) < 1.0; }

    juce::String findSimilar (const juce::StringArray& names, const juce::String& wanted)
    {
        if (wanted.isEmpty()) return {};
        for (auto& n : names) if (n == wanted) return n;
        for (auto& n : names) if (n.containsIgnoreCase (wanted) || wanted.containsIgnoreCase (n)) return n;
        // "Alto-falantes (Realtek(R) Audio)" x "Alto-falantes (Realtek(R) Au": compara o comeco
        for (auto& n : names) if (n.substring (0, 20).equalsIgnoreCase (wanted.substring (0, 20))) return n;
        return {};
    }
}

const std::vector<double>& AudioSettingsPanel::standardRates()
{
    static const std::vector<double> r { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 };
    return r;
}

juce::String AudioSettingsPanel::rateLabel (double sr)
{
    juce::String khz = juce::String (sr / 1000.0, sr == 44100.0 || sr == 88200.0 || sr == 176400.0 ? 1 : 0) + " kHz";
    if (sameRate (sr, 44100)) return khz + TR (" - qualidade de CD (recomendado)");
    if (sameRate (sr, 48000)) return khz + TR (" - padrão de vídeo / estúdio");
    if (sr >= 176000) return khz + TR (" - Hi-Res (usa muito mais CPU)");
    if (sr >= 88000) return khz + TR (" - alta resolução (mais CPU)");
    return khz;
}

juce::String AudioSettingsPanel::applySampleRate (juce::AudioDeviceManager& dm, double sr)
{
    auto* dev = dm.getCurrentAudioDevice();
    if (dev != nullptr && sameRate (dev->getCurrentSampleRate(), sr)) return {};

    const auto original = dm.getAudioDeviceSetup();
    const auto originalType = dm.getCurrentAudioDeviceType();

    auto trySetRate = [&dm, sr]() -> bool
    {
        auto* d = dm.getCurrentAudioDevice();
        if (d == nullptr) return false;
        bool supported = false;
        for (auto r : d->getAvailableSampleRates()) if (sameRate (r, sr)) supported = true;
        if (! supported) return false;
        auto setup = dm.getAudioDeviceSetup();
        setup.sampleRate = sr;
        dm.setAudioDeviceSetup (setup, true);
        return dm.getCurrentAudioDevice() != nullptr && sameRate (dm.getCurrentAudioDevice()->getCurrentSampleRate(), sr);
    };

    if (trySetRate()) return {};

    // o driver atual nao aceita: tenta os que convertem a taxa (DirectSound) ou acessam direto a placa (Exclusivo)
    for (auto typeName : { juce::String ("DirectSound"), juce::String ("Windows Audio (Exclusive Mode)"), juce::String ("ASIO") })
    {
        if (typeName == originalType) continue;
        juce::AudioIODeviceType* type = nullptr;
        for (auto* t : dm.getAvailableDeviceTypes()) if (t->getTypeName() == typeName) type = t;
        if (type == nullptr) continue;
        type->scanForDevices();
        const auto outs = type->getDeviceNames (false);
        if (outs.isEmpty()) continue;
        dm.setCurrentAudioDeviceType (typeName, true);
        auto setup = dm.getAudioDeviceSetup();
        auto out = findSimilar (outs, original.outputDeviceName);
        setup.outputDeviceName = out.isNotEmpty() ? out : outs[juce::jmax (0, type->getDefaultDeviceIndex (false))];
        if (original.inputDeviceName.isNotEmpty())
        {
            const auto ins = type->getDeviceNames (true);
            auto in = findSimilar (ins, original.inputDeviceName);
            setup.inputDeviceName = in.isNotEmpty() ? in : ins[juce::jmax (0, type->getDefaultDeviceIndex (true))];
            setup.inputChannels = original.inputChannels;
            setup.useDefaultInputChannels = false;
        }
        else setup.inputDeviceName = {};
        setup.sampleRate = sr;
        setup.bufferSize = 0;
        dm.setAudioDeviceSetup (setup, true);
        if (trySetRate())
            return TR ("O driver anterior só aceita a taxa configurada no Windows; para usar ") + rateLabel (sr).upToFirstOccurrenceOf (" - ", false, false)
                   + TR (", o Ja Mixei passou a usar o driver ") + typeName + ".";
    }

    // nada funcionou: volta como estava
    dm.setCurrentAudioDeviceType (originalType, true);
    dm.setAudioDeviceSetup (original, true);
    return TR ("Esta placa de som não aceita ") + rateLabel (sr).upToFirstOccurrenceOf (" - ", false, false)
           + TR (". Dica: no Windows, abra Configurações de som > Mais configurações de som > Propriedades da saída > Avançado e escolha \"24 bits, 44100 Hz\" ou \"16 bits, 44100 Hz (Qualidade de CD)\".");
}

// ----------------------------------------------------------------------------
AudioSettingsPanel::AudioSettingsPanel (juce::AudioDeviceManager& d) : dm (d)
{
    auto setupLabel = [this] (juce::Label& l, const juce::String& text)
    {
        l.setText (text, juce::dontSendNotification);
        l.setFont (Theme::font (12.5f, true));
        l.setColour (juce::Label::textColourId, Theme::textDim);
        l.setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (l);
    };
    setupLabel (typeL, TR ("Driver"));
    setupLabel (outL, TR ("Saída (fone / caixa)"));
    setupLabel (inL, TR ("Entrada (microfone)"));
    setupLabel (rateL, TR ("Taxa de amostragem"));
    setupLabel (bufL, TR ("Buffer (latência)"));
    for (auto* c : { &typeBox, &outBox, &inBox, &rateBox, &bufBox }) addAndMakeVisible (*c);
    typeBox.onChange = [this] { if (! updating) typeChanged(); };
    outBox.onChange = [this] { if (! updating) outputChanged(); };
    inBox.onChange = [this] { if (! updating) inputChanged(); };
    rateBox.onChange = [this] { if (! updating) rateChanged(); };
    bufBox.onChange = [this] { if (! updating) bufferChanged(); };

    info.setFont (Theme::mono (12.0f));
    info.setColour (juce::Label::textColourId, Theme::good);
    info.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (info);
    note.setFont (Theme::font (13.0f));
    note.setColour (juce::Label::textColourId, Theme::textDim);
    note.setJustificationType (juce::Justification::topLeft);
    note.setMinimumHorizontalScale (1.0f);
    note.setText (TR ("44,1 kHz é a qualidade de CD e usa menos CPU. Buffer menor = menos atraso ao tocar os pads; se o som falhar (estalos), aumente o buffer."),
                  juce::dontSendNotification);
    addAndMakeVisible (note);

    advancedBtn.setButtonText (TR ("Avançado..."));
    advancedBtn.setTooltip (TR ("Janela completa de configurações de áudio (canais, teste de som)"));
    advancedBtn.onClick = [this]
    {
        auto* sel = new juce::AudioDeviceSelectorComponent (dm, 0, 2, 2, 2, false, false, true, false);
        sel->setSize (620, 460);
        juce::DialogWindow::LaunchOptions o;
        o.content.setOwned (sel);
        o.dialogTitle = TR ("Configurações de áudio (avançado)");
        o.dialogBackgroundColour = Theme::panel;
        o.escapeKeyTriggersCloseButton = true;
        o.useNativeTitleBar = true;
        o.launchAsync();
    };
    closeBtn.setButtonText (TR ("Fechar"));
    closeBtn.onClick = [this] { if (auto* w = findParentComponentOfClass<juce::DialogWindow>()) w->exitModalState (0); };
    addAndMakeVisible (advancedBtn);
    addAndMakeVisible (closeBtn);

    dm.addChangeListener (this);
    refresh();
    setSize (660, 390);
}

AudioSettingsPanel::~AudioSettingsPanel() { dm.removeChangeListener (this); }

void AudioSettingsPanel::refresh()
{
    const juce::ScopedValueSetter<bool> svs (updating, true);
    typeBox.clear (juce::dontSendNotification);
    int i = 1;
    for (auto* t : dm.getAvailableDeviceTypes())
    {
        typeBox.addItem (t->getTypeName(), i);
        if (t->getTypeName() == dm.getCurrentAudioDeviceType()) typeBox.setSelectedId (i, juce::dontSendNotification);
        ++i;
    }
    const auto setup = dm.getAudioDeviceSetup();
    outBox.clear (juce::dontSendNotification);
    inBox.clear (juce::dontSendNotification);
    if (auto* type = dm.getCurrentDeviceTypeObject())
    {
        const auto outs = type->getDeviceNames (false);
        for (int k = 0; k < outs.size(); ++k)
        {
            outBox.addItem (outs[k], k + 1);
            if (outs[k] == setup.outputDeviceName) outBox.setSelectedId (k + 1, juce::dontSendNotification);
        }
        inBox.addItem (TR ("Nenhuma (desligada)"), 1);
        inBox.setSelectedId (1, juce::dontSendNotification);
        const auto ins = type->getDeviceNames (true);
        for (int k = 0; k < ins.size(); ++k)
        {
            inBox.addItem (ins[k], k + 2);
            if (ins[k] == setup.inputDeviceName) inBox.setSelectedId (k + 2, juce::dontSendNotification);
        }
    }

    rateBox.clear (juce::dontSendNotification);
    rateValues.clear();
    bufBox.clear (juce::dontSendNotification);
    bufValues.clear();
    auto* dev = dm.getCurrentAudioDevice();
    const double cur = dev != nullptr ? dev->getCurrentSampleRate() : 0.0;
    // todas as taxas comuns aparecem; as que este driver nao aceita direto trocam de driver sozinhas
    std::vector<double> rates = standardRates();
    if (cur > 0 && std::none_of (rates.begin(), rates.end(), [cur] (double r) { return sameRate (r, cur); })) rates.push_back (cur);
    std::sort (rates.begin(), rates.end());
    for (auto r : rates)
    {
        bool native = false;
        if (dev != nullptr) for (auto a : dev->getAvailableSampleRates()) if (sameRate (a, r)) native = true;
        rateValues.push_back (r);
        rateBox.addItem (rateLabel (r) + (native || dev == nullptr ? juce::String() : TR ("  (troca o driver)")), (int) rateValues.size());
        if (sameRate (r, cur)) rateBox.setSelectedId ((int) rateValues.size(), juce::dontSendNotification);
    }
    if (dev != nullptr)
    {
        const int curBuf = dev->getCurrentBufferSizeSamples();
        for (auto b : dev->getAvailableBufferSizes())
        {
            if (b < 32 || b > 8192) continue;
            bufValues.push_back (b);
            bufBox.addItem (juce::String (b) + TR (" amostras  (") + juce::String (1000.0 * b / juce::jmax (1.0, cur), 1) + " ms)", (int) bufValues.size());
            if (b == curBuf) bufBox.setSelectedId ((int) bufValues.size(), juce::dontSendNotification);
        }
        const double lat = (dev->getOutputLatencyInSamples() + curBuf) / juce::jmax (1.0, cur) * 1000.0;
        info.setColour (juce::Label::textColourId, cur > 50000 ? Theme::warn : Theme::good);
        info.setText (TR ("Agora: ") + dm.getCurrentAudioDeviceType() + "  |  " + juce::String (cur / 1000.0, 1) + " kHz  |  "
                      + juce::String (curBuf) + TR (" amostras  |  atraso ~") + juce::String (juce::roundToInt (lat)) + " ms", juce::dontSendNotification);
    }
    else
    {
        info.setColour (juce::Label::textColourId, Theme::danger);
        info.setText (TR ("Nenhum dispositivo de áudio aberto"), juce::dontSendNotification);
    }
}

void AudioSettingsPanel::typeChanged()
{
    dm.setCurrentAudioDeviceType (typeBox.getText(), true);
    refresh();
}

void AudioSettingsPanel::outputChanged()
{
    auto setup = dm.getAudioDeviceSetup();
    setup.outputDeviceName = outBox.getText();
    const auto err = dm.setAudioDeviceSetup (setup, true);
    if (err.isNotEmpty()) juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, TR ("Configurações de áudio"), err);
    refresh();
}

void AudioSettingsPanel::inputChanged()
{
    auto setup = dm.getAudioDeviceSetup();
    if (inBox.getSelectedId() <= 1) { setup.inputDeviceName = {}; setup.inputChannels.clear(); }
    else
    {
        setup.inputDeviceName = inBox.getText();
        setup.useDefaultInputChannels = false;
        setup.inputChannels.clear();
        setup.inputChannels.setRange (0, 2, true);
    }
    const auto err = dm.setAudioDeviceSetup (setup, true);
    if (err.isNotEmpty()) juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, TR ("Configurações de áudio"), err);
    refresh();
}

void AudioSettingsPanel::rateChanged()
{
    const int idx = rateBox.getSelectedId() - 1;
    if (! juce::isPositiveAndBelow (idx, (int) rateValues.size())) return;
    const auto msg = applySampleRate (dm, rateValues[(size_t) idx]);
    refresh();
    if (msg.isNotEmpty())
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, TR ("Taxa de amostragem"), msg);
}

void AudioSettingsPanel::bufferChanged()
{
    const int idx = bufBox.getSelectedId() - 1;
    if (! juce::isPositiveAndBelow (idx, (int) bufValues.size())) return;
    auto setup = dm.getAudioDeviceSetup();
    setup.bufferSize = bufValues[(size_t) idx];
    dm.setAudioDeviceSetup (setup, true);
    refresh();
}

void AudioSettingsPanel::paint (juce::Graphics& g) { g.fillAll (Theme::panel); }

void AudioSettingsPanel::resized()
{
    auto r = getLocalBounds().reduced (18, 16);
    auto row = [&r] (juce::Label& l, juce::Component& c)
    {
        auto a = r.removeFromTop (32);
        l.setBounds (a.removeFromLeft (170));
        a.removeFromLeft (10);
        c.setBounds (a.reduced (0, 2));
        r.removeFromTop (8);
    };
    row (typeL, typeBox);
    row (outL, outBox);
    row (inL, inBox);
    row (rateL, rateBox);
    row (bufL, bufBox);
    info.setBounds (r.removeFromTop (24));
    auto bottom = r.removeFromBottom (32);
    closeBtn.setBounds (bottom.removeFromRight (110));
    bottom.removeFromRight (8);
    advancedBtn.setBounds (bottom.removeFromRight (130));
    note.setBounds (r.reduced (0, 2));
}
} // namespace mp
