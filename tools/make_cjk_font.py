# Gera Resources/fonts/NotoSansSC-*.ttf (subconjunto do Noto Sans CJK SC, convertido para TrueType)
# Uso: python3 tools/make_cjk_font.py  (precisa do pacote fonts-noto-cjk e de fonttools)
import os, sys
here = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, here)
from i18n_table import T
from fontTools.ttLib import TTCollection, TTFont, newTable
from fontTools import subset
from fontTools.pens.cu2quPen import Cu2QuPen
from fontTools.pens.ttGlyphPen import TTGlyphPen

ui = set()
for t in T: ui |= set(t[3])
ui |= set(chr(c) for c in range(0x20, 0x7f)) | set(chr(c) for c in range(0xa0, 0x100))
ui |= set("中文简体（）：，。！？“”‘’、…—·●")
gb = set()
for hi in range(0xB0, 0xD8):
    for lo in range(0xA1, 0xFF):
        try: gb.add(bytes([hi, lo]).decode('gb2312'))
        except Exception: pass
gb |= set(chr(c) for c in range(0x3000, 0x3040)) | set(chr(c) for c in range(0xFF00, 0xFF60))

def pick(path):
    col = TTCollection(path)
    for i, f in enumerate(col.fonts):
        if f['name'].getDebugName(1).startswith('Noto Sans CJK SC'): return i
    raise SystemExit('Noto Sans CJK SC nao encontrado')

def otf2ttf(font):
    order = font.getGlyphOrder(); gs = font.getGlyphSet()
    glyf = newTable('glyf'); glyf.glyphOrder = order; glyf.glyphs = {}
    for name in order:
        pen = TTGlyphPen(gs)
        gs[name].draw(Cu2QuPen(pen, 1.0, reverse_direction=True))
        glyf[name] = pen.glyph()
    font['glyf'] = glyf; font['loca'] = newTable('loca')
    for t in ('CFF ', 'VORG'):
        if t in font: del font[t]
    maxp = font['maxp']; maxp.tableVersion = 0x00010000
    for a in ('maxTwilightPoints', 'maxStorage', 'maxFunctionDefs', 'maxInstructionDefs', 'maxStackElements', 'maxSizeOfInstructions', 'maxComponentElements'):
        setattr(maxp, a, 0)
    maxp.maxZones = 1
    for a in ('maxPoints', 'maxContours', 'maxCompositePoints', 'maxCompositeContours', 'maxComponentDepth'):
        setattr(maxp, a, 0)
    font['head'].indexToLocFormat = 0; font['head'].glyphDataFormat = 0
    font['post'].formatType = 3.0
    font.sfntVersion = '\x00\x01\x00\x00'

outdir = os.path.join(here, '..', 'Resources', 'fonts')
for weight, chars in (('Medium', ui | gb), ('Bold', ui)):
    path = '/usr/share/fonts/opentype/noto/NotoSansCJK-%s.ttc' % weight
    f = TTFont(path, fontNumber=pick(path))
    o = subset.Options(); o.hinting = False; o.layout_features = ['kern', 'liga']; o.notdef_outline = True
    s = subset.Subsetter(o); s.populate(text=''.join(chars)); s.subset(f)
    otf2ttf(f)
    out = os.path.join(outdir, 'NotoSansSC-%s.ttf' % weight)
    f.save(out)
    f2 = TTFont(out); f2['maxp'].recalc(f2); f2.save(out)   # recalcula maxp
    print(out, os.path.getsize(out))
