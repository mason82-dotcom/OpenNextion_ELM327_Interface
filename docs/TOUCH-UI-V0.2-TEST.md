# Touch-UI v0.2 – Hardwaretest auf Raspberry Pi 5

**Testmodus:** Ausschließlich offline. Ein erfolgreiches Build bestätigt die
Kompilierbarkeit; Display, Touch und Farben müssen anschließend am Gerät geprüft
werden. Ohne `firmware/ObdDisplay/Config.h` erzeugt die Anwendung weder WLAN-
noch OBD-Datenverkehr.

## 1. Aktualisieren und kompilieren

```bash
cd ~/Projekte/OpenNextion_ELM327_Interface
git status --short
git branch --show-current
git pull --ff-only
test ! -f firmware/ObdDisplay/Config.h
bash scripts/pi/doctor.sh
bash scripts/pi/build.sh offline
```

Nur auf dem Entwicklungsbranch `feature/read-only-obd-prototype-20261010` bauen.
Wenn `git status --short` lokale Änderungen meldet, zunächst prüfen, **nicht**
blind verwerfen. Der Build erzeugt die 16-MB-`*.merged.bin` unter
`build/firmware-offline/` und schreibt das vollständige Protokoll nach
`build/logs/firmware-offline-build.log`.

## 2. Flashen (überschreibt vorhandene Anwendung)

Das OpenNextion ONX3248G035 wurde zuvor als ESP32-S3 mit 16 MB Flash und
8 MB PSRAM über `/dev/ttyUSB0` erfolgreich identifiziert. Den Port erneut
kontrollieren, falls USB-Geräte umgesteckt wurden.

```bash
cd ~/Projekte/OpenNextion_ELM327_Interface
arduino-cli board list
ESPTOOL="$HOME/.arduino15/packages/esp32/tools/esptool_py/5.1.0/esptool"
BIN="build/firmware-offline/ObdDisplay.ino.merged.bin"
test -s "$BIN" && ls -lh "$BIN"
"$ESPTOOL" --chip esp32s3 --port /dev/ttyUSB0 --baud 115200 \
  --after hard-reset write-flash 0x0 "$BIN"
```

**Kein automatisches Flashen** in `scripts/pi/build.sh` oder CI.

## 3. Touchtest

1. Startbildschirm zeigt `OBD DIAG / READ-ONLY` und `OFFLINE-TEST`.
2. `LIVE` zeigt Platzhalter für RPM, Kühlmitteltemperatur und Tempo.
3. `GRAPH` zeigt ein dunkles Diagramm mit farbiger Null-Linie (keine realen Werte).
4. `DTC` zeigt die drei DTC-Kategorien 03 / 07 / 0A, die Schaltfläche
   `DTC gesperrt` reagiert im Offline-Modus nicht.
5. `INFO` zeigt `WLAN / OBD: ausgeschaltet`.
6. Zehnmal `Touch testen` drücken – Zähler erhöht sich für jeden Tipp.
7. Alle vier Reiter mehrfach anwählen; Touchpositionen und Seitenwechsel prüfen.
8. Mindestens fünf Minuten auf Neustarts, Flackern oder weißen Graphen prüfen.

Das Diagramm wurde bewusst dunkel gestaltet; bei weißem Hintergrund liegt
ein Darstellungsfehler vor. Die angezeigten Nullwerte sind im Offline-Modus
keine ECU-Messwerte.

## 4. Bei Fehlern

```bash
cd ~/Projekte/OpenNextion_ELM327_Interface
grep -nE 'fatal error:|error:|undefined reference|exit status' \
  build/logs/firmware-offline-build.log | tail -40
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
```

Anschließend Displayfoto und Terminalausgabe bereitstellen. **Noch keinen
ELM327-Adapter mit dem Fahrzeug verbinden.** WLAN-Tests folgen erst nach
Simulator-/Antwortparser-Tests.
