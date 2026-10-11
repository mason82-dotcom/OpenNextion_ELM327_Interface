"""Integration contracts for passive Fuel Trim test; no ESP32 emulator.

Native tests in test_fuel_trim.cpp validate the state machine. These guards
validate UI wiring and compile-time isolation from vehicle-write functions.
"""
from pathlib import Path
import unittest

BASE = Path(__file__).resolve().parents[1]
INO = (BASE / "firmware/ObdDisplay/ObdDisplay.ino").read_text(encoding="utf-8")
HEADER = (BASE / "firmware/ObdDisplay/FuelTrimTest.h").read_text(encoding="utf-8")


class TrimUiContractTest(unittest.TestCase):
    def test_laboratory_trim_screen_and_manual_actions(self):
        for fragment in (
            'showPage(7);',
            'makeButton(pages[4],"TRIM-TEST"',
            'makeButton(pages[7],"START"',
            'makeButton(pages[7],"ABBRUCH"',
            'trimTest.start();',
            'trimTest.cancel();',
            'renderTrimPage();',
        ):
            self.assertIn(fragment, INO)

    def test_prioritize_readonly_pid_polling_during_test(self):
        for fragment in (
            'if(trimTest.running()){',
            'static const uint8_t priorityPids[]={',
            '0x0C,0x0D,0x06,0x07,0x08,0x09,0x10,0x0B',
            'trimTest.sample(p.pid,value,millis());',
        ):
            self.assertIn(fragment, INO)

    def test_missing_pid_and_link_loss_cancel_measurement(self):
        self.assertIn('trimTest.cancel();', INO)
        self.assertIn('Vorherige Messung ungueltig.', INO)
        self.assertIn('!obd_pids::supported(pid,pidMasks)', INO)
        self.assertIn('dtcScanRequested=false;', INO)

    def test_not_an_actuator_control_and_lab_labeling(self):
        self.assertIn('LAB-SIMULATION / keine ECU-Messwerte', INO)
        self.assertIn('keine 3 FRISCHEN Werte pro Trim-PID', INO)
        for fragment in (
            'IDLE_MEASURE_MS=20000',
            'HIGH_MEASURE_MS=15000',
            'MAX_EXTRA_MS=30000',
            'MIN_TRIM_SAMPLES=3',
        ):
            self.assertIn(fragment, HEADER)
        self.assertNotIn('adapter.print(', HEADER)
        self.assertNotIn('throttleSet', INO)


if __name__ == "__main__":
    unittest.main()
