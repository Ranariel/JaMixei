# Envolve os textos visiveis do codigo C++ com TR("...") usando a tabela de traducoes.
import os, re, sys
here = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, here)
from i18n_table import T
keys = set(t[0] for t in T)
src = os.path.join(here, "..", "Source")
files = [os.path.join("UI", f) for f in os.listdir(os.path.join(src, "UI")) if f.endswith((".cpp", ".h"))]
files += ["Session.cpp", "Main.cpp", "Audio/AudioEngine.cpp", "Audio/SourcePool.cpp", "Audio/MFDecoder.cpp", "Model/Project.cpp"]

LIT = r'"(?:[^"\\\n]|\\.)*"'
SEQ = r'(?:' + LIT + r'\s*)+'
lit_re = re.compile(LIT)

def tr_join(seq):
    parts = lit_re.findall(seq)
    if not any(p[1:-1] in keys for p in parts):
        return None
    return " + ".join("TR (%s)" % p for p in parts)

def process(text):
    # 1) juce::String (juce::CharPointer_UTF8 ( "..." "..." )) e juce::CharPointer_UTF8 ( ... )
    def rep_cp(m):
        r = tr_join(m.group(1))
        return ("(" + r + ")") if r and " + " in r else (r if r else m.group(0))
    text = re.sub(r'juce::String\s*\(\s*juce::CharPointer_UTF8\s*\(\s*(' + SEQ + r')\)\s*\)', rep_cp, text)
    text = re.sub(r'juce::CharPointer_UTF8\s*\(\s*(' + SEQ + r')\)', rep_cp, text)
    # 2) literais soltos
    out, i = [], 0
    for m in re.finditer(SEQ, text):
        s, e = m.start(), m.end()
        line_start = text.rfind("\n", 0, s) + 1
        line = text[line_start:s]
        before = text[max(0, s - 40):s]
        stripped = line.lstrip()
        if stripped.startswith("//") or stripped.startswith("#") or "//" in line:
            continue
        if re.search(r'TR\s*\(\s*$', before) or re.search(r'getChildFile\s*\(\s*$', before) \
           or re.search(r'(Identifier|getValue|setValue|getIntValue|getBoolValue|track\s*\(\*this,|getProperty|setProperty|contains)\s*\(?\s*$', before):
            continue
        seq = m.group(0)
        trail = re.search(r'\s*$', seq).group(0)
        r = tr_join(seq.rstrip())
        if r is None:
            continue
        out.append(text[i:s]); out.append(r + trail); i = e
    out.append(text[i:])
    return "".join(out)

for f in files:
    p = os.path.join(src, f)
    t = open(p, encoding="utf-8").read()
    n = process(t)
    if n != t:
        open(p, "w", encoding="utf-8").write(n)
        print("alterado:", f, n.count("TR (") - t.count("TR ("))
