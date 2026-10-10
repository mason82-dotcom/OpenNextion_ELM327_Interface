# OpenNextion ELM327 Interface

Eigenständiger OBD-II-Monitor auf dem **OpenNextion ONX3248G035** (ESP32-S3, 3,5" Touch) mit ELM327-WLAN-Adapter. Kein Smartphone und kein Raspberry Pi im Fahrzeug notwendig.

> **Status: v0.1 Entwicklungsprototyp – NICHT am Fahrzeug validiert.**
> Hardware-/Fahrzeugfunktionen dürfen erst nach Display-, WLAN-, Parser- und Read-only-Prüfungen als funktionsfähig gelten.

## Umfang von v0.1

- Originale ONX3248G035-GPIO-Belegung für ST7796-LCD, CST826-Touch und PCF8574-Reset, übernommen aus dem Schwesterprojekt OpenNextion_Hermes_Dashboard.
- Ohne lokale `Config.h` **vollständig offline**: Display-/Touchtest ohne WLAN und ohne OBD-Traffic.
- Optionaler WLAN/TCP-Betrieb mit ELM327-Adapter über feste, typisierte Leseanfragen.
- Abfrage der unterstützten Standard-PIDs (Mode 01 PID 00). Drehzahl, Kühlmitteltemperatur und Geschwindigkeit bei Unterstützung, jeweils nur als Livewerte.
- Live-RPM-Diagramm mit LVGL; OBD-Verbindungsstatus und Fehleranzeige.
- Gespeicherte (03), ausstehende (07) und permanente (0A) DTCs **zunächst als rohe Adapterantwort**, noch ohne vollständigen Multi-ECU-/ISO-TP-Dekoder.
- Diagnose-Protokollzeilen über USB-Serial. **Noch keine persistente MicroSD-Aufzeichnung.**
- Zentrale, geschlossene Allowlist; keine Mode 04/08, keine UDS-Schreibdienste, keine frei eingebbaren CAN-/ELM-Befehle, kein Terminal.

## Sicherheit

Die Firmware kann ausschließlich Kommandos senden, die in `ReadOnlyPolicy.h` fest vorgegeben sind. Adapter-Befehle `ATZ/ATE0/ATL0/ATS0/ATH0/ATSP0` richten *nur den ELM327-Adapter* ein, während die Fahrzeuganfragen ausschließlich `0100, 0105, 010C, 010D, 03, 07, 0A` sind. Ein OBD-Lesezugriff sendet Diagnoseframes an das Steuergerät; das ist **keine passive CAN-Aufzeichnung**, verändert aber bei normkonformer ECU keine Codierung und löst keine Stellgliedtests aus.

Keine Verantwortung für abweichendes Verhalten fehlerhafter ELM327-Klone oder nicht normkonformer Steuergeräte. Erst am Tisch mit ELM327-Simulator testen, dann Fahrzeugtests bei sicherem Fahrzeugstillstand. Details: [Sicherheitskonzept](docs/SAFETY.md).

## Firmware / Hardware

- Board: ONX3248G035, ESP32-S3, LCD 320 × 480, 16 MB Flash, 8 MB OPI-PSRAM.
- Arduino ESP32 Core **3.3.2**; Bibliotheken **lvgl 8.3.11**, GFX Library for Arduino, Adafruit CST8XX Library, Adafruit PCF8574.
- Arduino-CLI FQBN: `esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB`
- Quellcode: `firmware/ObdDisplay/ObdDisplay.ino`.
- Standardadresse des zuvor verwendeten ELM327-WLAN-Dongles: `192.168.0.10:35000`. WLAN-SSID und tatsächliche Adapteradresse können abweichen.

### Auf Raspberry Pi 5 bauen (ohne Flashen)

Der sichere **Offline-Build** hat keine `Config.h`, startet daher weder WLAN noch OBD-Kommunikation. Die LVGL-Konfigurationsflags werden durch das Buildskript auch an die C-Quelldateien der Bibliothek übergeben; ohne diese Flags schlägt LVGL 8.3.11 beim Arduino-CLI-Build fehl.

```bash
# In ~/Projekte/OpenNextion_ELM327_Interface, auf Entwicklungsbranch:
bash scripts/pi/doctor.sh
bash scripts/pi/build.sh offline
```

Wenn die Arduino-CLI fehlt: `bash scripts/pi/install-arduino-cli.sh`.
Wenn ESP32-Core bzw. Bibliotheken fehlen: `bash scripts/pi/setup-toolchain.sh`.
Diese zwei Schritte installieren Pakete **nur im Benutzerprofil** (großer Download). Der eigentliche Build erzeugt keine Systemänderungen außer Artefakten im lokalen `build/`-Ordner.

Buildausgaben: `build/firmware-offline/` und `build/logs/firmware-offline-build.log`.
Zum reinen Kompilieren des vernetzten Firmwarezweigs erst eine **lokale, git-ignorierte** `firmware/ObdDisplay/Config.h` anlegen und anschließend `bash scripts/pi/build.sh connected` ausführen. **Keiner** dieser Befehle flasht das Board oder öffnet eine ELM327-Verbindung.

**Wichtig:** Der Offline- und Connected-Build werden zusätzlich in GitHub Actions kompiliert. Ein erfolgreicher Build ersetzt keinen Hardwaretest.

Erst nach erfolgreich bestandenem Offline-Test für den verbundenen *Laborbetrieb* `firmware/ObdDisplay/Config.h.example` nach `Config.h` kopieren und SSID, Passwort und Adapteradresse lokal anpassen. `Config.h` ist von Git ausgenommen. Es existiert **kein automatischer Upload/Flash-Vorgang** in CI.

Native Policy-Tests (ohne ESP32-Toolchain):

```bash
g++ -std=c++17 -Wall -Wextra -Werror -Ifirmware/ObdDisplay tests/test_read_only_policy.cpp -o /tmp/obd-policy-test
/tmp/obd-policy-test
```

## Geplante Ausbaustufen

1. **v0.1**: Display-/Touch-Bring-up, geschlossene Read-only-Allowlist, Basis-PIDs, Roh-DTCs und RPM-Chart (dieser Prototyp).
2. **v0.2**: ISO-TP-/Multi-ECU-Parser, vollständige DTC-Dekodierung, Freeze-Frames, Mode-01-PID-Erkennung, Timeout-/Reconnect-Härtung und Parser-Fuzzing.
3. **v0.3**: MicroSD-CSV mit RTC-Zeit, Rotationsprinzip, Stromausfall-Schutz, Messintervalle und Export; Diagnoseprofile.
4. **v0.4**: Zusätzliche Lesedaten (Fuel Trims, MAF, MAP, Readiness, Mode 06) und M272-spezifische Auswertung, sofern ECU unterstützt.
5. **Später, separates Projekt-/Firmwareprofil**: potenziell schreibende Diagnosedienste – niemals Bestandteil des Read-only-Images, nur nach expliziter Freigabe und detailliertem Sicherheitskonzept.

**Quellen:** [ONX3248G035 Herstellerunterlagen](https://github.com/OpenNextion/OpenNextion-SKU-ONX3248G035), [Schwesterprojekt OpenNextion_Hermes_Dashboard](https://github.com/mason82-dotcom/OpenNextion_Hermes_Dashboard), [ELM327 Android-Monitor](https://github.com/mason82-dotcom/ELM327).
