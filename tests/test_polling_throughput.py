"""Source contract: request pacing in loop() and transact().

The PID rate is capped by the scheduler intervals (tests/test_poll_scheduler.cpp);
loop() must not add a fixed cadence on top. Not proof of on-device timing.
"""
from pathlib import Path
import re
import unittest

SRC = (Path(__file__).resolve().parents[1] /
       "firmware/ObdDisplay/ObdDisplay.ino").read_text(encoding="utf-8")


def section(start, end):
    return SRC.split(start, 1)[1].split(end, 1)[0]


class PollingThroughputTest(unittest.TestCase):
    def test_no_fixed_poll_cadence(self):
        self.assertNotIn("POLL_INTERVAL_MS", SRC)
        gap = re.search(r"COMMAND_GAP_MS = (\d+);", SRC)
        self.assertIsNotNone(gap)
        self.assertLessEqual(int(gap.group(1)), 50)

    def test_gap_is_measured_after_the_reply(self):
        body = section("void loop() {", "delay(5);")
        self.assertIn("now - lastCommandDone >= COMMAND_GAP_MS", body)
        self.assertLess(body.index("else pollOnePid();"),
                        body.index("lastCommandDone = millis();"))

    def test_stats_label_is_not_redrawn_per_command(self):
        transact = section("static bool transact(", "static void updateLinkStats(")
        self.assertNotIn("statsText", transact)
        self.assertIn("lastLatencyMs = millis() - start;", transact)
        self.assertIn("if (elapsed < 1000) return;",
                      section("static void updateLinkStats(", "static void markDisconnected("))


if __name__ == "__main__":
    unittest.main()
