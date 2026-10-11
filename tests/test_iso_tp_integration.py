"""Source integration guard for the strict ISO-TP DTC feature.

Native ISO-TP tests validate the parser itself; this validates firmware
integration and that display actions remain explicit manual read actions.
"""
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]
INO=(ROOT/"firmware/ObdDisplay/ObdDisplay.ino").read_text(encoding="utf-8")
DTC=(ROOT/"firmware/ObdDisplay/ObdIsoTp.h").read_text(encoding="utf-8")


class DtcIsoTpIntegrationTest(unittest.TestCase):
    def test_dtc_and_inspection_both_use_strict_parser(self):
        self.assertIn('#include "ObdIsoTp.h"', INO)
        self.assertEqual(INO.count("obd_isotp::parseDtcs("), 2)
        self.assertNotIn("obd_diag::parseDtcs(answer.c_str()", INO)
        self.assertNotIn("obd_diag::parseDtcs(response.c_str()", INO)

    def test_manual_dtc_details_screen(self):
        for expected in (
            'showPage(8)',
            'makeButton(pages[2],"DETAILS"',
            'lv_obj_add_flag(detailCard,LV_OBJ_FLAG_SCROLLABLE)',
            'NICHT SICHER DEKODIERBAR',
            'ATH0: Antwortbloecke ohne ECU-Adressen',
            'obd_dtc_names::describe(code)',
        ):
            self.assertIn(expected, INO)

    def test_fail_closed_and_no_vehicle_write(self):
        for phrase in (
            "result.malformed=true",
            "size>=7",
            "count>64",
            "used!=expected",
            "encoding!=obd_diag::DtcEncoding::Can",
        ):
            self.assertIn(phrase, DTC)
        self.assertNotIn("adapter.print(", DTC)
        self.assertNotIn("write-flash", DTC)

    def test_scan_remains_user_triggered(self):
        self.assertIn('static void onScanButton(', INO)
        self.assertIn('dtcScanRequested = true;', INO)
        self.assertIn('dtcScanRequested = false;', INO)
        self.assertIn('Verbindung verloren: vorherige DTC-Daten ungueltig', INO)


if __name__=="__main__":
    unittest.main()
