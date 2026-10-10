#!/usr/bin/env python3
"""Labor-only ELM327 TCP simulator. NO vehicle, Bluetooth, CAN or USB access."""
import argparse
import socketserver

# Standard Mode 01 supported PIDs: 05 coolant, 0C RPM, 0D speed.
RESPONSES = {
    "0100": "41 00 1F BB 80 01",  # 04/05/06/07/08/09/0B/0C/0D/0F/10/11/20
    "0120": "41 20 00 00 20 01",  # 33/40
    "0140": "41 40 50 00 00 00",  # 42/44
    "0101": "41 01 00 07 25 00",  # MIL off, 0 DTC, monitors complete
    "0104": "41 04 4C",           # 29.8% load
    "0106": "41 06 80",           # STFT B1 0%
    "0107": "41 07 8A",           # LTFT B1 7.8125%
    "0108": "41 08 80",           # STFT B2 0%
    "0109": "41 09 87",           # LTFT B2 5.46875%
    "010B": "41 0B 28",           # MAP 40 kPa
    "010F": "41 0F 46",           # intake 30C
    "0110": "41 10 01 F4",        # MAF 5 g/s
    "0111": "41 11 20",           # throttle 12.55%
    "0133": "41 33 65",           # barometric 101 kPa
    "0142": "41 42 34 BC",        # ECU 13.500 V
    "0144": "41 44 80 00",        # target lambda 1.000
    "0121": "41 21 00 00",        # MIL distance
    "0130": "41 30 05",           # warmups since reset
    "0131": "41 31 01 2C",        # km since reset 300
    "020200": "42 02 00 00 00",  # freeze-frame DTC absent
    "0105": "41 05 82",       # 90 C
    "010C": "41 0C 13 48",    # 1234 rpm
    "010D": "41 0D 00",       # 0 km/h
    "06A0": "46 A0 7E 00 00 00",
    "06A2": "46 A2 0B 24 00 00 00 00 00 10",
    "06A3": "46 A3 0B 24 00 00 00 00 00 10",
    "06A4": "46 A4 0B 24 00 00 00 00 00 10",
    "06A5": "46 A5 0B 24 00 00 00 00 00 10",
    "06A6": "46 A6 0B 24 00 00 00 00 00 10",
    "06A7": "46 A7 0B 24 00 00 00 00 00 10",
    "03": "43 00",            # no stored DTC
    "07": "47 00",            # no pending DTC
    "0A": "4A 00",            # no permanent DTC
}
ADAPTER_COMMANDS = {"ATZ", "ATE0", "ATL0", "ATS0", "ATH0", "ATSP0", "ATDPN"}


def answer_for(command: str) -> str:
    """Return only fixture responses, never execute or forward a command."""
    command = command.strip().upper()
    if command == "ATZ":
        return "ELM327 LAB SIM v0.1"
    if command == "ATDPN":
        return "A6"  # ISO 15765-4 11-bit CAN 500 kbit/s (simulated)
    if command in ADAPTER_COMMANDS:
        return "OK"
    if command in RESPONSES:
        return RESPONSES[command]
    return "?"


class ElmCrHandler(socketserver.BaseRequestHandler):
    """ELM327-compatible CR terminator; one command per prompt."""
    def handle(self):
        self.request.settimeout(35)
        echo = True
        buf = bytearray()
        peer = self.client_address[0]
        print(f"[LAB] client {peer} connected", flush=True)
        try:
            while True:
                data = self.request.recv(64)
                if not data:
                    return
                for ch in data:
                    if ch in (10, 13):
                        if not buf:
                            continue
                        cmd = buf.decode("ascii", errors="replace").upper().strip()
                        buf.clear()
                        if not cmd:
                            continue
                        print(f"[LAB] RX {cmd}", flush=True)
                        reply = answer_for(cmd)
                        if reply == "?":
                            print(f"[LAB] REJECT {cmd}", flush=True)
                        payload = ((cmd + "\r") if echo else "") + reply + "\r>"
                        self.request.sendall(payload.encode("ascii"))
                        if cmd == "ATZ":
                            echo = True
                        elif cmd == "ATE0":
                            echo = False
                    elif len(buf) < 32:
                        buf.append(ch)
                    else:
                        print("[LAB] oversize command rejected", flush=True)
                        self.request.sendall(b"?\r>")
                        buf.clear()
        except (TimeoutError, ConnectionError, OSError):
            pass
        finally:
            print(f"[LAB] client {peer} disconnected", flush=True)


class Server(socketserver.TCPServer):
    allow_reuse_address = True


def main():
    parser = argparse.ArgumentParser(description="Read-only ELM327 LAB server; no vehicle access")
    parser.add_argument("--bind", default="127.0.0.1",
                        help="default loopback only; use Pi LAN IP explicitly for ESP32 bench tests")
    parser.add_argument("--port", type=int, default=35000)
    args = parser.parse_args()
    with Server((args.bind, args.port), ElmCrHandler) as server:
        print(f"[LAB] ELM327 simulator listening on {args.bind}:{args.port}", flush=True)
        print("[LAB] No vehicle/OBD adapter required; Ctrl+C to stop", flush=True)
        try:
            server.serve_forever(poll_interval=0.2)
        except KeyboardInterrupt:
            print("[LAB] stopped", flush=True)


if __name__ == "__main__":
    main()
