"""Source contract: Mode 06 results are logged like PID rows (Serial + SD).

Checked on the real firmware text; the row format itself is covered by
tests/test_sd_log_format.cpp. Not proof of on-device behaviour.
"""
from pathlib import Path
import unittest

SRC = (Path(__file__).resolve().parents[1] /
       "firmware/ObdDisplay/ObdDisplay.ino").read_text(encoding="utf-8")


def section(start, end):
    return SRC.split(start, 1)[1].split(end, 1)[0]


class Mode06LoggingTest(unittest.TestCase):
    def setUp(self):
        self.body = section("static void scanMisfireStep()", "#endif // OBD_CONNECTED_BUILD")

    def test_success_rows_go_to_serial_and_sd(self):
        self.assertIn('Serial.printf("CSV,%lu,06%02X,%s,%u,count,%lu\\n"', self.body)
        self.assertIn("obd_log::mode06Row(", self.body)

    def test_failures_and_skips_are_visible_in_logs(self):
        self.assertIn('"MODE06_UNAVAILABLE,06%02X\\n"', self.body)
        self.assertIn('Serial.println("MODE06_SKIPPED,no CAN protocol");', self.body)

    def test_logging_happens_only_after_generation_guard(self):
        guard = self.body.index("if(generation!=diagGeneration)return;")
        self.assertGreater(self.body.index("obd_log::mode06Row("), guard)
        self.assertGreater(self.body.index("MODE06_UNAVAILABLE"), guard)

    def test_skip_sends_nothing(self):
        skip = self.body.index("MODE06_SKIPPED")
        self.assertLess(skip, self.body.index("transact("))


if __name__ == "__main__":
    unittest.main()
