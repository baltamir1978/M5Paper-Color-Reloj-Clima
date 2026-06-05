#!/usr/bin/env python3
"""
Genera miniaturas para el gestor web del M5Paper Color.

Crea, en cada carpeta que tenga imagenes o videos, una subcarpeta ".thumbnails/"
con una miniatura JPEG (~320 px) por archivo. El firmware sirve esa miniatura en la
vista de "Miniaturas" (rapidisimo), y si no existe cae a la imagen original.

Convencion:  foto.jpg  ->  .thumbnails/foto.jpg     (mismo nombre, sin la extension original + .jpg)
             clip.mp4  ->  .thumbnails/clip.jpg     (un fotograma del video)

Uso:
    python make_thumbs.py [ruta_de_la_microSD_o_carpeta] [tamano_px]

Lo practico es copiar este .py a la RAIZ de la microSD: asi viaja con la tarjeta y,
ejecutandolo SIN argumentos, escanea toda la SD (la carpeta donde esta el propio script).

Ejemplos:
    python make_thumbs.py            (en la raiz de la SD: escanea TODA la SD, 320 px)
    python make_thumbs.py E:\\        (toda la SD por letra de unidad)
    python make_thumbs.py E:\\Fotos 256   (solo /Fotos, a 256 px)

Requisitos:
    - Pillow:  pip install pillow
    - ffmpeg en el PATH (opcional, solo para miniaturas de VIDEO)

Es incremental: si la miniatura ya existe y es mas nueva que el original, no la rehace.
"""
import os
import sys
import shutil
import subprocess

try:
    from PIL import Image, ImageOps
    HAVE_PIL = True
except ImportError:
    HAVE_PIL = False

IMG_EXT = {".jpg", ".jpeg", ".png", ".gif", ".bmp", ".webp"}
VID_EXT = {".mp4", ".mov", ".m4v", ".webm", ".mkv"}
SIZE = int(sys.argv[2]) if len(sys.argv) > 2 else 320


def find_ffmpeg():
    """Busca ffmpeg junto al script (util si lo llevas en la SD), en ./bin, y en el PATH."""
    here = os.path.dirname(os.path.abspath(__file__))
    for d in (here, os.path.join(here, "bin")):
        for n in ("ffmpeg.exe", "ffmpeg"):
            p = os.path.join(d, n)
            if os.path.isfile(p):
                return p
    return shutil.which("ffmpeg")


FFMPEG = find_ffmpeg()


def thumb_path(folder, name):
    base = os.path.splitext(name)[0]
    return os.path.join(folder, ".thumbnails", base + ".jpg")


def make_image_thumb(src, dst):
    if HAVE_PIL:                               # preferente: Pillow corrige la orientacion EXIF
        im = Image.open(src)
        im = ImageOps.exif_transpose(im)
        im = im.convert("RGB")
        im.thumbnail((SIZE, SIZE))
        im.save(dst, "JPEG", quality=72)
    elif FFMPEG:                               # respaldo con ffmpeg (ojo: no corrige EXIF -> alguna vertical puede salir girada)
        subprocess.run([FFMPEG, "-y", "-loglevel", "error", "-i", src,
                        "-vf", f"scale='min({SIZE},iw)':-2", dst], check=False)
        if not os.path.exists(dst):
            raise RuntimeError("ffmpeg no genero la miniatura")
    else:
        raise RuntimeError("sin Pillow ni ffmpeg")


def make_video_thumb(src, dst):
    if not FFMPEG:
        return False
    subprocess.run(
        [FFMPEG, "-y", "-loglevel", "error", "-ss", "1", "-i", src,
         "-vframes", "1", "-vf", f"scale='min({SIZE},iw)':-2", dst],
        check=False,
    )
    return os.path.exists(dst)


def main():
    if len(sys.argv) > 1 and sys.argv[1] in ("-h", "--help"):
        print(__doc__)
        return
    # Sin ruta: usa la carpeta donde esta este script (util si lo llevas en la raiz de la SD).
    root = sys.argv[1] if len(sys.argv) > 1 else os.path.dirname(os.path.abspath(__file__))
    if not os.path.isdir(root):
        print(f"No es una carpeta: {root}")
        return
    if not HAVE_PIL and not FFMPEG:
        print("Necesitas Pillow (py -m pip install pillow) o ffmpeg (ponlo junto a este script / en la SD).")
        return
    print(f"Generando miniaturas en: {root}  ({SIZE} px)")
    print("  fotos: " + ("Pillow" if HAVE_PIL else ("ffmpeg (ojo orientacion)" if FFMPEG else "NO")) +
          " | video: " + ("ffmpeg" if FFMPEG else "NO"))

    created = skipped = errors = 0
    for folder, dirs, files in os.walk(root):
        if os.path.basename(folder) == ".thumbnails":
            dirs[:] = []
            continue
        dirs[:] = [d for d in dirs if d != ".thumbnails"]
        media = [f for f in files if os.path.splitext(f)[1].lower() in (IMG_EXT | VID_EXT)]
        if not media:
            continue
        tdir = os.path.join(folder, ".thumbnails")
        os.makedirs(tdir, exist_ok=True)
        for f in media:
            src = os.path.join(folder, f)
            dst = thumb_path(folder, f)
            if os.path.exists(dst) and os.path.getmtime(dst) >= os.path.getmtime(src):
                skipped += 1
                continue
            ext = os.path.splitext(f)[1].lower()
            try:
                if ext in IMG_EXT:
                    make_image_thumb(src, dst)
                    created += 1
                elif make_video_thumb(src, dst):
                    created += 1
                print(f"  + {os.path.relpath(dst, root)}")
            except Exception as e:
                errors += 1
                print(f"  ! error con {f}: {e}")

    print(f"\nMiniaturas creadas/actualizadas: {created} | sin cambios: {skipped} | errores: {errors}")
    if not FFMPEG:
        print("Aviso: ffmpeg no encontrado -> sin miniaturas de VIDEO. (Ponlo junto al script o en el PATH.)")


if __name__ == "__main__":
    main()
