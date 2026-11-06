"""Source contract: RTC wall clock (temporarily off), NTP and manual dongle connect.

Register/calendar logic is covered natively in tests/test_rtc8563.cpp.
"""
from pathlib import Path
import unittest

SRC = (Path(__file__).resolve().parents[1] / "firmware/ObdDisplay/ObdDisplay.ino").read_text(encoding="utf-8")


def section(start, end):
    return SRC.split(start, 1)[1].split(end, 1)[0]


class ClockTest(unittest.TestCase):
    def test_ntp_only_in_connected_builds(self):
        self.assertEqual(SRC.count("configTime("), 1)
        self.assertIn("configTime(", section("static void ntpTick()", "#if OBD_TIME_SYNC"))
        self.assertIn("#if OBD_CONNECTED_BUILD\n#include <esp_sntp.h>", SRC)

    def test_vehicle_ntp_only_on_home_wlan_before_dongle(self):
        self.assertIn("#if OBD_CONNECTED_BUILD && !defined(OBD_LAB_BUILD) && defined(TIME_WIFI_SSID)", SRC)
        sync = section("static void startTimeSync()", "static void startDongle()")
        self.assertIn("WiFi.begin(TIME_WIFI_SSID, TIME_WIFI_PASSWORD);", sync)
        end = section("static void endTimeSync(", "static void timeSyncTick()")
        self.assertLess(end.index("esp_sntp_stop();"), end.index("WiFi.disconnect();"))
        tick = section("static void timeSyncTick()", "#endif")
        self.assertIn("ntpTick();", tick)
        button = section("static void onDongleButton(lv_event_t *event) {", "#endif // OBD_CONNECTED_BUILD")
        self.assertLess(button.index("endTimeSync("), button.index("startDongle();"))

    def test_dongle_connects_only_on_request(self):
        self.assertEqual(SRC.count("WiFi.begin(ELM_WIFI_SSID, ELM_WIFI_PASSWORD);"), 1)
        self.assertIn("WiFi.begin(ELM_WIFI_SSID", section("static void startDongle()", "static void stopDongle()"))
        setup = section("void setup() {", "void loop() {")
        vehicle = setup.split("#if defined(OBD_LAB_BUILD)\n  startDongle();", 1)[1]
        vehicle = vehicle.split("#else", 1)[1].split("Serial.println", 1)[0]
        self.assertNotIn("startDongle();", vehicle)
        self.assertIn("if (dongleRequested && !adapterReady &&", section("void loop() {", "delay(5);"))

    def test_rtc_temporarily_disabled(self):
        self.assertIn("#define OBD_RTC_ENABLED 0", SRC)
        setup = section("void setup() {", "void loop() {")
        self.assertIn("#if OBD_RTC_ENABLED\n  clockFromRtc();", setup)

    def test_rtc_is_read_before_sd_session_opens(self):
        setup = section("void setup() {", "void loop() {")
        self.assertLess(setup.index("clockFromRtc();"), setup.index("sdBegin();"))
        self.assertLess(setup.index("resetPanel();"), setup.index("clockFromRtc();"))

    def test_invalid_rtc_never_sets_system_time(self):
        body = section("static void clockFromRtc()", "static void updateClockText()")
        guard = body.index("rtc8563::MIN_VALID_EPOCH")
        self.assertLess(guard, body.index("settimeofday("))

    def test_rows_carry_utc_or_empty(self):
        self.assertIn("clockIso(utc, sizeof utc);", section("static void pollOnePid()", "static void scanOneDtcMode()"))
        mode06 = section("static void scanMisfireStep()", "#endif // OBD_CONNECTED_BUILD")
        self.assertIn("clockIso(utc,sizeof utc);", mode06)
        self.assertIn("r.value,latency,utc)", mode06)
        iso = section("static void clockIso(", "static void clockFromRtc()")
        self.assertIn("ClockSource::None) return;", iso)


if __name__ == "__main__":
    unittest.main()
