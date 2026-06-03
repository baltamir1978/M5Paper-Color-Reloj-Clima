# M5Paper Color — Estación multimodo

Firmware Arduino para el **M5Paper Color** (ESP32-S3, e-paper Spectra 6 a color 400×600)
con sensor SHT40, RTC RX8130CE y gestión de energía M5PM1.

## Modos (se cambian con doble-click en G1)

1. **Clima** — temperatura/humedad local + predicción AEMET de varias ubicaciones
   (hasta 7 días: cielo, máx/mín, humedad y prob. de lluvia) + observación de estación.
   Vista LOCAL con fecha, hora grande y batería.
2. **Carrusel** — fotos de la microSD a pantalla completa, con **auto-rotación del panel**
   según la orientación de cada foto. Avance automático configurable.
3. **Música** *(en construcción)* — reproductor desde la SD (MP3/M4A/FLAC vía ES8311).
4. **Libro** *(en construcción)* — lector TXT/EPUB.

## Controles (botones de usuario)

| Botón | Acción |
|-------|--------|
| **G1 (GPIO1)** doble-click | Cambiar de modo |
| **G1** mantener | Acción del modo (en Clima: actualizar por WiFi) |
| **UP (GPIO9) / DOWN (GPIO10)** | Navegar dentro del modo |

## Configuración: `/config.json` en la microSD

Toda la configuración va en un `config.json` en la **raíz de la microSD** (FAT32).
Hay un ejemplo en [`M5PaperColor_Reloj/config.json`](M5PaperColor_Reloj/config.json).
Incluye: varias redes WiFi, clave de AEMET OpenData, zona horaria, ubicaciones
(municipio INE + estación de observación opcional), carpetas y opciones del carrusel.

> ⚠️ Las credenciales reales **solo** van en la SD, nunca en el repositorio.

### Estructura de la microSD
```
/config.json
/Fotos/    (jpg, png, bmp)
/Musica/   (mp3/m4a/flac — para el modo música)
```

## Hardware (verificado contra el esquemático oficial)

- ESP32-S3R8 · 16 MB flash · 8 MB PSRAM
- e-paper Spectra 6 color, 400×600
- SHT40 (I2C 0x44) · RTC RX8130CE (0x32) · PMIC M5PM1 (0x6E) — bus interno SDA=G3, SCL=G2
- SPI compartido e-paper+microSD: MOSI=13, MISO=14, SCK=15 · e-paper CS=44/DC=43 · SD CS=47
- La microSD y el e-paper se alimentan del rail L3B del PMIC (hay que habilitar el LDO).

## Compilación (Arduino IDE)

- Placa: **ESP32S3 Dev Module** · Flash **16 MB** · PSRAM **OPI PSRAM** · USB CDC On Boot: **Enabled**
- Librerías: **M5Unified**, **M5GFX**, **M5UnitENV**, **M5PM1**, **ArduinoJson** (v7)
- Para el modo música (futuro): **ESP32-audioI2S** (schreibfaul1)

## Zona horaria

España peninsular: `CET-1CEST,M3.5.0,M10.5.0/3` (Canarias: `WET0WEST,M3.5.0/1,M10.5.0`).
