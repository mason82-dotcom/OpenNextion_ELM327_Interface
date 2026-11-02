#!/usr/bin/env python3
"""Labor-only ELM327 TCP simulator. NO vehicle, Bluetooth, CAN or USB access."""
import argparse
import socketserver

# Standard Mode 01 supported PIDs: 05 coolant, 0C RPM, 0D speed.
RESPONSES = {
    "0100": "41 00 08 18 00 00",
    "0105": "41 05 82",       # 90 C
    "010C": "41 0C 13 48",    # 1234 rpm
    "010D": "41 0D 00",       # 0 km/h
    "03": "43 00",            # no stored DTC
    "07": "47 00",            # no pending DTC
    "0A": "4A 00",            # no permanent DTC
}
ADAPTER_COMMANDS = {"ATZ", "ATE0", "ATL0", "ATS0", "ATH0", "ATSP0"}


def answer_for(command: str) -> str:
    """Return only fixture responses, never execute or forward a command."""
    command = command.strip().upper()
    if command == "ATZ":
        return "ELM327 LAB SIM v0.1"
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
