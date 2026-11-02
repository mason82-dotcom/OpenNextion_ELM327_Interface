# DTC Multiframe (ELM327 ATH0) – Entwicklungs- und LAB-Test

**Stand 2026-10-11: am ESP32-S3 gegen den Pi-Simulator geprüft
(alle vier Fixtures, siehe Prüfstatus). Kein Test an einem echten
ELM327 oder Steuergerät.**

## Neue Funktion

Die Diagnose-LED-/Fehlercodefunktionen aus der Android-App werden um
eine **begrenzte ELM327-ISO-TP-Reassemblierung** erweitert.

- Lesearten: `03` gespeicherte, `07` ausstehende und `0A` permanente
  abgasrelevante Standard-OBD-Fehlercodes; **niemals Modus `04`**.
- Einzelne CAN-Antworten (`43 01 01 71`) bleiben lesbar.
- Mehrteilige **ELM-formattierte Antworten ohne CAN-Header** können
  korrekt zusammengesetzt werden, etwa:

  ```text
  00A
  0:43 04 01 71 03 01 03
  1:02 03 03
  ```

  Das sind 10 Nutzdatenbytes: vier DTCs P0171, P0301, P0302, P0303.
  `00A` ist die Länge (Hex), `0:`/`1:` sind Segmentindizes.

- Mehrere **aufeinanderfolgende** Antwortblöcke werden gesammelt und
  dieselben DTC-Codes zusammengeführt. Auf der DTC-Seite zeigt
  `DETAILS` die einzelnen Codes und vorhandene generische
  Beschreibungen aus dem Android-Diagnoseprojekt.
- Defekte Längen, unvollständige Nachrichten, falsche Indexfolgen,
  nicht-hexadezimale Zeichen oder unvollständige DTC-Paare führen zu
  **`NICHT SICHER DEKODIERBAR`** und Rohdatenanzeige – keine
  scheinbar korrekten DTCs. Die Puffer sind statisch begrenzt
  (64 Byte je zusammengesetzter Antwort, maximal 24 Code-Einträge).
- HU/AU-Vorcheck verwendet denselben Parser; DTC-Daten werden bei
  Verbindungsabbruch weiterhin als ungültig markiert.

### Grenzen

Der Adapter arbeitet derzeit mit `ATH0` (Header aus).
Daher fehlen die CAN-Identifier: Mehrere Antwortblöcke können zwar
gezählt werden, aber **es lässt sich keine gesicherte ECU-Adresse
zuordnen**. Interleavte Mehrsteuergeräte-ISO-TP-Fragmente, Roh-CAN-
First/Consecutive-Frames mit `ATH1`, herstellerspezifische Mercedes-
Diagnosedienste, UDS und alle Aktortests werden nicht unterstützt.
Die Zustandslogik akzeptiert ausdrücklich nur das dokumentierte
ELM-Indexformat und plausibilisiert keine unbekannten Formate.

Außerdem können Standard-OBD-DTCs ohne Herstellerspezifika vom
Mercedes-ABS/ESP-Fehlerspeicher abweichen. Das ist kein Ersatz für DAS/Xentry.

## Ohne Fahrzeug testen

Die Laborumgebung ist weiterhin `192.168.178.63:35000` auf dem Pi
(aktuelle Pi-IP ggf. mit `ip -br -4 addr` überprüfen).

### 1. Pi-Test-/Buildschritte

```bash
cd ~/Projekte/OpenNextion_ELM327_Interface
git pull --ff-only
g++ -std=c++17 -Wall -Wextra -Werror -Ifirmware/ObdDisplay \
    tests/test_obd_iso_tp.cpp -o /tmp/obd-iso-tp-test
/tmp/obd-iso-tp-test
python3 -m unittest discover -s tests -p 'test_*.py' -v
bash scripts/pi/build.sh lab
```

Erst nach **`BUILD OK; NEVER FLASHED`** sowie erfolgreichen
GitHub-Tests einen neuen Flash-Test erwägen.

### 2. Erweiterten Simulator starten

Ersten Simulator mit Strg+C beenden und neu starten:

```bash
cd ~/Projekte/OpenNextion_ELM327_Interface
rm -f /tmp/elm327-lab-dtc
python3 tools/elm327_lab_simulator.py \
    --bind 192.168.178.63 --port 35000 \
    --dtc-file /tmp/elm327-lab-dtc
```

Ein zweites Pi-Terminal wählt anschließend eine **fest definierte**
Simulation, ohne beliebigen CAN-Text zu akzeptieren:

```bash
printf 'MULTI\n' > /tmp/elm327-lab-dtc
```

Auf dem Display `DTC → DTC lesen → DETAILS`: Die vier simulierten
gespeicherten Codes P0171, P0301, P0302 und P0303 müssen erscheinen.
`07` und `0A` bleiben im Simulator leer.

Weitere Szenarien:

```bash
printf 'TWO\n' > /tmp/elm327-lab-dtc
# zwei sequenzielle Antwortbloecke, zusaetzlich P0174
printf 'TRUNCATED\n' > /tmp/elm327-lab-dtc
# fehlender Folgeblock: unvollstaendig, nur Rohdaten
printf 'BAD_SEQUENCE\n' > /tmp/elm327-lab-dtc
# falscher Segmentindex: unvollstaendig, nur Rohdaten
rm -f /tmp/elm327-lab-dtc
# Rueckkehr zu leeren Standard-Simulator-DTCs
```

Nach jeder Umschaltung **manuell `DTC lesen`** drücken. Die Firmware
sendet keine DTC-Abfragen allein durch einen Seitenwechsel.
Weder Simulator noch Firmware senden `04`, Codierungs- oder
Stellgliedkommandos.

## Prüfstatus

**Boardtest 2026-10-11** (Lab-Image, ONX3248G035 am Pi-Simulator,
Protokoll per `ATDPN` als CAN erkannt; Fotos der DTC-/DETAILS-Seite):

| Fixture | Anzeige DTC-Seite | DETAILS | Ergebnis |
|---|---|---|---|
| `MULTI` | 03: `P0171 (+3)`, Status `4 Codes -> DETAILS` | P0171, P0301–P0303 mit Text und M272-Hinweis, 1 Antwortblock | bestanden |
| `TWO` | 03: `P0174 (+4)`, Status `5 Codes -> DETAILS` | 5 Codes aus 2 Antwortblöcken | bestanden |
| `TRUNCATED` | 03: `RAW …`, Status `RAW -> DETAILS` | `NICHT SICHER DEKODIERBAR` + Rohantwort | bestanden, keine Teilcodes |
| `BAD_SEQUENCE` | 03: `RAW …`, Status `RAW -> DETAILS` | `NICHT SICHER DEKODIERBAR` + Rohantwort | bestanden, keine Teilcodes |

Dabei gefunden und behoben: Die Statuszeile meldete `Keine DTCs`,
obwohl 03 Codes enthielt (letzter Modus 0A leer), und die
mehrzeilige RAW-Vorschau lief in die 07-Zeile.

Native Tests der statischen Parserbibliothek,
Python-Tests für die Auswahl der Laborantworten und Arduino-Builds
sind CI-Aufgaben. Ihre Ausführung ist kein Nachweis für die
Kompatibilität mit realen Mercedes-Steuergeräten.
