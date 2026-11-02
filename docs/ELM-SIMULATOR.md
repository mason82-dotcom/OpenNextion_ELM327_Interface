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

**Protokollsuche wie beim echten ELM327 (`ATSP0`):** Pro TCP-Sitzung
meldet `ATDPN` bis zur ersten OBD-Anfrage `A0` (Protokoll unbekannt).
Die erste OBD-Anfrage antwortet mit `SEARCHING...` und dauert
standardmäßig 1,5 s (`--search-delay 0` bis `10`); danach meldet
`ATDPN` `A6`. `ATZ` und `ATSP0` setzen das zurück. Die Firmware liest
`ATDPN` deshalb erst nach `0100` und wartet auf diese erste Antwort
bis zu 12 s. Der Simulator bedient wie ein WLAN-Dongle nur einen
Client gleichzeitig; ein neuer Client (z. B. das Board nach Flashen
oder Reset) ersetzt die alte Sitzung sofort, statt bis zu 35 s auf
der toten Verbindung zu warten.

**Antwortzeit:** Jede Antwort kommt nach `--reply-delay` Sekunden
(Standard 0,04 s, typisch für CAN über einen WLAN-ELM327; 0 bis 2 s).
Die Firmware sendet den nächsten Befehl 25 ms nach dem `>`-Prompt;
die Abfragerate begrenzen die PID-Intervalle (etwa 8–9 Anfragen/s,
Drehzahl etwa 2 Hz). Auf der INFO-Seite stehen Befehle pro Sekunde und
die letzte Antwortzeit. Mit `--reply-delay 0.3` lässt sich ein
langsamer K-Line-/KWP-Adapter nachstellen.

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
Für diesen Test gibt es jetzt ein **eigenes Buildprofil `lab`**.
Das ESP32-Gerät benötigt `firmware/ObdDisplay/LabConfig.h`,
niemals `Config.h`. Ein Lab-Build verweigert den Start des Compilers,
wenn eine echte `Config.h` vorhanden ist. Vor dem ersten Fahrzeugkommando
verifiziert die Firmware außerdem den genauen Begrüßungstext
`ELM327 LAB SIM v0.1` aus dem Pi-Simulator; ein normaler ELM327
wird vom Laborprofil damit abgewiesen.

Auf dem Pi die Adresse und WLAN-Verbindung ermitteln:

```bash
ip -br -4 addr
ip -4 route
```

Den Simulator im laufenden ersten Terminal mit Strg+C beenden und
nur an die **für das ESP32-Gerät erreichbare Pi-LAN-IP** binden:

```bash
python3 tools/elm327_lab_simulator.py --bind DEINE_PI_IP --port 35000
```

`127.0.0.1` ist vom ESP32 **nicht** erreichbar. WLAN-Gerät und Pi
müssen sich gegenseitig im lokalen Netz erreichen können, ohne
Client-Isolation, Gastnetztrennung oder Firewall-Blockade. Das ESP32-S3
benötigt ein kompatibles 2,4-GHz-WLAN.

In zweitem Pi-Terminal:

```bash
cd ~/Projekte/OpenNextion_ELM327_Interface
test ! -f firmware/ObdDisplay/Config.h
cp firmware/ObdDisplay/LabConfig.h.example firmware/ObdDisplay/LabConfig.h
chmod 600 firmware/ObdDisplay/LabConfig.h
nano firmware/ObdDisplay/LabConfig.h
bash scripts/pi/build.sh lab
```

In `LabConfig.h` ausschließlich **SSID und Passwort des lokalen
Labor-WLANs** sowie `ELM_HOST="DEINE_PI_IP"` eintragen. Kein Passwort
in den Chat oder das GitHub-Repository kopieren.

Das Ergebnis ist `build/firmware-lab/ObdDisplay.ino.merged.bin`.
**Noch nicht flashen, bevor der LAB-Build erfolgreich kompiliert und
die Pi-IP eindeutig bestätigt ist.**

## Grenzen der aktuellen Diagnose

- Mode 01 PIDs 00, 05, 0C und 0D werden strikt zeilenweise geprüft.
- Bei Antworten mehrerer ECUs werden die PID-Bitmaps aus gültigen,
  headerlosen Mode-01-Zeilen zusammengeführt.
- Unsupported Antworten, ISO-TP-Multiframe, CAN-Header und nichtnormierte
  Adapterausgaben werden **nicht erraten**, sondern verworfen.
- DTC 03/07/0A sind weiterhin reine Rohtexte ohne belastbaren Dekoder.
- Ein echter Fahrzeugtest ist erst nach Prüfung von Adapterverhalten,
  Timing, ECU-Auswahl und Fehlercodeparser sinnvoll.
