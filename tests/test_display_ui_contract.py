"""Regression check for LVGL 320x480 cards; no ESP32/toolchain needed.

A previous connected build put entire German field captions into 142px cards,
causing clipped values. The labels in buildUi already contain those captions.
"""
import pathlib
import unittest

FIRMWARE = (pathlib.Path(__file__).resolve().parents[1] /
            "firmware" / "ObdDisplay" / "ObdDisplay.ino").read_text(encoding="utf-8")


class UiContractTest(unittest.TestCase):
    def test_initial_readouts_are_compact(self):
        for label in ('"-- rpm"', '"-- C"', '"-- km/h"'):
            with self.subTest(label=label):
                self.assertIn(label, FIRMWARE)

    def test_connected_values_have_no_duplicate_caption(self):
        for fmt in ('"%d rpm", rpm', '"%d C", coolantC',
                    '"%d km/h", speedKph'):
            with self.subTest(fmt=fmt):
                self.assertIn(fmt, FIRMWARE)
        for obsolete in ('"Drehzahl: %d rpm"', '"Kuehlmittel: %d C"',
                         '"Geschwindigkeit: %d km/h"'):
            with self.subTest(obsolete=obsolete):
                self.assertNotIn(obsolete, FIRMWARE)

    def test_disconnection_values_fit_into_cards(self):
        self.assertIn('lv_label_set_text(rpmText, "-- rpm")', FIRMWARE)
        self.assertIn('lv_label_set_text(coolantText, "-- C")', FIRMWARE)
        self.assertIn('lv_label_set_text(speedText, "-- km/h")', FIRMWARE)
        self.assertNotIn('lv_label_set_text(rpmText, "Drehzahl:', FIRMWARE)

    def test_graph_data_origin_is_explicit(self):
        self.assertIn('"LABOR: simulierte Werte / keine ECU-Daten"', FIRMWARE)
        self.assertIn('"Offline: keine realen Messwerte"', FIRMWARE)
        self.assertIn('"Livewerte vom ELM327-Adapter"', FIRMWARE)


if __name__ == "__main__":
    unittest.main()
