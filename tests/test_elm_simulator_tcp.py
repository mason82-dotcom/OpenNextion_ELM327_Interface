"""End-to-end ELM327 LAB TCP framing and DTC fixture hot-switch tests.

Uses loopback and an OS-assigned ephemeral port. No vehicle, real adapter,
external network, fixed service or flash operation is involved.
"""
import importlib.util
from pathlib import Path
import socket
import tempfile
import threading
import time
import unittest


MODULE_FILE = Path(__file__).resolve().parents[1] / "tools" / "elm327_lab_simulator.py"
spec = importlib.util.spec_from_file_location("elm327_lab_tcp", MODULE_FILE)
lab = importlib.util.module_from_spec(spec)
spec.loader.exec_module(lab)


def exchange(sock: socket.socket, command: str) -> str:
    sock.sendall((command + "\r").encode("ascii"))
    received = bytearray()
    while b">" not in received:
        packet = sock.recv(512)
        if not packet:
            raise AssertionError("Simulator closed connection before ELM prompt")
        received.extend(packet)
        if len(received) > 2048:
            raise AssertionError("Response exceeds bounded test buffer")
    return received.decode("ascii")


class SimulatorTcpTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory(prefix="obd-tcp-fixture-")
        cls.fixture = Path(cls.tmp.name) / "dtc.txt"
        cls.rpm = Path(cls.tmp.name) / "rpm.txt"
        cls.fault = Path(cls.tmp.name) / "fault.txt"
        cls.server = lab.Server(("127.0.0.1", 0), lab.ElmCrHandler)
        cls.server.dtc_file = str(cls.fixture)
        cls.server.rpm_file = str(cls.rpm)
        cls.server.fault_file = str(cls.fault)
        cls.server.search_delay = 0.0
        cls.server.reply_delay = 0.0
        cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.thread.start()
        cls.address = cls.server.server_address

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server.server_close()
        cls.thread.join(timeout=5)
        cls.tmp.cleanup()

    def setUp(self):
        for path in (self.fixture, self.rpm, self.fault):
            path.unlink(missing_ok=True)
        self.sock = socket.create_connection(self.address, timeout=3)
        self.sock.settimeout(3)
        self.assertIn("ELM327 LAB SIM v0.1", exchange(self.sock, "ATZ"))
        self.assertIn("OK", exchange(self.sock, "ATE0"))

    def tearDown(self):
        self.sock.close()

    def test_indexed_frames_and_hot_switch_on_same_tcp_session(self):
        exchange(self.sock, "0100")  # completes the ATSP0 protocol search
        self.assertEqual(exchange(self.sock, "03"), "43 00\r>")
        self.fixture.write_text("MULTI\n", encoding="ascii")
        reply = exchange(self.sock, "03")
        self.assertIn("00A\r0:43 04 01 71 03 01 03\r1:02 03 03\r>", reply)
        self.fixture.write_text("TWO\n", encoding="ascii")
        reply = exchange(self.sock, "03")
        self.assertIn("004\r0:43 01 01 74\r00A", reply)
        self.assertIn("1:02 03 03", reply)
        self.fixture.write_text("TRUNCATED\n", encoding="ascii")
        self.assertEqual(exchange(self.sock, "03"),
                         "00A\r0:43 04 01 71 03 01 03\r>")
        self.fixture.write_text("BAD_SEQUENCE\n", encoding="ascii")
        self.assertIn("2:02 03 03", exchange(self.sock, "03"))
        self.fixture.unlink()
        self.assertEqual(exchange(self.sock, "03"), "43 00\r>")
        self.assertEqual(exchange(self.sock, "07"), "47 00\r>")
        self.assertEqual(exchange(self.sock, "0A"), "4A 00\r>")

    def test_protocol_is_unknown_until_first_obd_request(self):
        # A real ELM327 with ATSP0 reports "A0" before the protocol search.
        self.assertEqual(exchange(self.sock, "ATSP0"), "OK\r>")
        self.assertEqual(exchange(self.sock, "ATDPN"), "A0\r>")
        self.assertEqual(exchange(self.sock, "ATH0"), "OK\r>")
        self.assertEqual(exchange(self.sock, "04"), "?\r>")
        self.assertEqual(exchange(self.sock, "ATDPN"), "A0\r>")
        self.assertEqual(exchange(self.sock, "0100"),
                         "SEARCHING...\r41 00 9F BB 80 01\r>")
        self.assertEqual(exchange(self.sock, "ATDPN"), "A6\r>")
        self.assertEqual(exchange(self.sock, "0100"), "41 00 9F BB 80 01\r>")
        self.assertEqual(exchange(self.sock, "ATSP0"), "OK\r>")
        self.assertEqual(exchange(self.sock, "ATDPN"), "A0\r>")

    def test_new_tcp_session_starts_without_protocol(self):
        exchange(self.sock, "0100")
        self.assertEqual(exchange(self.sock, "ATDPN"), "A6\r>")
        # One client at a time, as with a real ELM327 WLAN adapter.
        self.sock.close()
        self.sock = socket.create_connection(self.address, timeout=3)
        self.sock.settimeout(3)
        self.assertEqual(exchange(self.sock, "ATDPN"), "ATDPN\rA0\r>")

    def test_new_client_replaces_dead_session_immediately(self):
        # Board reset during flashing: the old socket is never closed.
        exchange(self.sock, "0100")
        started = time.monotonic()
        with socket.create_connection(self.address, timeout=3) as board:
            board.settimeout(3)
            self.assertIn("ELM327 LAB SIM v0.1", exchange(board, "ATZ"))
            self.assertEqual(exchange(board, "ATE0"), "ATE0\rOK\r>")
            self.assertEqual(exchange(board, "ATDPN"), "A0\r>")
        self.assertLess(time.monotonic() - started, 2.0)
        # The replaced session was shut down by the simulator.
        self.assertEqual(self.sock.recv(64), b"")

    def test_first_obd_request_is_delayed_by_protocol_search(self):
        self.server.search_delay = 0.3
        try:
            self.assertEqual(exchange(self.sock, "ATSP0"), "OK\r>")
            started = time.monotonic()
            self.assertIn("SEARCHING...", exchange(self.sock, "0100"))
            self.assertGreaterEqual(time.monotonic() - started, 0.3)
            started = time.monotonic()
            self.assertNotIn("SEARCHING", exchange(self.sock, "0100"))
            self.assertLess(time.monotonic() - started, 0.3)
        finally:
            self.server.search_delay = 0.0

    def test_reply_delay_applies_to_every_command(self):
        exchange(self.sock, "0100")
        self.server.reply_delay = 0.1
        try:
            started = time.monotonic()
            self.assertEqual(exchange(self.sock, "010C"), "41 0C 13 48\r>")
            self.assertEqual(exchange(self.sock, "0105"), "41 05 82\r>")
            self.assertGreaterEqual(time.monotonic() - started, 0.2)
        finally:
            self.server.reply_delay = 0.0

    def test_invalid_pid_is_separate_from_dtc_fixtures(self):
        exchange(self.sock, "0100")  # completes the ATSP0 protocol search
        self.fault.write_text("010C\n", encoding="ascii")
        self.assertEqual(exchange(self.sock, "010C"), "NO DATA\r>")
        self.assertEqual(exchange(self.sock, "0105"), "41 05 82\r>")
        self.rpm.write_text("2500\n", encoding="ascii")
        self.fault.unlink()
        self.assertEqual(exchange(self.sock, "010C"), "41 0C 27 10\r>")
        self.assertEqual(exchange(self.sock, "04"), "?\r>")
        self.assertEqual(exchange(self.sock, "ATSH7E0"), "?\r>")
        self.assertEqual(exchange(self.sock, "010C"), "41 0C 27 10\r>")


if __name__ == "__main__":
    unittest.main()
