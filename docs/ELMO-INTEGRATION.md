# ELMo-Integration

## Eingelesener Projektstand

Basis ist `origin/main` bei `c087fe6b0379ea2b2f85c62416c31cda1b1242fd`
des OpenNextion_ELM327_Interface-Repositories. Vor der Veröffentlichung wurde
zusätzlich `origin/main` bei `b98224566948931d94b3803927a1c9362b22a124`
integriert; dessen inkrementeller DTC-Empfang bleibt erhalten.
Firmware, native Header, Tests, Simulator, Auditwerkzeug, Buildskripte und
Dokumentation wurden vor der Änderung eingelesen. Historische LAB-Berichte
sind keine neue Hardwareprüfung dieser Integration.

Das ONX3248G035 nutzt einen ESP32-S3 mit ST7796, CST826, PCF8574 und optionalem
MicroSD-Logging. Der bisherige Diagnosestack bleibt für PID-Auswahl,
Readiness, Fuel Trims, DTC-/Mode-06-Dekodierung, Sitzungsinvalidierung und CSV
zuständig. ELMo verwaltet Socket und gemeinsame inkrementelle Transaktion.

## Herkunft und Einbindung

- Upstream: https://github.com/rudydelorenzo/ELMo
- Version: 1.1.2, Commit `59a9df47486b2ebf0c3be80fb05876e8dc8f39bf`.
- Lokale Kopie: `firmware/ObdDisplay/src/ELMo/`, einschließlich MIT-Lizenz.
- Arduino kompiliert den Sketch-Unterordner `src/` rekursiv. Pi-Build und CI
  nutzen damit dieselben Quellen; keine zusätzliche Bibliotheksinstallation.
- Die Datei `src/ELMo/README.md` dokumentiert alle lokalen API-/Transportänderungen.

## Befehlsweg

`UI / Polling → startTransaction(Command) → ELMo::begin → Transaction / ReadOnlyPolicy → WiFiClient`

ELMo besitzt die gemeinsame `Transaction` aus `ObdTransaction.h` und den
Socket. `ELMo::poll` verarbeitet höchstens 64 Bytes ohne auf Daten zu warten.
Der DTC-Pfad kehrt zur Hauptschleife zurück; `transact` nutzt dieselbe
Transaktion als kooperativen Wrapper für Setup, PIDs, HU/AU und Mode 06.
Socket-Connect und TCP_NODELAY bleiben in der Firmware. Alte Eingangsbytes
werden vor einer Anfrage auf höchstens 64 Bytes begrenzt verworfen;
mehr verbleibende Bytes schließen die Verbindung ohne TX.
Die bisherige Setupfolge bleibt erhalten:
`ATZ`, `ATE0`, `ATL0`, `ATS0`, `ATH0`, `ATSP0`, danach PID-Erkennung und `ATDPN`.
Im LAB-Profil muss unmittelbar nach `ATZ` die Simulator-Kennung passen,
bevor irgendeine Fahrzeuganfrage möglich ist.

`ELMo::initialize()` wird bewusst nicht verwendet: Die automatische
Upstream-Initialisierung würde vor der LAB-Prüfung bereits `0100` senden.
`ELMo::stop()` sendet in der lokalen Version kein implizites `AT WS`.
Die separate S204-Erweiterung ergänzt ausschließlich dokumentierte Leseanfragen.

## Transportverhalten

- Ein geprüfter `write` für Befehl und CR; kein zusätzliches LF.
- Bestehende Deadlines in Millisekunden: regulär 2500 ms, DTC/Diagnosen
  3500 ms, Protokollsuche 12000 ms.
- UI-Callback während des Wartens, höchstens 64 Empfangsbytes je Durchlauf,
  `delay(2)` im kooperativen Wrapper für Netzwerk/Watchdog. DTCs sind asynchron.
- Höchstens 1024 Rohbytes vor dem Prompt, auch bei nicht druckbaren Zeichen.
- CR/LF werden wie bisher normalisiert. Vollständige Antworten inklusive
  gemischter Fehler-/Erfolgsblöcke bleiben für die Parser erhalten.
- Timeout, Verbindungsabbruch, Übergröße, kurzer Schreibvorgang und
  Speicherfehler im generischen Send-Overload schließen den unvollständigen Stream.
  Die Firmware-Transaktion verwendet einen festen Puffer. Kein Folgekommando
  auf einer unsynchronisierten Sitzung.
- UI-Abbruch verwirft Ergebnisse mittels bestehender Diagnosegeneration;
  eine bereits laufende Antwort wird weiter bis zum Prompt gelesen.

Die generische ELMo-API ist selbst keine Read-only-Policy. Die Firmware
verwendet ausschließlich `begin(Command)` mit der geschlossenen Allowlist.
Der generische Send-Overload bleibt für API-Kompatibilität und eigene Tests
erhalten und lehnt Zugriffe ab, solange die typisierte Transaktion belegt ist.
Die APIs sind nicht thread-sicher und initialisieren den Adapter nicht automatisch.

## Ursprünglicher Dongle

Die vorgegebene Adresse `192.168.0.10:35000` stimmt mit der bisherigen
Android-App überein. SSID und WLAN-Passwort müssen vom Anwender stammen.
Sie gehören ausschließlich in die lokale Git-ignorierte `Config.h`.
Ein Benutzername-/Passwort-Handshake über TCP ist nicht implementiert.
LAB-Zugangsdaten bleiben in der getrennten `LabConfig.h`.

## Prüfungen und verbleibende Abnahme

Die Baseline bestand lokal 82 Python-Tests. Nach Integration prüfen die
zusätzlichen Tests die tatsächliche ELMo-Implementierung mit Arduino-/TCP-
Host-Doubles: Framing, Normalisierung, verspätete Antwort, `millis()`-Überlauf,
fehlender Prompt, verlorene Verbindung, Größenlimit einschließlich Steuerbytes,
kurzer Write, Speicherfehler, Wiedereintritt und Schließen ohne versteckte TX.
Der bestehende DTC-Ablauftest verwendet ebenfalls den echten ELMo-Quellcode.

### Lokal ausgeführt am 2026-11-03

Diese Zahlen dokumentieren die ursprüngliche ELMo-Integration vor der
[nachfolgenden S204/M272-Erweiterung](M272-S204-DIAGNOSTICS.md).

- 84 Python-Unittests: bestanden, einschließlich TCP-Simulator, tatsächlichem
  ELMo-Transport und DTC-Workflow mit Host-Doubles.
- Alle neun bisherigen nativen C++-Testprogramme: mit
  `-std=c++17 -Wall -Wextra -Werror` kompiliert und erfolgreich ausgeführt.
- Alle drei vollständigen Firmware-Builds: `BUILD OK; NEVER FLASHED`.

| Profil | Programmspeicher | Statischer RAM | Ergebnis |
|---|---:|---:|---|
| Offline | 1.182.059 Byte (37 %) | 121.204 Byte (36 %) | bestanden |
| Connected | 1.305.347 Byte (41 %) | 123.756 Byte (37 %) | bestanden |
| LAB | 1.305.575 Byte (41 %) | 123.756 Byte (37 %) | bestanden |

Host: Windows/Git Bash, Arduino CLI 1.5.1, ESP32 Core 3.3.2 und die in CI
festgelegten Displaybibliotheken. Alle Builds liefen mit dem vorhandenen
`scripts/pi/build.sh` in getrennten Prüfkopien unter `build/verify-<profil>/`.
FQBN: `esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB`.
Die Build-Quellen wurden byteweise mit dem aktuellen Firmwarestand verglichen.
Connected enthält die vom Anwender gelieferte lokale Dongle-Konfiguration;
Offline und LAB enthalten keine Fahrzeug-`Config.h`. `git check-ignore`
bestätigt den Ausschluss der lokalen Zugangsdaten. Diese Ergebnisse sind
lokale Build-/Hosttest-Nachweise, kein neuer GitHub-CI- oder Hardwarelauf.

```bash
python3 -m unittest discover -s tests -p 'test_*.py' -v
bash scripts/pi/build.sh offline
# connected nur mit eigener Config.h; LAB nur ohne Config.h und mit LabConfig.h
```

Nach dem Transportwechsel sind Display-/Touchreaktion, DTC-Abbruch und
Reconnect erneut nach `DTC-LAB-RUNBOOK.md` am Board zu prüfen. Die vorhandenen
Host-/Simulatortests ersetzen diesen Nachweis und einen echten Adaptertest
nicht. Es wurde kein Board geflasht und keine Fahrzeugverbindung hergestellt.
