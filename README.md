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
- Gespeicherte (03), ausstehende (07) und permanente (0A) DTCs werden dekodiert (CAN mit Count-Byte, ISO-TP-Indexformat `00A`/`0:`/`1:`, Legacy-7-Byte-Frames, mehrere ECU-Blöcke mit Duplikatfilter). Geprüft gegen die Dekodier-Vektoren der Android-App (`tests/test_obd_dtc_vectors_android.cpp`); am Fahrzeug noch nicht validiert.
- Diagnose-Protokollzeilen über USB-Serial und (v0.3, im Verbindungsmodus) als CSV auf MicroSD: `/obd/S####_P##.csv`, Mode-06-Aussetzerzähler mit `pid`=`06A2`…`06A7`, eine Datei pro Sitzung, neue Teildatei ab 4 MB, Schreibpuffer jede Sekunde auf die Karte, Status auf der INFO-Seite. Noch keine Uhrzeit (keine RTC): Zeilen tragen Millisekunden seit Start.
- Zentrale, geschlossene Allowlist; keine Mode 04/08, keine UDS-Schreibdienste, keine frei eingebbaren CAN-/ELM-Befehle, kein Terminal.

## Sicherheit

Die Firmware kann ausschließlich Kommandos senden, die in `ReadOnlyPolicy.h` fest vorgegeben sind. Adapter-Befehle `ATZ/ATE0/ATL0/ATS0/ATH0/ATSP0/ATDPN` konfigurieren bzw. lesen den ELM327-Adapter. Fahrzeugseitig sind ausgewählte Mode-01-PIDs, feste Mode-02-PIDs aus Freeze Frame 00, DTC-Lesearten 03/07/0A, Mode-06-Unterstützung und Aussetzer-MIDs sowie ausschließlich `0900/0902` für FIN-Unterstützung/FIN erlaubt. Ein OBD-Lesezugriff sendet Diagnoseframes an das Steuergerät; das ist **keine passive CAN-Aufzeichnung**, verändert aber bei normkonformer ECU keine Codierung und löst keine Stellgliedtests aus.

Keine Verantwortung für abweichendes Verhalten fehlerhafter ELM327-Klone oder nicht normkonformer Steuergeräte. Erst am Tisch mit ELM327-Simulator testen, dann Fahrzeugtests bei sicherem Fahrzeugstillstand. Details: [Sicherheitskonzept](docs/SAFETY.md).

## Firmware / Hardware

- Board: ONX3248G035, ESP32-S3, LCD 320 × 480, 16 MB Flash, 8 MB OPI-PSRAM.
- Arduino ESP32 Core **3.3.2**; Bibliotheken **lvgl 8.3.11**, GFX Library for Arduino, Adafruit CST8XX Library, Adafruit PCF8574.
- **ELMo 1.1.2** ist mit gezielten Transportanpassungen direkt unter `firmware/ObdDisplay/src/ELMo/` eingebunden (MIT, fester Upstream-Commit). Keine separate ELMo-Installation erforderlich. Architektur, Unterschiede und Prüfungen: [ELMo-Integration](docs/ELMO-INTEGRATION.md).
- Arduino-CLI FQBN: `esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB`
- Quellcode: `firmware/ObdDisplay/ObdDisplay.ino`.
- Fahrzeugprofil: `firmware/ObdDisplay/VehicleProfile.h`, aktiv **C350 S204 M272.961 (2008)**, Heckantrieb / Baumuster 204.256 / 200 kW, gemäß Nutzerangabe und Mercedes-Archiv. 6 Zylinder, 2 Bänke, erwartet CAN, wird nur geprüft und nie erzwungen. Generisch: `-DOBD_VEHICLE_GENERIC`.
- Standardadresse des zuvor verwendeten ELM327-WLAN-Dongles: `192.168.0.10:35000`. WLAN-SSID und tatsächliche Adapteradresse können abweichen.

### Recherchierte Funktionen für den C350 S204 / M272.961

- **21 Live-PIDs:** die bisherigen 15 plus Zündwinkel Zylinder 1, Motorlaufzeit,
  Tankentlüftungs-Sollwert, Tankfüllstand und ECU-Katalysatortemperatur B1/B2.
  Abfragen erfolgen ausschließlich nach positiver Mode-01-Unterstützung.
- **DTC → HU/AU:** ergänzt FIN über `0900/0902` (CAN) und Freeze Frame 00 mit
  Drehzahl, Tempo, Last, Temperaturen, Fuel Trims, MAF/MAP, Drossel und Zündwinkel.
  Ein eindeutig gespeicherter Freeze-Frame-DTC und die eigene Mode-02-Bitmap
  entscheiden, welche Snapshot-Werte gelesen werden.
- **ALLE PIDS → MODE 06:** liest zunächst die MID-Unterstützungskette bis `06A0`.
  Nur angekündigte Aussetzer-MIDs A2–A7 werden abgefragt. Nicht unterstützt,
  widersprüchlich oder nicht sicher lesbar ergibt keine erfundenen Nullzähler.
- Quellen, Formeln, Fahrzeugzuordnung, Simulatorprüfung und Grenzen gegenüber
  Mercedes-Spezialdiagnose: [M272/S204-Diagnosefunktionen](docs/M272-S204-DIAGNOSTICS.md).

Die neuen Funktionen sind implementiert und werden mit synthetischen
LAB-Daten geprüft; die tatsächlich vom Fahrzeug angebotenen PIDs/MIDs sind
ohne Fahrzeugantworten weiterhin nicht bestätigt.

### Ursprünglichen WLAN-Dongle konfigurieren

Der bisherige TCP-Endpunkt `192.168.0.10:35000` ist in `Config.h.example`
vorbelegt und entspricht dem bisherigen Android-Projekt. Für die WLAN-Anmeldung
werden zusätzlich die tatsächliche **SSID** und gegebenenfalls das
**WLAN-Passwort** benötigt; diese sind nicht im Repository hinterlegt.
Der TCP-Transport implementiert keinen separaten Benutzername-/Passwort-Login.

Die lokale `firmware/ObdDisplay/Config.h` aus der Vorlage erstellen und nur dort
die WLAN-Zugangsdaten eintragen. Bei einem bestätigten offenen Dongle-WLAN
bleibt `ELM_WIFI_PASSWORD` leer. Die Datei ist Git-ignoriert. Ihre Anwesenheit
aktiviert den Connected-Build; für Offline- und LAB-Build muss sie fehlen.
Die Integration baut und testet Firmware, flasht aber kein Board.

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

**Bestätigter Pi-WLAN-Labortest (2026-11-03, Stand `f22c914`):** Lab-Build
auf dem Pi 5 (76 Tests OK, `BUILD OK`, 41 % Flash, 37 % RAM) geflasht.
Seriell: `OBD: profile MB C350 W204 M272 (2008)`, `CAN=1`, keine
`PROFILE_WARN`. Mode-01-Polling stabil mit 46–60 ms Antwortzeit pro
Anfrage (ein TCP-Segment, Nagle aus), keine Timeouts oder Reconnects.
MODE 06: `06A2`–`06A7` gesendet (je ca. 80 ms) und alle sechs
**Mehrframe**-Antworten dekodiert, Display zeigt Zyl. 1–3 B1 und Zyl. 4–6 B2
mit EWMA/Zyklus. DTC-Padding per Fixture `PADDED` bestanden (4 Codes,
1 Antwortblock). **Nicht geprüft:** alles am
echten ELM327/Fahrzeug. Ab dieser Version schreibt Mode 06 zusätzlich
`CSV`-Zeilen (`ms,06A2,Zyl1 EWMA,0,count,latency`) auf Serial und SD.

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
`CSV,`-Messzeilen erscheinen über USB-Serial und zusätzlich als SD-Datei, wenn eine Karte steckt.

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
alle M272-Erklärungstexte, Uhrzeit (RTC) im SD-Log, ein
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
