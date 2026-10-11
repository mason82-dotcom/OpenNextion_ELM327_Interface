# OpenNextion ELM327 Interface

Eigenständiger OBD-II-Monitor auf dem **OpenNextion ONX3248G035** (ESP32-S3, 3,5" Touch) mit ELM327-WLAN-Adapter. Kein Smartphone und kein Raspberry Pi im Fahrzeug notwendig.

> **Status: v0.2 LAB hardwarevalidiert – LIVE/GRAPH/DTC/INFO, ESP32-S3 → WLAN → Pi-Simulator, drei Standard-PIDs und manuelle DTC-Rohdaten auf Fotos bestätigt. Echter ELM327-Dongle und Fahrzeugdiagnose weiterhin NICHT validiert.**
> Hardware-/Fahrzeugfunktionen dürfen erst nach Display-, WLAN-, Parser- und Read-only-Prüfungen als funktionsfähig gelten.

## Umfang von v0.1/v0.2

- Originale ONX3248G035-GPIO-Belegung für ST7796-LCD, CST826-Touch und PCF8574-Reset, übernommen aus dem Schwesterprojekt OpenNextion_Hermes_Dashboard.
- Ohne lokale `Config.h` **vollständig offline**: Display-/Touchtest ohne WLAN und ohne OBD-Traffic.
- Optionaler WLAN/TCP-Betrieb mit ELM327-Adapter über feste, typisierte Leseanfragen.
- Abfrage der unterstützten Standard-PIDs (Mode 01 PID 00). Drehzahl, Kühlmitteltemperatur und Geschwindigkeit bei Unterstützung, jeweils nur als Livewerte.
- **v0.2:** Navigation über vier Touchseiten LIVE, GRAPH, DTC und INFO; dunkles Diagramm mit 48 Messpunkten. 
- **v0.2:** Getrennter Touch-Test und manuelle DTC-Schaltfläche: Seitenwechsel/Touch-Test lösen keine OBD-Abfragen aus. 
- **v0.2:** WLAN-/ELM-Verbindungsanzeige und TX-/Timeout-/Antwortpuffer-Zähler (am Pi-Simulator bestätigt; echter Adapter nicht geprüft).
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

**Am Pi erfolgreich aufgespielt: Offline-v0.2 und Labormodus mit Pi-Simulator.** [Schritt-für-Schritt-Anleitung](docs/TOUCH-UI-V0.2-TEST.md). Zuerst `git pull --ff-only`, dann `bash scripts/pi/build.sh offline`. Erst nach erfolgreichem Build gezielt flashen.

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

Für den sicheren Pi-Simulator-Test das **separate** Laborprofil mit `LabConfig.h.example` und `bash scripts/pi/build.sh lab` nach [ELM-SIMULATOR.md](docs/ELM-SIMULATOR.md) verwenden. Das Fahrzeugprofil mit `Config.h` ist noch **nicht freigegeben**. Lokale Zugangsdaten bleiben aus Git ausgeschlossen. Es existiert **kein automatischer Upload/Flash-Vorgang** in CI.

Native Policy-Tests (ohne ESP32-Toolchain):

```bash
g++ -std=c++17 -Wall -Wextra -Werror -Ifirmware/ObdDisplay tests/test_read_only_policy.cpp -o /tmp/obd-policy-test
/tmp/obd-policy-test
```

## Aktueller Entwicklungsstand / Laborprüfung

**Bestätigter Pi-WLAN-Labortest (2026-10-11):** Im separaten `lab`-Profil
meldet das echte OpenNextion-Display `LAB: Pi-Simulator verbunden`. Auf der
LIVE-Seite wurden 1.234 rpm, 90 C und 0 km/h angezeigt; auf GRAPH die
Drehzahlkurve, auf DTC die simulierten Antworten `43 00`, `47 00`,
`4A 00` und auf INFO die bestätigte Simulator-Kennung. Zum Zeitpunkt
des Fotos waren 175 Befehle bei 0 Timeouts und 0 Pufferfehlern gezählt.
Das Serverprotokoll zeigt zyklische Leseanfragen `010C`, `0105`, `010D`
sowie einmal `03`, `07`, `0A`. Das ist **kein Fahrzeugtest** und
ersetzt keine Prüfung an einem echten ELM327-Adapter.

**UI-Nachbesserung (nach Laborfoto, noch einmal am Board zu prüfen):**
Die größeren Messwertkarten zeigen dynamisch nur `1234 rpm`, `90 C`,
`0 km/h` statt wiederholter Überschriften; nach Verbindungsverlust
gelten dieselben kurzen Platzhalter. Die GRAPH-Unterschrift unterscheidet
nun explizit Offline-, Simulator- und reale Connected-Daten.
Ein Regressionstest verhindert eine Wiederholung dieser Layoutprobleme.



**Hardwarefoto-Befund (v0.2):** Alle Reiter LIVE, GRAPH, DTC und INFO sind sichtbar. Das RPM-Diagramm besitzt einen dunklen Hintergrund, die DTC-Schaltfläche ist im Offline-Modus deaktiviert und auf der INFO-Seite wurde der Touch-Zähler mit 8 registrierten Betätigungen fotografiert. Das ist eine UI-/Touch-Prüfung, **kein** Nachweis für korrekte OBD-Fahrzeugdaten.

**Vor jeder ELM327-Fahrzeugverbindung:** Den Pi-Labortest gemäß
[ELM-SIMULATOR.md](docs/ELM-SIMULATOR.md) durchlaufen. Die Mode-01-Auswertung wurde auf einen strikt zeilenorientierten Parser umgestellt; dieser akzeptiert nur vollständige, headerlose 0x41-PID-Antworten und verwirft unsichere oder fehlerhafte Formate. DTCs bleiben unverändert Rohdaten ohne bestätigte ISO-TP-Dekodierung.

## Android ELM327 v1.0.9 – Funktionsportierung (Feature-Branch)

**Aktueller Entwicklungsbranch:** `feature/elm327-android-function-port-20261011`;
eigener Draft-Pull-Request [#2](https://github.com/mason82-dotcom/OpenNextion_ELM327_Interface/pull/2).
Diese Portierung ist noch **nicht** auf echter Hardware oder am Fahrzeug abgenommen.

Die neue native Firmware integriert die **15 Android-Live-PIDs** inklusive
STFT/LTFT beider Bänke, MAF, MAP, Lambda, ECU-Spannung und Luftdruck;
zusätzlich `0100/0120/0140`-PID-Erkennung, mehrfach abgestuftes Polling,
KOEO/KOER-Basisstatus und eine scrollbare **ALLE PIDS**-Seite.
Manuelle Diagnoseansichten bieten Readiness/HU-AU-Vorcheck,
Freeze-Frame-DTC, Mode-06-Aussetzerwerte sowie konservative DTC-Dekodierung
und ausgewählte M272-Hinweise.
`CSV,`-Messzeilen erscheinen **nur über USB-Serial**, noch nicht als SD-Datei.

**Phase 2 im Entwicklungsbranch:** Die passive Leerlauf-/2500-rpm-Fuel-Trim-
Testführung ist mit frischen Messwerten, Stabilitätsprüfungen und
Ergebnisbericht implementiert und am Display gegen den Pi-Simulator
geprüft (2026-10-11). **Nicht an einem echten ELM327/Fahrzeug getestet.**
Anleitung: [Fuel-Trim-Labortest](docs/FUEL-TRIM-LAB-TEST.md).

**Nicht gleichzusetzen mit vollständiger Android-Funktionsgleichheit:**
ISO-TP-Multiframe-Dekodierung fuer ELM-ATH0-Indexformat
ist am Board gegen den Pi-Simulator geprüft (alle vier Fixtures,
2026-10-11), aber **nicht an einem echten ELM327/Steuergerät**;
siehe [ISO-TP-DTC-Labortest](docs/ISO-TP-DTC-LAB-TEST.md).
Die vollstaendige CAN-/UDS-Mehrsteuergeraete-Dekodierung,
alle M272-Erklärungstexte, persistente CSV-Speicherung, ein
Read-only-Terminal und die gesamte Android-Timeout-/Resync-Logik
sind noch nicht übernommen. Ein Fahrzeugtest ist nicht freigegeben.

Siehe [Feature-Matrix und sicherer Pi-Build](docs/ANDROID-ELM327-FEATURE-PORT.md).
Wegen der erweiterten Antworten muss der laufende Pi-Simulator
**nach `git switch` neu gestartet** werden, bevor das LAB-Image
auf dem Display getestet werden kann.

## Geplante Ausbaustufen

1. **v0.1**: Display-/Touch-Bring-up, geschlossene Read-only-Allowlist, Basis-PIDs, Roh-DTCs und RPM-Chart (Offline-Display und Touch real geprüft).
2. **v0.2 UI**: vier Touchseiten, dunkles Diagramm und Verbindungsdiagnose (Offline- und Pi-Simulator-LAB-Boardtest bestätigt).
3. **Weiterer v0.2 Diagnosestack**: ISO-TP-/Multi-ECU-Parser, vollständige DTC-Dekodierung, Freeze-Frames, Mode-01-PID-Erkennung, Timeout-/Reconnect-Härtung und Parser-Fuzzing.
4. **v0.3**: MicroSD-CSV mit RTC-Zeit, Rotationsprinzip, Stromausfall-Schutz, Messintervalle und Export; Diagnoseprofile.
5. **v0.4**: Zusätzliche Lesedaten (Fuel Trims, MAF, MAP, Readiness, Mode 06) und M272-spezifische Auswertung, sofern ECU unterstützt.
6. **Später, separates Projekt-/Firmwareprofil**: potenziell schreibende Diagnosedienste – niemals Bestandteil des Read-only-Images, nur nach expliziter Freigabe und detailliertem Sicherheitskonzept.

**Quellen:** [ONX3248G035 Herstellerunterlagen](https://github.com/OpenNextion/OpenNextion-SKU-ONX3248G035), [Schwesterprojekt OpenNextion_Hermes_Dashboard](https://github.com/mason82-dotcom/OpenNextion_Hermes_Dashboard), [ELM327 Android-Monitor](https://github.com/mason82-dotcom/ELM327).
