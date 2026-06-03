#!/usr/bin/env python3
"""
Descarga el inventario COMPLETO de estaciones de observacion de AEMET y lo guarda
como CSV (idema, nombre, provincia, altitud). Asi cualquiera puede buscar el codigo
"idema" de su estacion para ponerlo en config.json ("estacion": "...").

Uso:
    python aemet_estaciones.py TU_API_KEY [salida.csv]

Solo usa la libreria estandar (urllib). No necesita dependencias.
"""
import sys, json, ssl, csv, urllib.request

if len(sys.argv) < 2:
    print("Uso: python aemet_estaciones.py TU_API_KEY [salida.csv]")
    sys.exit(1)

API = sys.argv[1]
OUT = sys.argv[2] if len(sys.argv) > 2 else "estaciones_aemet.csv"

ctx = ssl.create_default_context()
ctx.check_hostname = False
ctx.verify_mode = ssl.CERT_NONE

def get(url):
    req = urllib.request.Request(url, headers={"cache-control": "no-cache"})
    with urllib.request.urlopen(req, context=ctx, timeout=30) as r:
        return json.loads(r.read().decode("latin-1"))

BASE = ("https://opendata.aemet.es/opendata/api/valores/climatologicos/"
        "inventarioestaciones/todasestaciones")

meta = get(f"{BASE}?api_key={API}")
if "datos" not in meta:
    print("Respuesta inesperada de AEMET:", meta)
    sys.exit(1)
data = get(meta["datos"])

rows = [(e.get("indicativo", ""), e.get("nombre", ""),
         e.get("provincia", ""), e.get("altitud", "")) for e in data]
rows.sort(key=lambda r: (r[2], r[1]))

with open(OUT, "w", newline="", encoding="utf-8") as f:
    w = csv.writer(f)
    w.writerow(["idema", "nombre", "provincia", "altitud"])
    w.writerows(rows)

print(f"{len(rows)} estaciones -> {OUT}")
