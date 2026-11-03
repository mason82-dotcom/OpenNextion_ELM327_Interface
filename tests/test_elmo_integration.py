"""Compile the vendored transport and guard the firmware's typed entry point."""
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SKETCH = ROOT / "firmware/ObdDisplay"
SRC = (SKETCH / "ObdDisplay.ino").read_text(encoding="utf-8")


class ElmoIntegrationTest(unittest.TestCase):
    def test_actual_transport_with_host_doubles(self):
        with tempfile.TemporaryDirectory() as tmp:
            binary = Path(tmp) / "elmo-test"
            result = subprocess.run([
                "g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                "-I" + str(ROOT / "tests/host"), "-I" + str(SKETCH),
                str(ROOT / "tests/test_elmo_transport.cpp"),
                str(SKETCH / "src/ELMo/ELMo.cpp"), "-o", str(binary),
            ], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            result = subprocess.run([str(binary)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)

    def test_all_firmware_requests_pass_policy_before_elmo(self):
        code = re.sub(r"//[^\n]*", "", SRC)
        self.assertIn('#include "src/ELMo/ELMo.h"', code)
        self.assertIn("static ELMo elm;", code)
        self.assertEqual(code.count("elm.begin("), 1)
        self.assertEqual(code.count("elm.poll("), 1)
        self.assertNotIn("elm.initialize(", code)
        self.assertNotIn("adapter.write(", code)
        self.assertIn("transport = elm.transaction", code)
        header = (SKETCH / "src/ELMo/ELMo.h").read_text()
        self.assertIn("transaction.begin(client, command, now, timeoutMs)", header)
        engine = (SKETCH / "ObdTransaction.h").read_text()
        self.assertLess(engine.index("obd_policy::allowedWire(command)"), engine.index("io.write("))
        self.assertIn("BYTE_BUDGET = 64", engine)
        self.assertIn("ELM327 LAB SIM v0.1", code)


if __name__ == "__main__":
    unittest.main()
