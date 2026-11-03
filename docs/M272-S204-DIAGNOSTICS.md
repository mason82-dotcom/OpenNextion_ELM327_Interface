# C350 S204 / M272.961: recherchierte Diagnosefunktionen

Recherche und Implementierung: 2026-11-03. Fahrzeugangabe des Anwenders:
C350 T-Modell, Baujahr 2008, Heckantrieb, 200 kW / 272 PS.

## Fahrzeugzuordnung und Beleggrenzen

Das [Mercedes-Benz Public Archive zum C350 T-Modell 2007–2011](https://mercedes-benz-publicarchive.com/marsClassic/en/instance/ko/C-350-Estate-2007---2011.xhtml?oid=189508191)
nennt Baumuster **204.256**, Motor **M272.961 / M272 E35**, V6 mit 3498 cm³,
200 kW bei 6000 min⁻¹ und 350 N·m bei 2400–5000 min⁻¹, Saugrohreinspritzung.
Das aktive Profil heißt deshalb `C350 S204 M272.961 (2008)`.
CGI, 4MATIC und M276 werden diesem Fahrzeug nicht unterstellt.

Eine [Daimler-Herstellerinformation LI07.08-P-057165](https://static.nhtsa.gov/odi/tsbs/2013/SB-10062083-2280.pdf)
führt ME 9.7/ME97 für M272 und die Baureihe 204 auf. Daraus folgt keine
Kenntnis der konkreten Software-/Kalibrierungsvariante dieses Fahrzeugs.
FIN, Motornummer und tatsächliche PID-/MID-Unterstützung wurden nicht am
Fahrzeug ausgelesen. Das Profil ist eine begründete Zuordnung anhand der
Nutzerangabe, keine elektronische Bestätigung des eingebauten Motors.

## Was die Firmware ergänzt

Der vorhandene ELMo-Transport, die manuelle Diagnose und die typisierte
Read-only-Allowlist sind die Basis. Alle optionalen Livewerte werden erst
nach positiver ECU-Unterstützung aus `0100/0120/0140` gepollt. Diese Bitmaps
werden nicht aus einem festen Mercedes-Katalog vorgegeben.

### Sechs zusätzliche Livewerte, insgesamt 21 PIDs

Die [PID-Dokumentation des Messgeräteherstellers CSS Electronics](https://www.csselectronics.com/pages/obd2-pid-table-on-board-diagnostics-j1979)
belegt die folgenden Standard-Skalierungen. A/B sind die Nutzdatenbytes,
nach Service- und PID-Kennung. Diese Formeln beschreiben den Standard;
ihre Verfügbarkeit am konkreten M272 bleibt bis zum Auslesen offen.

| Anfrage | Anzeige | Berechnung | Mindestintervall |
|---|---|---|---:|
| `010E` | Zündwinkel Zylinder 1, positiv vor OT | A/2 − 64 ° | 2,5 s |
| `011F` | Laufzeit seit Motorstart | 256·A + B s | 10 s |
| `012E` | angeforderte Tankentlüftung | 100·A/255 % | 6,5 s |
| `012F` | Tankfüllstand | 100·A/255 % | 30 s |
| `013C` | ECU-Katalysatortemperatur Bank 1, Sensor 1 | (256·A+B)/10 − 40 °C | 10 s |
| `013D` | ECU-Katalysatortemperatur Bank 2, Sensor 1 | (256·A+B)/10 − 40 °C | 10 s |

Die Werte erscheinen unter **LIVE → ALLE PIDS** und werden wie die bisherigen
Live-PIDs auf Serial/SD als CSV protokolliert. Ein Intervall ist eine
Anforderungsbegrenzung, keine garantierte Aktualisierungsrate. Bei langsamen
Adaptern steigt die Wartezeit. Der geführte Fuel-Trim-Test verwendet weiter
seine eigene Prioritätsliste für frische RPM-/Tempo-/Trim-Werte.

Wichtige Bedeutung: `010E` ist kein Nockenwellen-Soll-/Istwinkel. `012E` liest
einen Sollwert und betätigt kein Ventil. `012F` wird nicht in vermeintlich exakte
Liter oder Reichweite umgerechnet. Eine ECU-Katalysatortemperatur kann
modelliert sein; die Firmware behauptet keinen zusätzlichen Temperatursensor.

### Freeze Frame 00 im manuellen HU/AU-Bericht

Der [ELM327-Hersteller beschreibt Service 02 als Freeze-Frame-Lesezugriff](https://elmelectronics.com/wp-content/uploads/2020/05/ELM327DSL.pdf#page=33).
Die [Command Tables von python-OBD](https://python-obd.readthedocs.io/en/latest/Command%20Tables/)
dokumentieren die Standarddienste und ihre PID-Bedeutungen.

Unter **DTC → HU/AU** wird zuerst `020200` gelesen. Nur ein eindeutig
dekodierter DTC ungleich `0000` schaltet die weiteren Snapshot-Anfragen frei.
`020000` liefert die eigene Mode-02-Unterstützungsbitmap; eine Live-PID-Bitmap
ersetzt diese nicht. Danach werden nur darin angekündigte Werte gelesen:

| Anfrage, jeweils Frame 00 | Gespeicherter Wert |
|---|---|
| `020400`, `020500` | Last, Kühlmitteltemperatur |
| `020C00`, `020D00` | Drehzahl, Geschwindigkeit |
| `020600`, `020700`, `020800`, `020900` | STFT/LTFT Bank 1 und 2 |
| `021000`, `020B00` | Luftmasse, Saugrohr-Absolutdruck |
| `020F00`, `021100`, `020E00` | Ansauglufttemperatur, Drosselposition, Zündwinkel |

Die Skalierungen entsprechen den jeweiligen Standard-Live-PIDs. Jede Zeile
trägt `FF` und bleibt von aktuellen Messwerten/Live-CSV getrennt. Abweichende
Frame-Nummern, unvollständige Antworten, gemischte Fehlertexte oder
widersprüchliche Werte mehrerer Antwortblöcke ergeben `nicht sicher lesbar`.
Ohne gespeicherten Freeze-Frame-DTC wird kein Snapshot erfunden oder abgefragt.

### FIN über Mode 09

Nach `0900` erfolgt `0902` nur, wenn das Unterstützungsbit für PID 02 gesetzt
ist und ATDPN ein CAN-Protokoll meldet. Der Parser akzeptiert vollständige
CAN-Nachrichten `49 02 01` mit exakt 17 zulässigen FIN-Zeichen, auch im
ELM-Indexformat `014 / 0: / 1: / 2:`. Grundlage ist das
[ELM327-Datenblatt, Seite 44](https://elmelectronics.com/wp-content/uploads/2020/05/ELM327DSL.pdf#page=44).

Unvollständige, widersprüchliche oder fremde Formate ergeben n/v. Es gibt
keine geratene J1850-Reassemblierung und keine automatische Motorcode- oder
Baumusterbestimmung aus der FIN. Ein gültiges Textformat ist kein
Echtheitsnachweis. Die FIN erscheint im manuellen Bericht und erhält keine
neuen Serial-/SD-Logzeilen. `0904`/CAL-ID und andere Mode-09-PIDs sind nicht
Teil der neuen Allowlist.

### Mode-06-Aussetzer: Unterstützung vor Zylinderanfrage

Die [primäre python-OBD-Projektdokumentation](https://python-obd.readthedocs.io/en/latest/Command%20Tables/)
führt `0600/0620/0640/0660/0680/06A0` und A2–A7 für Zylinder 1–6 auf.
Die [Ford-Herstellerdokumentation zur J1979-Misfire-Ausgabe, Seite 16](https://www.fordservicecontent.com/ford_content/catalog/motorcraft/OBDSM804.pdf#page=16)
belegt die standardisierten Test-IDs 0B/0C. Das ist ein Formatbeleg, keine
Bestätigung einer Mercedes-ECU-Unterstützung oder eines Ford-Grenzwerts für M272.

**ALLE PIDS → MODE 06** verfolgt die Unterstützungskette. Nur wenn Bit 0 der
aktuellen Bitmap den nächsten 32er-Bereich ankündigt, wird dieser gelesen.
Nur angekündigte MIDs A2–A7 werden anschließend angefragt. Fehlt die
Unterstützung oder ist eine Bitmap fehlerhaft, entstehen keine Nullzähler.

Die vorhandene Auswertung liest nur UASID 0x24 als unskalierte Ereigniszähler:
TID 0B als ECU-EWMA, TID 0C als gemeldeten letzten/aktuellen Fahrzyklus.
Das sind keine Aussetzer pro Sekunde und keine universellen Mercedes-
Werkstattgrenzen. Bank 1/Zylinder 1–3 und Bank 2/Zylinder 4–6 bleiben die
vorhandene M272-Zuordnung. Ohne ATH1 wird keine ECU-Adresse zugeordnet.
Widersprüchliche Zähler mehrerer Antworten oder Erfolg gemischt mit Fehlertext
werden verworfen; identische Duplikate werden nicht addiert.
Der [Skalierungseintrag im primären python-OBD-Quellcode](https://github.com/brendan-w/python-OBD/blob/a378bdd81d58c67d08050e4244173a9a7dbda73d/obd/UnitsAndScaling.py#L107)
bestätigt UASID 0x24 als vorzeichenlosen Zähler mit Faktor 1.

## Was Mercedes-Unterlagen nicht als freie OBD-PIDs belegen

Die [MBUSA-Publikation StarTuned, März 2014, VVT](https://automotivetechinfo.com/wp-content/uploads/2018/03/Mercedes-Benz-StarTuned-March-2014.pdf)
beschreibt die variable Ventilsteuerung des M272. Ihre Existenz liefert aber
keine dokumentierten ELM327-Abfrage-IDs für Soll-/Istwinkel oder Adaptionswerte.
Entsprechendes gilt für die
[M272/M273-Saugrohrfunktionen in StarTuned, Oktober 2014](https://automotivetechinfo.com/wp-content/uploads/2018/03/Mercedes-Benz-StarTuned-October-2014.pdf).
Die Firmware gibt deshalb aus einem generischen P-Code keine bewiesene
mechanische Einzelursache aus.

Die [MBUSA-Information zur Ausgleichswelle vom 30.10.2015](https://static.oemdtc.com/NHTSA-PDFs/MC-11031629-0001.pdf)
bezieht die beschriebene US-Regelung auf bestimmte Modelljahre 2005–2007 und
Motornummernbereiche. Das wird nicht pauschal auf einen S204 Baujahr 2008
übertragen. Motornummer, Tauschmotor-/Reparaturhistorie und konkrete Befunde
wären für eine fahrzeugbezogene Einordnung erforderlich.

Herstellerinterne Fehlernummern wie `2237` aus LI07.08-P-057165 sind außerdem
nicht automatisch der SAE-Code `P2237`. Die Firmware erfindet solche
Übersetzungen nicht.

| Noch nicht implementiert | Für eine belastbare Erweiterung benötigt |
|---|---|
| Nockenwellen-Soll-/Istwinkel, Adaptionen | gültige ME9.7-Variante, dokumentierte Requests/DIDs, Nutzdatenformat, Einheiten |
| Mercedes-Laufruhe-/Gemischadaptionen jenseits SAE-STFT/LTFT | dokumentierte Wertezuordnung und konkrete ECU-Antworten |
| 722.9-Getriebeöltemperatur | verifiziertes EGS-Protokoll, Adressierung, lesender Dienst und Skalierung |
| ABS/ESP, SRS, ASSYST, vollständiger Steuergeräte-Kurztest | jeweilige herstellerspezifische Kommunikations-/Datenbeschreibung |

Keine geratenen Service-22-IDs oder CAN-Header werden freigeschaltet.
Codierung, Fehlerlöschen, Adaptionsreset und Stellgliedtests gehören nicht
zum Read-only-Image.

## Reproduzierbare LAB-Prüfung

Der Simulator enthält ausschließlich synthetische Daten. Sein neues Profil
meldet alle sechs zusätzlichen PIDs, die sechs MID-Unterstützungsbereiche
und eine ausdrücklich erfundene FIN. Die Fixture `PADDED`, `MULTI` oder `TWO`
aktiviert zusätzlich einen synthetischen Freeze-Frame-DTC P0171.

```bash
python3 -m unittest discover -s tests -p 'test_*.py' -v
python3 tools/elm327_lab_simulator.py --bind 127.0.0.1 --port 35000 \
  --dtc-file /tmp/m272-lab-dtc
```

Zweites Terminal, nur Simulator-Fixture ändern:

```bash
printf 'PADDED\n' > /tmp/m272-lab-dtc
```

Für das Board ist statt Loopback die tatsächlich geprüfte Pi-LAN-IP und ein
separates LAB-Checkout ohne Fahrzeug-`Config.h` mit eigener `LabConfig.h`
erforderlich. Diese Änderung hat keinen Dienst auf einem Pi verändert und
kein Board geflasht.

Erwartete synthetische Werte: Zündwinkel 11,0°, Laufzeit 300 s,
Tankentlüftungs-Sollwert 25,1 %, Tankfüllstand 50,2 %, Kat B1/B2 660/650 °C.
Im HU/AU-Bericht: FIN `WDD2042561F123456`, Freeze-DTC P0171, beispielsweise
FF-Drehzahl 1234 min⁻¹. Diese Zahlen sind Fixtures, keine Fahrzeugmessungen.

Die Tests führen die tatsächlichen Diagnose-Handler mit Host-Doubles und
dem tatsächlichen ELMo-Sendepfad aus. Sie prüfen fehlende Unterstützung,
vorzeitiges Ende der MID-Kette, gezielte Zylinderauswahl, fehlenden Snapshot,
Diagnosewechsel während einer Antwort und keine FIN-Ausgabe im Serial-Log.
Native Parserfälle prüfen zusätzlich widersprüchliche ECU-Antworten,
falsche Frame-Nummern, abgeschnittene ISO-TP-Nachrichten und Transportfehler.

Ein erfolgreicher Host-/Simulator-/Buildtest bestätigt weder die tatsächlich
angebotenen Mercedes-Messwerte noch die Bedienreaktion auf dem realen Display.

## Validierung des zusammengeführten Stands

Am 2026-11-03 wurde der inzwischen auf `main` integrierte inkrementelle
DTC-Empfang (`b982245`) übernommen und mit der typisierten ELMo-Sitzung
verbunden. Die Firmware nutzt für alle Anfragen denselben festen Puffer;
der DTC-Pfad kehrt weiterhin zur Hauptschleife zurück. Auch verworfene
Steuerbytes zählen zur Grenze von 1024 Rohbytes pro Antwort.

- 88 Python-Unittests lokal bestanden, einschließlich tatsächlicher
  ELMo-Implementierung und zusammengeführter Diagnose-Handler mit Host-Doubles.
- Alle elf nativen C++-Testprogramme mit
  `-std=c++17 -Wall -Wextra -Werror` kompiliert und erfolgreich ausgeführt.
- Lokale `Config.h` ist Git-ignoriert; kein Fahrzeugpasswort und keine
  erzeugten Firmware-Dateien werden veröffentlicht.
- Die drei Firmware-Profile werden nach dem Abgleich mit `main` erneut gebaut.
  Den laufenden GitHub-Nachweis enthält der Firmware-Build-Workflow am PR.
  Bis zu dessen Abschluss wird kein erfolgreicher Build dieses Stands behauptet.
