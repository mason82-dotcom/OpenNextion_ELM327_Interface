"""The simulator's PADDED fixture decodes with the firmware's own parser.

Builds a tiny host program around obd_isotp::parseDtcs and feeds it the exact
fixture text, so the LAB fixture and the firmware cannot drift apart.
"""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import elm327_lab_simulator as lab  # noqa: E402

PROGRAM = r'''
#include "ObdIsoTp.h"
#include <cstdio>
#include <string>
int main(int, char **argv){
  std::string raw(argv[1]);
  auto r=obd_isotp::parseDtcs(raw.c_str(),0x43,obd_diag::DtcEncoding::Can);
  std::printf("%d %d %zu",r.gotAnswer,r.malformed,r.count);
  for(std::size_t i=0;i<r.count;++i)std::printf(" %s",r.entries[i].code);
  return 0;
}
'''


class PaddedFixtureDecodeTest(unittest.TestCase):
    def run_parser(self, raw):
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp) / "p.cpp"
            exe = Path(tmp) / "p"
            src.write_text(PROGRAM)
            subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                            "-I" + str(ROOT / "firmware/ObdDisplay"),
                            str(src), "-o", str(exe)], check=True)
            return subprocess.run([str(exe), raw], check=True,
                                  capture_output=True, text=True).stdout

    def test_padded_fixture_yields_four_codes(self):
        out = self.run_parser(lab.DTC_FIXTURES["PADDED"]["03"])
        self.assertEqual(out, "1 0 4 P0171 P0301 P0302 P0303")

    def test_multi_and_padded_decode_identically(self):
        self.assertEqual(self.run_parser(lab.DTC_FIXTURES["MULTI"]["03"]),
                         self.run_parser(lab.DTC_FIXTURES["PADDED"]["03"]))


if __name__ == "__main__":
    unittest.main()
