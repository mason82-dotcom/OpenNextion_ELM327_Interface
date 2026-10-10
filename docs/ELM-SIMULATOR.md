# ELM327-Labortest auf dem Raspberry Pi 5 (ohne Fahrzeug)

**Status:** Vorbereitung für den verbundenen Firmware-Test, noch nicht an
echtem ELM327 oder Fahrzeug erprobt. Das OpenNextion-Display ist im
**Offline-Build** bereits anhand der vier Touchseiten bestätigt.

Der Simulator ist ein lokaler TCP-Server. Er besitzt weder OBD-, CAN-,
Bluetooth- noch USB-Zugriff und kann deshalb keine Diagnosebefehle
an ein Fahrzeug weiterleiten. **Einen realen ELM327-Adapter dabei nicht
mit dem Fahrzeug verbinden.**

## 1. Erst Parser-/Policy-Tests auf dem Pi

```bash
cd ~/Projekte/OpenNextion_ELM327_Interface
git pull --ff-only
g++ -std=c++17 -Wall -Wextra -Werror -Ifirmware/ObdDisplay \
  tests/test_obd_response_parser.cpp -o /tmp/obd-parser-test
/tmp/obd-parser-test
python3 -m unittest discover -s tests -p 'test_*.py' -v
bash scripts/pi/build.sh offline
```

## 2. Pi-Simulator starten (erst localhost)

```bash
python3 tools/elm327_lab_simulator.py --bind 127.0.0.1 --port 35000
```

In einem zweiten Pi-Terminal ist z. B. mit Python ein Test ohne
Zusatzpakete möglich:

```bash
python3 - <<'PY'
import socket
with socket.create_connection(('127.0.0.1', 35000), timeout=3) as s:
    s.sendall(b'010C\r')
    print(s.recv(256).decode('ascii', 'replace'))
PY
```

Erwartet: eine `41 0C 13 48`-Antwort und der ELM-Prompt `>`.
Verbotene oder unbekannte Kommandos werden mit `?` abgelehnt,
nicht weitergeleitet.

## 3. Erst danach ESP32 mit Pi-Labortestnetz verbinden

Für einen **späteren** Hardware-Labortest den Simulator ausschließlich
an die Pi-IP in einem kontrollierten, lokalen WLAN binden
(z. B. `--bind 192.168.x.y`). Kein Router-Portforwarding.
Das ESP32-Gerät benötigt eine lokale, von Git ignorierte
`firmware/ObdDisplay/Config.h` mit **der WLAN-SSID und IP-Adresse
des Pi**, nicht mit der SSID des realen ELM327.

`ELM_HOST` muss dabei auf die IP des Pi und `ELM_PORT` auf 35000 zeigen.
Erst nach bestandenem Parser-, Sicherheits- und Offline-Build
`bash scripts/pi/build.sh connected` ausführen. Flashen nur nach
gesonderter Freigabe.

## Grenzen der aktuellen Diagnose

- Mode 01 PIDs 00, 05, 0C und 0D werden strikt zeilenweise geprüft.
- Bei Antworten mehrerer ECUs werden die PID-Bitmaps aus gültigen,
  headerlosen Mode-01-Zeilen zusammengeführt.
- Unsupported Antworten, ISO-TP-Multiframe, CAN-Header und nichtnormierte
  Adapterausgaben werden **nicht erraten**, sondern verworfen.
- DTC 03/07/0A sind weiterhin reine Rohtexte ohne belastbaren Dekoder.
- Ein echter Fahrzeugtest ist erst nach Prüfung von Adapterverhalten,
  Timing, ECU-Auswahl und Fehlercodeparser sinnvoll.
