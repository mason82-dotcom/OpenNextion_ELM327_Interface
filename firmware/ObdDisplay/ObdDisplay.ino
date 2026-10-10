/* OpenNextion OBD Display v0.1; NO ECU write, clear or actuator commands.
 * Default build: offline LCD+touch demo; only local Config.h enables WiFi.
 */
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <WiFi.h>
#include <Arduino_GFX_Library.h>
#include <Adafruit_CST8XX.h>
#include <Adafruit_PCF8574.h>
#define LV_CONF_SKIP
#include <lvgl.h>
#include "BoardConfig.h"
#include "ReadOnlyPolicy.h"

#if __has_include("Config.h")
#include "Config.h"
#define OBD_CONNECTED_BUILD 1
#else
#define OBD_CONNECTED_BUILD 0
#endif

#if OBD_CONNECTED_BUILD
#if !defined(ELM_WIFI_SSID) || !defined(ELM_WIFI_PASSWORD) || !defined(ELM_HOST) || !defined(ELM_PORT)
#error "Config.h must define all ELM_* settings."
#endif
#endif

using obd_policy::Command;
static constexpr uint32_t LCD_SPI_HZ = 80000000;
static constexpr uint32_t WIFI_RETRY_MS = 12000;
static constexpr uint32_t POLL_INTERVAL_MS = 500;
static constexpr uint16_t BUF_LINES = 40;
static constexpr uint32_t CMD_TIMEOUT_MS = 1700;

Arduino_DataBus *bus = new Arduino_HWSPI(
    LCD_DC_PIN, LCD_CS_PIN, LCD_SCLK_PIN, LCD_MOSI_PIN, LCD_MISO_PIN, &SPI, true);
Arduino_GFX *gfx = new Arduino_ST7796(
    bus, GFX_NOT_DEFINED, 0, false, LCD_H_RES, LCD_V_RES);
Adafruit_CST8XX touch;
Adafruit_PCF8574 expander;
static lv_color_t displayPixels[LCD_H_RES * BUF_LINES];
static lv_disp_draw_buf_t drawBuffer;
static lv_disp_drv_t dispDriver;
static lv_obj_t *statusText, *rpmText, *coolantText, *speedText, *scanText, *touchText;
static lv_obj_t *rpmChart;
static lv_chart_series_t *rpmSeries;
static lv_obj_t *pages[4], *navButtons[4], *linkText, *statsText;
static lv_obj_t *dtcModeText[3];
static uint32_t tapCount = 0;

#if OBD_CONNECTED_BUILD
static WiFiClient adapter;
static uint32_t lastAttempt = 0, lastPoll = 0;
static bool adapterReady = false, dtcScanRequested = false;
static int rpm = -1, coolantC = -1000, speedKph = -1;
static uint32_t pidMask = 0;
static uint8_t pollIndex = 0, dtcIndex = 0;
static uint32_t txCount = 0, timeoutCount = 0, inputErrorCount = 0;
#endif

static void resetPanel() {
  if (!expander.begin(PCF8574_ADDR, &Wire)) {
    Serial.println("WARN: LCD reset expander missing");
    return;
  }
  expander.digitalWriteByte(0xFF);
  delay(20);
  expander.digitalWriteByte((uint8_t)(0xFF & ~(1U << PCF8574_PIN_LCD_RST)));
  delay(120);
  expander.digitalWriteByte(0xFF);
  delay(120);
}

static void displayFlush(lv_disp_drv_t *driver, const lv_area_t *area, lv_color_t *pixels) {
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;
#if LV_COLOR_16_SWAP != 0
  gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)&pixels->full, w, h);
#else
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&pixels->full, w, h);
#endif
  lv_disp_flush_ready(driver);
}

static void touchRead(lv_indev_drv_t*, lv_indev_data_t *data) {
  static int16_t x = 0, y = 0;
  if (touch.touched()) {
    auto p = touch.getPoint();
    x = constrain(p.x, 0, (int16_t)gfx->width() - 1);
    y = constrain(p.y, 0, (int16_t)gfx->height() - 1);
    data->state = LV_INDEV_STATE_PR;
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
  data->point.x = x;
  data->point.y = y;
}

// Portrait layout: header 0..73, pages 74..423, navigation 432..472.
static constexpr uint32_t BG = 0x0D1523, CARD = 0x152338, BORDER = 0x344A66;
static constexpr uint32_t CYAN = 0x70C8FF, LIGHT = 0xECF4FF, MUTED = 0xA1B8D2;

static lv_obj_t* label(lv_obj_t *parent, const char *initial, int x, int y,
                       int width, uint32_t color) {
  lv_obj_t *obj = lv_label_create(parent);
  lv_obj_set_pos(obj, x, y);
  lv_obj_set_width(obj, width);
  lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
  lv_label_set_long_mode(obj, LV_LABEL_LONG_WRAP);
  lv_label_set_text(obj, initial);
  return obj;
}

static lv_obj_t* makeCard(lv_obj_t *parent, int x, int y, int w, int h) {
  lv_obj_t *box = lv_obj_create(parent);
  lv_obj_set_pos(box, x, y);
  lv_obj_set_size(box, w, h);
  lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(box, lv_color_hex(CARD), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(box, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(box, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(box, lv_color_hex(BORDER), LV_PART_MAIN);
  lv_obj_set_style_radius(box, 8, LV_PART_MAIN);
  lv_obj_set_style_pad_all(box, 0, LV_PART_MAIN);
  return box;
}

static lv_obj_t* makeButton(lv_obj_t *parent, const char *title, int x, int y,
                            int w, int h) {
  lv_obj_t *btn = lv_btn_create(parent);
  lv_obj_set_pos(btn, x, y);
  lv_obj_set_size(btn, w, h);
  lv_obj_set_style_radius(btn, 6, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_color(btn, lv_color_hex(0x225889), LV_PART_MAIN);
  lv_obj_t *txt = lv_label_create(btn);
  lv_label_set_text(txt, title);
  lv_obj_set_style_text_color(txt, lv_color_hex(LIGHT), 0);
  lv_obj_center(txt);
  return btn;
}

static void showPage(uint8_t page) {
  if (page >= 4) return;
  for (uint8_t i = 0; i < 4; ++i) {
    if (i == page) lv_obj_clear_flag(pages[i], LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(pages[i], LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(navButtons[i],
      lv_color_hex(i == page ? 0x225889 : 0x17283D), LV_PART_MAIN);
    lv_obj_set_style_border_width(navButtons[i], i == page ? 1 : 0, LV_PART_MAIN);
    lv_obj_set_style_border_color(navButtons[i], lv_color_hex(CYAN), LV_PART_MAIN);
  }
}

static void navClicked(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
  showPage((uint8_t)(uintptr_t)lv_event_get_user_data(event));
}

static void onTouchButton(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
  ++tapCount;
  lv_label_set_text_fmt(touchText, "Touch: %lu", (unsigned long)tapCount);
}

static void onScanButton(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
#if OBD_CONNECTED_BUILD
  if (!adapterReady) {
    lv_label_set_text(scanText, "ELM327 nicht verbunden");
    return;
  }
  dtcScanRequested = true;
  dtcIndex = 0;
  lv_label_set_text(scanText, "DTC-Leseabfrage angefordert");
  for (int i = 0; i < 3; ++i) lv_label_set_text(dtcModeText[i], "Wartet auf Antwort...");
#endif
}

static void buildUi() {
  lv_obj_t *screen = lv_scr_act();
  lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(screen, lv_color_hex(BG), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_pad_all(screen, 0, LV_PART_MAIN);

  label(screen, "OBD DIAG  /  READ-ONLY", 11, 11, 295, CYAN);
  statusText = label(screen, "Initialisierung...", 11, 38, 296, MUTED);
  for (uint8_t i = 0; i < 4; ++i) {
    pages[i] = lv_obj_create(screen);
    lv_obj_set_pos(pages[i], 0, 74);
    lv_obj_set_size(pages[i], 320, 349);
    lv_obj_clear_flag(pages[i], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(pages[i], LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(pages[i], 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(pages[i], 0, LV_PART_MAIN);
  }
  // LIVE: large, distinct readouts.
  label(pages[0], "LIVEWERTE", 12, 3, 290, CYAN);
  lv_obj_t *rpmCard = makeCard(pages[0], 12, 29, 296, 89);
  label(rpmCard, "MOTORDREHZAHL", 12, 12, 269, MUTED);
  rpmText = label(rpmCard, "-- rpm", 12, 48, 269, LIGHT);
  lv_obj_t *tempCard = makeCard(pages[0], 12, 129, 142, 92);
  label(tempCard, "KUEHLMITTEL", 10, 11, 124, MUTED);
  coolantText = label(tempCard, "-- C", 10, 51, 124, LIGHT);
  lv_obj_t *speedCard = makeCard(pages[0], 166, 129, 142, 92);
  label(speedCard, "TEMPO", 10, 11, 124, MUTED);
  speedText = label(speedCard, "-- km/h", 10, 51, 124, LIGHT);
  label(pages[0], "Werte nur bei ECU-Unterstuetzung", 12, 254, 290, MUTED);
  label(pages[0], "Keine Codierung / keine DTC-Loeschung", 12, 285, 291, 0x53D7CF);

  // GRAPH: explicit dark LVGL chart styling (no white plot rectangle).
  label(pages[1], "DREHZAHLVERLAUF", 12, 3, 292, CYAN);
  label(pages[1], "0 bis 7000 rpm / letzte 48 Werte", 12, 30, 292, MUTED);
  rpmChart = lv_chart_create(pages[1]);
  lv_obj_set_pos(rpmChart, 12, 69);
  lv_obj_set_size(rpmChart, 296, 208);
  lv_obj_set_style_bg_color(rpmChart, lv_color_hex(CARD), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(rpmChart, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(rpmChart, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(rpmChart, lv_color_hex(BORDER), LV_PART_MAIN);
  lv_obj_set_style_line_color(rpmChart, lv_color_hex(0x2E425D), LV_PART_MAIN);
  lv_obj_set_style_line_width(rpmChart, 1, LV_PART_MAIN);
  lv_obj_set_style_radius(rpmChart, 8, LV_PART_MAIN);
  lv_chart_set_type(rpmChart, LV_CHART_TYPE_LINE);
  lv_chart_set_div_line_count(rpmChart, 7, 5);
  lv_chart_set_point_count(rpmChart, 48);
  lv_chart_set_range(rpmChart, LV_CHART_AXIS_PRIMARY_Y, 0, 7000);
  lv_chart_set_update_mode(rpmChart, LV_CHART_UPDATE_MODE_SHIFT);
  lv_obj_set_style_size(rpmChart, 0, LV_PART_INDICATOR);
  rpmSeries = lv_chart_add_series(rpmChart, lv_color_hex(0x53D7CF), LV_CHART_AXIS_PRIMARY_Y);
  for (int i = 0; i < 48; ++i) lv_chart_set_next_value(rpmChart, rpmSeries, 0);
  label(pages[1], "Offline: keine realen Messwerte", 12, 303, 290, MUTED);

  // DTC: not sent on page switch; explicitly requested with button.
  label(pages[2], "FEHLERCODES / ROHDATEN", 12, 3, 291, CYAN);
  label(pages[2], "03 / 07 / 0A  -  kein Loeschen", 12, 28, 292, 0x53D7CF);
  lv_obj_t *dtcCard = makeCard(pages[2], 12, 62, 296, 161);
  const char *dtcCaptions[] = {
      "03 gespeichert: --", "07 ausstehend: --", "0A permanent: --"};
  for (int i = 0; i < 3; ++i)
    dtcModeText[i] = label(dtcCard, dtcCaptions[i], 10, 15 + i*49, 273, LIGHT);
  lv_obj_t *dtcButton = makeButton(pages[2],
#if OBD_CONNECTED_BUILD
      "DTC lesen",
#else
      "DTC gesperrt",
#endif
      12, 238, 146, 42);
  lv_obj_add_event_cb(dtcButton, onScanButton, LV_EVENT_CLICKED, nullptr);
#if !OBD_CONNECTED_BUILD
  lv_obj_add_state(dtcButton, LV_STATE_DISABLED);
#endif
  scanText = label(pages[2], "Noch kein DTC-Scan", 12, 290, 292, MUTED);
  label(pages[2], "Rohdaten: keine Dekodierung", 12, 322, 289, 0xFFCC76);

  // INFO: link diagnostics and local touch counter.
  label(pages[3], "SYSTEM / VERBINDUNG", 12, 3, 290, CYAN);
  lv_obj_t *sysCard = makeCard(pages[3], 12, 32, 296, 172);
  linkText = label(sysCard, "WLAN / OBD: --", 10, 12, 274, LIGHT);
  statsText = label(sysCard, "Keine Diagnosedaten", 10, 62, 274, MUTED);
  label(sysCard, "Feste Read-only-Befehlsfreigabe", 10, 126, 274, 0x53D7CF);
  lv_obj_t *touchBtn = makeButton(pages[3], "Touch testen", 12, 230, 146, 43);
  lv_obj_add_event_cb(touchBtn, onTouchButton, LV_EVENT_CLICKED, nullptr);
  touchText = label(pages[3], "Touch: 0", 175, 243, 132, LIGHT);
  label(pages[3], "Keine Schreib-, Stellglied- oder Loeschfunktion", 12, 307, 290, MUTED);

  const char *tabs[4] = {"LIVE", "GRAPH", "DTC", "INFO"};
  for (uint8_t i = 0; i < 4; ++i) {
    navButtons[i] = makeButton(screen, tabs[i], 5+i*79, 431, 74, 41);
    lv_obj_add_event_cb(navButtons[i], navClicked,
                        LV_EVENT_CLICKED, (void *)(uintptr_t)i);
  }
  showPage(0);
}

static void uiTick() {
  static uint32_t lastTick = millis();
  uint32_t now = millis();
  lv_tick_inc(now - lastTick);
  lastTick = now;
  lv_timer_handler();
}

#if OBD_CONNECTED_BUILD
// Sole TCP transmit path. A raw string cannot reach the adapter.
static bool transact(Command command, String &answer, uint32_t timeoutMs = CMD_TIMEOUT_MS) {
  const char *wire = obd_policy::wireCommand(command);
  if (!adapter.connected() || wire == nullptr) return false;
  adapter.print(wire);
  adapter.write('\r');
  ++txCount;
  lv_label_set_text_fmt(statsText, "Gesendete Befehle: %lu\nTimeouts: %lu  Pufferfehler: %lu",
      (unsigned long)txCount, (unsigned long)timeoutCount,
      (unsigned long)inputErrorCount);
  answer = "";
  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    uiTick();
    while (adapter.available()) {
      char c = (char)adapter.read();
      if (c == '>') return true;
      if (answer.length() >= 1024) {
        ++inputErrorCount;
        adapter.stop();
        return false;
      }
      if (c >= 32 && c <= 126) answer += c;
    }
    if (!adapter.connected()) break;
    delay(2);
  }
  // Missing prompt = unsynchronized stream. NEVER send next command on it.
  ++timeoutCount;
  adapter.stop();
  return false;
}

static void markDisconnected(const char *reason) {
  adapterReady = false;
  pidMask = 0;
  rpm = -1; coolantC = -1000; speedKph = -1;
  lv_label_set_text(statusText, reason);
  lv_label_set_text(linkText, "ELM327: getrennt");
  lv_label_set_text(rpmText, "Drehzahl: -- rpm");
  lv_label_set_text(coolantText, "Kuehlmittel: -- C");
  lv_label_set_text(speedText, "Geschwindigkeit: -- km/h");
}

// Find strictly matching Mode 01/PID response within compact hex bytes.
// Tolerates ELM spaces/line breaks, but rejects malformed/incomplete payloads.
static bool pidBytes(const String &response, uint8_t pid, uint8_t *bytes, int count) {
  String compact;
  compact.reserve(response.length());
  for (unsigned i=0; i<response.length(); ++i) {
    char c = response[i];
    if (c >= 'a' && c <= 'f') c -= 32;
    if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F')) compact += c;
  }
  char signature[5];
  snprintf(signature, sizeof(signature), "41%02X", pid);
  int pos = compact.indexOf(signature);
  if (pos < 0 || (unsigned)(pos + 4 + count*2) > compact.length()) return false;
  for (int i=0; i<count; ++i) {
    auto nibble = [](char h)->uint8_t {
      return (h >= 'A') ? (uint8_t)(h - 'A' + 10) : (uint8_t)(h - '0');
    };
    char a = compact[pos + 4 + i*2], b = compact[pos + 5 + i*2];
    if (!isxdigit((unsigned char)a) || !isxdigit((unsigned char)b)) return false;
    bytes[i] = (uint8_t)((nibble(a) << 4) | nibble(b));
  }
  return true;
}

static bool pidSupported(uint8_t pid) {
  if (pid < 1 || pid > 0x20) return false;
  return (pidMask & (1UL << (0x20 - pid))) != 0;
}

static bool initAdapter() {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (!adapter.connect(ELM_HOST, ELM_PORT, 1800)) return false;
  const Command setup[] = {
      Command::AdapterReset, Command::AdapterEchoOff, Command::AdapterLinesOff,
      Command::AdapterSpacesOff, Command::AdapterHeadersOff, Command::AdapterAutoProtocol};
  String response;
  for (Command c : setup) {
    if (!transact(c, response, c == Command::AdapterReset ? 3500 : 1700)) return false;
    if (response.indexOf('?') >= 0 || response.indexOf("ERROR") >= 0) return false;
  }
  if (!transact(Command::SupportedPids00, response, 3000)) return false;
  uint8_t mask[4];
  if (!pidBytes(response, 0x00, mask, 4)) return false;
  pidMask = ((uint32_t)mask[0] << 24) | ((uint32_t)mask[1] << 16)
          | ((uint32_t)mask[2] << 8) | mask[3];
  adapterReady = true;
  lv_label_set_text(statusText, "ELM327 verbunden | Mode 01 aktiv");
  lv_label_set_text(linkText, "WLAN / ELM327: verbunden");
  Serial.printf("OBD: Supported PIDs bitmap=0x%08lX\n", (unsigned long)pidMask);
  return true;
}

static void pollOnePid() {
  const Command commands[] = {Command::Rpm, Command::Coolant, Command::Speed};
  const uint8_t pids[] = {0x0C, 0x05, 0x0D};
  uint8_t idx = pollIndex++ % 3;
  if (!pidSupported(pids[idx])) return;
  String response;
  if (!transact(commands[idx], response)) { markDisconnected("ELM327: Timeout/Sync-Verlust"); return; }
  uint8_t b[2];
  if (!pidBytes(response, pids[idx], b, idx == 0 ? 2 : 1)) return;
  if (idx == 0) {
    rpm = (256*(int)b[0] + b[1])/4;
    lv_label_set_text_fmt(rpmText, "Drehzahl: %d rpm", rpm);
    lv_chart_set_next_value(rpmChart, rpmSeries, rpm);
  } else if (idx == 1) {
    coolantC = (int)b[0]-40;
    lv_label_set_text_fmt(coolantText, "Kuehlmittel: %d C", coolantC);
  } else {
    speedKph = b[0];
    lv_label_set_text_fmt(speedText, "Geschwindigkeit: %d km/h", speedKph);
  }
  Serial.printf("SAMPLE,%lu,%d,%d,%d\n", (unsigned long)millis(), rpm, coolantC, speedKph);
}

static void scanOneDtcMode() {
  const Command modes[] = {Command::StoredDtcs, Command::PendingDtcs, Command::PermanentDtcs};
  const char *names[] = {"03 gespeichert", "07 ausstehend", "0A permanent"};
  if (dtcIndex >= 3) { dtcScanRequested = false; return; }
  String answer;
  if (!transact(modes[dtcIndex], answer, 3500)) {
    markDisconnected("DTC: Timeout/Sync-Verlust");
    dtcScanRequested = false; return;
  }
  // Raw result, deliberately not parsed as trustworthy ECU error codes yet.
  Serial.printf("DTC_RAW,%s,%s\n", names[dtcIndex], answer.c_str());
  String preview = answer.substring(0, 38);
  lv_label_set_text_fmt(dtcModeText[dtcIndex], "%s: %s", names[dtcIndex], preview.c_str());
  lv_label_set_text(scanText, "Leseabfrage abgeschlossen / Rohdaten");
  ++dtcIndex;
  if (dtcIndex == 3) dtcScanRequested = false;
}
#endif // OBD_CONNECTED_BUILD

void setup() {
  Serial.begin(115200);
  Wire.begin(NEXTION_IIC_SDA_PIN, NEXTION_IIC_SCL_PIN, 400000);
  resetPanel();
  gfx->begin(LCD_SPI_HZ);
  gfx->setRotation(0);
  gfx->fillScreen(RGB565_BLACK);
  pinMode(LCD_BL_PIN, OUTPUT);
  digitalWrite(LCD_BL_PIN, HIGH);
  if (!touch.begin(&Wire, CST826_ADDR)) Serial.println("WARN: Touch not initialized");
  lv_init();
  lv_disp_draw_buf_init(&drawBuffer, displayPixels, nullptr, LCD_H_RES*BUF_LINES);
  lv_disp_drv_init(&dispDriver);
  dispDriver.hor_res = gfx->width();
  dispDriver.ver_res = gfx->height();
  dispDriver.draw_buf = &drawBuffer;
  dispDriver.flush_cb = displayFlush;
  lv_disp_drv_register(&dispDriver);
  static lv_indev_drv_t indev;
  lv_indev_drv_init(&indev);
  indev.type = LV_INDEV_TYPE_POINTER;
  indev.read_cb = touchRead;
  lv_indev_drv_register(&indev);
  buildUi();
#if OBD_CONNECTED_BUILD
  WiFi.mode(WIFI_STA);
  WiFi.begin(ELM_WIFI_SSID, ELM_WIFI_PASSWORD);
  lastAttempt = millis();
  lv_label_set_text(statusText, "WLAN zum ELM327 startet...");
  lv_label_set_text(linkText, "WLAN: verbindet...");
  Serial.println("OBD DIAG: CONNECTED BUILD; READ-ONLY COMMAND POLICY");
#else
  lv_label_set_text(statusText, "OFFLINE-TEST | WLAN und OBD AUS");
  lv_label_set_text(linkText, "WLAN / OBD: ausgeschaltet");
  lv_label_set_text(statsText, "Offline: kein WLAN, keine ECU-Kommandos");
  Serial.println("OBD DIAG: OFFLINE DEMO; NO WIFI, NO VEHICLE COMMANDS");
#endif
}

void loop() {
  uiTick();
#if OBD_CONNECTED_BUILD
  uint32_t now = millis();
  if (!adapterReady && (now - lastAttempt >= WIFI_RETRY_MS || lastAttempt == 0)) {
    lastAttempt = now;
    adapter.stop();
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.reconnect();
      lv_label_set_text(statusText, "WLAN getrennt / Reconnect");
      lv_label_set_text(linkText, "WLAN: getrennt");
    } else if (!initAdapter()) {
      adapter.stop();
      markDisconnected("ELM327/ECU nicht erreichbar");
    }
  }
  if (adapterReady && now - lastPoll >= POLL_INTERVAL_MS) {
    lastPoll = now;
    if (dtcScanRequested) scanOneDtcMode();
    else pollOnePid();
  }
#endif
  delay(5);
}
