#!/usr/bin/env python3
"""
Genera una fuente .vlw (formato LovyanGFX/M5GFX) a partir de un TTF, incluyendo
ASCII + acentos del espanol. Uso:
    python make_vlw.py [fuente.ttf] [tamano_px] [salida.vlw]
La .vlw resultante se copia a la microSD (p.ej. /fonts/title.vlw) y se carga con
canvas.loadFont(buffer) en el firmware.

Formato VLW (big-endian):
  Cabecera 24 B: gCount, version(11), size, 0, ascent, descent
  Por glifo 28 B: unicode, height, width, xAdvance, dY(top bearing), dX(left bearing), 0
  Glifos ORDENADOS por unicode. Despues, los bitmaps (8-bit alpha) concatenados.
"""
import freetype, struct, sys, os

FONT = sys.argv[1] if len(sys.argv) > 1 else r"C:\Windows\Fonts\verdanab.ttf"
SIZE = int(sys.argv[2]) if len(sys.argv) > 2 else 44
OUT  = sys.argv[3] if len(sys.argv) > 3 else os.path.join(os.path.dirname(__file__), "..", "fonts", "title.vlw")

# ASCII imprimible + acentos/signos del espanol (y algunos extra utiles)
cps = list(range(0x20, 0x7F)) + [
    0xA1, 0xBF, 0xB0, 0xAB, 0xBB, 0xBA, 0xAA,            # ¡ ¿ ° « » º ª
    0xC0, 0xC1, 0xC7, 0xC9, 0xCD, 0xD1, 0xD3, 0xDA, 0xDC,  # À Á Ç É Í Ñ Ó Ú Ü
    0xE0, 0xE1, 0xE7, 0xE8, 0xE9, 0xEC, 0xED, 0xF1, 0xF2, 0xF3, 0xF9, 0xFA, 0xFC,  # à á ç è é ì í ñ ò ó ù ú ü
]
cps = sorted(set(cps))

face = freetype.Face(FONT)
face.set_pixel_sizes(0, SIZE)
ascent  = face.size.ascender >> 6
descent = abs(face.size.descender >> 6)

glyphs = []  # (unicode, w, h, xadv, dY, dX, bitmap_bytes)
for cp in cps:
    face.load_char(chr(cp), freetype.FT_LOAD_RENDER | freetype.FT_LOAD_TARGET_NORMAL)
    g = face.glyph
    bm = g.bitmap
    w, h, pitch, buf = bm.width, bm.rows, bm.pitch, bm.buffer
    data = bytearray()
    for row in range(h):
        s = row * pitch
        data += bytes(buf[s:s + w])
    glyphs.append((cp, w, h, g.advance.x >> 6, g.bitmap_top, g.bitmap_left, bytes(data)))

os.makedirs(os.path.dirname(os.path.abspath(OUT)), exist_ok=True)
with open(OUT, "wb") as f:
    f.write(struct.pack(">IIIIII", len(glyphs), 11, SIZE, 0, ascent, descent))
    for (cp, w, h, xadv, dY, dX, _) in glyphs:
        f.write(struct.pack(">I", cp))
        f.write(struct.pack(">I", h))
        f.write(struct.pack(">I", w))
        f.write(struct.pack(">I", xadv))
        f.write(struct.pack(">i", dY))     # con signo
        f.write(struct.pack(">i", dX))     # con signo
        f.write(struct.pack(">I", 0))
    for (_, _, _, _, _, _, data) in glyphs:
        f.write(data)

print("OK:", os.path.abspath(OUT), "|", len(glyphs), "glifos |",
      os.path.getsize(OUT), "bytes | ascent", ascent, "descent", descent)
