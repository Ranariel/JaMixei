#include "../I18n.h"
#include "AudioEngine.h"
#include "Mp3Format.h"

namespace mp
{
AudioEngine::AudioEngine()
{
    previewThread.startThread (juce::Thread::Priority::normal);
    recThread.startThread (juce::Thread::Priority::high);
}

AudioEngine::~AudioEngine()
{
    shutdown();
    if (audioRecording) stopAudioRecording();
    previewThread.stopThread (2000);
    recThread.stopThread (4000);
}

void AudioEngine::initialise (const juce::XmlElement* savedState)
{
   #if JUCE_WINDOWS
    if (savedState == nullptr)
    {
        // primeira execucao: tenta o modo de baixa latencia do Windows (WASAPI compartilhado)
        deviceManager.initialise (0, 2, nullptr, true);
        deviceManager.setCurrentAudioDeviceType ("Windows Audio (Low Latency Mode)", true);
        if (deviceManager.getCurrentAudioDevice() == nullptr)
        {
            deviceManager.setCurrentAudioDeviceType ("Windows Audio", true);
            if (deviceManager.getCurrentAudioDevice() == nullptr) deviceManager.initialiseWithDefaultDevices (0, 2);
        }
        deviceManager.addAudioCallback (this);
        return;
    }
   #endif
    auto err = deviceManager.initialise (0, 2, savedState, true);
    if (err.isNotEmpty() || deviceManager.getCurrentAudioDevice() == nullptr)
        deviceManager.initialiseWithDefaultDevices (0, 2);
    deviceManager.addAudioCallback (this);
}

void AudioEngine::shutdown()
{
    deviceManager.removeAudioCallback (this);
    deviceManager.closeAudioDevice();
    std::unique_ptr<juce::AudioTransportSource> fp;
    std::unique_ptr<juce::AudioFormatReaderSource> fr;
    {
        const juce::SpinLock::ScopedLockType sl (filePreviewLock);
        fp = std::move (filePreview);
        fr = std::move (filePreviewReader);
    }
    if (fp) fp->setSource (nullptr);
}

void AudioEngine::setSnapshot (SnapshotPtr s)
{
    const juce::SpinLock::ScopedLockType sl (snapLock);
    pendingSnap = std::move (s);
}

void AudioEngine::play()  { playReq = 1; uiPlaying = true; }
void AudioEngine::stop()  { playReq = 0; uiPlaying = false; }
void AudioEngine::seek (double beat) { seekPos = juce::jmax (0.0, beat); seekReq = true; uiPos = juce::jmax (0.0, beat); }

void AudioEngine::previewBuffer (BufferPtr buf, double sr, double startSec, double endSec)
{
    Renderer::Preview p;
    p.buf = std::move (buf);
    p.sr = sr;
    p.pos = juce::jmax (0.0, startSec * sr);
    p.end = endSec * sr;
    p.active = p.buf != nullptr && p.end > p.pos;
    const juce::SpinLock::ScopedLockType sl (renderer.previewLock);
    renderer.pendingPreview = p;
    renderer.previewChanged = true;
}

void AudioEngine::stopPreview()
{
    const juce::SpinLock::ScopedLockType sl (renderer.previewLock);
    renderer.pendingPreview = Renderer::Preview();
    renderer.previewChanged = true;
}

bool AudioEngine::previewFile (const juce::File& f, juce::AudioFormatManager& fm)
{
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (f));
    if (reader == nullptr) { stopFilePreview(); return false; }
    const double srcRate = reader->sampleRate;
    auto rs = std::make_unique<juce::AudioFormatReaderSource> (reader.release(), true);
    auto tp = std::make_unique<juce::AudioTransportSource>();
    tp->prepareToPlay (blockSize, sampleRate.load());
    tp->setSource (rs.get(), 32768, &previewThread, srcRate, 2);
    tp->setGain (0.8f);
    tp->start();
    std::unique_ptr<juce::AudioTransportSource> oldTp;
    std::unique_ptr<juce::AudioFormatReaderSource> oldRs;
    {
        const juce::SpinLock::ScopedLockType sl (filePreviewLock);
        oldTp = std::move (filePreview);
        oldRs = std::move (filePreviewReader);
        filePreview = std::move (tp);
        filePreviewReader = std::move (rs);
    }
    if (oldTp) oldTp->setSource (nullptr);
    return true;
}

void AudioEngine::stopFilePreview()
{
    std::unique_ptr<juce::AudioTransportSource> oldTp;
    std::unique_ptr<juce::AudioFormatReaderSource> oldRs;
    {
        const juce::SpinLock::ScopedLockType sl (filePreviewLock);
        oldTp = std::move (filePreview);
        oldRs = std::move (filePreviewReader);
    }
    if (oldTp) oldTp->setSource (nullptr);
}

bool AudioEngine::isFilePreviewing() const
{
    const juce::SpinLock::ScopedLockType sl (const_cast<juce::SpinLock&> (filePreviewLock));
    return filePreview != nullptr && filePreview->isPlaying();
}

void AudioEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    const double sr = device->getCurrentSampleRate();
    blockSize = device->getCurrentBufferSizeSamples();
    sampleRate = sr;
    renderer.prepare (sr, blockSize);
    previewTmp.setSize (2, juce::jmax (4096, blockSize * 2));
    inputTmp.setSize (2, juce::jmax (4096, blockSize * 2));
    {
        const juce::SpinLock::ScopedLockType sl (filePreviewLock);
        if (filePreview) filePreview->prepareToPlay (blockSize, sr);
    }
    juce::MessageManager::callAsync ([this] { if (onSampleRateChanged) onSampleRateChanged(); });
}

void AudioEngine::audioDeviceStopped() {}

void AudioEngine::audioDeviceIOCallbackWithContext (const float* const* in, int numIn, float* const* out, int numOut,
                                                    int numSamples, const juce::AudioIODeviceCallbackContext&)
{
    // a entrada pode compartilhar memoria com a saida: copia antes de renderizar
    int inCh = 0;
    if (numIn > 0 && in != nullptr && numSamples <= inputTmp.getNumSamples())
    {
        for (int c = 0; c < juce::jmin (2, numIn); ++c)
            if (in[c] != nullptr) { inputTmp.copyFrom (c, 0, in[c], numSamples); ++inCh; }
        if (inCh == 1) inputTmp.copyFrom (1, 0, inputTmp, 0, 0, numSamples);
        if (inCh > 0)
        {
            const float pk = juce::jmax (inputTmp.getMagnitude (0, 0, numSamples), inputTmp.getMagnitude (1, 0, numSamples));
            if (pk > inputPeak.load()) inputPeak = pk;
        }
    }

    {
        const juce::SpinLock::ScopedTryLockType sl (snapLock);
        if (sl.isLocked() && pendingSnap != nullptr)
            current = std::move (pendingSnap);
    }

    juce::AudioBuffer<float> buffer (out, numOut, numSamples);
    if (current == nullptr || numOut == 0)
    {
        buffer.clear();
        return;
    }

    renderer.patternMode = patternModeReq.load();
    renderer.recording = recordingReq.load();
    if (seekReq.exchange (false)) renderer.posBeats = seekPos.load();
    const int pr = playReq.exchange (-1);
    if (pr == 1) renderer.playing = true;
    else if (pr == 0) { renderer.playing = false; renderer.allNotesOff (false); }
    if (panicReq.exchange (false)) renderer.allNotesOff (true);

    PadEvent e;
    while (padEvents.pop (e))
    {
        if (e.on) renderer.padOn (*current, e.pad, e.vel, current->liveTrackSlot, 0, -1, true);
        else renderer.padOff (e.pad);
    }

    const double blockStartBeat = renderer.posBeats;
    const bool wasPlaying = renderer.playing;
    renderer.process (*current, buffer, 0, numSamples);

    {
        const juce::SpinLock::ScopedTryLockType sl (filePreviewLock);
        if (sl.isLocked() && filePreview != nullptr && numSamples <= previewTmp.getNumSamples())
        {
            juce::AudioSourceChannelInfo info (&previewTmp, 0, numSamples);
            filePreview->getNextAudioBlock (info);
            for (int c = 0; c < juce::jmin (2, numOut); ++c)
                buffer.addFrom (c, 0, previewTmp, c, 0, numSamples);
        }
    }

    // gravacao / monitoracao
    if (audioRecording.load())
    {
        const juce::SpinLock::ScopedTryLockType sl (recLock);
        if (sl.isLocked() && recWriter != nullptr)
        {
            if (recSource == RecSource::Input && recMonitor && inCh > 0)
                for (int c = 0; c < juce::jmin (2, numOut); ++c)
                    buffer.addFrom (c, 0, inputTmp, c, 0, numSamples);

            if (wasPlaying)
            {
                if (! recGotFirst.load()) { recStartBeat = blockStartBeat; recGotFirst = true; }
                const float* chans[2];
                if (recSource == RecSource::Input)
                {
                    if (inCh == 0) inputTmp.clear (0, numSamples);
                    chans[0] = inputTmp.getReadPointer (0);
                    chans[1] = inputTmp.getReadPointer (1);
                }
                else
                {
                    chans[0] = buffer.getReadPointer (0);
                    chans[1] = buffer.getReadPointer (numOut > 1 ? 1 : 0);
                }
                if (recWriter->write (chans, numSamples)) recSamples += numSamples;
            }
        }
    }

    uiPos = renderer.posBeats;
}

bool AudioEngine::hasInputDevice()
{
    if (auto* t = deviceManager.getCurrentDeviceTypeObject())
        return t->getDeviceNames (true).size() > 0;
    return false;
}

bool AudioEngine::setInputEnabled (bool enabled, juce::String& error)
{
    auto setup = deviceManager.getAudioDeviceSetup();
    if (enabled)
    {
        auto* type = deviceManager.getCurrentDeviceTypeObject();
        if (type == nullptr) { error = TR ("Nenhum dispositivo de audio"); return false; }
        if (setup.inputDeviceName.isEmpty())
        {
            auto names = type->getDeviceNames (true);
            if (names.isEmpty()) { error = TR ("Nenhum microfone ou entrada de audio encontrado"); return false; }
            setup.inputDeviceName = names[juce::jmax (0, type->getDefaultDeviceIndex (true))];
        }
        setup.useDefaultInputChannels = false;
        setup.inputChannels.clear();
        setup.inputChannels.setRange (0, 2, true);
    }
    else
    {
        setup.inputChannels.clear();
        setup.useDefaultInputChannels = false;
        setup.inputDeviceName = {};
    }
    error = deviceManager.setAudioDeviceSetup (setup, true);
    return error.isEmpty();
}

bool AudioEngine::startAudioRecording (const juce::File& file, RecSource source, bool mp3, bool monitorInput, juce::String& error)
{
    if (audioRecording) { error = TR ("Ja esta gravando"); return false; }
    file.getParentDirectory().createDirectory();
    file.deleteFile();
    std::unique_ptr<juce::OutputStream> stream (file.createOutputStream());
    if (stream == nullptr) { error = TR ("Nao foi possivel criar ") + file.getFullPathName(); return false; }
    std::unique_ptr<juce::AudioFormat> fmt;
    if (mp3) fmt = std::make_unique<Mp3AudioFormat>();
    else fmt = std::make_unique<juce::WavAudioFormat>();
    const double sr = sampleRate.load();
    std::unique_ptr<juce::AudioFormatWriter> w (fmt->createWriterFor (stream.get(), sr, 2, 24, {}, 0));
    if (w == nullptr) { error = TR ("Formato de gravacao indisponivel"); return false; }
    stream.release();
    auto tw = std::make_unique<juce::AudioFormatWriter::ThreadedWriter> (w.release(), recThread, (int) (sr * 4));
    recGotFirst = false;
    recSamples = 0;
    recFile = file;
    {
        const juce::SpinLock::ScopedLockType sl (recLock);
        recSource = source;
        recMonitor = monitorInput;
        recWriter = std::move (tw);
    }
    audioRecording = true;
    return true;
}

AudioEngine::RecordingInfo AudioEngine::stopAudioRecording()
{
    RecordingInfo info;
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> tw;
    {
        const juce::SpinLock::ScopedLockType sl (recLock);
        tw = std::move (recWriter);
        audioRecording = false;
    }
    tw.reset();   // grava o que falta e fecha o arquivo
    info.file = recFile;
    info.startBeat = recStartBeat.load();
    info.sampleRate = sampleRate.load();
    info.fromInput = recSource == RecSource::Input;
    info.gotAudio = recGotFirst.load() && recSamples.load() > 0;
    if (auto* dev = deviceManager.getCurrentAudioDevice())
        info.latencySec = (dev->getInputLatencyInSamples() + dev->getOutputLatencyInSamples()) / juce::jmax (1.0, dev->getCurrentSampleRate());
    return info;
}

} // namespace mp
