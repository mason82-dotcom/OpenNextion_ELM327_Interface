"""Offline evidence boundaries, exact policy alignment and adversarial captures."""
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools/audit_obd_tx.py"
spec = importlib.util.spec_from_file_location("audit_obd_tx", SCRIPT)
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)
LAB = b"OBD DIAG: LAB BUILD; SIMULATOR IDENTITY REQUIRED\n"


class AuditTest(unittest.TestCase):
    def test_exact_allowlist_matches_firmware_wire_mapping(self):
        source = (ROOT / "firmware/ObdDisplay/ReadOnlyPolicy.h").read_text()
        wire = source.split("constexpr const char* wireCommand(", 1)[1].split(
            "// Compile-time audit", 1)[0]
        declared = set(re.findall(r'case Command::\w+: return "([A-Z0-9]+)";', wire))
        self.assertEqual(audit.ALLOWED_COMMANDS, declared)
        self.assertTrue(declared)

    def test_valid_capture_counts_requests_without_claiming_results(self):
        raw = LAB + (b"OBD_TX,0,1,ATZ\r\nOBD_TX,100,2,010C\n"
                     b"OBD_TX,150,3,0105\nOBD_TX,200,4,010D\n"
                     b"OBD_TX,250,5,03\nOBD_TX,300,6,07\nOBD_TX,350,7,0A\n")
        report = audit.audit_capture(raw)
        self.assertEqual(set(report["checks"].values()), {"PASS"})
        self.assertEqual(report["observed"]["pidRequests"], {"010C": 1, "0105": 1, "010D": 1})
        self.assertEqual(set(report["unverified"].values()), {"UNBEKANNT"})
        self.assertEqual(audit.exit_status(report), 0)

    def test_empty_or_unrelated_log_is_unknown(self):
        for data in (b"", b"CSV_SOURCE,LAB_SIMULATED\n", LAB):
            with self.subTest(data=data):
                report = audit.audit_capture(data)
                self.assertEqual(set(report["checks"].values()), {"UNBEKANNT"})
                self.assertEqual(audit.exit_status(report), 2)

    def test_unlisted_reads_and_all_vehicle_writes_fail(self):
        for command in ("04", "08", "14", "2EF19000", "2701", "3101", "ATSH7E0", "ATMA", "01FF"):
            with self.subTest(command=command):
                report = audit.audit_capture(f"OBD_TX,0,1,{command}\n".encode())
                self.assertEqual(report["checks"]["txAllowlist"], "FAIL")
                self.assertEqual(report["observed"]["unlistedCommands"], 1)
                self.assertNotIn(command, report["observed"]["commandCounts"])
                self.assertEqual(audit.exit_status(report), 1)

    def test_malformed_records_cannot_be_silently_skipped(self):
        for bad in (b"OBD_TX,0,1,010c", b"prefix OBD_TX,0,1,03", b"OBD_TX,0,1,03,extra",
                    b"OBD_TX,-1,1,03", b"OBD_TX,4294967296,1,03", b"OBD_TX,0,4294967296,03",
                    b"OBD_TX,0,1,03\xff", b"OBD_TX,0,1,03\x00", b"OBD_TX", b"x" * 4097):
            with self.subTest(bad=bad[:30]):
                report = audit.audit_capture(b"OBD_TX,0,1,ATZ\n" + bad)
                self.assertEqual(set(report["checks"].values()), {"FAIL"})

    def test_gaps_duplicates_and_unmarked_counter_reset_fail(self):
        for tail in (b"OBD_TX,2,3,03", b"OBD_TX,2,1,03", b"OBD_TX,2,0,03"):
            report = audit.audit_capture(b"OBD_TX,1,1,ATZ\n" + tail)
            self.assertEqual(report["checks"]["captureContinuity"], "FAIL")
            self.assertEqual(report["observed"]["counterDiscontinuities"], 1)

    def test_partial_start_is_unknown_even_if_all_commands_allowed(self):
        report = audit.audit_capture(b"OBD_TX,500,42,03\nOBD_TX,510,43,07\n")
        self.assertEqual(report["checks"], {"txAllowlist": "PASS", "captureContinuity": "UNBEKANNT"})
        self.assertEqual(audit.exit_status(report), 2)

    def test_explicit_boot_separates_sessions_without_hardware_claim(self):
        report = audit.audit_capture(LAB + b"OBD_TX,100,1,ATZ\n" + LAB + b"OBD_TX,0,1,ATZ\n")
        self.assertEqual(set(report["checks"].values()), {"PASS"})
        self.assertEqual(report["observed"]["bootMarkers"], 2)
        report = audit.audit_capture(LAB + b"OBD_TX,100,1,ATZ\n" + LAB)
        self.assertEqual(report["checks"]["captureContinuity"], "UNBEKANNT")

    def test_millis_rollover_allowed_but_clock_rollback_fails(self):
        report = audit.audit_capture(b"OBD_TX,4294967290,1,03\nOBD_TX,5,2,07\n")
        self.assertEqual(report["checks"]["captureContinuity"], "PASS")
        report = audit.audit_capture(b"OBD_TX,100,1,03\nOBD_TX,99,2,07\n")
        self.assertEqual(report["checks"]["captureContinuity"], "FAIL")

    def test_counter_rollover_does_not_create_false_gap(self):
        report = audit.audit_capture(b"OBD_TX,10,4294967295,03\nOBD_TX,20,0,07\nOBD_TX,30,1,0A\n")
        self.assertEqual(report["observed"]["counterDiscontinuities"], 0)
        self.assertEqual(report["checks"]["captureContinuity"], "UNBEKANNT")  # capture started mid-run

    def test_unknown_text_and_secrets_are_not_reproduced(self):
        secret = "PRIVATE_PASSWORD_EXAMPLE"
        report = audit.audit_capture((secret + "\nOBD_TX,0,1," + secret + "\n").encode())
        self.assertNotIn(secret, json.dumps(report))
        self.assertEqual(report["checks"]["txAllowlist"], "FAIL")

    def test_capture_limit_is_enforced(self):
        with self.assertRaises(ValueError):
            audit.audit_capture(b"x" * (audit.MAX_CAPTURE_BYTES + 1))

    def test_cli_reads_only_local_file_and_returns_distinct_exit_codes(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "serial.txt"
            for content, code in ((b"OBD_TX,0,1,ATZ\n", 0), (b"OBD_TX,0,1,04\n", 1), (b"", 2)):
                path.write_bytes(content)
                result = subprocess.run([sys.executable, str(SCRIPT), str(path)],
                                        capture_output=True, text=True, check=False)
                self.assertEqual(result.returncode, code, result.stderr)
                self.assertEqual(json.loads(result.stdout)["schema"], "obd-tx-audit-v1")
                self.assertEqual(path.read_bytes(), content)
            path.unlink()
            result = subprocess.run([sys.executable, str(SCRIPT), str(path)],
                                    capture_output=True, text=True, check=False)
            self.assertEqual(result.returncode, 2)
            self.assertEqual(result.stdout, "")


if __name__ == "__main__":
    unittest.main()
