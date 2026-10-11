#include "I18n.h"
#include <unordered_map>

namespace mp
{
namespace i18n
{
namespace
{
    struct Entry { const char* pt; const char* en; const char* es; const char* zh; };
    const Entry entries[] =
    {
       #include "I18nTable.inc"
    };

    Lang current = Lang::PtBR;

    const std::unordered_map<std::string, const Entry*>& table()
    {
        static const auto map = []
        {
            std::unordered_map<std::string, const Entry*> m;
            for (auto& e : entries) m.emplace (e.pt, &e);
            return m;
        }();
        return map;
    }
}

void setLanguage (Lang l) { current = l; }
Lang language() { return current; }

juce::String code (Lang l)
{
    switch (l)
    {
        case Lang::En: return "en";
        case Lang::Es: return "es";
        case Lang::Zh: return "zh";
        default:       return "pt";
    }
}

Lang fromCode (const juce::String& c)
{
    const auto s = c.trim().toLowerCase();
    if (s.startsWith ("en")) return Lang::En;
    if (s.startsWith ("es")) return Lang::Es;
    if (s.startsWith ("zh")) return Lang::Zh;
    return Lang::PtBR;
}

juce::String nativeName (Lang l)
{
    switch (l)
    {
        case Lang::En: return "English";
        case Lang::Es: return juce::String (juce::CharPointer_UTF8 ("Espa\xc3\xb1ol"));
        case Lang::Zh: return juce::String (juce::CharPointer_UTF8 ("\xe4\xb8\xad\xe6\x96\x87 (\xe7\xae\x80\xe4\xbd\x93)"));
        default:       return juce::String (juce::CharPointer_UTF8 ("Portugu\xc3\xaas (Brasil)"));
    }
}

juce::String tr (const char* text)
{
    if (current != Lang::PtBR)
    {
        auto& m = table();
        auto it = m.find (text);
        if (it != m.end())
        {
            const char* t = current == Lang::En ? it->second->en : current == Lang::Es ? it->second->es : it->second->zh;
            return juce::String (juce::CharPointer_UTF8 (t));
        }
    }
    return juce::String (juce::CharPointer_UTF8 (text));
}

} // namespace i18n
} // namespace mp
