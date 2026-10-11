"""Source contract: MicroSD logging (v0.3) and the expander bit map.

Complements tests/test_sd_log_format.cpp (row format, file names, sessions);
not proof of on-device SD behaviour.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1] / "firmware" / "ObdDisplay"
SRC = (ROOT / "ObdDisplay.ino").read_text(encoding="utf-8")
BOARD = (ROOT / "BoardConfig.h").read_text(encoding="utf-8")


def section(start, end):
    return SRC.split(start, 1)[1].split(end, 1)[0]


class BoardMapTest(unittest.TestCase):
    def test_lcd_reset_and_sd_cs_use_schematic_bits(self):
        self.assertRegex(BOARD, r"PCF8574_PIN_LCD_RST = 5;")
        self.assertRegex(BOARD, r"PCF8574_PIN_SD_CS = 6;")
        for pin, gpio in (("SD_CLK_PIN", 11), ("SD_CMD_PIN", 10), ("SD_D0_PIN", 9)):
            with self.subTest(pin=pin):
                self.assertRegex(BOARD, rf"{pin} = {gpio};")

    def test_panel_reset_leaves_sd_cs_high(self):
        body = section("static void resetPanel()", "static void displayFlush(")
        pulse = body.index("~(1U << PCF8574_PIN_LCD_RST)")
        # The last expander write releases every bit, SD CS# included.
        self.assertGreater(body.rindex("expander.digitalWriteByte(0xFF);"), pulse)


class SdLoggingTest(unittest.TestCase):
    def test_mount_is_one_bit_and_never_formats(self):
        body = section("static void sdBegin()", "#endif")
        self.assertIn("SD_MMC.setPins(SD_CLK_PIN, SD_CMD_PIN, SD_D0_PIN)", body)
        self.assertIn('SD_MMC.begin("/sdcard", true, false, SDMMC_FREQ_DEFAULT)', body)
        self.assertNotIn("format(", body)

    def test_sd_starts_after_panel_reset(self):
        setup = section("void setup() {", "void loop() {")
        self.assertLess(setup.index("resetPanel();"), setup.index("sdBegin();"))

    def test_write_error_stops_logging(self):
        body = section("static void sdWrite(", "static void sdEvent(")
        self.assertIn('sdStop("Schreibfehler, Logging gestoppt");', body)
        stop = section("static void sdStop(", "static bool sdOpenPart()")
        self.assertIn("sdReady = false;", stop)

    def test_rows_are_logged_and_flushed(self):
        self.assertIn("obd_log::pidRow(", section("static void pollOnePid()", "static void scanOneDtcMode()"))
        self.assertIn("obd_log::dtcRow(", section("static void scanOneDtcMode()", "static void scanInspectionStep()"))
        self.assertIn("sdFlushIfDue(millis());", section("void loop() {", "delay(5);"))

    def test_info_page_shows_sd_state(self):
        self.assertIn('sdText = label(sysCard, "SD: --"', SRC)
        stats = section("static void updateLinkStats(", "static void markDisconnected(")
        self.assertIn("sdProblem", stats)


if __name__ == "__main__":
    unittest.main()
