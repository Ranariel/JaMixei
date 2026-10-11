// Decodificador via Windows Media Foundation: M4A / AAC / MP4 / WMA / WEBM (quando o
// Windows tem as extensoes de midia instaladas). Usado quando o JUCE nao le o formato.
#include "../I18n.h"
#include "MFDecoder.h"
#include <mutex>

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
 #include <mfapi.h>
 #include <mfidl.h>
 #include <mfreadwrite.h>
 #include <mferror.h>
 #if defined (_MSC_VER)
  #pragma comment (lib, "mfuuid.lib")
  #pragma comment (lib, "ole32.lib")
 #endif
#endif

namespace mp
{
#if JUCE_WINDOWS
template <typename T> struct ComPtr
{
    T* p = nullptr;
    ~ComPtr() { if (p) p->Release(); }
    T** operator&() { return &p; }
    T* operator->() const { return p; }
    operator bool() const { return p != nullptr; }
};

bool decodeWithMediaFoundation (const juce::File& file, juce::AudioBuffer<float>& out, double& sampleRate, juce::String& error)
{
    const HRESULT co = CoInitializeEx (nullptr, COINIT_MULTITHREADED);
    const bool coOwned = SUCCEEDED (co);
    // carregamento dinamico: o programa abre mesmo em Windows sem Media Foundation (edicoes N)
    using StartupFn = HRESULT (WINAPI*) (ULONG, DWORD);
    using CreateReaderFn = HRESULT (WINAPI*) (LPCWSTR, IMFAttributes*, IMFSourceReader**);
    using CreateTypeFn = HRESULT (WINAPI*) (IMFMediaType**);
    static StartupFn pStartup = nullptr;
    static CreateReaderFn pCreateReader = nullptr;
    static CreateTypeFn pCreateType = nullptr;
    static std::once_flag once;
    static HRESULT startup = E_FAIL;
    std::call_once (once, []
    {
        HMODULE plat = LoadLibraryW (L"mfplat.dll");
        HMODULE rw = LoadLibraryW (L"mfreadwrite.dll");
        if (plat == nullptr || rw == nullptr) return;
        pStartup = reinterpret_cast<StartupFn> (reinterpret_cast<void*> (GetProcAddress (plat, "MFStartup")));
        pCreateType = reinterpret_cast<CreateTypeFn> (reinterpret_cast<void*> (GetProcAddress (plat, "MFCreateMediaType")));
        pCreateReader = reinterpret_cast<CreateReaderFn> (reinterpret_cast<void*> (GetProcAddress (rw, "MFCreateSourceReaderFromURL")));
        if (pStartup && pCreateType && pCreateReader) startup = pStartup (MF_VERSION, MFSTARTUP_NOSOCKET);
    });
    if (FAILED (startup))
    {
        error = TR ("Este Windows não tem o Media Foundation (necessário para ") + file.getFileExtension() + TR ("). Converta para MP3 ou WAV.");
        if (coOwned) CoUninitialize();
        return false;
    }

    bool ok = false;
    {
        ComPtr<IMFSourceReader> reader;
        if (FAILED (pCreateReader (file.getFullPathName().toWideCharPointer(), nullptr, &reader)))
        {
            error = TR ("Formato não suportado pelo Windows: ") + file.getFileExtension();
        }
        else
        {
            reader->SetStreamSelection ((DWORD) MF_SOURCE_READER_ALL_STREAMS, FALSE);
            reader->SetStreamSelection ((DWORD) MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE);
            ComPtr<IMFMediaType> partial;
            pCreateType (&partial);
            partial->SetGUID (MF_MT_MAJOR_TYPE, MFMediaType_Audio);
            partial->SetGUID (MF_MT_SUBTYPE, MFAudioFormat_Float);
            if (FAILED (reader->SetCurrentMediaType ((DWORD) MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, partial.p)))
            {
                error = TR ("Não foi possível converter o áudio");
            }
            else
            {
                ComPtr<IMFMediaType> type;
                reader->GetCurrentMediaType ((DWORD) MF_SOURCE_READER_FIRST_AUDIO_STREAM, &type);
                UINT32 channels = 2, rate = 44100;
                type->GetUINT32 (MF_MT_AUDIO_NUM_CHANNELS, &channels);
                type->GetUINT32 (MF_MT_AUDIO_SAMPLES_PER_SECOND, &rate);
                channels = juce::jmax (1u, channels);
                std::vector<float> interleaved;
                interleaved.reserve ((size_t) rate * channels * 240);
                const size_t maxSamples = (size_t) rate * channels * 60 * 40;
                for (;;)
                {
                    DWORD flags = 0;
                    ComPtr<IMFSample> sample;
                    if (FAILED (reader->ReadSample ((DWORD) MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, &flags, nullptr, &sample))) break;
                    if (flags & MF_SOURCE_READERF_ENDOFSTREAM) break;
                    if (! sample) continue;
                    ComPtr<IMFMediaBuffer> buf;
                    if (FAILED (sample->ConvertToContiguousBuffer (&buf))) continue;
                    BYTE* data = nullptr; DWORD len = 0;
                    if (SUCCEEDED (buf->Lock (&data, nullptr, &len)))
                    {
                        const size_t n = len / sizeof (float);
                        const float* f = reinterpret_cast<const float*> (data);
                        interleaved.insert (interleaved.end(), f, f + n);
                        buf->Unlock();
                    }
                    if (interleaved.size() > maxSamples) break;
                }
                const int frames = (int) (interleaved.size() / channels);
                if (frames > 0)
                {
                    const int outCh = (int) juce::jmin (2u, channels);
                    out.setSize (outCh, frames);
                    for (int c = 0; c < outCh; ++c)
                    {
                        auto* d = out.getWritePointer (c);
                        for (int i = 0; i < frames; ++i) d[i] = interleaved[(size_t) i * channels + (size_t) c];
                    }
                    sampleRate = rate;
                    ok = true;
                }
                else error = TR ("Arquivo sem áudio");
            }
        }
    }
    if (coOwned) CoUninitialize();
    return ok;
}
#else
bool decodeWithMediaFoundation (const juce::File&, juce::AudioBuffer<float>&, double&, juce::String& error)
{
    error = TR ("Formato não suportado");
    return false;
}
#endif
} // namespace mp
