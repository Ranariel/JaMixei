#pragma once
#include <JuceHeader.h>

// ----------------------------------------------------------------------------
//  Idiomas do Ja Mixei: Portugues (Brasil, padrao), English, Espanol, Chines.
//  O texto original (portugues) e a chave; TR ("texto") devolve a traducao.
// ----------------------------------------------------------------------------
namespace mp
{
namespace i18n
{
    enum class Lang { PtBR = 0, En = 1, Es = 2, Zh = 3 };

    void setLanguage (Lang l);
    Lang language();
    juce::String code (Lang l);                 // "pt", "en", "es", "zh"
    Lang fromCode (const juce::String& c);      // padrao: PtBR
    juce::String nativeName (Lang l);           // nome do idioma no proprio idioma
    juce::String tr (const char* utf8Portuguese);
}
} // namespace mp

#define TR(text) ::mp::i18n::tr (text)
