"""Static integration guard: stale values and abandoned diagnostic scans.

The firmware requires an ESP32 toolchain; this source contract complements
native and simulator tests, but is not proof of on-device behavior.
"""
from pathlib import Path
import unittest

SRC = (Path(__file__).resolve().parents[1] /
       "firmware/ObdDisplay/ObdDisplay.ino").read_text(encoding="utf-8")


def section(start, end):
    return SRC.split(start, 1)[1].split(end, 1)[0]


class SessionInvalidationTest(unittest.TestCase):
    def test_adapter_session_ends_pending_scans(self):
        f = section("static void markDisconnected(", "static bool pidSupported(")
        for fragment in (
            "dtcScanRequested = false;",
            "inspectionRequested = false;",
            "misfireRequested = false;",
            "dtcIndex = inspectionStep = misfireStep = 0;",
        ):
            self.assertIn(fragment, f)

    def test_disconnect_invalidates_diagnostic_screens(self):
        f = section("static void markDisconnected(", "static bool pidSupported(")
        for fragment in (
            "vorherige HU/AU-Daten ungueltig",
            "vorherige Mode06-Daten ungueltig",
            "DTC neu lesen",
            'lv_label_set_text(rpmText, "-- rpm")',
        ):
            self.assertIn(fragment, f)

    def test_scrolled_warning_is_repositioned(self):
        f = section("static void markDisconnected(", "static bool pidSupported(")
        self.assertIn(
            "lv_obj_scroll_to_y(lv_obj_get_parent(inspectionText), 0, LV_ANIM_OFF);",
            f,
        )
        self.assertIn(
            "lv_obj_scroll_to_y(lv_obj_get_parent(misfireText), 0, LV_ANIM_OFF);",
            f,
        )

    def test_lab_status_does_not_claim_vehicle_ecu(self):
        f = section("static void markDisconnected(", "static bool pidSupported(")
        self.assertIn("LAB: Pi-Simulator nicht erreichbar", f)
        self.assertIn("LAB-Simulator: getrennt", f)

    def test_missing_pid_invalidates_cached_sample(self):
        f = section("static void pollOnePid()", "static void scanOneDtcMode()")
        self.assertIn("pidValid[index] = false;", f)
        self.assertIn('lv_label_set_text(rpmText,"-- rpm")', f)
        self.assertIn('lv_label_set_text(coolantText,"-- C")', f)
        self.assertIn('lv_label_set_text(speedText,"-- km/h")', f)
        self.assertIn("updateExtraData();", f)

    def test_diagnostics_identify_lab_data(self):
        self.assertIn("HU/AU-Vorabcheck: SIMULIERTE OBD-Daten", SRC)
        self.assertIn("Mode 06 (LAB-SIMULATION)", SRC)
        self.assertIn("CSV_SOURCE,LAB_SIMULATED", SRC)
        self.assertNotIn("Rohdaten: keine Dekodierung", SRC)


if __name__ == "__main__":
    unittest.main()
