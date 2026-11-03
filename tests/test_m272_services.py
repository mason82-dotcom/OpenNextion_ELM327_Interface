"""Run the actual native decoders against standard and exact LAB fixtures."""
from pathlib import Path
import subprocess
import tempfile
import unittest
import sys

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"tools"))
import elm327_lab_simulator as lab  # noqa: E402


class M272ServicesTest(unittest.TestCase):
    def test_decoders_and_simulator_vin(self):
        with tempfile.TemporaryDirectory() as tmp:
            binary=Path(tmp)/"service-test"
            result=subprocess.run([
                "g++","-std=c++17","-Wall","-Wextra","-Werror",
                "-I"+str(ROOT/"firmware/ObdDisplay"),
                str(ROOT/"tests/test_m272_services.cpp"),"-o",str(binary),
            ],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
            subprocess.run([str(binary)],check=True)
            subprocess.run([str(binary),lab.answer_for("0902"),"WDD2042561F123456"],check=True)

    def test_synthetic_capabilities_and_freeze_context(self):
        self.assertEqual(lab.answer_for("0600"),"46 00 00 00 00 01")
        self.assertEqual(lab.answer_for("06A0"),"46 A0 7E 00 00 00")
        self.assertEqual(lab.answer_for("020200"),"42 02 00 00 00")
        self.assertEqual(lab.answer_for("020200",dtc_fixture="PADDED"),"42 02 00 01 71")
        for forbidden in ("04","08","0904","020C01","ATSH7E0"):
            self.assertEqual(lab.answer_for(forbidden),"?")


if __name__=="__main__":
    unittest.main()
