"""Source contract: RTC wall clock and LAB-only NTP.

Register/calendar logic is covered natively in tests/test_rtc8563.cpp.
"""
from pathlib import Path
import unittest

SRC = (Path(__file__).resolve().parents[1] / "firmware/ObdDisplay/ObdDisplay.ino").read_text(encoding="utf-8")


def section(start, end):
    return SRC.split(start, 1)[1].split(end, 1)[0]


class ClockTest(unittest.TestCase):
    def test_ntp_exists_only_in_lab_build(self):
        self.assertEqual(SRC.count("configTime("), 1)
        lab = section("#if defined(OBD_LAB_BUILD)\n// LAB only: the lab WLAN", "#endif")
        self.assertIn("configTime(", lab)
        self.assertIn("#if defined(OBD_LAB_BUILD)\n#include <esp_sntp.h>", SRC)

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
