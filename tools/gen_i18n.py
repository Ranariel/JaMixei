# Gera Source/I18nTable.inc a partir de tools/i18n_table.py
import os, sys
here = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, here)
from i18n_table import T

def lit(s):
    return '"' + s + '"'

out = ["// Gerado por tools/gen_i18n.py - nao editar a mao", ""]
for pt, en, es, zh in T:
    out.append("{ %s, %s, %s, %s }," % (lit(pt), lit(en), lit(es), lit(zh)))
path = os.path.join(here, "..", "Source", "I18nTable.inc")
open(path, "w", encoding="utf-8").write("\n".join(out) + "\n")
print("ok", len(T), "entradas")
