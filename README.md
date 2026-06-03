<h1 align="center">M5Paper Color — Estación multimodo</h1>

<p align="center">
  <img alt="Plataforma" src="https://img.shields.io/badge/MCU-ESP32--S3-informational">
  <img alt="Framework" src="https://img.shields.io/badge/Framework-Arduino-blue">
  <img alt="Pantalla" src="https://img.shields.io/badge/Display-e--paper%20Spectra%206-9cf">
  <img alt="Estado" src="https://img.shields.io/badge/Estado-en%20desarrollo-yellow">
</p>

<p align="center">
  Firmware para el <b>M5Paper Color</b> (ESP32-S3, tinta electrónica a color) que reúne
  varias utilidades en un solo dispositivo de bajo consumo: estación meteorológica con
  predicción de <b>AEMET</b>, marco de fotos, y (próximamente) reproductor de música y lector de libros.
  Toda la configuración se edita cómodamente en un archivo de la microSD.
</p>

---

## ✨ Características

- 🌦️ **Clima** — temperatura y humedad interior (sensor SHT40) + **predicción AEMET** de varias
  localidades (hasta 7 días: estado del cielo, máx/mín, humedad y probabilidad de lluvia) +
  **observación en tiempo real** de una estación AEMET (p. ej. El Goloso para Madrid).
  Incluye una vista **LOCAL** con fecha, reloj grande y nivel de batería.
- 🖼️ **Carrusel de fotos** — imágenes de la microSD a pantalla completa, con **auto-rotación
  del panel** según la orientación de cada foto (vertical/horizontal) para aprovechar todo el espacio.
- 🎵 **Música** *(en construcción)* — reproductor desde la SD (MP3 / M4A / FLAC) usando el códec ES8311.
- 📖 **Libro** *(en construcción)* — lector de TXT / EPUB.
- 🕒 **Hora siempre correcta** — sincronización por **NTP** y mantenimiento en el **RTC** (RX8130CE),
  con horario de verano automático.
- 🔋 **Pensado para batería** — el WiFi solo se enciende cuando hace falta; la tinta electrónica
  no consume al mostrar contenido estático.
- 🗂️ **Config sin recompilar** — redes WiFi, clave AEMET, ubicaciones, carpetas y más, todo en
  un `config.json` en la microSD.

---

## 🎛️ Controles

Tres botones de usuario (el cuarto es el de encendido):

| Botón | Pulsación | Acción |
|-------|-----------|--------|
| **G1** (GPIO1, superior) | Doble click | Cambiar de modo (Clima → Carrusel → Música → Libro) |
| **G1** | Mantener (~0,8 s) | Acción del modo · *Clima:* actualizar por WiFi (NTP + AEMET) |
| **UP** (GPIO9) | Click | Navegar · *Clima:* siguiente vista · *Carrusel:* foto siguiente |
| **DOWN** (GPIO10) | Click | Navegar · *Clima:* vista anterior · *Carrusel:* foto anterior |

El modo activo se guarda en memoria no volátil (NVS) y se restaura al reiniciar.

---

## 💾 La microSD

Formatea la tarjeta en **FAT32** y crea esta estructura en la raíz:

```
/config.json     ← configuración (ver más abajo)
/Fotos/          ← imágenes del carrusel  (.jpg .jpeg .png .bmp)
/Musica/         ← canciones (.mp3 .m4a .flac …) para el modo música
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
| `timezone` | Cadena POSIX TZ. España peninsular: `CET-1CEST,M3.5.0,M10.5.0/3` · Canarias: `WET0WEST,M3.5.0/1,M10.5.0`. |
| `carousel_seconds` | Segundos entre fotos del carrusel. |
| `photo_auto_rotate` | `true` para girar el panel según la orientación de cada foto. |
| `fotos_dir` / `musica_dir` | Carpetas en la SD. |
| `locations` | Localidades del modo clima. `municipio` = código INE; `estacion` = idema de observación AEMET (opcional, `""` para solo predicción). |

> 🔒 Las credenciales reales viven **solo en la microSD**, nunca en este repositorio.

Si falta la SD o el archivo, el firmware arranca con valores por defecto (sin credenciales válidas).

---

## 🧩 Hardware

Verificado contra el esquemático oficial `C151 PaperColor V0.5`.

| Componente | Detalle |
|------------|---------|
| SoC | ESP32-S3R8 · 16 MB flash · 8 MB PSRAM |
| Pantalla | e-paper **Spectra 6** a color, 400 × 600 |
| Sensor T/H | **SHT40** (I²C `0x44`) |
| RTC | **RX8130CE** (I²C `0x32`) |
| PMIC | **M5PM1** (I²C `0x6E`) |
| Audio | códec **ES8311** + ADC ES7210 + ampli AW8737A |
| Otros | microSD, IR, 2× RGB LED, batería 1250 mAh |

### Mapa de pines

| Función | GPIO |
|---------|------|
| I²C interno (SHT40 / RTC / PMIC) | SDA = **G3**, SCL = **G2** |
| SPI (e-paper + microSD) | MOSI = **13**, MISO = **14**, SCK = **15** |
| e-paper | CS = **44**, DC = **43**, BUSY = 11, RST = 12 |
| microSD | CS = **47** |
| Botones | G1 = **GPIO1**, UP = **GPIO9**, DOWN = **GPIO10** |

> ⚠️ La microSD y el e-paper se alimentan del rail **L3B** del PMIC; el firmware habilita el LDO
> del M5PM1 **antes** de inicializar la SD.

---

## 🛠️ Compilación (Arduino IDE)

**Placa:** `ESP32S3 Dev Module` (o `M5PaperColor` si aparece)
**Opciones:** Flash Size **16 MB** · PSRAM **OPI PSRAM** · USB CDC On Boot **Enabled**

**Librerías:**

- [M5Unified](https://github.com/m5stack/M5Unified) (≥ 0.2.15)
- [M5GFX](https://github.com/m5stack/M5GFX) (≥ 0.2.21)
- [M5UnitENV](https://github.com/m5stack/M5Unit-ENV) (≥ 1.4.0) — sensor SHT40
- [M5PM1](https://github.com/m5stack/M5PM1) — gestión de energía
- [ArduinoJson](https://arduinojson.org/) (v7)
- *(modo música, futuro)* [ESP32-audioI2S](https://github.com/schreibfaul1/ESP32-audioI2S)

Abre `M5PaperColor_Reloj/M5PaperColor_Reloj.ino`, selecciona la placa y sube.

---

## 🌦️ AEMET

El modo clima usa la API gratuita de **AEMET OpenData**:

1. Solicita tu clave en el [alta de usuario](https://opendata.aemet.es/centrodedescargas/altaUsuario) (llega por correo).
2. Pégala en `aemet_api_key` del `config.json`.
3. Añade tus localidades por **código de municipio INE**. La `estacion` (idema) es opcional y
   muestra la observación actual de esa estación.

La predicción es por municipio; la observación es por estación. Para Madrid, la observación se toma
de la estación **El Goloso** (`3126Y`).

---

## 🗺️ Estado y hoja de ruta

- [x] Framework de modos (cambio con G1, persistencia en NVS)
- [x] **Modo 1 — Clima** (sensor + AEMET predicción + observación, multi-localidad)
- [x] **Modo 2 — Carrusel** (auto-rotación, avance automático)
- [x] Configuración por `config.json` en la SD
- [ ] **Modo 3 — Música** (ES8311 + ESP32-audioI2S)
- [ ] **Modo 4 — Libro** (TXT / EPUB)
- [ ] Modo de bajo consumo (deep sleep)

---

## 📂 Estructura del repositorio

```
M5PaperColor_Reloj/
  ├── M5PaperColor_Reloj.ino   ← firmware
  └── config.json              ← ejemplo de configuración (cópialo a la SD)
README.md
.gitignore
```

---

## 📜 Licencia y créditos

Proyecto personal de uso libre con fines educativos. El esquemático y las librerías son propiedad
de **M5Stack**. Datos meteorológicos cortesía de **AEMET**.

Desarrollado para el M5Paper Color con ayuda de Claude (Anthropic).
