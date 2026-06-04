/*
 * ============================================================================
 *  M5Paper Color (ESP32-S3) - Estacion multimodo
 * ============================================================================
 *  4 modos (boton G1, 1 click para rotar):
 *    1 CLIMA   : vistas Up/Down -> LOCAL (fecha/hora + sensor) + ciudades AEMET
 *    2 CARRUSEL: fotos de la microSD a pantalla completa
 *    3 MUSICA  : reproductor MP3/M4A de la SD (controles tipo iPod Shuffle)
 *    4 LIBRO   : lector de TXT (lista de /Libros, recuerda la pagina de cada uno)
 *  Modo oculto: G1 doble-click = TV-B-Gone (IR), feedback con el LED RGB.
 *
 *  TODA la configuracion va en /config.json de la microSD (ver ejemplo en el
 *  repo). Si falta la SD o el archivo, se usan los valores por defecto de abajo.
 *
 *  BOTONES (GPIO, activos a nivel bajo):
 *    G1=GPIO1 (top): 1 click=cambia de modo | doble-click=TV-B-Gone |
 *                    mantener=accion del modo (CLIMA: actualizar AEMET por WiFi)
 *    UP=GPIO9 / DOWN=GPIO10: navegacion del modo (volumen/pista en musica, pagina en libro)
 *
 *  ARRANQUE RAPIDO: se pinta la pantalla local enseguida (hora del RTC + sensor) y
 *    la descarga WiFi/NTP/AEMET se hace en segundo plano (no bloquea ni muestra "Sin WiFi").
 *  AHORRO: light sleep en reposo; tras 'deep_sleep_minutes' de inactividad se APAGA por
 *    PMIC (M5.Power.powerOff) -> se enciende pulsando el boton de ENCENDIDO. El e-paper
 *    conserva la ultima imagen aunque este apagado. (En musica reproduciendo no duerme.)
 *
 *  HARDWARE: SHT40 0x44 / RTC RX8130CE 0x32 / PMIC M5PM1 0x6E (I2C SDA=3 SCL=2)
 *    SPI e-paper+SD: MOSI=13 MISO=14 SCK=15; EINK_DC=43 CS=44; SD_CS=47
 *    SD/e-paper en rail L3B del PMIC -> habilitar LDO antes de SD.begin
 *  Librerias: M5Unified, M5GFX, M5UnitENV, M5PM1, ArduinoJson, ESP32-audioI2S, IRremoteESP8266
 *  Arduino IDE: "ESP32S3 Dev Module", Flash 16MB, PSRAM "OPI PSRAM", USB CDC On.
 * ============================================================================
 */

#include <Arduino.h>
#include <M5GFX.h>
#include <M5Unified.h>
#include <M5UnitENV.h>
#include <M5PM1.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_sntp.h>
#include <sntp.h>
#include <SPI.h>
#include <SD.h>
#include <Preferences.h>
#include <time.h>
#include <vector>
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "Audio.h"        // ESP32-audioI2S (schreibfaul1) v3.4.6
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include "tvbgone_codes.h"   // base de codigos TV-B-Gone (modo oculto)

// Declaraciones adelantadas (el IDE de Arduino inserta los prototipos auto-
// generados tras los #include, antes de los struct; esto evita el error).
struct WifiCred; struct Location; struct ClimaDay; struct ClimaCache;

// ============================ VALORES POR DEFECTO ============================
// Solo se usan si no se puede leer /config.json de la SD.
#define DEF_TZ "CET-1CEST,M3.5.0,M10.5.0/3"   // Espana peninsular (verano: ult.dom marzo -> ult.dom octubre)
#define NTP_SERVER1 "0.pool.ntp.org"
#define NTP_SERVER2 "1.pool.ntp.org"

static constexpr uint32_t WIFI_PER_NET_MS  = 8000;     // espera por cada red
static constexpr uint32_t SNTP_TIMEOUT_MS  = 20000;

// Botones
static constexpr int PIN_BTN_MODE = 1, PIN_BTN_UP = 9, PIN_BTN_DOWN = 10;
// Pines I2C/SPI
static constexpr int SHT_SDA_PIN = 3, SHT_SCL_PIN = 2;
static constexpr int SD_SCK_PIN = 15, SD_MISO_PIN = 14, SD_MOSI_PIN = 13, SD_CS_PIN = 47;
#define CONFIG_PATH "/config.json"
#define DEF_FOTOS_DIR  "/Fotos"
#define DEF_MUSICA_DIR "/Musica"

// ============================ CONFIG (cargada de la SD) ============================
struct WifiCred { String ssid, pass; };
struct Location { String name, municipio, estacion; };   // estacion = idema obs (o "")

std::vector<WifiCred> g_wifi;
std::vector<Location> g_locs;
String   g_apiKey;
String   g_tz = DEF_TZ;
uint32_t g_carouselMs = 300000;   // 5 min
String   g_fotosDir  = DEF_FOTOS_DIR;
String   g_musicaDir = DEF_MUSICA_DIR;
String   g_librosDir = "/Libros";                // carpeta de libros (modo 4)
String   g_titleFontPath = "/fonts/title.vlw";   // fuente VLW con acentos para titulos
bool     g_photoAutoRotate = true;   // girar el panel segun la orientacion de la foto

int numLocs()  { return (int)g_locs.size(); }
int numViews() { return numLocs() + 1; }   // vista 0 = LOCAL

// ============================ ESTADO GLOBAL ============================
enum Mode { MODE_CLIMA = 0, MODE_CARRUSEL, MODE_MUSICA, MODE_LIBRO, MODE_COUNT };
const char* MODE_NAMES[] = {"CLIMA", "CARRUSEL", "MUSICA", "LIBRO"};

M5Canvas   canvas(&M5.Display);
M5PM1      pm1;
SHT4X      sht4;
Preferences prefs;
m5::Button_Class btnMode, btnUp, btnDown;

Mode  g_mode = MODE_CLIMA;
bool  g_needRedraw = true, g_busy = false;
float g_temp = NAN, g_hum = NAN;
bool  sht_ready = false, sd_ready = false;
// Refresco inteligente de la vista LOCAL
uint32_t g_lastInput = 0;          // ultimo toque de boton (para el ahorro a los 5 min)

// Carrusel
std::vector<String> g_images;
size_t g_img_idx = 0;
int g_rot = -1;   // rotacion actual del panel (1=apaisado, 0=vertical)

// Musica (estilo iPod Shuffle)
Audio  audio;                // motor de audio (I2S + decodificador)
std::vector<String> g_music;
size_t g_track = 0;
int    g_volume = 12;        // 0..21
bool   g_playing = false;
bool   g_loaded = false;     // hay una pista cargada
bool   g_audioReady = false; // ES8311 + I2S inicializados
bool   g_audioPowered = false; // codec/ampli encendidos (solo en musica, ahorro)
// Ahorro de bateria
static constexpr uint32_t IDLE_SLEEP_MS = 4000;       // margen tras el ultimo boton antes de dormir
uint32_t g_deepSleepMs = 3600000UL;                   // inactividad -> deep sleep (config deep_sleep_minutes)
uint32_t g_ignoreInputUntil = 0;                      // ignora botones justo tras arrancar/despertar
bool     g_bootFetchPending = true;                   // arranque: baja WiFi/NTP/AEMET tras pintar local
time_t   g_lastActiveSec = 0;                         // momento (RTC, segundos) del ultimo boton real
time_t   g_lastLocalUpdateSec = 0;                    // ultimo refresco del reloj local (RTC)
time_t   g_lastCarouselSec = 0;                       // ultimo avance del carrusel (RTC)
volatile bool g_trackEnded = false;   // lo pone el callback de la libreria al acabar la pista
uint8_t* g_titleFont = nullptr;       // buffer del .vlw en memoria (acentos)
bool     g_titleFontReady = false;

// Libro (lector TXT)
std::vector<String> g_books;
int      g_bookIdx = 0;
uint32_t g_pageStart = 0, g_nextPageStart = 0;
std::vector<uint32_t> g_pageStack;    // historial de offsets para "pagina anterior"
uint8_t* g_bodyFont = nullptr;        // fuente de lectura (mas pequena, con acentos)
bool     g_bodyFontReady = false;
String   g_bodyFontPath = "/fonts/body.vlw";
enum LibState { LIB_LIST, LIB_READING };
LibState g_libState = LIB_LIST;       // al entrar al modo libro: selector de archivos
int      g_sel = 0;                   // libro resaltado en el selector
// Pines de audio (esquematico C151): ES8311 @ I2C interno 0x18, MCLK=BCLK (sin pin MCLK)
static constexpr int I2S_BCLK_PIN = 40, I2S_LRCK_PIN = 41, I2S_DOUT_PIN = 38;
static constexpr int PIN_CODEC_EN = 45, PIN_SPK_EN = 46;
static constexpr uint8_t ES8311_ADDR = 0x18;
// Emisor IR (modo oculto TV-B-Gone)
static constexpr int IR_TX_PIN = 48;
IRsend irsend(IR_TX_PIN);

// Clima
int g_view = 0;   // 0=LOCAL, 1..N = ciudad (loc = g_view-1)
struct ClimaDay {
  char fecha[12]; int wday, tMax, tMin, probLluvia, humMax, humMin; char cielo[40];
  int vientoVel, racha, uvMax;                     // viento km/h, racha km/h, indice UV
  char vientoDir[5];                               // direccion del viento (N, NE, NO...)
};
struct ClimaHour { int hour, temp; char cielo[32]; };   // prevision por hora (icono + temp)
struct ClimaCache {
  ClimaDay day[7]; int nDays; bool valid; int updH, updM;
  bool obsValid; float obsTemp, obsHum; char obsTime[6];
  ClimaHour hour[6]; int nHours;                   // proximas horas (HOY)
};
std::vector<ClimaCache> g_clima;

const char* DIAS_C[] = {"Dom","Lun","Mar","Mie","Jue","Vie","Sab"};
const char* DIAS[]   = {"Domingo","Lunes","Martes","Miercoles","Jueves","Viernes","Sabado"};
const char* MESES[]  = {"Enero","Febrero","Marzo","Abril","Mayo","Junio",
                        "Julio","Agosto","Septiembre","Octubre","Noviembre","Diciembre"};

// ============================ CONFIG: cargar / defaults ============================
void loadDefaults() {
  g_wifi.clear();   g_wifi.push_back({"TU_RED_WIFI", "TU_CONTRASENA"});
  g_apiKey = "";
  g_tz = DEF_TZ;
  g_carouselMs = 300000;
  g_fotosDir  = DEF_FOTOS_DIR;
  g_musicaDir = DEF_MUSICA_DIR;
  g_librosDir = "/Libros";
  g_titleFontPath = "/fonts/title.vlw";
  g_bodyFontPath  = "/fonts/body.vlw";
  g_photoAutoRotate = true;
  g_deepSleepMs = 3600000UL;   // 60 min
  g_locs.clear();
  g_locs.push_back({"Madrid",           "28079", "3126Y"});  // obs El Goloso
  g_locs.push_back({"San Ildefonso",    "40181", ""});
  g_locs.push_back({"Posada de Llanes", "33036", ""});
}

bool loadConfig() {
  if (!sd_ready) return false;
  File f = SD.open(CONFIG_PATH);
  if (!f) { Serial.println("No hay /config.json (uso defaults)."); return false; }
  JsonDocument doc;
  DeserializationError e = deserializeJson(doc, f);
  f.close();
  if (e) { Serial.printf("config.json error: %s (uso defaults)\n", e.c_str()); return false; }

  // WiFi
  g_wifi.clear();
  for (JsonObject w : doc["wifi"].as<JsonArray>()) {
    WifiCred c; c.ssid = (const char*)(w["ssid"] | ""); c.pass = (const char*)(w["pass"] | "");
    if (c.ssid.length()) g_wifi.push_back(c);
  }
  // Clave AEMET + ajustes
  g_apiKey = (const char*)(doc["aemet_api_key"] | "");
  if (doc["timezone"].is<const char*>()) g_tz = (const char*)doc["timezone"];
  if (doc["carousel_seconds"].is<int>()) g_carouselMs = (uint32_t)doc["carousel_seconds"] * 1000UL;
  if (doc["fotos_dir"].is<const char*>())  g_fotosDir  = (const char*)doc["fotos_dir"];
  if (doc["musica_dir"].is<const char*>()) g_musicaDir = (const char*)doc["musica_dir"];
  if (doc["libros_dir"].is<const char*>()) g_librosDir = (const char*)doc["libros_dir"];
  if (doc["font_title"].is<const char*>()) g_titleFontPath = (const char*)doc["font_title"];
  if (doc["font_body"].is<const char*>())  g_bodyFontPath  = (const char*)doc["font_body"];
  if (doc["photo_auto_rotate"].is<bool>()) g_photoAutoRotate = doc["photo_auto_rotate"];
  if (doc["deep_sleep_minutes"].is<int>()) g_deepSleepMs = (uint32_t)doc["deep_sleep_minutes"] * 60000UL;
  // Ubicaciones
  g_locs.clear();
  for (JsonObject l : doc["locations"].as<JsonArray>()) {
    Location loc;
    loc.name = (const char*)(l["name"] | "");
    loc.municipio = (const char*)(l["municipio"] | "");
    loc.estacion = (const char*)(l["estacion"] | "");
    if (loc.name.length()) g_locs.push_back(loc);
  }
  // Respaldos por si vienen vacios
  if (g_wifi.empty() || g_locs.empty()) {
    Serial.println("config.json incompleto; completo con defaults.");
    if (g_wifi.empty()) g_wifi.push_back({"TU_RED_WIFI", "TU_CONTRASENA"});
    if (g_locs.empty()) { g_locs.push_back({"Madrid","28079","3126Y"}); }
  }
  Serial.printf("config.json OK: %u redes, %u ubicaciones, key=%s\n",
                (unsigned)g_wifi.size(), (unsigned)g_locs.size(),
                g_apiKey.length() ? "si" : "no");
  return true;
}

// ============================ SENSOR / SD / RED ============================
bool initSHT40() {
  i2c_port_t p = M5.In_I2C.getPort();
  TwoWire* w = (p == I2C_NUM_1) ? &Wire1 : &Wire;
  return sht4.begin(w, SHT40_I2C_ADDR_44, SHT_SDA_PIN, SHT_SCL_PIN, 400000U);
}
void readSHT40() { if (sht_ready && sht4.update()) { g_temp = sht4.cTemp; g_hum = sht4.humidity; } }

bool isImageFile(const String& n0) {
  String n = n0; n.toLowerCase();
  return n.endsWith(".jpg")||n.endsWith(".jpeg")||n.endsWith(".png")||n.endsWith(".bmp");
}
void loadImageList(const char* dirpath) {
  g_images.clear();
  File dir = SD.open(dirpath);
  if (!dir || !dir.isDirectory()) { if (dir) dir.close(); return; }
  for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
    if (!f.isDirectory() && isImageFile(f.path())) g_images.push_back(f.path());
    f.close();
  }
  dir.close();
}
bool initSD() {
  SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  if (!SD.begin(SD_CS_PIN, SPI, 20000000)) { Serial.println("microSD no detectada."); return false; }
  Serial.println("microSD montada.");
  return true;
}
// Escanea las fotos de la carpeta configurada (con respaldo a la raiz).
void scanImages() {
  if (!sd_ready) return;
  loadImageList(g_fotosDir.c_str());
  if (g_images.empty() && g_fotosDir != "/") loadImageList("/");
  Serial.printf("Fotos en %s: %u\n", g_fotosDir.c_str(), (unsigned)g_images.size());
}

// Carga un .vlw (fuente con acentos) de la SD a un buffer en PSRAM. Devuelve el buffer o null.
uint8_t* loadVlwFile(const String& path) {
  if (!sd_ready) return nullptr;
  File f = SD.open(path.c_str());
  if (!f) { Serial.printf("Sin fuente %s (uso integrada)\n", path.c_str()); return nullptr; }
  size_t sz = f.size();
  if (sz == 0 || sz > 1000000UL) { f.close(); return nullptr; }
  uint8_t* buf = (uint8_t*)ps_malloc(sz);
  if (!buf) buf = (uint8_t*)malloc(sz);
  if (!buf) { f.close(); Serial.println("Sin memoria para la fuente"); return nullptr; }
  size_t rd = f.read(buf, sz);
  f.close();
  Serial.printf("Fuente %s: %s (%u bytes)\n", path.c_str(), (rd == sz) ? "OK" : "ERROR", (unsigned)sz);
  if (rd != sz) { free(buf); return nullptr; }
  return buf;
}
void loadFonts() {
  g_titleFont = loadVlwFile(g_titleFontPath); g_titleFontReady = (g_titleFont != nullptr);
  g_bodyFont  = loadVlwFile(g_bodyFontPath);  g_bodyFontReady  = (g_bodyFont  != nullptr);
}

// Lista los libros (.txt) de /Libros.
void scanBooks() {
  g_books.clear();
  if (!sd_ready) return;
  File dir = SD.open(g_librosDir.c_str());
  if (!dir || !dir.isDirectory()) { if (dir) dir.close(); return; }
  for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
    if (!f.isDirectory()) {
      String n = f.path(); String low = n; low.toLowerCase();
      if (low.endsWith(".txt")) g_books.push_back(n);
    }
    f.close();
  }
  dir.close();
  Serial.printf("Libros (.txt) en %s: %u\n", g_librosDir.c_str(), (unsigned)g_books.size());
}

bool connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return true;
  WiFi.mode(WIFI_STA);
  for (auto& c : g_wifi) {
    Serial.printf("WiFi: probando '%s'...\n", c.ssid.c_str());
    WiFi.begin(c.ssid.c_str(), c.pass.c_str());
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED) {
      if (millis() - t0 >= WIFI_PER_NET_MS) break;
      delay(200);
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("WiFi OK (%s): %s\n", c.ssid.c_str(), WiFi.localIP().toString().c_str());
      return true;
    }
    WiFi.disconnect();
  }
  Serial.println("No se pudo conectar a ninguna red.");
  return false;
}
void wifiOff() { WiFi.disconnect(true); WiFi.mode(WIFI_OFF); }

bool syncRtcFromSntp() {
  configTzTime(g_tz.c_str(), NTP_SERVER1, NTP_SERVER2);
  uint32_t t0 = millis();
  while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED) {
    if (millis() - t0 >= SNTP_TIMEOUT_MS) return false;
    delay(400);
  }
  time_t now = time(nullptr) + 1;
  while (now > time(nullptr)) delay(10);
  struct tm* lt = localtime(&now);
  if (!lt) return false;
  M5.Rtc.setDateTime(lt);
  Serial.printf("RTC sincronizado por NTP -> %02d:%02d:%02d (TZ=%s)\n",
                lt->tm_hour, lt->tm_min, lt->tm_sec, g_tz.c_str());
  return true;
}

int wdayFromFecha(const char* f) {
  struct tm t = {}; int y, m, d;
  if (sscanf(f, "%d-%d-%d", &y, &m, &d) != 3) return 0;
  t.tm_year = y - 1900; t.tm_mon = m - 1; t.tm_mday = d; t.tm_hour = 12;
  time_t tt = mktime(&t);
  struct tm* r = localtime(&tt);
  return r ? r->tm_wday : 0;
}

// --- Helpers de mensaje (usados por fetchAllOnline y los modos) ---
void drawCenteredMsg(const char* msg, int y, uint16_t color = BLACK,
                     const lgfx::IFont* font = &fonts::FreeSansBold18pt7b) {
  canvas.setFont(font);
  canvas.setTextDatum(middle_center);
  canvas.setTextColor(color);
  canvas.drawString(msg, M5.Display.width() / 2, y);
}
void showBusy(const char* msg) {
  g_busy = true;
  canvas.fillSprite(WHITE);
  drawCenteredMsg(msg, M5.Display.height() / 2);
  canvas.pushSprite(0, 0);
}

// ============================ AEMET ============================
bool aemetFetch(int idx) {
  if (g_apiKey.length() == 0) return false;
  String url = String("https://opendata.aemet.es/opendata/api/prediccion/especifica/municipio/diaria/")
             + g_locs[idx].municipio + "?api_key=" + g_apiKey;
  WiFiClientSecure c1; c1.setInsecure();
  HTTPClient h1;
  if (!h1.begin(c1, url)) return false;
  int code = h1.GET();
  if (code != 200) { Serial.printf("AEMET paso1 HTTP %d\n", code); h1.end(); return false; }
  JsonDocument meta; deserializeJson(meta, h1.getStream()); h1.end();
  String datos = meta["datos"] | "";
  if (datos.isEmpty()) return false;

  WiFiClientSecure c2; c2.setInsecure();
  HTTPClient h2;
  if (!h2.begin(c2, datos)) return false;
  if (h2.GET() != 200) { h2.end(); return false; }

  JsonDocument filter;
  JsonObject dia = filter[0]["prediccion"]["dia"][0].to<JsonObject>();
  dia["fecha"] = true;
  dia["temperatura"]["maxima"] = true;
  dia["temperatura"]["minima"] = true;
  dia["humedadRelativa"]["maxima"] = true;
  dia["humedadRelativa"]["minima"] = true;
  dia["estadoCielo"][0]["descripcion"] = true;
  dia["estadoCielo"][0]["periodo"] = true;
  dia["probPrecipitacion"][0]["value"] = true;
  dia["probPrecipitacion"][0]["periodo"] = true;
  dia["viento"][0]["direccion"] = true;
  dia["viento"][0]["velocidad"] = true;
  dia["viento"][0]["periodo"] = true;
  dia["rachaMax"][0]["value"] = true;
  dia["rachaMax"][0]["periodo"] = true;
  dia["uvMax"] = true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, h2.getStream(), DeserializationOption::Filter(filter));
  h2.end();
  if (err) { Serial.printf("AEMET JSON err: %s\n", err.c_str()); return false; }

  JsonArray dias = doc[0]["prediccion"]["dia"].as<JsonArray>();
  ClimaCache& cc = g_clima[idx];
  cc.nDays = 0;
  for (JsonObject day : dias) {
    if (cc.nDays >= 4) break;   // hoy + 3 proximos
    ClimaDay& cd = cc.day[cc.nDays];
    strlcpy(cd.fecha, day["fecha"] | "", sizeof(cd.fecha));
    cd.wday = wdayFromFecha(cd.fecha);
    cd.tMax = day["temperatura"]["maxima"] | -99;
    cd.tMin = day["temperatura"]["minima"] | -99;
    cd.humMax = day["humedadRelativa"]["maxima"] | -1;
    cd.humMin = day["humedadRelativa"]["minima"] | -1;
    cd.uvMax = day["uvMax"] | -1;
    cd.cielo[0] = 0; cd.probLluvia = 0;
    cd.vientoVel = -1; cd.vientoDir[0] = 0; cd.racha = -1;
    for (JsonObject e : day["estadoCielo"].as<JsonArray>()) {
      const char* per = e["periodo"] | ""; const char* des = e["descripcion"] | "";
      if (strlen(des) && (strcmp(per, "00-24") == 0 || cd.cielo[0] == 0))
        strlcpy(cd.cielo, des, sizeof(cd.cielo));
    }
    for (JsonObject p : day["probPrecipitacion"].as<JsonArray>()) {
      const char* per = p["periodo"] | "";
      if (strcmp(per, "00-24") == 0) { cd.probLluvia = p["value"] | 0; break; }
      cd.probLluvia = p["value"] | cd.probLluvia;
    }
    // Viento (direccion + velocidad) y racha maxima: preferimos el periodo de dia completo "00-24"
    for (JsonObject v : day["viento"].as<JsonArray>()) {
      const char* per = v["periodo"] | "";
      if (strcmp(per, "00-24") == 0 || cd.vientoVel < 0) {
        cd.vientoVel = v["velocidad"] | 0;
        strlcpy(cd.vientoDir, v["direccion"] | "", sizeof(cd.vientoDir));
        if (strcmp(per, "00-24") == 0) break;
      }
    }
    for (JsonObject r : day["rachaMax"].as<JsonArray>()) {
      const char* per = r["periodo"] | "";
      int val = atoi(r["value"] | "0");           // rachaMax llega como cadena
      if (strcmp(per, "00-24") == 0) { cd.racha = val; break; }
      if (cd.racha < 0) cd.racha = val;
    }
    cc.nDays++;
  }
  auto dt = M5.Rtc.getDateTime();
  cc.updH = dt.time.hours; cc.updM = dt.time.minutes;
  cc.valid = cc.nDays > 0;
  Serial.printf("AEMET %s: %d dias\n", g_locs[idx].name.c_str(), cc.nDays);
  return cc.valid;
}

bool aemetFetchObs(int idx) {
  if (g_apiKey.length() == 0 || g_locs[idx].estacion.length() == 0) return false;
  String url = String("https://opendata.aemet.es/opendata/api/observacion/convencional/datos/estacion/")
             + g_locs[idx].estacion + "?api_key=" + g_apiKey;
  WiFiClientSecure c1; c1.setInsecure();
  HTTPClient h1;
  if (!h1.begin(c1, url)) return false;
  if (h1.GET() != 200) { h1.end(); return false; }
  JsonDocument meta; deserializeJson(meta, h1.getStream()); h1.end();
  String datos = meta["datos"] | "";
  if (datos.isEmpty()) return false;

  WiFiClientSecure c2; c2.setInsecure();
  HTTPClient h2;
  if (!h2.begin(c2, datos)) return false;
  if (h2.GET() != 200) { h2.end(); return false; }

  JsonDocument filter;
  JsonObject o = filter[0].to<JsonObject>();
  o["fint"] = true; o["ta"] = true; o["hr"] = true;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, h2.getStream(), DeserializationOption::Filter(filter));
  h2.end();
  if (err) { Serial.printf("OBS JSON err: %s\n", err.c_str()); return false; }

  ClimaCache& cc = g_clima[idx];
  cc.obsValid = false;
  for (JsonObject ob : doc.as<JsonArray>()) {     // ordenado ascendente: nos quedamos el ultimo valido
    if (ob["ta"].is<float>()) {
      cc.obsTemp = ob["ta"] | NAN;
      cc.obsHum  = ob["hr"] | NAN;
      const char* fint = ob["fint"] | "";
      if (strlen(fint) >= 16) strlcpy(cc.obsTime, fint + 11, sizeof(cc.obsTime));
      else cc.obsTime[0] = 0;
      cc.obsValid = true;
    }
  }
  Serial.printf("OBS %s: %.1fC %.0f%% @%s\n", g_locs[idx].name.c_str(), cc.obsTemp, cc.obsHum, cc.obsTime);
  return cc.obsValid;
}

// Buffer en PSRAM que actua como Stream de escritura: para volcar el cuerpo HTTP COMPLETO
// (HTTPClient::writeToStream de-trocea el "chunked") y parsear el JSON despues, evitando el
// IncompleteInput que daba al parsear el stream directamente en respuestas grandes.
struct PsBuf : public Stream {
  char* data = nullptr; size_t len = 0, cap = 0; bool ok = true;
  ~PsBuf() { if (data) free(data); }
  size_t write(uint8_t b) override { return write(&b, 1); }
  size_t write(const uint8_t* p, size_t n) override {
    if (!ok) return 0;
    if (len + n + 1 > cap) {
      size_t ncap = cap ? cap : 16384;
      while (len + n + 1 > ncap) ncap += ncap / 2 + 1;
      char* nb = (char*)ps_realloc(data, ncap);
      if (!nb) { ok = false; return 0; }
      data = nb; cap = ncap;
    }
    memcpy(data + len, p, n); len += n; data[len] = 0; return n;
  }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  void flush() override {}
};

// Prediccion HORARIA: rellena cc.hour[] con las proximas horas (icono + temperatura).
bool aemetFetchHoraria(int idx) {
  if (g_apiKey.length() == 0) return false;
  String url = String("https://opendata.aemet.es/opendata/api/prediccion/especifica/municipio/horaria/")
             + g_locs[idx].municipio + "?api_key=" + g_apiKey;
  WiFiClientSecure c1; c1.setInsecure();
  HTTPClient h1; h1.setTimeout(20000); h1.setConnectTimeout(15000);
  if (!h1.begin(c1, url)) { Serial.println("HORARIA: begin paso1 fallo"); return false; }
  int code1 = h1.GET();
  if (code1 != 200) { Serial.printf("HORARIA paso1 HTTP %d\n", code1); h1.end(); return false; }
  JsonDocument meta; deserializeJson(meta, h1.getStream()); h1.end();
  String datos = meta["datos"] | "";
  if (datos.isEmpty()) { Serial.println("HORARIA: sin URL de datos"); return false; }

  WiFiClientSecure c2; c2.setInsecure();
  HTTPClient h2; h2.setTimeout(20000); h2.setConnectTimeout(15000);   // horaria es grande: mas timeout
  if (!h2.begin(c2, datos)) { Serial.println("HORARIA: begin paso2 fallo"); return false; }
  int code2 = h2.GET();
  if (code2 != 200) { Serial.printf("HORARIA paso2 HTTP %d\n", code2); h2.end(); return false; }

  // Volcar el cuerpo completo a PSRAM (de-trocea) y parsear desde ahi.
  PsBuf pb;
  h2.writeToStream(&pb);
  h2.end();
  if (!pb.ok || pb.len == 0) { Serial.printf("HORARIA: lectura incompleta (%u bytes)\n", (unsigned)pb.len); return false; }

  JsonDocument filter;
  JsonObject dia = filter[0]["prediccion"]["dia"][0].to<JsonObject>();
  dia["temperatura"][0]["value"]      = true;
  dia["temperatura"][0]["periodo"]    = true;
  dia["estadoCielo"][0]["descripcion"] = true;
  dia["estadoCielo"][0]["periodo"]    = true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, pb.data, pb.len, DeserializationOption::Filter(filter));
  if (err) { Serial.printf("HORARIA JSON err: %s (%u bytes)\n", err.c_str(), (unsigned)pb.len); return false; }

  int nDias = doc[0]["prediccion"]["dia"].as<JsonArray>().size();
  int curHour = M5.Rtc.getDateTime().time.hours;
  ClimaCache& cc = g_clima[idx];
  cc.nHours = 0;
  int diaIdx = 0;
  for (JsonObject dia2 : doc[0]["prediccion"]["dia"].as<JsonArray>()) {
    bool today = (diaIdx == 0);
    for (JsonObject te : dia2["temperatura"].as<JsonArray>()) {
      if (cc.nHours >= 5) break;
      const char* per = te["periodo"] | "";
      int h = atoi(per);
      if (today && h <= curHour) continue;          // solo las horas que quedan de hoy
      ClimaHour& ch = cc.hour[cc.nHours];
      ch.hour = h;
      ch.temp = te["value"].as<int>();               // value puede llegar como cadena o numero
      ch.cielo[0] = 0;
      for (JsonObject ec : dia2["estadoCielo"].as<JsonArray>()) {
        if (strcmp(ec["periodo"] | "", per) == 0) { strlcpy(ch.cielo, ec["descripcion"] | "", sizeof(ch.cielo)); break; }
      }
      cc.nHours++;
    }
    if (cc.nHours >= 5) break;
    diaIdx++;
  }
  Serial.printf("HORARIA %s: %d horas (dias=%d, horaActual=%d)\n", g_locs[idx].name.c_str(), cc.nHours, nDias, curHour);
  return cc.nHours > 0;
}

// Descarga (NTP + prediccion + observacion + horaria) de todas las ubicaciones.
// Sin aviso si no hay WiFi: el clima ya delata el fallo (sin datos / hora de actualizacion vieja).
void fetchAllOnline(bool withNtp) {
  if (!connectWiFi()) return;
  if (withNtp) syncRtcFromSntp();
  if (g_apiKey.length() > 0) {
    for (int i = 0; i < numLocs(); i++) {   // sin refrescos por ciudad (el e-paper es lento)
      Serial.printf("Descargando %s...\n", g_locs[i].name.c_str());
      aemetFetch(i);
      aemetFetchObs(i);
      aemetFetchHoraria(i);
    }
  }
  wifiOff();
}

// ============================ DIBUJO ============================
// --- Iconos del tiempo (aprovechan el color del e-paper) ---
void fillCloud(int cx, int cy, int s, uint16_t col) {
  canvas.fillCircle(cx - s, cy, (s * 2) / 3, col);
  canvas.fillCircle(cx + s, cy, (s * 2) / 3, col);
  canvas.fillCircle(cx, cy - s / 3, s, col);
  canvas.fillRect(cx - s, cy - s / 3, 2 * s, (s * 2) / 3 + 2, col);
}
void drawWeatherIcon(int cx, int cy, int s, const char* desc) {
  String d = desc ? desc : ""; d.toLowerCase();
  bool storm = d.indexOf("torment") >= 0;
  bool snow  = !storm && d.indexOf("niev") >= 0;
  bool rain  = !storm && !snow && (d.indexOf("lluvia") >= 0 || d.indexOf("lluv") >= 0 || d.indexOf("chubas") >= 0);
  bool fog   = d.indexOf("niebl") >= 0 || d.indexOf("brum") >= 0;
  bool partly= d.indexOf("poco nub") >= 0 || d.indexOf("intervalos") >= 0 || d.indexOf("nubes altas") >= 0;
  bool cloudy= d.indexOf("cubierto") >= 0 || d.indexOf("muy nub") >= 0 || (d.indexOf("nub") >= 0 && !partly);
  bool hasCloud = storm || snow || rain || cloudy || partly;

  if (fog) {
    for (int i = 0; i < 3; i++) canvas.fillRect(cx - s, cy - s / 2 + i * (s / 2), 2 * s, s / 6 + 1, BLUE);
    return;
  }
  if (!hasCloud || partly) {                       // sol (solo, o asomando tras la nube)
    int sx = hasCloud ? cx - s / 2 : cx;
    int sy = hasCloud ? cy - s / 2 : cy;
    int sr = hasCloud ? s / 2 : (s * 2) / 3;
    if (!hasCloud)
      for (int a = 0; a < 360; a += 45) {
        float r = a * 3.14159f / 180.0f;
        canvas.drawLine(sx + cosf(r) * sr * 1.3f, sy + sinf(r) * sr * 1.3f,
                        sx + cosf(r) * sr * 1.7f, sy + sinf(r) * sr * 1.7f, YELLOW);
      }
    canvas.fillCircle(sx, sy, sr, YELLOW);
  }
  if (hasCloud) {                                  // nube con contorno negro
    int ccy = cy + s / 4;
    fillCloud(cx, ccy, (s * 2) / 3, BLACK);
    fillCloud(cx, ccy, (s * 2) / 3 - 3, WHITE);
  }
  int py = cy + s + 2;
  if (rain)  for (int i = -1; i <= 1; i++) canvas.fillCircle(cx + i * (s / 2), py, 2, BLUE);
  if (storm) canvas.fillTriangle(cx - 3, cy + s / 3, cx + 7, cy + s / 3, cx - 1, py + 5, YELLOW);
  if (snow)  for (int i = -1; i <= 1; i++) { int x = cx + i * (s / 2); canvas.drawLine(x - 3, py, x + 3, py, BLUE); canvas.drawLine(x, py - 3, x, py + 3, BLUE); }
}

// Color del indice UV segun la paleta del panel (Spectra 6): bajo=verde, moderado=amarillo, alto=rojo.
uint16_t uvColor(int uv) { return (uv <= 2) ? GREEN : (uv <= 5) ? YELLOW : RED; }

// Tira de prevision por horas (proximas horas): hora + icono pequeno + temperatura, en N columnas.
void drawHourly(ClimaCache& cc, int y0) {
  if (cc.nHours <= 0) return;
  const int W = M5.Display.width();
  int n = cc.nHours > 5 ? 5 : cc.nHours;
  int cw = W / n;
  for (int i = 0; i < n; i++) {
    ClimaHour& h = cc.hour[i];
    int cx = cw * i + cw / 2;
    if (i > 0) canvas.drawFastVLine(cw * i, y0 + 2, 70, BLACK);   // separador entre columnas
    char hl[8]; snprintf(hl, sizeof(hl), "%dh", h.hour);
    canvas.setFont(&fonts::FreeSansBold12pt7b); canvas.setTextDatum(top_center); canvas.setTextColor(BLACK);
    canvas.drawString(hl, cx, y0);
    drawWeatherIcon(cx, y0 + 33, 13, h.cielo);
    char tt[8]; snprintf(tt, sizeof(tt), "%dC", h.temp);
    canvas.setTextDatum(bottom_center); canvas.setTextColor(BLACK);
    canvas.drawString(tt, cx, y0 + 73);
  }
}

// Bloque destacado de HOY: icono + temps + UV + cielo + viento + lluvia + humedad + tira horaria.
void drawToday(ClimaCache& cc, int top, int bottom) {
  const int W = M5.Display.width();
  ClimaDay& d = cc.day[0];
  canvas.setFont(&fonts::FreeSansBold18pt7b); canvas.setTextDatum(top_left); canvas.setTextColor(BLACK);
  canvas.drawString("HOY", 12, top);

  drawWeatherIcon(60, top + 92, 34, d.cielo);

  int rx = 128;
  // Temperaturas grandes max/min
  canvas.setFont(&fonts::FreeSansBold24pt7b); canvas.setTextDatum(top_left);
  char s[8];
  snprintf(s, sizeof(s), "%d", d.tMax);
  canvas.setTextColor(RED);   canvas.drawString(s, rx, top + 14); int wmax = canvas.textWidth(s);
  canvas.setTextColor(BLACK); canvas.drawString(" / ", rx + wmax, top + 14); int wsl = canvas.textWidth(" / ");
  snprintf(s, sizeof(s), "%d", d.tMin);
  canvas.setTextColor(BLUE);  canvas.drawString(s, rx + wmax + wsl, top + 14);

  canvas.setFont(&fonts::FreeSansBold12pt7b); canvas.setTextDatum(top_left); canvas.setTextColor(BLACK);
  canvas.drawString("max / min  C", rx, top + 50);
  // UV (a la derecha, color segun nivel)
  if (d.uvMax >= 0) {
    char uvs[12]; snprintf(uvs, sizeof(uvs), "UV %d", d.uvMax);
    canvas.setTextDatum(top_right); canvas.setTextColor(uvColor(d.uvMax));
    canvas.drawString(uvs, W - 10, top + 50);
    canvas.setTextDatum(top_left); canvas.setTextColor(BLACK);
  }
  // Cielo en una linea (el icono ya lo ilustra)
  { char cl[28]; strlcpy(cl, d.cielo, sizeof(cl)); canvas.drawString(cl, rx, top + 74); }
  // Viento (subido: justo bajo el cielo)
  if (d.vientoVel >= 0) {
    char vt[36];
    if (d.racha > 0) snprintf(vt, sizeof(vt), "Viento %s %d (r%d) km/h", d.vientoDir, d.vientoVel, d.racha);
    else             snprintf(vt, sizeof(vt), "Viento %s %d km/h", d.vientoDir, d.vientoVel);
    canvas.drawString(vt, rx, top + 98);
  }
  // Lluvia + humedad
  canvas.setTextColor(BLUE);
  char lab[28]; snprintf(lab, sizeof(lab), "Lluvia %d%%", d.probLluvia);
  canvas.drawString(lab, rx, top + 122);
  if (d.humMax >= 0) { char hr[28]; snprintf(hr, sizeof(hr), "Humedad %d-%d%%", d.humMin, d.humMax); canvas.drawString(hr, rx, top + 144); }

  // Tira de las proximas horas, a todo el ancho, antes de las filas de los proximos dias
  drawHourly(cc, bottom - 80);
}

// Filas compactas de los proximos dias (icono + dia + cielo + max/min + lluvia).
void drawUpcomingRows(ClimaCache& cc, int startIdx, int count, int top, int bottom) {
  const int W = M5.Display.width();
  int rows = 0;
  for (int k = 0; k < count && (startIdx + k) < cc.nDays; k++) rows++;
  if (rows == 0) return;
  int rowH = (bottom - top) / rows;
  for (int k = 0; k < rows; k++) {
    ClimaDay& d = cc.day[startIdx + k];
    int y0 = top + k * rowH, cy = y0 + rowH / 2;
    if (k > 0) canvas.drawFastHLine(10, y0, W - 20, BLACK);

    drawWeatherIcon(32, cy, (int)(rowH * 0.26f), d.cielo);

    canvas.setFont(&fonts::FreeSansBold18pt7b); canvas.setTextColor(BLACK); canvas.setTextDatum(middle_left);
    canvas.drawString(DIAS_C[d.wday], 62, cy - 11);
    canvas.setFont(&fonts::FreeSansBold12pt7b);
    char cielo[20]; strlcpy(cielo, d.cielo, sizeof(cielo));
    canvas.drawString(cielo, 62, cy + 13);

    canvas.setFont(&fonts::FreeSansBold18pt7b); canvas.setTextDatum(middle_right);
    char s[8];
    snprintf(s, sizeof(s), "%d", d.tMin);
    canvas.setTextColor(BLUE);  canvas.drawString(s, W - 12, cy - 11); int wmin = canvas.textWidth(s);
    canvas.setTextColor(BLACK); canvas.drawString("/", W - 12 - wmin, cy - 11); int wsl = canvas.textWidth("/");
    snprintf(s, sizeof(s), "%d", d.tMax);
    canvas.setTextColor(RED);   canvas.drawString(s, W - 12 - wmin - wsl, cy - 11);
    canvas.setFont(&fonts::FreeSansBold12pt7b); canvas.setTextColor(BLUE); canvas.setTextDatum(middle_right);
    char lab[16]; snprintf(lab, sizeof(lab), "lluvia %d%%", d.probLluvia);
    canvas.drawString(lab, W - 12, cy + 13);
  }
}

// --- Icono de bateria (verde/amarillo/rojo segun carga). Sirve para canvas y M5.Display. ---
template<typename G> void drawBatteryIcon(G& g, int x, int y) {
  int bat = M5.Power.getBatteryLevel(); if (bat < 0) bat = 0;
  uint16_t col = (bat >= 60) ? GREEN : (bat >= 25) ? YELLOW : RED;
  const int w = 42, h = 20;
  g.fillRect(x - 2, y - 2, w + 9, h + 4, WHITE);      // limpia la zona
  g.drawRect(x, y, w, h, BLACK);
  g.fillRect(x + w, y + 6, 3, h - 12, BLACK);          // borne
  g.fillRect(x + 2, y + 2, (w - 4) * bat / 100, h - 4, col);
}

// --- Campos de la vista LOCAL (mismas coords para dibujo completo y parcial) ---
static const int LOC_CLK_Y = 150, LOC_WD_Y = 250, LOC_DT_Y = 300, LOC_TP_Y = 415, LOC_HM_Y = 520;
template<typename G> void locClock(G& g, const m5::rtc_datetime_t& dt) {
  char hm[8]; snprintf(hm, sizeof(hm), "%02d:%02d", dt.time.hours, dt.time.minutes);
  g.setFont(&fonts::Font7); g.setTextSize(2); g.setTextColor(BLACK); g.setTextDatum(middle_center);
  g.drawString(hm, M5.Display.width() / 2, LOC_CLK_Y); g.setTextSize(1);
}
template<typename G> void locDate(G& g, const m5::rtc_datetime_t& dt) {
  const int W = M5.Display.width();
  int wd = (dt.date.weekDay >= 0 && dt.date.weekDay <= 6) ? dt.date.weekDay : 0;
  int mo = (dt.date.month >= 1 && dt.date.month <= 12) ? dt.date.month - 1 : 0;
  g.setTextColor(BLACK); g.setTextDatum(middle_center);
  g.setFont(&fonts::FreeSansBold24pt7b); g.drawString(DIAS[wd], W / 2, LOC_WD_Y);
  char d2[40]; snprintf(d2, sizeof(d2), "%d de %s %d", dt.date.date, MESES[mo], dt.date.year);
  g.setFont(&fonts::FreeSansBold18pt7b); g.drawString(d2, W / 2, LOC_DT_Y);
}
template<typename G> void locTemp(G& g) {
  char b[16]; snprintf(b, sizeof(b), isnan(g_temp) ? "--.- C" : "%.1f C", g_temp);
  g.setFont(&fonts::FreeSansBold24pt7b); g.setTextColor(RED); g.setTextDatum(middle_center);
  g.drawString(b, M5.Display.width() / 2, LOC_TP_Y);
}
template<typename G> void locHum(G& g) {
  char b[16]; snprintf(b, sizeof(b), isnan(g_hum) ? "--.- %%" : "%.0f %%", g_hum);
  g.setFont(&fonts::FreeSansBold24pt7b); g.setTextColor(BLUE); g.setTextDatum(middle_center);
  g.drawString(b, M5.Display.width() / 2, LOC_HM_Y);
}

// Dibujo COMPLETO de la vista LOCAL (al entrar / limpieza periodica).
void drawWeatherLocal(const m5::rtc_datetime_t& dt) {
  const int W = M5.Display.width();
  canvas.fillSprite(WHITE);
  canvas.setFont(&fonts::FreeSansBold12pt7b); canvas.setTextDatum(top_left); canvas.setTextColor(BLACK);
  char hdr[16]; snprintf(hdr, sizeof(hdr), "LOCAL  1/%d", numViews());
  canvas.drawString(hdr, 12, 10);
  drawBatteryIcon(canvas, W - 53, 8);
  locClock(canvas, dt);
  locDate(canvas, dt);
  canvas.drawFastHLine(40, 355, W - 80, BLACK);
  locTemp(canvas);
  canvas.setFont(&fonts::FreeSansBold18pt7b); canvas.setTextColor(BLACK); canvas.setTextDatum(middle_center);
  canvas.drawString("Temperatura", W / 2, 460);
  locHum(canvas);
  canvas.drawString("Humedad", W / 2, 565);
  canvas.pushSprite(0, 0);
  g_lastLocalUpdateSec = rtcNow();
}

void drawWeatherCity(const m5::rtc_datetime_t& dt, int loc) {
  const int W = M5.Display.width(), H = M5.Display.height();
  canvas.fillSprite(WHITE);
  // Cabecera compacta (texto pequeno para que no choque): nombre | hora v/N
  canvas.setFont(&fonts::FreeSansBold12pt7b);
  canvas.setTextDatum(top_left); canvas.setTextColor(BLACK);
  canvas.drawString(g_locs[loc].name.c_str(), 12, 12);
  canvas.setTextDatum(top_right);
  char hv[16]; snprintf(hv, sizeof(hv), "%02d:%02d  %d/%d", dt.time.hours, dt.time.minutes, loc + 2, numViews());
  canvas.drawString(hv, W - 12, 12);
  canvas.drawFastHLine(12, 40, W - 24, BLACK);

  if (g_apiKey.length() == 0) {
    drawCenteredMsg("Configura aemet_api_key", H / 2 - 20, RED, &fonts::FreeSansBold18pt7b);
    drawCenteredMsg("en /config.json", H / 2 + 16, RED, &fonts::FreeSansBold18pt7b);
    canvas.pushSprite(0, 0); return;
  }

  ClimaCache& cc = g_clima[loc];
  int top = 48;
  // Observacion actual de estacion (p.ej. El Goloso en Madrid)
  if (cc.obsValid) {
    canvas.setTextDatum(top_left); canvas.setFont(&fonts::FreeSansBold12pt7b); canvas.setTextColor(BLACK);
    char lbl[24]; snprintf(lbl, sizeof(lbl), "Ahora %s", cc.obsTime);
    canvas.drawString(lbl, 12, 46);
    canvas.setTextDatum(top_right); char v[24];
    snprintf(v, sizeof(v), "%.1fC  %.0f%%", cc.obsTemp, cc.obsHum);
    canvas.setTextColor(RED); canvas.drawString(v, W - 12, 46);
    canvas.drawFastHLine(12, 74, W - 24, BLACK);
    top = 82;
  }

  if (!cc.valid) {
    // Mensaje en 2 lineas y fuente pequena (antes se salia de pantalla)
    drawCenteredMsg("Manten pulsado G1", H / 2 - 16, BLACK, &fonts::FreeSansBold12pt7b);
    drawCenteredMsg("para descargar la prediccion", H / 2 + 12, BLACK, &fonts::FreeSansBold12pt7b);
    canvas.pushSprite(0, 0); return;
  }

  // HOY destacado (con tira de proximas horas) + proximos dias
  int todayBottom = top + 250;
  drawToday(cc, top, todayBottom);
  canvas.drawFastHLine(10, todayBottom, W - 20, BLACK);
  drawUpcomingRows(cc, 1, 3, todayBottom + 2, H - 22);

  // Pie: hora de actualizacion (dcha) + icono de bateria (izq)
  canvas.setFont(&fonts::FreeSansBold12pt7b); canvas.setTextColor(BLACK);
  canvas.setTextDatum(bottom_right);
  char upd[24]; snprintf(upd, sizeof(upd), "AEMET %02d:%02d", cc.updH, cc.updM);
  canvas.drawString(upd, W - 8, H - 4);
  drawBatteryIcon(canvas, 10, H - 24);
  canvas.pushSprite(0, 0);
}

// Dibuja una imagen ajustada/centrada. Carga el fichero a un buffer (PSRAM) y usa
// las variantes drawJpg/drawPng/drawBmp por memoria (evita la plantilla de filesystem
// de M5GFX, que es abstracta en esta version).
void drawImageFitted(const String& path, int x, int y, int w, int h) {
  File f = SD.open(path);
  if (!f) return;
  size_t sz = f.size();
  if (sz == 0) { f.close(); return; }
  uint8_t* buf = (uint8_t*)ps_malloc(sz);     // preferimos PSRAM
  if (!buf) buf = (uint8_t*)malloc(sz);
  if (!buf) { f.close(); Serial.println("Sin memoria para la imagen"); return; }
  size_t rd = f.read(buf, sz);
  f.close();
  if (rd == sz) {
    String n = path; n.toLowerCase();
    // (x,y)=esquina de la caja, (w,h)=tamano de la caja, escala 0 -> auto-ajuste,
    // datum middle_center -> centrado. (Antes pasaba el centro como esquina y se salia.)
    if (n.endsWith(".png"))      canvas.drawPng(buf, sz, x, y, w, h, 0, 0, 0.0f, 0.0f, middle_center);
    else if (n.endsWith(".bmp")) canvas.drawBmp(buf, sz, x, y, w, h, 0, 0, 0.0f, 0.0f, middle_center);
    else                         canvas.drawJpg(buf, sz, x, y, w, h, 0, 0, 0.0f, 0.0f, middle_center);
  }
  free(buf);
}

// Cambia la rotacion del panel y recrea el lienzo al nuevo tamano.
void setPanelRotation(int rot) {
  if (rot == g_rot) return;
  g_rot = rot;
  M5.Display.setRotation(rot);
  canvas.deleteSprite();
  canvas.setPsram(true);
  canvas.createSprite(M5.Display.width(), M5.Display.height());
}

// Lee ancho/alto de la imagen desde su cabecera (JPG/PNG/BMP) sin decodificarla.
bool jpgSize(File& f, int& w, int& h) {
  if (f.read() != 0xFF || f.read() != 0xD8) return false;          // SOI
  while (f.available()) {
    int b = f.read();
    if (b != 0xFF) continue;
    int marker = f.read();
    while (marker == 0xFF) marker = f.read();
    if (marker >= 0xC0 && marker <= 0xCF && marker != 0xC4 && marker != 0xC8 && marker != 0xCC) {
      f.read(); f.read();            // longitud
      f.read();                      // precision
      h = (f.read() << 8) | f.read();
      w = (f.read() << 8) | f.read();
      return true;
    }
    int len = (f.read() << 8) | f.read();
    if (len < 2) return false;
    f.seek(f.position() + len - 2);
  }
  return false;
}
bool getImageSize(const String& path, int& w, int& h) {
  File f = SD.open(path);
  if (!f) return false;
  String n = path; n.toLowerCase();
  bool ok = false;
  if (n.endsWith(".png")) {
    uint8_t b[24];
    if (f.read(b, 24) == 24) {
      w = (b[16]<<24)|(b[17]<<16)|(b[18]<<8)|b[19];
      h = (b[20]<<24)|(b[21]<<16)|(b[22]<<8)|b[23]; ok = true;
    }
  } else if (n.endsWith(".bmp")) {
    uint8_t b[26];
    if (f.read(b, 26) == 26) {
      w = b[18]|(b[19]<<8)|(b[20]<<16)|(b[21]<<24);
      int hh = b[22]|(b[23]<<8)|(b[24]<<16)|(b[25]<<24);
      h = abs(hh); ok = true;
    }
  } else {
    ok = jpgSize(f, w, h);
  }
  f.close();
  return ok && w > 0 && h > 0;
}

void drawCarrusel() {
  if (!(sd_ready && !g_images.empty())) {
    setPanelRotation(1);
    canvas.fillSprite(WHITE);
    drawCenteredMsg("Sin imagenes en la microSD", M5.Display.height() / 2);
    canvas.pushSprite(0, 0);
    return;
  }
  const String& path = g_images[g_img_idx % g_images.size()];
  // Orientacion del panel segun la foto (vertical -> rot 0, horizontal -> rot 1)
  int rot = 1, iw = 0, ih = 0;
  if (g_photoAutoRotate && getImageSize(path, iw, ih) && ih > iw) rot = 0;
  setPanelRotation(rot);

  const int W = M5.Display.width(), H = M5.Display.height();
  canvas.fillSprite(WHITE);
  drawImageFitted(path, 0, 0, W, H);
  canvas.pushSprite(0, 0);
}

// ============================ MODO 3: MUSICA (estilo iPod Shuffle) ============================
// Controles: UP/DOWN click = cancion sig/ant · doble-click = volumen +/- · mantener = play/pausa.
// NOTA: la salida de audio (decodificacion + codec ES8311) se integra como siguiente paso con
//       la libreria ESP32-audioI2S. De momento esta funciona la navegacion, volumen y UI.

bool isAudioFile(const String& n0) {
  String n = n0; n.toLowerCase();
  return n.endsWith(".mp3") || n.endsWith(".m4a") || n.endsWith(".flac") ||
         n.endsWith(".wav") || n.endsWith(".aac");
}
void scanMusic() {
  g_music.clear();
  if (!sd_ready) return;
  File dir = SD.open(g_musicaDir.c_str());
  if (!dir || !dir.isDirectory()) { if (dir) dir.close(); return; }
  for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
    if (!f.isDirectory() && isAudioFile(f.path())) g_music.push_back(f.path());
    f.close();
  }
  dir.close();
  Serial.printf("Musica en %s: %u\n", g_musicaDir.c_str(), (unsigned)g_music.size());
}
// Nombre legible de la pista (sin ruta ni extension).
String trackName(const String& path) {
  int slash = path.lastIndexOf('/');
  String n = (slash >= 0) ? path.substring(slash + 1) : path;
  int dot = n.lastIndexOf('.');
  if (dot > 0) n = n.substring(0, dot);
  return n;
}
// Devuelve el texto en UTF-8 valido (para que la fuente DejaVu pinte los acentos).
// Si la SD devuelve el nombre en Latin-1 (1 byte por acento), lo expande a UTF-8.
String utf8Fix(const String& in) {
  int n = in.length();
  bool valid = true;
  for (int i = 0; i < n && valid;) {
    uint8_t c = in[i];
    if (c < 0x80) i++;
    else if ((c & 0xE0) == 0xC0) { valid = (i + 1 < n && (in[i+1] & 0xC0) == 0x80); i += 2; }
    else if ((c & 0xF0) == 0xE0) { valid = (i + 2 < n && (in[i+1] & 0xC0) == 0x80 && (in[i+2] & 0xC0) == 0x80); i += 3; }
    else if ((c & 0xF8) == 0xF0) { valid = (i + 3 < n && (in[i+1] & 0xC0) == 0x80 && (in[i+2] & 0xC0) == 0x80 && (in[i+3] & 0xC0) == 0x80); i += 4; }
    else valid = false;
  }
  if (valid) return in;                       // ya es UTF-8
  String out;                                 // tratar como Latin-1 -> UTF-8
  for (int i = 0; i < n; i++) {
    uint8_t c = in[i];
    if (c < 0x80) out += (char)c;
    else { out += (char)(0xC0 | (c >> 6)); out += (char)(0x80 | (c & 0x3F)); }
  }
  return out;
}

// Inicializa el codec ES8311 (secuencia M5Unified PaperColor, MCLK=BCLK) y el I2S.
// Init I2S + callback (UNA vez). El codec/ampli arrancan APAGADOS (ahorro): solo se encienden en musica.
void audioInit() {
  pinMode(PIN_CODEC_EN, OUTPUT); digitalWrite(PIN_CODEC_EN, LOW);
  pinMode(PIN_SPK_EN, OUTPUT);   digitalWrite(PIN_SPK_EN, LOW);
  audio.setPinout(I2S_BCLK_PIN, I2S_LRCK_PIN, I2S_DOUT_PIN);   // MCLK no necesario (=BCLK)
  audio.setAudioTaskCore(0);     // tarea de audio en el core 0 (Arduino corre en el 1)
  // Fin de pista fiable via callback de la libreria (evt_eof)
  Audio::audio_info_callback = [](Audio::msg_t m) { if (m.e == Audio::evt_eof) g_trackEnded = true; };
  g_audioReady = true;
  Serial.println("Audio (I2S) listo; codec apagado hasta reproducir.");
}
// Enciende codec ES8311 + ampli e inicializa registros (al entrar en musica).
void audioPowerOn() {
  if (g_audioPowered) return;
  digitalWrite(PIN_CODEC_EN, HIGH); digitalWrite(PIN_SPK_EN, HIGH);
  delay(10);
  static const uint8_t seq[][2] = {
    {0x00, 0x80}, {0x01, 0xB5}, {0x02, 0x18}, {0x0D, 0x01},
    {0x12, 0x00}, {0x13, 0x10}, {0x32, 0xCF}, {0x37, 0x08}
  };
  for (auto& r : seq) M5.In_I2C.writeRegister8(ES8311_ADDR, r[0], r[1], 100000);
  audio.setVolume(g_volume);
  g_audioPowered = true;
  Serial.println("Codec ES8311 encendido.");
}
// Para la reproduccion y apaga codec + ampli (al salir de musica / ahorro).
void audioPowerOff() {
  if (g_audioReady && audio.isRunning()) audio.stopSong();
  g_playing = false; g_loaded = false;
  digitalWrite(PIN_CODEC_EN, LOW); digitalWrite(PIN_SPK_EN, LOW);
  g_audioPowered = false;
}

// --- Acciones de audio (estilo iPod Shuffle) ---
void musicPlayCurrent() {
  if (g_music.empty() || !g_audioReady) return;
  audioPowerOn();   // asegura codec/ampli encendidos
  Serial.printf("[MUSICA] play %s (vol %d)\n", trackName(g_music[g_track]).c_str(), g_volume);
  audio.connecttoFS(SD, g_music[g_track].c_str());
  audio.setVolume(g_volume);
  g_loaded = true; g_playing = true; g_trackEnded = false;
}
void musicNext() {
  if (g_music.empty()) return;
  g_track = (g_track + 1) % g_music.size();
  musicPlayCurrent(); g_needRedraw = true;
}
void musicPrev() {
  if (g_music.empty()) return;
  g_track = (g_track + g_music.size() - 1) % g_music.size();
  musicPlayCurrent(); g_needRedraw = true;
}
// Volumen y play/pausa NO refrescan la pantalla (evita cortes de audio por el e-paper).
void musicVolUp()   { if (g_volume < 21) g_volume++; if (g_audioReady) audio.setVolume(g_volume); Serial.printf("[MUSICA] vol %d\n", g_volume); }
void musicVolDown() { if (g_volume > 0)  g_volume--; if (g_audioReady) audio.setVolume(g_volume); Serial.printf("[MUSICA] vol %d\n", g_volume); }
void musicTogglePlay() {
  if (g_music.empty()) return;
  // primer play: la pista actual YA esta en pantalla (se dibujo al entrar al modo) -> no redibujar
  if (!g_loaded) { musicPlayCurrent(); }
  else { audio.pauseResume(); g_playing = !g_playing; }
  Serial.printf("[MUSICA] %s\n", g_playing ? "play" : "pausa");
}


void drawMusic() {
  const int W = M5.Display.width(), H = M5.Display.height();
  canvas.fillSprite(WHITE);

  // Cabecera
  canvas.setFont(&fonts::FreeSansBold18pt7b); canvas.setTextDatum(top_left); canvas.setTextColor(BLACK);
  canvas.drawString("MUSICA", 12, 8);
  drawBatteryIcon(canvas, W - 53, 8);
  canvas.drawFastHLine(12, 42, W - 24, BLACK);

  if (g_music.empty()) {
    drawCenteredMsg("Sin musica en", H / 2 - 16, BLACK, &fonts::FreeSansBold18pt7b);
    drawCenteredMsg(g_musicaDir.c_str(), H / 2 + 16, BLACK, &fonts::FreeSansBold18pt7b);
    canvas.pushSprite(0, 0); return;
  }

  // Pista actual (n / total)
  canvas.setFont(&fonts::FreeSansBold12pt7b); canvas.setTextDatum(top_right); canvas.setTextColor(BLACK);
  char idx[16]; snprintf(idx, sizeof(idx), "%u / %u", (unsigned)(g_track + 1), (unsigned)g_music.size());
  canvas.drawString(idx, W - 12, 50);

  // Nombre de la pista (grande, centrado, hasta 2 lineas). Fuente VLW con acentos si la hay
  // (cargada de la SD); si no, DejaVu24 integrada. Sin icono ni barra -> solo refresca al cambiar.
  bool useVlw = g_titleFontReady && canvas.loadFont(g_titleFont);
  if (!useVlw) canvas.setFont(&fonts::DejaVu24);
  canvas.setTextColor(BLACK); canvas.setTextDatum(middle_center);
  {
    String name = utf8Fix(trackName(g_music[g_track]));
    if (canvas.textWidth(name) <= W - 24) {
      canvas.drawString(name, W / 2, H / 2);
    } else {
      int half = name.length() / 2;
      int sp = name.indexOf(' ', half); if (sp < 0) sp = half;
      canvas.drawString(name.substring(0, sp), W / 2, H / 2 - 28);
      canvas.drawString(name.substring(sp + 1), W / 2, H / 2 + 28);
    }
  }
  if (useVlw) canvas.unloadFont();   // vuelve a fuente por defecto para el resto

  // Ayuda de controles (2 lineas para que no se corte)
  canvas.setFont(&fonts::FreeSansBold12pt7b); canvas.setTextColor(BLACK); canvas.setTextDatum(bottom_center);
  canvas.drawString("Click: volumen    x2: cancion", W / 2, H - 34);
  canvas.drawString("Manten: play / pausa", W / 2, H - 10);

  canvas.pushSprite(0, 0);
}

// ============================ MODO 4: LIBRO (lector TXT) ============================
// Pagina el fichero de texto en streaming (sin cargarlo entero). Renderiza con la
// fuente de lectura (acentos). Guarda libro+pagina en NVS. TXT en UTF-8.
static constexpr int LIB_MARGIN = 14;   // margen lateral
static constexpr int LIB_TOP    = 56;   // inicio del texto (bajo cabecera)
static constexpr int LIB_LINE_H = 28;   // alto de linea

// Clave NVS unica por libro (hash FNV-1a del path) para recordar su pagina.
String bookKey() {
  uint32_t h = 2166136261u;
  const String& p = g_books[g_bookIdx];
  for (size_t i = 0; i < p.length(); i++) { h ^= (uint8_t)p[i]; h *= 16777619u; }
  char k[12]; snprintf(k, sizeof(k), "b%08lX", (unsigned long)h);
  return String(k);
}
void bookSavePos() {
  if (g_books.empty()) return;
  prefs.putUInt(bookKey().c_str(), g_pageStart);
  prefs.putInt("lastbook", g_bookIdx);
}

// Dibuja la pagina que empieza en g_pageStart y calcula g_nextPageStart.
void drawLibro() {
  const int W = M5.Display.width(), H = M5.Display.height();
  canvas.fillSprite(WHITE);

  bool useBody = g_bodyFontReady && canvas.loadFont(g_bodyFont);

  // Cabecera: nombre + bateria
  canvas.setTextDatum(top_left); canvas.setTextColor(BLACK);
  if (!useBody) canvas.setFont(&fonts::FreeSansBold12pt7b);
  if (g_books.empty()) {
    if (useBody) canvas.unloadFont();
    drawCenteredMsg("Sin libros (.txt) en", H / 2 - 16, BLACK, &fonts::FreeSansBold18pt7b);
    drawCenteredMsg(g_librosDir.c_str(), H / 2 + 16, BLACK, &fonts::FreeSansBold18pt7b);
    canvas.pushSprite(0, 0); return;
  }
  String name = trackName(g_books[g_bookIdx]);   // nombre sin ruta/extension
  canvas.drawString(name, LIB_MARGIN, 10);
  drawBatteryIcon(canvas, W - 53, 8);
  canvas.drawFastHLine(LIB_MARGIN, 44, W - 2 * LIB_MARGIN, BLACK);

  // Leer un trozo desde la posicion actual y paginar por palabras
  const int maxW = W - 2 * LIB_MARGIN;
  // Alto de linea segun la fuente realmente cargada: un body.vlw mas pequeno (p.ej. 18px)
  // da automaticamente mas lineas por pagina. Para la fuente integrada se usa LIB_LINE_H.
  int fh = useBody ? canvas.fontHeight() : 0;
  if (fh < 10 || fh > 80) fh = 22;                           // salvaguarda ante valores raros
  const int lineH = useBody ? (fh + 6) : LIB_LINE_H;
  const int linesPerPage = (H - LIB_TOP - 34) / lineH;        // deja hueco para la ayuda
  File f = SD.open(g_books[g_bookIdx].c_str());
  bool opened = (bool)f;
  size_t fsz = 0;
  String chunk;
  if (opened) {
    fsz = f.size();
    if (g_pageStart > fsz) g_pageStart = 0;       // offset invalido -> al principio
    f.seek(g_pageStart);
    const size_t CHUNK = 7000;
    chunk.reserve(CHUNK);
    for (size_t i = 0; i < CHUNK && f.available(); i++) chunk += (char)f.read();
    f.close();
  }
  Serial.printf("[LIBRO] %s open=%d size=%u start=%u read=%u\n",
                g_books[g_bookIdx].c_str(), opened ? 1 : 0,
                (unsigned)fsz, (unsigned)g_pageStart, (unsigned)chunk.length());
  if (!opened || chunk.length() == 0) {
    if (useBody) canvas.unloadFont();
    drawCenteredMsg(opened ? "Fichero vacio o fin" : "No se pudo abrir", H / 2, RED, &fonts::FreeSansBold18pt7b);
    canvas.pushSprite(0, 0); return;
  }

  int pos = 0, len = chunk.length(), y = LIB_TOP, linesDrawn = 0;
  canvas.setTextDatum(top_left); canvas.setTextColor(BLACK);
  while (linesDrawn < linesPerPage && pos < len) {
    if (chunk[pos] == '\r') { pos++; continue; }
    if (chunk[pos] == '\n') { pos++; y += lineH; linesDrawn++; continue; }  // parrafo / linea en blanco
    String line; int linePos = pos;
    while (linePos < len && chunk[linePos] != '\n') {
      int ws = linePos; while (ws < len && chunk[ws] != ' ' && chunk[ws] != '\n') ws++;  // palabra
      String word = chunk.substring(linePos, ws);
      String cand = line.length() ? line + " " + word : word;
      if (canvas.textWidth(cand) <= maxW || line.length() == 0) {
        line = cand; linePos = ws;
        if (linePos < len && chunk[linePos] == ' ') linePos++;   // consume el espacio
      } else break;   // no cabe -> fin de linea
    }
    canvas.drawString(line, LIB_MARGIN, y);
    pos = linePos;
    if (pos < len && chunk[pos] == '\n') pos++;   // consume el salto que cierra la linea
    y += lineH; linesDrawn++;
  }
  g_nextPageStart = g_pageStart + pos;

  if (useBody) canvas.unloadFont();
  // Ayuda discreta abajo
  canvas.setFont(&fonts::FreeSans9pt7b); canvas.setTextColor(BLACK); canvas.setTextDatum(bottom_center);
  canvas.drawString("UP/DN pagina   manten G1: lista   G1: salir", W / 2, H - 4);
  canvas.pushSprite(0, 0);
}

void pageNext() {
  if (g_books.empty()) return;
  File f = SD.open(g_books[g_bookIdx].c_str()); size_t fsz = f ? f.size() : 0; if (f) f.close();
  if (g_nextPageStart <= g_pageStart || g_nextPageStart >= fsz) return;   // fin del libro
  g_pageStack.push_back(g_pageStart);
  g_pageStart = g_nextPageStart;
  bookSavePos(); g_needRedraw = true;
}
void pagePrev() {
  if (g_books.empty() || g_pageStack.empty()) return;
  g_pageStart = g_pageStack.back(); g_pageStack.pop_back();
  bookSavePos(); g_needRedraw = true;
}
// Mover la seleccion en el listado.
void listMove(int d) {
  if (g_books.empty()) return;
  int n = g_books.size();
  g_sel = (g_sel + d % n + n) % n;
  g_needRedraw = true;
}
// Abrir el libro seleccionado en su ultima pagina leida.
void bookOpenSelected() {
  if (g_books.empty()) return;
  g_bookIdx = g_sel;
  g_pageStart = prefs.getUInt(bookKey().c_str(), 0);   // posicion guardada de ESTE libro
  g_pageStack.clear();
  g_libState = LIB_READING;
  bookSavePos(); g_needRedraw = true;
}

// Selector de archivos (lista de /Libros).
void drawLibroList() {
  const int W = M5.Display.width(), H = M5.Display.height();
  canvas.fillSprite(WHITE);
  bool useBody = g_bodyFontReady && canvas.loadFont(g_bodyFont);
  if (!useBody) canvas.setFont(&fonts::FreeSansBold18pt7b);

  canvas.setTextDatum(top_left); canvas.setTextColor(BLACK);
  canvas.drawString("LIBROS", LIB_MARGIN, 10);
  canvas.drawFastHLine(LIB_MARGIN, 44, W - 2 * LIB_MARGIN, BLACK);

  if (g_books.empty()) {
    if (useBody) canvas.unloadFont();
    drawCenteredMsg("Sin libros (.txt) en", H / 2 - 16, BLACK, &fonts::FreeSansBold18pt7b);
    drawCenteredMsg(g_librosDir.c_str(), H / 2 + 16, BLACK, &fonts::FreeSansBold18pt7b);
    canvas.pushSprite(0, 0); return;
  }

  const int rowH = 34, top = 56;
  int visible = (H - top - 30) / rowH;
  int n = g_books.size();
  int first = 0;                                   // ventana de scroll alrededor de la seleccion
  if (n > visible) { first = g_sel - visible / 2; if (first < 0) first = 0; if (first > n - visible) first = n - visible; }
  for (int i = 0; i < visible && (first + i) < n; i++) {
    int idx = first + i, y = top + i * rowH;
    if (idx == g_sel) canvas.fillRect(LIB_MARGIN - 4, y - 2, W - 2 * LIB_MARGIN + 8, rowH - 2, BLUE);
    canvas.setTextColor(idx == g_sel ? WHITE : BLACK);
    canvas.setTextDatum(top_left);
    canvas.drawString(trackName(g_books[idx]), LIB_MARGIN, y);
  }
  if (useBody) canvas.unloadFont();
  canvas.setFont(&fonts::FreeSans9pt7b); canvas.setTextColor(BLACK); canvas.setTextDatum(bottom_center);
  canvas.drawString("UP/DN elegir   x2: abrir   G1: salir", W / 2, H - 4);
  canvas.pushSprite(0, 0);
}

void drawCurrentMode() {
  auto dt = M5.Rtc.getDateTime();
  switch (g_mode) {
    case MODE_CLIMA:
      if (g_view == 0 || numLocs() == 0) drawWeatherLocal(dt);
      else                               drawWeatherCity(dt, g_view - 1);
      break;
    case MODE_CARRUSEL: drawCarrusel(); break;
    case MODE_MUSICA:   drawMusic(); break;
    case MODE_LIBRO:    if (g_libState == LIB_LIST) drawLibroList(); else drawLibro(); break;
    default: break;
  }
}

void applyEpdMode() {
  // En el ED2208 los modos solo cambian el DITHERING (no la velocidad): se elige el logico
  // por contenido. Fotos -> quality (mejor color); libros -> text (texto nitido);
  // resto (clima/musica, con color) -> fast.
  epd_mode_t m;
  switch (g_mode) {
    case MODE_CARRUSEL: m = epd_mode_t::epd_quality; break;
    case MODE_LIBRO:    m = epd_mode_t::epd_text;    break;
    default:            m = epd_mode_t::epd_fast;    break;
  }
  M5.Display.setEpdMode(m);
}

// ============================ MODO OCULTO: TV-B-Gone (IR) ============================
// Parpadeo del LED RGB integrado (sin tocar la pantalla).
void ledBlink(uint8_t r, uint8_t g, uint8_t b, int times) {
  if (!M5.Led.isEnabled()) return;
  M5.Led.setBrightness(120);
  for (int i = 0; i < times; i++) {
    M5.Led.setAllColor(r, g, b); delay(180);
    M5.Led.setAllColor(0, 0, 0); delay(150);
  }
}

// Doble-click en G1: emite la tanda de codigos de apagado de TVs europeas.
// NO usa la pantalla: 2 parpadeos verdes al empezar, rojos al terminar.
void tvBGone() {
  ledBlink(0, 255, 0, 2);                   // verde: empezando
  bool cancel = false;
  for (int i = 0; i < numCodigosEU; i++) {
    if (digitalRead(PIN_BTN_MODE) == LOW) { cancel = true; break; }   // G1 cancela
    const TVCode& c = codigosEU[i];
    switch (c.proto) {
      case P_NEC:       irsend.sendNEC(c.code, c.bits);         break;
      case P_SAMSUNG:   irsend.sendSAMSUNG(c.code, c.bits);     break;
      case P_SONY12:    irsend.sendSony(c.code, 12, 2);         break;
      case P_SONY15:    irsend.sendSony(c.code, 15, 2);         break;
      case P_SONY20:    irsend.sendSony(c.code, 20, 2);         break;
      case P_RC5:       irsend.sendRC5(c.code, c.bits);         break;
      case P_RC6:       irsend.sendRC6(c.code, c.bits);         break;
      case P_PANASONIC: irsend.sendPanasonic64(c.code, c.bits); break;
      case P_JVC:       irsend.sendJVC(c.code, c.bits, 1);      break;
    }
    delay(75);
  }
  ledBlink(255, 0, 0, cancel ? 3 : 2);      // rojo: terminado (3 parpadeos si se cancelo)
  M5.Led.setAllColor(0, 0, 0);              // apagar LED
}

// ============================ CONTROL DE MODOS ============================
void changeMode() {
  g_mode = (Mode)((g_mode + 1) % MODE_COUNT);
  prefs.putUChar("mode", (uint8_t)g_mode);
  Serial.printf("-> Modo %s\n", MODE_NAMES[g_mode]);
  if (g_mode == MODE_CARRUSEL) g_lastCarouselSec = rtcNow();
  else setPanelRotation(0);          // el resto de modos en vertical
  if (g_mode == MODE_LIBRO) { g_libState = LIB_LIST; g_sel = g_bookIdx; }  // al entrar: selector
  if (g_mode == MODE_MUSICA) audioPowerOn();   // enciende codec/ampli solo en musica
  else                       audioPowerOff();  // al salir: para audio y apaga codec/ampli (ahorro)
  applyEpdMode();
  g_needRedraw = true;
}
void modeLongPress() {
  if (g_mode == MODE_CLIMA) {
    showBusy("Actualizando (WiFi)...");
    fetchAllOnline(true);
    g_busy = false; g_needRedraw = true;
  } else if (g_mode == MODE_LIBRO) {           // volver al selector de libros
    g_libState = LIB_LIST; g_sel = g_bookIdx; g_needRedraw = true;
  }
}
void modeUp() {
  if (g_mode == MODE_CLIMA) { g_view = (g_view + 1) % numViews(); g_needRedraw = true; }
  else if (g_mode == MODE_CARRUSEL && !g_images.empty()) {
    g_img_idx = (g_img_idx + 1) % g_images.size(); g_lastCarouselSec = rtcNow(); g_needRedraw = true;
  }
}
void modeDown() {
  if (g_mode == MODE_CLIMA) { g_view = (g_view + numViews() - 1) % numViews(); g_needRedraw = true; }
  else if (g_mode == MODE_CARRUSEL && !g_images.empty()) {
    g_img_idx = (g_img_idx + g_images.size() - 1) % g_images.size(); g_lastCarouselSec = rtcNow(); g_needRedraw = true;
  }
}

// ============================ AHORRO DE BATERIA ============================
// Tiempo actual del RTC en segundos (inmune al sleep, a diferencia de millis()).
time_t rtcNow() {
  auto dt = M5.Rtc.getDateTime();
  struct tm t = {};
  t.tm_year = dt.date.year - 1900; t.tm_mon = dt.date.month - 1; t.tm_mday = dt.date.date;
  t.tm_hour = dt.time.hours; t.tm_min = dt.time.minutes; t.tm_sec = dt.time.seconds;
  return mktime(&t);
}

void enterDeepSleep() {
  // Apagado gestionado por el PMIC (M5PM1). Consumo minimo y NO despierta solo:
  // los botones G1/UP/DOWN son GPIO del ESP y su rail de pull-ups se apaga al dormir,
  // asi que el unico despertar fiable es el boton de ENCENDIDO (cuelga del PMIC).
  // El e-paper conserva la ultima imagen en pantalla aunque este apagado.
  Serial.println("Deep sleep por inactividad (apagado PMIC). Pulsa ENCENDIDO para despertar.");
  Serial.flush();
  audioPowerOff();
  WiFi.mode(WIFI_OFF);
  delay(50);
  M5.Power.powerOff();      // no retorna: el PMIC corta la alimentacion
}

void lightSleepFor(uint32_t ms) {
  gpio_wakeup_enable((gpio_num_t)PIN_BTN_MODE, GPIO_INTR_LOW_LEVEL);
  gpio_wakeup_enable((gpio_num_t)PIN_BTN_UP,   GPIO_INTR_LOW_LEVEL);
  gpio_wakeup_enable((gpio_num_t)PIN_BTN_DOWN, GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();
  esp_sleep_enable_timer_wakeup((uint64_t)ms * 1000ULL);
  esp_light_sleep_start();   // despierta por boton o temporizador; el loop continua
  // Si desperto por TEMPORIZADOR (no boton), ignora glitches de pin ~250 ms
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) g_ignoreInputUntil = millis() + 250;
}

// Reposo: light sleep en inactividad (despierta con boton o el proximo refresco); deep sleep a la hora.
void powerTick() {
  uint32_t now = millis();
  // No dormir si hay refresco pendiente/ocupado o estamos en musica (audio activo)
  if (g_needRedraw || g_busy || g_mode == MODE_MUSICA) { delay(5); return; }
  if (now - g_lastInput < IDLE_SLEEP_MS) { delay(10); return; }   // margen para doble-click/mantener

  // Todo medido con el RTC (segundos), inmune a que millis() no avance en el light sleep
  time_t nowSec = rtcNow();
  long idleSec = (long)(nowSec - g_lastActiveSec);
  if (idleSec >= (long)(g_deepSleepMs / 1000)) { enterDeepSleep(); }   // no retorna

  long wakeSec = 600;   // tope 10 min
  if (g_mode == MODE_CLIMA && g_view == 0) {          // proximo refresco del reloj local
    long intervalSec = (idleSec >= 300) ? 300 : 60;
    long dueIn = intervalSec - (long)(nowSec - g_lastLocalUpdateSec);
    if (dueIn < 1) dueIn = 1; if (dueIn < wakeSec) wakeSec = dueIn;
  } else if (g_mode == MODE_CARRUSEL && sd_ready && !g_images.empty()) {  // proxima foto
    long dueIn = (long)(g_carouselMs / 1000) - (long)(nowSec - g_lastCarouselSec);
    if (dueIn < 1) dueIn = 1; if (dueIn < wakeSec) wakeSec = dueIn;
  }
  lightSleepFor((uint32_t)wakeSec * 1000UL);
}

// ============================ SETUP ============================
void setup() {
  auto cfg = M5.config();
  cfg.clear_display = false;
  cfg.internal_spk  = false;   // el codec ES8311 lo gestiona la libreria de audio, no M5.Speaker
  M5.begin(cfg);
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== M5Paper Color - Estacion multimodo ===");

  M5.Display.setEpdMode(epd_mode_t::epd_fast);
  setPanelRotation(0);               // UI en vertical (400x600) + crea el lienzo

  pinMode(PIN_BTN_MODE, INPUT_PULLUP);
  pinMode(PIN_BTN_UP,   INPUT_PULLUP);
  pinMode(PIN_BTN_DOWN, INPUT_PULLUP);
  btnMode.setHoldThresh(800);
  btnUp.setHoldThresh(700);                  // mantener UP/DOWN = play/pausa en musica
  btnDown.setHoldThresh(700);

  showBusy("Iniciando..."); g_busy = false;

  if (pm1.begin(&M5.In_I2C, M5PM1_DEFAULT_ADDR, M5PM1_I2C_FREQ_100K) == M5PM1_OK) {
    pm1.setLdoEnable(true);
    Serial.println("M5PM1 OK, LDO habilitado.");
  } else Serial.println("AVISO: M5PM1 no inicializado.");

  if (!M5.Rtc.isEnabled()) Serial.println("AVISO: RTC no encontrado.");

  sht_ready = initSHT40();
  if (!sht_ready) Serial.println("AVISO: SHT40 no encontrado.");
  readSHT40();

  // SD primero, luego configuracion (de la SD), luego dimensionar caches
  sd_ready = initSD();
  loadDefaults();
  loadConfig();                          // sobreescribe defaults si hay /config.json
  g_clima.assign(numLocs(), ClimaCache{});
  scanImages();                          // lista fotos de la carpeta configurada (/Fotos)
  scanMusic();                           // lista canciones de /Musica
  scanBooks();                           // lista libros .txt de /Libros
  loadFonts();                           // fuentes VLW con acentos (titulo y cuerpo)
  audioInit();                           // codec ES8311 + I2S
  irsend.begin();                        // emisor IR (modo oculto TV-B-Gone)

  // Arranque rapido: NO se bloquea con el WiFi. Se pinta primero la pantalla local
  // (hora del RTC + sensor, instantaneo) y la descarga WiFi/NTP/AEMET se hace en segundo
  // plano tras el primer pintado (g_bootFetchPending, gestionado en loop()).
  g_busy = false;

  prefs.begin("papercolor", false);
  g_mode = (Mode)prefs.getUChar("mode", MODE_CLIMA);
  if (g_mode >= MODE_COUNT) g_mode = MODE_CLIMA;
  applyEpdMode();
  // El selector de libros arranca en el ultimo leido (cada libro carga su pagina al abrirlo)
  int lastBook = prefs.getInt("lastbook", 0);
  if (!g_books.empty()) {
    if (lastBook < 0 || lastBook >= (int)g_books.size()) lastBook = 0;
    g_bookIdx = lastBook; g_sel = lastBook;
  }
  g_libState = LIB_LIST;
  Serial.printf("Modo inicial: %s\n", MODE_NAMES[g_mode]);

  g_lastInput = millis();
  g_lastActiveSec = rtcNow();              // baseline de inactividad (RTC)
  g_lastLocalUpdateSec = rtcNow();
  g_lastCarouselSec = rtcNow();
  g_ignoreInputUntil = millis() + 1000;   // ignora el boton que nos despierta (~1s)
  g_needRedraw = true;
}

// ============================ LOOP ============================
void loop() {
  if (g_audioReady) audio.loop();      // alimenta el decodificador (lo mas a menudo posible)
  M5.update();

  uint32_t ms = millis();
  btnMode.setRawState(ms, digitalRead(PIN_BTN_MODE) == LOW);
  btnUp.setRawState(ms,   digitalRead(PIN_BTN_UP)   == LOW);
  btnDown.setRawState(ms, digitalRead(PIN_BTN_DOWN) == LOW);

  if (ms >= g_ignoreInputUntil) {   // ignora la pulsacion que nos despierta / arranque
  if (btnMode.wasDoubleClicked())      tvBGone();        // doble-click: apaga la TV (IR)
  else if (btnMode.wasHold())          modeLongPress();  // mantener: accion del modo
  else if (btnMode.wasSingleClicked()) changeMode();     // 1 click (confirmado): cambia de modo

  if (g_mode == MODE_MUSICA) {
    // click=volumen +/-, doble-click=cancion sig/ant, mantener=play/pausa
    if (btnUp.wasDoubleClicked())      musicNext();
    else if (btnUp.wasHold())          musicTogglePlay();
    else if (btnUp.wasClicked())       musicVolDown();   // (volumen corregido: estaba al reves)
    if (btnDown.wasDoubleClicked())    musicPrev();
    else if (btnDown.wasHold())        musicTogglePlay();
    else if (btnDown.wasClicked())     musicVolUp();
  } else if (g_mode == MODE_LIBRO) {
    if (g_libState == LIB_LIST) {
      // selector: click simple mueve, doble-click abre. (wasSingleClicked evita mover en el
      // primer click de un doble-click, que rompia la deteccion al redibujar la lista.)
      if (btnUp.wasDoubleClicked() || btnDown.wasDoubleClicked()) bookOpenSelected();
      else if (btnUp.wasSingleClicked())   listMove(+1);   // (UP/DOWN invertido a peticion)
      else if (btnDown.wasSingleClicked()) listMove(-1);
    } else {
      // lectura: UP/DOWN pasa pagina (invertido)
      if (btnUp.wasClicked())   pageNext();
      if (btnDown.wasClicked()) pagePrev();
    }
  } else {
    if (btnUp.wasPressed())   modeUp();      // resto de modos: respuesta inmediata
    if (btnDown.wasPressed()) modeDown();
  }
  }  // fin guardia de pulsaciones iniciales
  // Solo una PULSACION REAL reinicia la inactividad, y no en la ventana de guarda tras despertar
  // (asi un glitch del pin al salir del light sleep no resetea el contador del deep sleep).
  if (ms >= g_ignoreInputUntil &&
      (btnMode.wasPressed() || btnUp.wasPressed() || btnDown.wasPressed())) {
    g_lastInput = ms; g_lastActiveSec = rtcNow();
  }

  // Temporizadores por RTC (millis() no avanza de forma fiable durante el light sleep)
  time_t nowSec = ((g_mode == MODE_CLIMA && g_view == 0) || g_mode == MODE_CARRUSEL) ? rtcNow() : 0;

  // Vista LOCAL: 1 refresco COMPLETO cada minuto; tras 5 min sin tocar boton, cada 5 min (ahorro).
  if (g_mode == MODE_CLIMA && g_view == 0 && !g_busy && !g_needRedraw) {
    long idleSec = (long)(nowSec - g_lastActiveSec);
    long intervalSec = (idleSec >= 300) ? 300 : 60;
    if ((long)(nowSec - g_lastLocalUpdateSec) >= intervalSec) {
      readSHT40();
      g_needRedraw = true;     // drawWeatherLocal actualiza g_lastLocalUpdateSec
    }
  }

  // Carrusel automatico (RTC; infinito por modulo)
  if (g_mode == MODE_CARRUSEL && sd_ready && !g_images.empty()
      && (long)(nowSec - g_lastCarouselSec) >= (long)(g_carouselMs / 1000)) {
    g_lastCarouselSec = nowSec;
    g_img_idx = (g_img_idx + 1) % g_images.size();
    g_needRedraw = true;
  }

  // Fin de pista -> siguiente (lo marca el callback evt_eof de la libreria)
  if (g_trackEnded) { g_trackEnded = false; if (g_playing) musicNext(); }

  if (g_needRedraw && !g_busy) { g_needRedraw = false; drawCurrentMode(); }

  // Arranque rapido: la pantalla local ya esta pintada; ahora baja WiFi+NTP+AEMET UNA vez,
  // en segundo plano. NO se repinta: el local se refresca solo en su ciclo (1/5 min, con la hora
  // ya sincronizada por NTP) y las tarjetas AEMET se ven al navegar a una ciudad.
  if (g_bootFetchPending && !g_needRedraw && !g_busy) {
    g_bootFetchPending = false;
    fetchAllOnline(true);
  }

  // Ahorro de bateria: light sleep en reposo, deep sleep tras 1h. (En musica reproduciendo no duerme.)
  if (g_mode == MODE_MUSICA && g_playing) { /* sin delay: audio fluido */ }
  else powerTick();
}
