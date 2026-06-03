<h1 align="center">M5Paper Color — Estación multimodo</h1>

<p align="center">
  <img alt="Plataforma" src="https://img.shields.io/badge/MCU-ESP32--S3-informational">
  <img alt="Framework" src="https://img.shields.io/badge/Framework-Arduino-blue">
  <img alt="Pantalla" src="https://img.shields.io/badge/Display-e--paper%20Spectra%206-9cf">
  <img alt="Estado" src="https://img.shields.io/badge/Estado-en%20desarrollo-yellow">
</p>

<p align="center">
  Firmware para el <b>M5Paper Color</b> (ESP32-S3, tinta electrónica a color) que reúne varias
  utilidades en un dispositivo de bajo consumo: estación meteorológica con predicción de
  <b>AEMET</b>, marco de fotos, reproductor de música y (próximamente) lector de libros.
  Incluye un <b>modo oculto TV-B-Gone</b> por infrarrojos. Todo se configura desde la microSD.
</p>

---

## ✨ Características

- 🌦️ **Clima** — temperatura/humedad interior (SHT40) + **predicción AEMET** de varias localidades
  (hoy destacado con icono a color + 3 días) + **observación en tiempo real** de una estación AEMET.
  Vista **LOCAL** con fecha, reloj grande y batería.
- 🖼️ **Carrusel de fotos** — imágenes de la microSD a pantalla completa, con **auto-rotación del
  panel** según la orientación de cada foto.
- 🎵 **Música** — reproductor desde la SD (**MP3 / M4A / FLAC / WAV / AAC**) con el códec ES8311.
  Títulos con acentos correctos (fuente propia).
- 📖 **Libro** — lector de **TXT** desde `/Libros` con selector de archivos y memoria de página por libro (EPUB próximamente).
- 📺 **TV-B-Gone (modo oculto)** — apaga televisores por IR (108 códigos europeos).
- 🕒 Hora por **NTP** mantenida en el **RTC**, con horario de verano automático.

---

## 🎛️ Controles

Tres botones de usuario (el cuarto es el de encendido):

| Botón | Gesto | Acción |
|-------|-------|--------|
| **G1** (GPIO1) | 1 click | **Cambiar de modo** (Clima → Carrusel → Música → Libro) |
| **G1** | Doble click | 📺 **TV-B-Gone** (modo oculto: apaga la TV) |
| **G1** | Mantener (~0,8 s) | Acción del modo · *Clima:* actualizar por WiFi (NTP + AEMET) |
| **UP** (GPIO9) / **DOWN** (GPIO10) | — | Navegación del modo (ver abajo) |

**Navegación UP/DOWN por modo:**

| Modo | UP / DOWN | Doble click UP/DOWN | Mantener UP/DOWN |
|------|-----------|---------------------|------------------|
| Clima | Vista anterior/siguiente | — | — |
| Carrusel | Foto anterior/siguiente | — | — |
| Música | **Volumen** −/+ | **Canción** ant/sig | **Play / Pausa** |
| Libro *(lista)* | Mover selección | **Abrir** libro | — |
| Libro *(leyendo)* | Página ant/sig | — | **Volver a la lista** (mantener G1) |

El modo activo se guarda en NVS y se restaura al reiniciar. En **Libro**, cada archivo recuerda su última página.

---

## 💾 La microSD

Formatea en **FAT32** y crea esta estructura en la raíz:

```
/config.json        ← configuración
/Fotos/             ← imágenes del carrusel  (.jpg .jpeg .png .bmp)
/Musica/            ← canciones  (.mp3 .m4a .flac .wav .aac)
/Libros/            ← libros (.txt en UTF-8; .epub próximamente)
/fonts/title.vlw    ← fuente con acentos para los títulos
/fonts/body.vlw     ← fuente con acentos para el lector de libros
```

### `config.json`

```jsonc
{
  "wifi": [
    { "ssid": "TU_RED_WIFI", "pass": "TU_CONTRASENA" },
    { "ssid": "OTRA_RED",    "pass": "OTRA_CONTRASENA" }
  ],

  "aemet_api_key": "PEGA_AQUI_TU_TOKEN_AEMET",
  "timezone": "CET-1CEST,M3.5.0,M10.5.0/3",

  "carousel_seconds": 300,
  "photo_auto_rotate": true,

  "fotos_dir": "/Fotos",
  "musica_dir": "/Musica",
  "libros_dir": "/Libros",
  "font_title": "/fonts/title.vlw",

  "locations": [
    { "name": "Madrid",           "municipio": "28079", "estacion": "3126Y" },
    { "name": "San Ildefonso",    "municipio": "40181", "estacion": "" },
    { "name": "Posada de Llanes", "municipio": "33036", "estacion": "" }
  ]
}
```

| Campo | Descripción |
|-------|-------------|
| `wifi` | Lista de redes; se prueban en orden hasta conectar. |
| `aemet_api_key` | Token gratuito de [AEMET OpenData](https://opendata.aemet.es/centrodedescargas/altaUsuario). |
| `timezone` | POSIX TZ. Peninsular: `CET-1CEST,M3.5.0,M10.5.0/3` · Canarias: `WET0WEST,M3.5.0/1,M10.5.0`. |
| `carousel_seconds` | Segundos entre fotos. |
| `photo_auto_rotate` | `true` = gira el panel según la orientación de cada foto. |
| `fotos_dir`/`musica_dir`/`libros_dir` | Carpetas en la SD. |
| `font_title` | Fuente VLW (con acentos) para los títulos. |
| `font_body` | Fuente VLW (con acentos) para el lector de libros. |
| `locations` | Localidades del clima: `municipio` = código INE (predicción); `estacion` = idema de observación AEMET (opcional, `""` = solo predicción). |

> 🔒 Las credenciales reales viven **solo en la microSD**, nunca en el repositorio.

---

## 🌦️ Clima y AEMET

Usa la API gratuita de **AEMET OpenData**:
1. Solicita tu clave en el [alta de usuario](https://opendata.aemet.es/centrodedescargas/altaUsuario) (llega por email) y ponla en `aemet_api_key`.
2. Añade tus localidades por **código de municipio INE** (la predicción es por municipio).
3. Opcionalmente, una **estación de observación** (`estacion`) para ver el dato actual real.

Mantén **G1 pulsado** en el modo Clima para actualizar por WiFi (resincroniza la hora + descarga AEMET).

### 📡 Estaciones AEMET (cómo encontrar tu `estacion` / idema)

El `idema` es el código de la estación de observación. Para encontrar el tuyo:

- **Método web (el más rápido):** entra en
  [aemet.es → El tiempo → Observación → Últimos datos](https://www.aemet.es/es/eltiempo/observacion/ultimosdatos),
  elige tu estación y mira la **URL**: contiene `?l=XXXXX`. Eso es el idema.
  Ej.: *Madrid, El Goloso* → `...ultimosdatos?l=3126Y` → idema **`3126Y`**.
- **Lista completa (todas las estaciones):** ejecuta el script incluido con tu clave:
  ```
  python tools/aemet_estaciones.py TU_API_KEY estaciones.csv
  ```
  Genera un CSV con `idema, nombre, provincia, altitud` de **todas** las estaciones.

**Ejemplos habituales** (verifica el tuyo con el método de arriba — los sufijos pueden variar):

| Lugar | idema | Lugar | idema |
|-------|-------|-------|-------|
| Madrid, El Goloso | `3126Y` | Bilbao Aeropuerto | `1082` |
| Madrid, Retiro | `3195` | Zaragoza Aeropuerto | `9434` |
| Madrid-Barajas Aerop. | `3129` | Málaga Aeropuerto | `6155A` |
| Barcelona Aeropuerto | `0076` | Murcia | `7178I` |
| Valencia (Viveros) | `8416` | Alicante (Ciudad Jardín) | `8025` |
| Sevilla Aeropuerto | `5783` | Santander Aeropuerto | `1109` |
| A Coruña | `1387E` | Palma Aeropuerto | `B954` |
| Granada Aeropuerto | `5530E` | Gran Canaria Aeropuerto | `C649I` |

> ⚠️ AEMET puede cambiar/retirar estaciones. Si una no devuelve datos, confirma el idema con el método web o el script.

---

## 🎵 Música y acentos (fuente VLW)

Las fuentes integradas de M5GFX son solo ASCII (los acentos salen mal) y M5GFX no rasteriza TTF.
La solución es una fuente **VLW** (bitmap, generada de un TTF) que se carga desde la SD:

```
python tools/make_vlw.py "C:\Windows\Fonts\verdanab.ttf" 44 fonts/title.vlw
```

Copia el `fonts/title.vlw` resultante a la microSD (`/fonts/title.vlw`). Si no existe, se usa una
fuente integrada (sin acentos). El `.vlw` no se versiona (puede derivar de una fuente propietaria).

---

## 📺 Modo oculto TV-B-Gone

**Doble click en G1** emite una tanda de **108 códigos de apagado de TVs europeas** (NEC, Samsung,
Sony, RC5/RC6, Panasonic, JVC…) por el **emisor IR (GPIO48)**. No usa la pantalla: el **LED RGB**
parpadea **verde** al empezar y **rojo** al terminar (3 veces si se cancela con G1). Apunta a la TV.

Los códigos están en `M5PaperColor_Reloj/tvbgone_codes.h` (portados de
[m5stick-weather-tvbgone](https://github.com/baltamir1978/m5stick-weather-tvbgone)).

---

## 🧩 Hardware (verificado contra el esquemático C151 V0.5)

| Componente | Detalle |
|------------|---------|
| SoC | ESP32-S3R8 · 16 MB flash · 8 MB PSRAM |
| Pantalla | e-paper **Spectra 6** color, 400 × 600 |
| Sensor T/H | **SHT40** (I²C 0x44) |
| RTC | **RX8130CE** (0x32) |
| PMIC | **M5PM1** (0x6E) |
| Audio | códec **ES8311** (0x18) + ADC ES7210 + ampli AW8737A |
| Otros | microSD, **IR (GPIO48)**, 2× LED RGB (GPIO21), batería 1250 mAh |

### Mapa de pines

| Función | GPIO |
|---------|------|
| I²C interno (SHT40/RTC/PMIC/ES8311) | SDA = **3**, SCL = **2** |
| SPI (e-paper + microSD) | MOSI=**13**, MISO=**14**, SCK=**15** |
| e-paper | CS=**44**, DC=**43** · microSD CS=**47** |
| Botones | G1=**1**, UP=**9**, DOWN=**10** |
| Audio I²S (ES8311) | BCLK=**40**, LRCK=**41**, DATA=**38** · codec_en=**45**, spk_en=**46** |
| IR / LED RGB | IR=**48** · LED=**21** |

> La microSD, el e-paper y el códec se alimentan del rail **L3B** del PMIC; el firmware habilita el LDO al arrancar.

---

## 🛠️ Compilación (Arduino IDE)

**Placa:** `ESP32S3 Dev Module` · Flash **16 MB** · PSRAM **OPI PSRAM** · USB CDC On Boot **Enabled**

Abre `M5PaperColor_Reloj/M5PaperColor_Reloj.ino`, selecciona la placa y sube.

### Versiones probadas (junio 2026)

> ⚠️ La placa es **muy nueva** y las librerías se actualizan a menudo estos primeros meses.
> Estas son las versiones con las que **compila y funciona**. Si una actualización rompe algo,
> vuelve a estas (o usa PlatformIO, que las fija automáticamente — ver abajo).

| Componente | Versión |
|------------|---------|
| Core **M5Stack/ESP32** (Arduino) | **3.3.7** |
| M5Unified | 0.2.17 |
| M5GFX | 0.2.22 |
| M5Unit-ENV | 1.4.0 |
| M5PM1 | 1.0.7 |
| ArduinoJson | 7.4.3 |
| ESP32-audioI2S (schreibfaul1) | 3.4.6 |
| IRremoteESP8266 | 2.9.0 |

Enlaces: [M5Unified](https://github.com/m5stack/M5Unified) · [M5GFX](https://github.com/m5stack/M5GFX) ·
[M5Unit-ENV](https://github.com/m5stack/M5Unit-ENV) · [M5PM1](https://github.com/m5stack/M5PM1) ·
[ArduinoJson](https://arduinojson.org/) · [ESP32-audioI2S](https://github.com/schreibfaul1/ESP32-audioI2S) ·
[IRremoteESP8266](https://github.com/crankyoldgit/IRremoteESP8266)

### Compilar con PlatformIO (versiones fijadas)

Para una compilación **reproducible** (sin pelear con el gestor de librerías) hay un
[`platformio.ini`](platformio.ini) con la plataforma y todas las librerías **fijadas a las versiones
de arriba**. Usa el fork **pioarduino** (la plataforma oficial de PlatformIO aún no soporta
arduino-esp32 3.x):

```bash
pio run             # compilar
pio run -t upload   # subir
pio device monitor  # monitor serie
```

---

## 🧰 Herramientas (`tools/`)

| Script | Para qué |
|--------|----------|
| `make_vlw.py` | Genera la fuente `.vlw` con acentos desde un TTF (necesita `pip install freetype-py`). |
| `aemet_estaciones.py` | Descarga el listado completo de estaciones AEMET a CSV (solo librería estándar). |

---

## 🗺️ Estado y hoja de ruta

- [x] Framework de modos (cambio con G1, persistencia en NVS)
- [x] Clima (sensor + AEMET predicción + observación, multi-localidad, iconos)
- [x] Carrusel (auto-rotación)
- [x] Música (ES8311, MP3/M4A/FLAC, controles iPod Shuffle, acentos VLW)
- [x] Modo oculto TV-B-Gone (IR + feedback LED)
- [x] Modo libro **TXT** (selector + paginado + memoria por libro)
- [x] Refresco LOCAL con cadencia inteligente (1 min; 5 min en reposo)
- [ ] Modo libro **EPUB** (descompresión ZIP + extracción de texto)
- [ ] Modo de bajo consumo (deep sleep)

> ℹ️ **Sobre los refrescos:** el e-paper a color (Spectra 6) solo hace **refrescos completos**
> (no admite refresco parcial de una región como los e-paper monocromos). Por eso la estrategia es
> **minimizar la frecuencia**: la vista LOCAL del clima se refresca **una vez por minuto**, y pasa a
> **una vez cada 5 minutos** si no se toca ningún botón. Las vistas de AEMET solo refrescan al
> navegar o al actualizar (mantener G1).

---

## 📜 Licencia y créditos

Proyecto personal de uso libre con fines educativos. Esquemático y librerías son de **M5Stack**.
Datos meteorológicos cortesía de **AEMET**. Códigos IR portados de *m5stick-weather-tvbgone*.

Desarrollado para el M5Paper Color con ayuda de Claude (Anthropic).
