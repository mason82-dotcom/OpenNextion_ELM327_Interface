"""Source contract: ELM protocol detection order and superseded scan replies.

transact() keeps LVGL running, so touch callbacks can restart or replace a
diagnostic while a reply is pending. Complements simulator tests; not proof
of on-device behavior.
"""
from pathlib import Path
import re
import unittest

SRC = (Path(__file__).resolve().parents[1] /
       "firmware/ObdDisplay/ObdDisplay.ino").read_text(encoding="utf-8")


def section(start, end):
    return SRC.split(start, 1)[1].split(end, 1)[0]


class AdapterInitOrderTest(unittest.TestCase):
    def setUp(self):
        self.init = section("static bool initAdapter()", "static String trimNumber(")

    def test_protocol_id_is_read_after_protocol_search(self):
        search = self.init.index("transact(Command::SupportedPids00")
        dpn = self.init.index("transact(Command::AdapterProtocolId")
        self.assertLess(search, dpn)
        self.assertLess(dpn, self.init.index("adapterReady = true;"))

    def test_protocol_search_has_long_timeout(self):
        self.assertIn(
            "transact(Command::SupportedPids00, response, PROTOCOL_SEARCH_TIMEOUT_MS)",
            self.init)
        match = re.search(r"PROTOCOL_SEARCH_TIMEOUT_MS = (\d+);", SRC)
        self.assertIsNotNone(match)
        self.assertGreaterEqual(int(match.group(1)), 10000)


class AdapterConnectTest(unittest.TestCase):
    def test_connect_shows_hint_and_blocks_briefly(self):
        init = section("static bool initAdapter()", "static String trimNumber(")
        self.assertLess(init.index("uiTick();"), init.index("adapter.connect("))
        self.assertIn("ADAPTER_CONNECT_TIMEOUT_MS)", init)
        match = re.search(r"ADAPTER_CONNECT_TIMEOUT_MS = (\d+);", SRC)
        self.assertLessEqual(int(match.group(1)), 1000)


class InspectionPidMaskTest(unittest.TestCase):
    def test_unsupported_mode01_steps_are_not_sent(self):
        body = section("static void scanInspectionStep()", "static void scanMisfireStep()")
        self.assertIn("stepPids[]={0x01,0x21,0x30,0x31}", body)
        skip = body.index("if (!pidSupported(stepPids[inspectionStep]))")
        self.assertLess(skip, body.index("transact("))


class DtcScanSummaryTest(unittest.TestCase):
    """Regression: codes in 03 were followed by "Keine DTCs" from empty 0A."""

    def setUp(self):
        self.body = section("static void scanOneDtcMode()", "static void scanInspectionStep()")

    def test_status_is_summarised_only_after_all_modes(self):
        self.assertIn("dtcScanCodes += static_cast<uint16_t>(dtcs.count);", self.body)
        final = self.body.index("dtcScanRequested = false;\n  // Undecodable")
        for text in ('"Keine DTCs"', '"%u Codes -> DETAILS"', '"RAW -> DETAILS"'):
            with self.subTest(text=text):
                self.assertEqual(self.body.count(text), 1)
                self.assertGreater(self.body.index(text), final)

    def test_undecodable_mode_is_never_reported_as_empty(self):
        tail = self.body[self.body.index("// Undecodable"):]
        self.assertLess(tail.index("dtcScanUndecodable"), tail.index('"Keine DTCs"'))
        self.assertLess(tail.index("dtcScanCodes>0"), tail.index('"Keine DTCs"'))

    def test_new_scan_resets_summary(self):
        handler = section("static void onScanButton(", "static void renderTrimPage();")
        self.assertIn("dtcScanCodes = 0;", handler)
        self.assertIn("dtcScanUndecodable = false;", handler)


class ScanGenerationTest(unittest.TestCase):
    def test_every_scan_step_discards_superseded_replies(self):
        for start, end in (
            ("static void scanOneDtcMode()", "static void scanInspectionStep()"),
            ("static void scanInspectionStep()", "static void scanMisfireStep()"),
            ("static void scanMisfireStep()", "#endif // OBD_CONNECTED_BUILD"),
        ):
            with self.subTest(step=start):
                body = section(start, end)
                body = re.sub(r"\s+", "", body)
                token = body.index("constuint32_tgeneration=diagGeneration;")
                send = body.index("transact(")
                guard = body.index("if(generation!=diagGeneration)return;")
                self.assertLess(token, send)
                self.assertLess(send, guard)
                # The guard must precede any use of the reply.
                self.assertLess(guard, body.index("++", guard))

    def test_ui_actions_that_change_scans_bump_generation(self):
        for start, end in (
            ("static void onScanButton(", "static void renderTrimPage();"),
            ("static void onTrimStart(", "static void onTrimCancel("),
            ("static void onInspectionButton(", "static void onMode06Button("),
            ("static void onMode06Button(", "static void buildUi()"),
            ("static void markDisconnected(", "static bool pidSupported("),
        ):
            with self.subTest(handler=start):
                self.assertIn("++diagGeneration;", section(start, end))


if __name__ == "__main__":
    unittest.main()
