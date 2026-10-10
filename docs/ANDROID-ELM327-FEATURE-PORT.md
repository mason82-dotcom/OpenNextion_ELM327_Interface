# ELM327 Android v1.0.9 → OpenNextion ESP32-S3: Feature-Portierung

**Entwicklungsstand:** Phase 1 im isolierten Draft-Branch
`feature/elm327-android-function-port-20261011`. **Die neu portierten
Ansichten wurden am echten Display mit dem Pi-Simulator fotografisch bestätigt.**
Weder echter ELM327 noch Fahrzeug wurden geprüft. Das ist **keine Freigabe
für den Fahrzeugeinsatz**.

## Hardware-Abnahme: Pi-Simulator → ESP32-S3 (LAB)

Die folgenden Werte stammen aus Fotos des realen OpenNextion-Displays,
nicht aus Messungen am Mercedes C350:

| Bereich | Beobachtung | Laborurteil |
| --- | --- | --- |
| 15 Live-PIDs | 1234 rpm, 90 C, 0 km/h, Last 29,8 %, Drossel 12,5 %, MAF 5,00 g/s, MAP 40 kPa, Ansaugluft 30 C, STFT B1/B2 0 %, LTFT B1 7,8 %, LTFT B2 5,5 %, ECU 13,500 V, Soll-Lambda 1,000, Umgebungsdruck 101 kPa | Sichtbar plausibel, Laborwerte |
| KOER | Bei simulierten 1234 rpm wird `KOER: Motor laeuft` angezeigt | Anzeige funktioniert; kein echter Motor beteiligt |
| Mode 06 | Zylinder 1–6 mit Bank 1/2; EWMA 0 und aktueller Zyklus `--` | Korrekt kein aktueller Zähler erfunden |
| HU/AU | Monitorstatus, 0 km mit MIL, 5 Warmläufe seit Reset, 300 km seit Reset, Freeze-DTC keiner, gespeicherte/pending/permanente Codes jeweils 0 | Basis-Readiness und Sequenz im LAB sichtbar |
| INFO | `LAB-Simulator: ID bestaetigt`, 489 gesendete Befehle, 0 Timeouts und 0 Pufferfehler | Im Foto keine Transportfehler gemeldet |
| Touch | Reiterwechsel und Testzähler bis 6 | Funktioniert |

Die angezeigten Daten sind vollständig vom Labor-Simulator vorgegeben.
Eine Labor-Anzeige, die `KOER` meldet, ist **kein Beleg für laufenden
Fahrzeugmotor**. Mode 06 enthält aktuell nur den EWMA-Wert; `--`
bei Zyklus bedeutet `nicht gemeldet`, nicht `null Aussetzer`.

**Nachfolgecommit:** Kennzeichnet HU/AU, Mode 06 und serielle CSV-Ausgabe
nun abhängig vom Build als Simulation bzw. echte Adapterquelle; dieser
Text-Fix benötigt noch einen neuen Build und Displaytest.

## Gegenüberstellung

| Android-ELM327 v1.0.9 | ESP32-S3 | Grenzen |
| --- | --- | --- |
| 15 Live-PIDs, STFT/LTFT B1+B2, MAF, MAP, Last, Tempo, Drossel, Temperatur, ECU-Spannung, Lambda, Baro | **Portiert** in `ObdPidCatalog.h` | Anzeige über scrollbare Seite; Pollzeiten abhängig vom Adapter |
| Erkennung unterstützter PIDs | **Portiert** für `0100/0120/0140` und mehrere ECU-Antworten | Headerlose, eindeutige Mode-01-Antworten |
| KOEO/KOER | **Basisfunktion portiert** | Erkennung anhand RPM=0 bzw. RPM>0; kein alternativer Motorstatus |
| Getaktetes Polling | **Portiert** | Nacheinander, keine gleichzeitigen TCP-Anfragen |
| DTC 03,07,0A | **Teilweise portiert** | CAN/Legacy nur bei sicherem `ATDPN`-Format; komplexes ISO-TP weiter nur Rohdaten |
| M272-Codehinweise | **Auszug portiert** | Schlüsselcodes P0016–19, P0171/174, P030x, P200x, P0128, P0101 |
| HU/AU-Check | **Vorprüfung portiert** | MIL, DTC-Zahl, Monitore, Laufstrecken, Warmläufe, Freeze-DTC und Code-Abfragen. Keine amtliche HU/AU-Aussage |
| Readiness 0101 | **Portiert** | Mehrere gültige ECU-Antworten zusammengeführt |
| Mode06 Zylinder-Aussetzer | **Portiert** für `06A2…06A7` | Nur unzweideutige UASID 0x24-Zähler. Unbekannte Werte als nicht verfügbar |
| Freeze-Frame | **DTC aus 020200 portiert** | Vollständige Snapshot-PID-Anzeige noch ausstehend |
| Geführter 20s Leerlauf / 15s 2500 rpm-Fuel-Trim-Test | **Ausstehend** | Nicht durch ungeprüfte Automatik ersetzen |
| ELM-Kommandoterminal | **Ausstehend** | Kein frei eingebbarer Terminalzugriff; niemals Raw-CAN-Durchleitung |
| CSV | **USB-Serial-Zeilen portiert** | Keine persistente microSD-/Flash-Datei, keine echte Echtzeit-Uhr |
| Robustere Reconnect-, Timeout-Resync-, Session-Policy der Android-App | **Ausstehend** | Aktuelles Fail-Closed-Verhalten trennt bei Syncverlust |
| Fahrzeugprotokoll-Erkennung | **ATDPN portiert** | Adressierungs-/Headeränderungen weiterhin gesperrt |

## Bedienung

- **LIVE**: große RPM-, Kühlmittel- und Tempoanzeigen; Schaltfläche
  **ALLE PIDS** öffnet die scrollbare Sensor- und Fuel-Trim-Liste.
- **GRAPH**: Drehzahlkurve (48 Messpunkte).
- **DTC**: ausschließlich manueller Modus 03/07/0A Scan;
  **HU/AU** öffnet den separaten emissionsbezogenen Vorcheck.
- **ALLE PIDS**: STFT, LTFT, MAF, MAP etc.; **MODE 06** öffnet
  den manuellen Aussetzercheck für Zylinder 1 bis 6.
- **INFO**: Linkstatus, Befehls-/Timeoutzähler, Touch-Test.

Die zusätzlichen Diagnoseansichten pausieren das Live-Polling während
ihrer Einzelabfragen; alle Kommandos passieren dieselbe geschlossene,
typisierte `ReadOnlyPolicy.h`-Allowlist.

## Pi 5: neuen Entwicklungsbranch verwenden

Voraussetzung: das bereits funktionierende Repository und die vorhandene
geheime lokale `LabConfig.h`. Nicht die echte `Config.h` erstellen!

```bash
cd ~/Projekte/OpenNextion_ELM327_Interface
git status --short
git fetch origin
git switch --track origin/feature/elm327-android-function-port-20261011
test ! -f firmware/ObdDisplay/Config.h
test -f firmware/ObdDisplay/LabConfig.h
python3 -m unittest discover -s tests -p 'test_*.py' -v
bash scripts/pi/build.sh lab
```

Wenn der Branch bereits lokal existiert, statt `git switch --track`
`git switch feature/elm327-android-function-port-20261011` verwenden.
`LabConfig.h` wird nicht nach Git eingecheckt.

Da die Simulatorantworten erheblich erweitert wurden, den im ersten
Pi-Terminal laufenden bisherigen Simulator **mit Strg+C beenden** und
neu starten:

```bash
cd ~/Projekte/OpenNextion_ELM327_Interface
python3 tools/elm327_lab_simulator.py --bind 192.168.178.63 --port 35000
```

Die IP `192.168.178.63` ist die zuletzt vom Anwender bestätigte
Pi-Adresse; vor dem Test erneut mit `ip -br -4 addr` prüfen.
Verbindung nur im lokalen WLAN. Noch **nicht** flashen, bevor die CI der
neuen Feature-Portierung und der lokale LAB-Build erfolgreich sind.

## Sicherheit & Validierungsgrenzen

- `offline` ohne WLAN, `lab` nur mit lokaler `LabConfig.h` und
  Pflichtkennung `ELM327 LAB SIM v0.1`, `connected` für echte Adapter
  derzeit nicht freigegeben.
- Adaptersetup und Mode-01/02/03/06/07/0A sind fest kodiert.
  Keine DTC-Löschung, kein UDS-Schreiben, kein Actuator Test, kein
  Terminal, keine frei konfigurierbaren CAN-Header.
- Unbekannte DTC-Transportformate werden roh angezeigt statt geraten.
- Ein fehlerfreier Simulator- oder CI-Test beweist **keine**
  Kompatibilität mit Fahrzeugsteuergeräten.
