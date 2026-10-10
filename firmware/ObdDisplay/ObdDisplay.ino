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
#include "ObdResponseParser.h"
#include "ObdPidCatalog.h"
#include "ObdDiagnostics.h"

#if defined(OBD_LAB_BUILD)
#if __has_include("Config.h")
#error "Lab mode requires Config.h to be absent; keep vehicle configuration separate."
#endif
#include "LabConfig.h"
#define OBD_CONNECTED_BUILD 1
#elif __has_include("Config.h")
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
static constexpr uint32_t POLL_INTERVAL_MS = 350;
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
static lv_obj_t *pages[7], *navButtons[4], *linkText, *statsText;
static lv_obj_t *extraDataText, *engineStateText, *inspectionText, *misfireText;
static lv_obj_t *dtcModeText[3];
static uint32_t tapCount = 0;

#if OBD_CONNECTED_BUILD
static WiFiClient adapter;
static uint32_t lastAttempt = 0, lastPoll = 0;
static bool adapterReady = false, dtcScanRequested = false;
static int rpm = -1, coolantC = -1000, speedKph = -1;
static uint32_t pidMasks[3] = {0, 0, 0};
static obd_pids::Scheduler pollScheduler;
static double pidValues[obd_pids::count] = {};
static bool pidValid[obd_pids::count] = {};
static uint32_t lastExtraRefresh = 0;
static obd_diag::DtcEncoding dtcEncoding = obd_diag::DtcEncoding::Unknown;
static bool inspectionRequested = false, misfireRequested = false;
static uint8_t inspectionStep = 0, misfireStep = 0;
static String inspectionReport, misfireReport;
static uint8_t dtcIndex = 0;
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
  if (page >= 7) return;
  for (uint8_t i = 0; i < 7; ++i) {
    if (i == page) lv_obj_clear_flag(pages[i], LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(pages[i], LV_OBJ_FLAG_HIDDEN);
    if (i >= 4) continue;
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
  inspectionRequested = false; misfireRequested = false;
  dtcScanRequested = true;
  dtcIndex = 0;
  lv_label_set_text(scanText, "DTC-Leseabfrage angefordert");
  for (int i = 0; i < 3; ++i) lv_label_set_text(dtcModeText[i], "Wartet auf Antwort...");
#endif
}

static void onMorePidButton(lv_event_t *event) {
  if (lv_event_get_code(event)==LV_EVENT_CLICKED) showPage(4);
}
static void onInspectionButton(lv_event_t *event) {
  if (lv_event_get_code(event)!=LV_EVENT_CLICKED) return;
  showPage(5);
#if OBD_CONNECTED_BUILD
  if (!adapterReady) {lv_label_set_text(inspectionText,"Keine Verbindung");return;}
#if defined(OBD_LAB_BUILD)
  inspectionReport="HU/AU-Vorabcheck: SIMULIERTE OBD-Daten\n";
#else
  inspectionReport="HU/AU-Vorabcheck: OBD-Lesedaten\n";
#endif
  lv_label_set_text(inspectionText,inspectionReport.c_str());
  inspectionRequested=true; misfireRequested=false; dtcScanRequested=false;
  inspectionStep=0;
#else
  lv_label_set_text(inspectionText,"Offline: keine ECU-Daten");
#endif
}
static void onMode06Button(lv_event_t *event) {
  if (lv_event_get_code(event)!=LV_EVENT_CLICKED) return;
  showPage(6);
#if OBD_CONNECTED_BUILD
  if (!adapterReady) {lv_label_set_text(misfireText,"Keine Verbindung");return;}
#if defined(OBD_LAB_BUILD)
  misfireReport="M272 Aussetzer / Mode 06 (LAB-SIMULATION)\n";
#else
  misfireReport="M272 Aussetzer / Mode 06 (ECU-Lesedaten)\n";
#endif
  lv_label_set_text(misfireText,misfireReport.c_str());
  misfireRequested=true; inspectionRequested=false; dtcScanRequested=false;
  misfireStep=0;
#else
  lv_label_set_text(misfireText,"Offline: keine ECU-Daten");
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
  for (uint8_t i = 0; i < 7; ++i) {
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
  rpmText = label(rpmCard, "-- rpm", 12, 45, 269, LIGHT);
  lv_obj_set_style_text_font(rpmText, &lv_font_montserrat_24, LV_PART_MAIN);
  lv_obj_t *tempCard = makeCard(pages[0], 12, 129, 142, 92);
  label(tempCard, "KUEHLMITTEL", 10, 11, 124, MUTED);
  coolantText = label(tempCard, "-- C", 10, 48, 124, LIGHT);
  lv_obj_set_style_text_font(coolantText, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_t *speedCard = makeCard(pages[0], 166, 129, 142, 92);
  label(speedCard, "TEMPO", 10, 11, 124, MUTED);
  speedText = label(speedCard, "-- km/h", 10, 48, 124, LIGHT);
  lv_obj_set_style_text_font(speedText, &lv_font_montserrat_20, LV_PART_MAIN);
#if defined(OBD_LAB_BUILD)
  label(pages[0], "LAB: Testdaten", 12, 260, 148, MUTED);
#elif OBD_CONNECTED_BUILD
  label(pages[0], "OBD-Livewerte", 12, 260, 148, MUTED);
#else
  label(pages[0], "Offline-Modus", 12, 260, 148, MUTED);
#endif
  lv_obj_t *morePids=makeButton(pages[0],"ALLE PIDS",166,249,142,43);
  lv_obj_add_event_cb(morePids,onMorePidButton,LV_EVENT_CLICKED,nullptr);
  label(pages[0], "Keine Codierung / keine DTC-Loeschung", 12, 295, 291, 0x53D7CF);

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
#if defined(OBD_LAB_BUILD)
  label(pages[1], "LABOR: simulierte Werte / keine ECU-Daten", 12, 303, 290, MUTED);
#elif OBD_CONNECTED_BUILD
  label(pages[1], "Livewerte vom ELM327-Adapter", 12, 303, 290, MUTED);
#else
  label(pages[1], "Offline: keine realen Messwerte", 12, 303, 290, MUTED);
#endif

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
  lv_obj_t *checkButton=makeButton(pages[2],"HU/AU",169,238,139,42);
  lv_obj_add_event_cb(checkButton,onInspectionButton,LV_EVENT_CLICKED,nullptr);
#if !OBD_CONNECTED_BUILD
  lv_obj_add_state(dtcButton, LV_STATE_DISABLED);
#endif
  scanText = label(pages[2], "Noch kein DTC-Scan", 12, 290, 292, MUTED);
  label(pages[2], "CAN/Legacy: begrenzte DTC-Dekodierung", 12, 322, 289, 0xFFCC76);

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

  // Additional pages reached via large buttons, keeping existing four tabs.
  label(pages[4],"SENSOREN / FUEL TRIMS",12,3,165,CYAN);
  lv_obj_t *mode06Btn=makeButton(pages[4],"MODE 06",180,0,128,35);
  lv_obj_add_event_cb(mode06Btn,onMode06Button,LV_EVENT_CLICKED,nullptr);
  lv_obj_t *pidPanel=makeCard(pages[4],12,44,296,250);
  lv_obj_add_flag(pidPanel,LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(pidPanel,LV_DIR_VER);
  extraDataText=label(pidPanel,"PIDs werden erkannt...",10,7,272,LIGHT);
  engineStateText=label(pages[4],"Betriebszustand: noch unbekannt",12,308,295,MUTED);
  label(pages[5],"HU/AU-VORCHECK / OBD",12,3,296,CYAN);
  label(pages[5],"Nur Indikation, keine amtliche Pruefung",12,28,296,MUTED);
  lv_obj_t *insPanel=makeCard(pages[5],12,63,296,276);
  lv_obj_add_flag(insPanel,LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(insPanel,LV_DIR_VER);
  inspectionText=label(insPanel,"HU/AU auf DTC-Seite starten",9,10,273,LIGHT);
  label(pages[6],"MODE 06 / AUSSETZER",12,3,296,CYAN);
  label(pages[6],"M272: Bank 1 Zyl.1-3 | Bank 2 Zyl.4-6",12,27,295,MUTED);
  lv_obj_t *misPanel=makeCard(pages[6],12,62,296,274);
  lv_obj_add_flag(misPanel,LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(misPanel,LV_DIR_VER);
  misfireText=label(misPanel,"Mode 06 ueber ALLE PIDS starten",9,10,274,LIGHT);

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
      if (c == '\r' || c == '\n') answer += '\n';
      else if (c >= 32 && c <= 126) answer += c;
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
  pidMasks[0] = pidMasks[1] = pidMasks[2] = 0;
  pollScheduler.reset();
  for (std::size_t i=0;i<obd_pids::count;++i) pidValid[i]=false;
  rpm = -1; coolantC = -1000; speedKph = -1;
  dtcEncoding = obd_diag::DtcEncoding::Unknown;
  inspectionRequested = false;
  misfireRequested = false;
  if (extraDataText) lv_label_set_text(extraDataText, "Keine Verbindung");
  if (engineStateText) lv_label_set_text(engineStateText, "Betriebszustand: unbekannt");
  lv_label_set_text(statusText, reason);
  lv_label_set_text(linkText, "ELM327: getrennt");
  lv_label_set_text(rpmText, "-- rpm");
  lv_label_set_text(coolantText, "-- C");
  lv_label_set_text(speedText, "-- km/h");
}

static bool pidSupported(uint8_t pid) {
  return obd_pids::supported(pid, pidMasks);
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
#if defined(OBD_LAB_BUILD)
    // Fail closed: no PID/DTC request unless the dedicated Pi simulator
    // positively identified itself. A real ELM327 must not pass this gate.
    if (c == Command::AdapterReset &&
        response.indexOf("ELM327 LAB SIM v0.1") < 0) {
      Serial.println("LAB SAFETY: unexpected adapter identity; no vehicle requests");
      adapter.stop();
      return false;
    }
#endif
  }
  if (!transact(Command::AdapterProtocolId, response, 1700)) return false;
  String proto = response;
  proto.trim();
  if (proto.startsWith("A") && proto.length() == 2) proto.remove(0,1);
  if (proto.length() == 1 && proto[0]>='6' && proto[0]<='9')
    dtcEncoding = obd_diag::DtcEncoding::Can;
  else if (proto.length() == 1 && proto[0]>='1' && proto[0]<='5')
    dtcEncoding = obd_diag::DtcEncoding::Legacy;
  else dtcEncoding = obd_diag::DtcEncoding::Unknown;
  if (!transact(Command::SupportedPids00, response, 3000)) return false;
  if (!obd_response::supportedBitmap(response.c_str(), 0x00, pidMasks[0])) return false;
  pidMasks[1] = pidMasks[2] = 0;
  if (pidMasks[0] & 1) {
    if (transact(Command::SupportedPids20, response, 3000)) {
      (void)obd_response::supportedBitmap(response.c_str(), 0x20, pidMasks[1]);
    } else return false;
    if (pidMasks[1] & 1) {
      if (transact(Command::SupportedPids40, response, 3000)) {
        (void)obd_response::supportedBitmap(response.c_str(), 0x40, pidMasks[2]);
      } else return false;
    }
  }
  pollScheduler.reset();
  for (std::size_t i=0;i<obd_pids::count;++i) pidValid[i]=false;
  adapterReady = true;
#if defined(OBD_LAB_BUILD)
  lv_label_set_text(statusText, "LAB: Pi-Simulator verbunden");
  lv_label_set_text(linkText, "LAB-Simulator: ID bestaetigt");
#else
  lv_label_set_text(statusText, "ELM327 verbunden | Mode 01 aktiv");
  lv_label_set_text(linkText, "WLAN / ELM327: verbunden");
#endif
  Serial.printf("OBD: PIDs 00=%08lX 20=%08lX 40=%08lX CAN=%d\n",
       (unsigned long)pidMasks[0],(unsigned long)pidMasks[1],
       (unsigned long)pidMasks[2],dtcEncoding==obd_diag::DtcEncoding::Can);
#if defined(OBD_LAB_BUILD)
  Serial.println("CSV_SOURCE,LAB_SIMULATED");
#else
  Serial.println("CSV_SOURCE,ECU_ADAPTER_UNVALIDATED");
#endif
  Serial.println("CSV,millis,pid,label,value,unit,latency_ms");
  return true;
}

static void updateExtraData() {
  if (!extraDataText) return;
  String summary;
  summary.reserve(1024);
  for (std::size_t i=0;i<obd_pids::count;++i) {
    const auto &p=obd_pids::catalog[i];
    summary += p.title;
    summary += ": ";
    if (!pidSupported(p.pid)) summary += "n/v";
    else if (!pidValid[i]) summary += "--";
    else summary += String(pidValues[i], static_cast<unsigned int>(p.decimals));
    if (pidValid[i]) {
      summary += " ";
      summary += p.unit;
    }
    summary += "\n";
  }
  lv_label_set_text(extraDataText, summary.c_str());
}
static void pollOnePid() {
  const int index = pollScheduler.choose(millis(), pidMasks);
  if (index < 0) return;
  const auto &p = obd_pids::catalog[index];
  String response;
  const uint32_t started=millis();
  if (!transact(p.command, response)) {
    markDisconnected("ELM327: Timeout/Sync-Verlust"); return;
  }
  uint8_t bytes[2];
  if (!obd_response::readPid(response.c_str(), p.pid, bytes,
                             obd_pids::bytesNeeded(p.formula))) {
    ++inputErrorCount;
    return; // no substitute zero or stale value
  }
  double value;
  if (!obd_pids::decode(p, bytes, obd_pids::bytesNeeded(p.formula), value))
    return;
  pidValues[index]=value;
  pidValid[index]=true;
  if (p.pid==0x0C) {
    rpm=(int)value;
    lv_label_set_text_fmt(rpmText, "%d rpm", rpm);
    lv_chart_set_next_value(rpmChart,rpmSeries,rpm);
    if (engineStateText)
      lv_label_set_text(engineStateText,
        rpm==0 ? "KOEO: Zuendung EIN / Motor AUS; Trimwerte bedingt aussagekraeftig"
               : "KOER: Motor laeuft");
  } else if (p.pid==0x05) {
    coolantC=(int)value;
    lv_label_set_text_fmt(coolantText, "%d C", coolantC);
  } else if (p.pid==0x0D) {
    speedKph=(int)value;
    lv_label_set_text_fmt(speedText, "%d km/h", speedKph);
  }
  Serial.printf("CSV,%lu,01%02X,%s,%.*f,%s,%lu\n",
      (unsigned long)millis(),p.pid,p.title,p.decimals,value,p.unit,
      (unsigned long)(millis()-started));
  if (millis()-lastExtraRefresh >= 500) {
    updateExtraData(); lastExtraRefresh=millis();
  }
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
  Serial.printf("DTC_RAW,%s,%s\n", names[dtcIndex], answer.c_str());
  const uint8_t responseMode[] = {0x43,0x47,0x4A};
  auto dtcs=obd_diag::parseDtcs(answer.c_str(),responseMode[dtcIndex],dtcEncoding);
  if (!dtcs.gotAnswer || dtcs.malformed) {
    String preview=answer.substring(0,30);
    lv_label_set_text_fmt(dtcModeText[dtcIndex],"%s: RAW %s",
                          names[dtcIndex],preview.c_str());
    lv_label_set_text(scanText,"DTC-Format nicht sicher dekodierbar");
  } else if (dtcs.count==0) {
    lv_label_set_text_fmt(dtcModeText[dtcIndex],"%s: keine",names[dtcIndex]);
    lv_label_set_text(scanText,"Leseabfrage abgeschlossen");
  } else {
    lv_label_set_text_fmt(dtcModeText[dtcIndex],"%s: %s (+%u)",
        names[dtcIndex],dtcs.entries[0].code,(unsigned)(dtcs.count-1));
    const char *hint=obd_diag::m272Hint(dtcs.entries[0].code);
    if (hint) lv_label_set_text_fmt(scanText,"M272-Hinweis: %s",hint);
    else lv_label_set_text(scanText,"Codes siehe seriellem Rohprotokoll");
    for (std::size_t i=0;i<dtcs.count;++i)
      Serial.printf("DTC,%s,%s\n",names[dtcIndex],dtcs.entries[i].code);
  }
  ++dtcIndex;
  if (dtcIndex == 3) dtcScanRequested = false;
}

static void scanInspectionStep() {
  static const Command commands[] = {
      Command::Readiness,Command::MilDistance,Command::Warmups,
      Command::ClearDistance,Command::FreezeDtc,Command::StoredDtcs,
      Command::PendingDtcs,Command::PermanentDtcs};
  if (inspectionStep>=8) {inspectionRequested=false;return;}
  const Command c=commands[inspectionStep];
  String response;
  if (!transact(c,response,3500)) {
    markDisconnected("HU/AU: Adapter-Synchronisation verloren");
    inspectionRequested=false; return;
  }
  uint8_t data[2] = {};
  if (inspectionStep==0) {
    obd_diag::Readiness readiness;
    if (obd_diag::parseReadiness(response.c_str(),readiness)) {
      inspectionReport += "MIL: ";
      inspectionReport += readiness.mil ? "AN\n" : "AUS\n";
      inspectionReport += "ECU-DTC-Anzahl: "+String(readiness.dtcs)+"\n";
      inspectionReport += "ECUs: "+String(readiness.ecus)+"\n";
      inspectionReport += "Monitore offen: "+String(readiness.incompleteCount())+"\n";
      for (int i=0;i<11;++i) {
        const bool supported=i<3 ?
            (readiness.supportedContinuous & (1u<<i)) :
            (readiness.supportedOther & (1u<<(i-3)));
        if (!supported) continue;
        const bool incomplete=i<3 ?
            (readiness.openContinuous & (1u<<i)) :
            (readiness.openOther & (1u<<(i-3)));
        inspectionReport += String(obd_diag::monitorName(i,readiness.compression));
        inspectionReport += incomplete ? ": offen\n" : ": fertig\n";
      }
      if (readiness.mil || readiness.dtcs>0 || readiness.incompleteCount()>0)
        inspectionReport += "OBD-Monitore: auffaellig\n";
      else inspectionReport += "OBD-Monitore: derzeit abgeschlossen\n";
    } else inspectionReport += "Readiness: nicht verfuegbar\n";
  } else if (inspectionStep>=1 && inspectionStep<=3) {
    const uint8_t pids[]={0x21,0x30,0x31};
    const char *labels[]={"km mit MIL","Warmlaeufe nach Reset","km nach Reset"};
    uint8_t pid=pids[inspectionStep-1];
    const int needed=pid==0x30?1:2;
    if (obd_response::readPid(response.c_str(),pid,data,needed)) {
      int v=needed==1 ? data[0] : (data[0]<<8)|data[1];
      inspectionReport+=String(labels[inspectionStep-1])+": "+String(v)+"\n";
    } else inspectionReport+=String(labels[inspectionStep-1])+": n/v\n";
  } else if (inspectionStep==4) {
    if (obd_response::readFreezeFrame00(response.c_str(),0x02,data,2)) {
      if (data[0]!=0 || data[1]!=0) {
        char code[6];obd_diag::decodeDtc(data[0],data[1],code);
        inspectionReport+="Freeze-Frame DTC: "+String(code)+"\n";
      } else inspectionReport+="Freeze-Frame DTC: keiner\n";
    } else inspectionReport+="Freeze-Frame DTC: n/v\n";
  } else {
    const char *labels[]={"Gespeichert","Pending","Permanent"};
    const uint8_t modes[]={0x43,0x47,0x4A};
    const uint8_t i=inspectionStep-5;
    auto dtcs=obd_diag::parseDtcs(response.c_str(),modes[i],dtcEncoding);
    if(!dtcs.gotAnswer || dtcs.malformed)
      inspectionReport+=String(labels[i])+": nicht sicher lesbar\n";
    else {
      inspectionReport+=String(labels[i])+": "+String(dtcs.count)+"\n";
      for (std::size_t c=0;c<dtcs.count && c<8;++c)
        inspectionReport+="  "+String(dtcs.entries[c].code)+"\n";
    }
  }
  lv_label_set_text(inspectionText,inspectionReport.c_str());
  ++inspectionStep;
  if (inspectionStep>=8) {
    inspectionReport+="OBD-Vorabcheck beendet. Keine amtliche HU/AU-Freigabe.\n";
    lv_label_set_text(inspectionText,inspectionReport.c_str());
    inspectionRequested=false;
  }
}

static void scanMisfireStep() {
  if(misfireStep>=6){misfireRequested=false;return;}
  static const Command modes[]={
      Command::MisfireCyl1,Command::MisfireCyl2,Command::MisfireCyl3,
      Command::MisfireCyl4,Command::MisfireCyl5,Command::MisfireCyl6};
  String reply;
  if(!transact(modes[misfireStep],reply,3500)){
    markDisconnected("Mode06: Adapter-Synchronisation verloren");
    misfireRequested=false;return;
  }
  obd_diag::Misfire m;
  const uint8_t cyl=misfireStep+1;
  misfireReport+="Zyl. "+String(cyl)+" (B"+String(cyl<=3?1:2)+"): ";
  if(obd_diag::parseMisfire(reply.c_str(),cyl,m)) {
    misfireReport+="EWMA "+String(m.hasEwma ? String(m.ewma) : String("--"));
    misfireReport+=" / Zyklus "+String(m.hasCurrent ? String(m.current) : String("--"));
  } else misfireReport+="nicht verfuegbar (UASID24)";
  misfireReport+="\n";
  lv_label_set_text(misfireText,misfireReport.c_str());
  ++misfireStep;
  if(misfireStep>=6){
    misfireReport+="Nur SAE Mode06. Keine Stellgliedpruefung.\n";
    lv_label_set_text(misfireText,misfireReport.c_str());
    misfireRequested=false;
  }
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
#if defined(OBD_LAB_BUILD)
  lv_label_set_text(statusText, "LABOR: Pi-Simulator / kein Fahrzeug");
  lv_label_set_text(linkText, "Labor-WLAN: verbindet...");
  Serial.println("OBD DIAG: LAB BUILD; SIMULATOR IDENTITY REQUIRED");
#else
  lv_label_set_text(statusText, "WLAN zum ELM327 startet...");
  lv_label_set_text(linkText, "WLAN: verbindet...");
  Serial.println("OBD DIAG: CONNECTED BUILD; READ-ONLY COMMAND POLICY");
#endif
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
    if (inspectionRequested) scanInspectionStep();
    else if (misfireRequested) scanMisfireStep();
    else if (dtcScanRequested) scanOneDtcMode();
    else pollOnePid();
  }
#endif
  delay(5);
}
