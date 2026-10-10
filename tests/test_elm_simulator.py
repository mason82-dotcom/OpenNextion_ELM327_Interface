import importlib.util
import pathlib
import tempfile
import unittest

MODULE_FILE = pathlib.Path(__file__).resolve().parents[1] / "tools" / "elm327_lab_simulator.py"
spec = importlib.util.spec_from_file_location("elm327_lab_simulator", MODULE_FILE)
lab = importlib.util.module_from_spec(spec)
spec.loader.exec_module(lab)

class LabTest(unittest.TestCase):
    def test_expected_read_commands(self):
        self.assertEqual(lab.answer_for("010C"), "41 0C 13 48")
        self.assertEqual(lab.answer_for("0120"), "41 20 80 01 A0 01")
        self.assertEqual(lab.answer_for("0140"), "41 40 50 00 00 00")
        self.assertEqual(lab.answer_for("0107"), "41 07 8A")
        self.assertEqual(lab.answer_for("0142"), "41 42 34 BC")
        self.assertEqual(lab.answer_for("0144"), "41 44 80 00")
        self.assertEqual(lab.answer_for("0101"), "41 01 00 07 25 00")
        self.assertEqual(lab.answer_for("0105"), "41 05 82")
        self.assertEqual(lab.answer_for("0100"), "41 00 9F BB 80 01")
        self.assertEqual(lab.answer_for("03"), "43 00")
        self.assertEqual(lab.answer_for("07"), "47 00")
        self.assertEqual(lab.answer_for("0A"), "4A 00")
        self.assertEqual(lab.answer_for("06A2"), "46 A2 0B 24 00 00 00 00 00 10")
    def test_reject_modifying_requests(self):
        for cmd in ("04", "08", "14", "2E", "31", "2701", "ATSH7E0", "ATCRA7E8", "FF", ""):
            with self.subTest(cmd=cmd):
                self.assertEqual(lab.answer_for(cmd), "?")
    def test_pid_no_data_fault_does_not_disable_tcp_or_other_pids(self):
        self.assertEqual(lab.answer_for("010C", "010C"), "NO DATA")
        self.assertEqual(lab.answer_for("0105", "010C"), "41 05 82")
        self.assertEqual(lab.answer_for("ATZ", "010C"), "ELM327 LAB SIM v0.1")
        self.assertEqual(lab.answer_for("04", "010C"), "?")

    def test_fault_file_hot_toggle_and_strict_allowlist(self):
        with tempfile.TemporaryDirectory() as tmp:
            filename = pathlib.Path(tmp) / "pid-fault"
            self.assertIsNone(lab.fault_pid_from_file(str(filename)))
            filename.write_text("010C\n", encoding="ascii")
            self.assertEqual(lab.fault_pid_from_file(str(filename)), "010C")
            filename.write_text("04\n", encoding="ascii")
            self.assertIsNone(lab.fault_pid_from_file(str(filename)))
            filename.write_text("01FF", encoding="ascii")
            self.assertIsNone(lab.fault_pid_from_file(str(filename)))
            filename.unlink()
            self.assertIsNone(lab.fault_pid_from_file(str(filename)))

    def test_adapter_setup(self):
        self.assertEqual(lab.answer_for("ATSP0"), "OK")
        self.assertEqual(lab.answer_for("ATZ"), "ELM327 LAB SIM v0.1")
if __name__ == "__main__":
    unittest.main()
