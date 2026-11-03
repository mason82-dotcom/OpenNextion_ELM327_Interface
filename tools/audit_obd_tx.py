#!/usr/bin/env python3
"""Offline audit of captured USB OBD_TX lines; no serial/network access.

PASS applies only to the supplied records. It is never a firmware attestation,
an ECU result or a display/hardware acceptance. Unknown text is not echoed.
"""
import argparse
import hashlib
import json
import re
from pathlib import Path

MAX_CAPTURE_BYTES = 8 * 1024 * 1024
MAX_LINE_BYTES = 4096
UINT32 = 0xFFFFFFFF
ADAPTER_COMMANDS = frozenset({
    "ATZ", "ATE0", "ATL0", "ATS0", "ATH0", "ATSP0", "ATDPN",
})
# Deliberately exact, not a service-shape matcher: new commands require review.
VEHICLE_COMMANDS = frozenset({
    "0100", "0120", "0140", "010C", "0105", "010D", "0104", "0111",
    "0110", "010B", "010F", "0106", "0107", "0108", "0109", "0142",
    "0144", "0133", "0101", "0121", "0130", "0131", "020200",
    "06A2", "06A3", "06A4", "06A5", "06A6", "06A7", "03", "07", "0A",
    "010E", "011F", "012E", "012F", "013C", "013D",
    "0600", "0620", "0640", "0660", "0680", "06A0", "0900", "0902",
    "020000", "020400", "020500", "020C00", "020D00", "020600", "020700",
    "020800", "020900", "021000", "020B00", "020F00", "021100", "020E00",
})
ALLOWED_COMMANDS = ADAPTER_COMMANDS | VEHICLE_COMMANDS
RECORD = re.compile(rb"OBD_TX,([0-9]{1,10}),([0-9]{1,10}),([A-Z0-9]{1,16})")
BOOT_MARKERS = {
    b"OBD DIAG: LAB BUILD; SIMULATOR IDENTITY REQUIRED": "LAB_REPORTED",
    b"OBD DIAG: CONNECTED BUILD; READ-ONLY COMMAND POLICY": "CONNECTED_REPORTED",
    b"OBD DIAG: OFFLINE DEMO; NO WIFI, NO VEHICLE COMMANDS": "OFFLINE_REPORTED",
}


def audit_capture(data: bytes) -> dict:
    if len(data) > MAX_CAPTURE_BYTES:
        raise ValueError("capture exceeds the 8 MiB limit")
    counts = {}
    parsed = malformed = forbidden = counter_gaps = clock_errors = 0
    partial_starts = boot_markers = 0
    previous = None
    waiting_after_boot = False
    for line in data.split(b"\n"):
        line = line.removesuffix(b"\r")
        if len(line) > MAX_LINE_BYTES:
            malformed += 1
            continue
        if line in BOOT_MARKERS:
            if waiting_after_boot:
                partial_starts += 1  # boot without any captured transmission
            boot_markers += 1
            previous = None
            waiting_after_boot = True
            continue
        if b"OBD_TX" not in line:
            continue  # unrelated serial output is never copied into the report
        match = RECORD.fullmatch(line)
        if match is None:
            malformed += 1
            continue
        stamp, counter = int(match[1]), int(match[2])
        if stamp > UINT32 or counter > UINT32:
            malformed += 1
            continue
        command = match[3].decode("ascii")
        parsed += 1
        if command in ALLOWED_COMMANDS:
            counts[command] = counts.get(command, 0) + 1
        else:
            forbidden += 1  # never echo untrusted commands or arbitrary text
        if previous is None:
            if counter != 1:
                partial_starts += 1
        else:
            old_stamp, old_counter = previous
            if counter != ((old_counter + 1) & UINT32):
                counter_gaps += 1
            # Permit millis() rollover, reject rollback/ambiguous >=2^31 gap.
            if ((stamp - old_stamp) & UINT32) >= 0x80000000:
                clock_errors += 1
        previous = stamp, counter
        waiting_after_boot = False

    incomplete = partial_starts + int(waiting_after_boot)
    allowlist = "FAIL" if malformed or forbidden else "PASS" if parsed else "UNBEKANNT"
    continuity = (
        "FAIL" if malformed or counter_gaps or clock_errors else
        "UNBEKANNT" if not parsed or incomplete else "PASS"
    )
    return {
        "schema": "obd-tx-audit-v1",
        "scope": "supplied capture only; authenticity and capture completeness unverified",
        "capture": {"sha256": hashlib.sha256(data).hexdigest(), "bytes": len(data)},
        "checks": {"txAllowlist": allowlist, "captureContinuity": continuity},
        "observed": {
            "parsedTx": parsed, "bootMarkers": boot_markers,
            "malformedRecords": malformed, "unlistedCommands": forbidden,
            "counterDiscontinuities": counter_gaps, "clockDiscontinuities": clock_errors,
            "incompleteSegments": incomplete,
            "commandCounts": dict(sorted(counts.items())),
            "pidRequests": {pid: counts.get(pid, 0) for pid in ("010C", "0105", "010D")},
            "dtcRequests": {mode: counts.get(mode, 0) for mode in ("03", "07", "0A")},
        },
        "unverified": {
            "pidResponses": "UNBEKANNT", "exclusiveAdapterAccess": "UNBEKANNT",
            "dtcUiWorkflow": "UNBEKANNT", "hardwareAcceptance": "UNBEKANNT",
        },
    }


def exit_status(report: dict) -> int:
    checks = report["checks"].values()
    return 1 if "FAIL" in checks else 2 if "UNBEKANNT" in checks else 0


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path, help="existing raw USB-Serial text capture")
    args = parser.parse_args(argv)
    try:
        if not args.capture.is_file():
            raise ValueError("capture must be a regular file")
        with args.capture.open("rb") as source:
            data = source.read(MAX_CAPTURE_BYTES + 1)
        report = audit_capture(data)
    except (OSError, ValueError):
        parser.error("capture is unreadable, not a regular file, or exceeds 8 MiB")
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return exit_status(report)


if __name__ == "__main__":
    raise SystemExit(main())
