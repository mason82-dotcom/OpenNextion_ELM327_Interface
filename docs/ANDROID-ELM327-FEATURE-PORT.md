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

**Nachfolgekorrekturen (Quellcode eingecheckt, Hardwaretest noch offen):**
HU/AU-, Mode-06- und USB-CSV-Ausgaben unterscheiden LAB-Simulation von
ECU-Lesedaten. Bei einem abgebrochenen TCP-Adapterkanal werden
HU/AU- und Mode-06-Reports sowie DTC-Status ausdrücklich als
**nicht mehr gültig** markiert; kein offener Scan wird nach einem
Reconnect automatisch fortgesetzt. Fehlende Antworten auf bereits
gelernte PIDs setzen den jeweiligen Messwert auf `--` statt einen
veralteten Wert weiter anzuzeigen. Die INFO-Anzeige bezeichnet den
kombinierten Zähler für fehlende/ungültige Antworten als
**Antwortfehler** und nicht irreführend nur als Pufferfehler.

**Sitzungsverlust jetzt auf echter Hardware validiert:** Weitere Nutzerfotos
zeigen während eines gestoppten Pi-Simulators `-- rpm`, `-- C`,
`-- km/h`, ausdrücklich ungültige Mode-06-Ergebnisse und die Warnung
`Verbindung verloren: vorherige HU/AU-Daten ungueltig. Neu lesen.`
Die Verbindung wurde zuvor nach einem Timeout erfolgreich erneut
aufgebaut. Damit ist die Invalidierung für den gezeigten Verlustfall
bestätigt; ein kontrollierter ungültiger PID bei **bestehender TCP-Verbindung**
ist noch separat zu testen.

**Nachfolgende UI-Korrektur (noch nicht auf Hardware geprüft):**
Im LAB-Modus erscheint bei fehlendem Pi die eindeutige Meldung
`LAB: Pi-Simulator nicht erreichbar`. Beim Verbindungsverlust werden die
HU/AU- und Mode-06-Scrollpositionen zurück an den Anfang gesetzt,
damit die Ungültigkeitswarnung sofort sichtbar wird.

Die Python-Integrationstests sind zusätzliche Quellcode-Checks,
keine Hardware- oder Kommunikationsnachweise. Vor jedem erneuten Flashen
müssen CI und `bash scripts/pi/build.sh lab` erfolgreich sein.

## Gegenüberstellung

| Android-ELM327 v1.0.9 | ESP32-S3 | Grenzen |
| --- | --- | --- |
| 15 Live-PIDs, STFT/LTFT B1+B2, MAF, MAP, Last, Tempo, Drossel, Temperatur, ECU-Spannung, Lambda, Baro | **Portiert** in `ObdPidCatalog.h` | Anzeige über scrollbare Seite; Pollzeiten abhängig vom Adapter |
| Erkennung unterstützter PIDs | **Portiert** für `0100/0120/0140` und mehrere ECU-Antworten | Headerlose, eindeutige Mode-01-Antworten |
| KOEO/KOER | **Basisfunktion portiert** | Erkennung anhand RPM=0 bzw. RPM>0; kein alternativer Motorstatus |
| Getaktetes Polling | **Portiert** | Nacheinander, keine gleichzeitigen TCP-Anfragen |
| DTC 03,07,0A | **ELM ATH0-ISO-TP-Indexformat in Phase 3 portiert, ungetestet am Board** | Nur strikt vollständige `00A`/`0:`/`1:`-Nachrichten; ATH1/Raw-CAN und interleavte ECU-Ströme weiter nur Rohdaten |
| M272-Codehinweise | **Auszug portiert** | Schlüsselcodes P0016–19, P0171/174, P030x, P200x, P0128, P0101 |
| HU/AU-Check | **Vorprüfung portiert** | MIL, DTC-Zahl, Monitore, Laufstrecken, Warmläufe, Freeze-DTC und Code-Abfragen. Keine amtliche HU/AU-Aussage |
| Readiness 0101 | **Portiert** | Mehrere gültige ECU-Antworten zusammengeführt |
| Mode06 Zylinder-Aussetzer | **Portiert** für `06A2…06A7` | Nur unzweideutige UASID 0x24-Zähler. Unbekannte Werte als nicht verfügbar |
| Freeze-Frame | **DTC aus 020200 portiert** | Vollständige Snapshot-PID-Anzeige noch ausstehend |
| Geführter 20s Leerlauf / 15s 2500 rpm-Fuel-Trim-Test | **Phase 2 implementiert – noch keine Hardware-Abnahme** | Nur neue frische STFT-/LTFT-Werte, RPM-/Tempo-Stabilitätsfenster, Timeout statt Scheinbericht. [LAB-Testanleitung](FUEL-TRIM-LAB-TEST.md) |
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
  den manuellen Aussetzercheck für Zylinder 1 bis 6. **TRIM-TEST**
  führt zur passiven Leerlauf-/2500-rpm-Testseite; keine Motorbefehle.
- **INFO**: Linkstatus, Befehls-/Timeoutzähler, Touch-Test.

Die bisherigen Einzel-Diagnoseansichten pausieren das Live-Polling während
ihrer Einzelabfragen. Der Fuel-Trim-Test verwendet **stattdessen** das
gleiche schreibgeschützte sequenzielle Mode-01-Live-Polling und priorisiert
RPM, Tempo und die vier Trim-Kanäle. Alle Kommandos passieren dieselbe
geschlossene, typisierte `ReadOnlyPolicy.h`-Allowlist.

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


## Gezielter NO-DATA-Test ohne TCP-Trennung

Der Simulator besitzt eine **explizit einzuschaltende Laborfehler-Injektion**.
Hierbei bleibt die TCP-Verbindung erhalten; nur eine ausgewählte
Mode-01-PID bekommt die ELM-Antwort `NO DATA`.
Unbekannte/unsichere Kommandos werden weiterhin mit `?` abgelehnt.
Der Simulator kann keine Fahrzeugbefehle weiterleiten.

**Vorbereitung:** Die derzeit laufende Simulator-Instanz mit Strg+C
beenden. Die Firmware verwendet weiter die vorhandene lokale
`LabConfig.h` und den LAB-Build; kein Wechsel zum Fahrzeugprofil.

Terminal A auf dem Pi:

```bash
cd ~/Projekte/OpenNextion_ELM327_Interface
rm -f /tmp/elm327-lab-pid-fault
python3 tools/elm327_lab_simulator.py \
  --bind 192.168.178.63 --port 35000 \
  --fault-file /tmp/elm327-lab-pid-fault
```

Nach vollständiger Verbindung und angezeigten Testwerten im zweiten
Pi-Terminal den Fehler **ohne Server-Neustart** einschalten:

```bash
printf '010C\n' > /tmp/elm327-lab-pid-fault
```

Erwartet: Das Simulatorprotokoll meldet
`[LAB] INJECT NO DATA 010C (TCP remains open)`. Die ESP32-Firmware
setzt Drehzahl auf `-- rpm`, der INFO-Zähler `Antwortfehler` steigt.
Temperatur und Tempo bleiben weiterhin verfügbar. Der Verbindungsstatus
sollte unverändert `LAB: Pi-Simulator verbunden` anzeigen.

Fehler wieder ausschalten:

```bash
rm -f /tmp/elm327-lab-pid-fault
```

Erwartet: RPM erscheint nach der nächsten gültigen `010C`-Antwort
erneut mit 1234 rpm – **ohne TCP-Neustart und ohne Löschen von Fehlercodes**.

Andere einzelne Mode-01-PIDs, etwa `0105` oder `0107`, lassen sich
entsprechend testen, sofern sie in der Simulator-Whitelist stehen.
Bei diesem Test dürfen keine echten ELM327-Adapter oder Fahrzeuge
angeschlossen sein.

### Hardware-Ergebnis: Einzel-PID `NO DATA` – bestanden

Der Anwender hat vier Displayfotos vom Test auf dem echten ESP32-S3
bereitgestellt. Es handelt sich ausschließlich um Simulatorwerte:

| Zustand | Sichtbares Ergebnis | Bewertung |
| --- | --- | --- |
| `010C` absichtlich `NO DATA` | `-- rpm` | Alter RPM-Wert ungültig gesetzt |
| Während des PID-Fehlers | 90 C Kühlmittel und 0 km/h | Andere PIDs weiter verfügbar |
| Während des PID-Fehlers | `LAB: Pi-Simulator verbunden` | TCP-Session bleibt bestehen |
| INFO beim Fehler | 1.234 gesendete Befehle, 2 Timeouts, 8 Antwortfehler | Fehler gezählt; Timeouts kumuliert |
| Fehler-Injektion entfernt | 1.234 rpm bei weiterhin bestehender Verbindung | Neuer gültiger Messwert automatisch übernommen |
| INFO später | 1.319 gesendete Befehle, 2 Timeouts, 11 Antwortfehler | Antwortfehler steigen; Timeout-Zahl bleibt unverändert |

Die Fotos bestätigen die lokale PID-Invalidierung und automatische
Wiederaufnahme nach Rückkehr gültiger Antworten. Die sichtbaren
Antwortfehler steigen um drei; die Anzahl an Timeouts bleibt konstant.
Das ist eine gezielte Einzelantwort-Störung, **kein** Nachweis gegen
sämtliche möglichen Adapterfehler, CAN-Header-/ISO-TP-Formate oder
Fahrzeugsteuergeräte. Ein anschließender Reboot-/Langzeittest ist offen.

Der Simulator besitzt keinerlei Verbindung zu einem Fahrzeug; seine
Antworten dürfen nicht als reale Diagnosewerte interpretiert werden.



## Phase 3: ISO-TP-DTC-Indexformat

Der neue Parser `ObdIsoTp.h` setzt ausschließlich das vom ELM327
mit `ATH0` ausgegebene Indexformat (Länge `00A`, Segmente `0:`, `1:`)
zusammen. Er prüft Längen, Puffergrenzen und Segmentreihenfolge.
Mehrere nacheinander empfangene DTC-Antwortblöcke werden aggregiert.
Die DTC-Detailsicht zeigt bis zu 24 einzigartige Codes pro
Leseart sowie generische Android-DTC-Beschreibungen. **Eine
ECU-Adresszuordnung ist mit `ATH0` nicht möglich.**
Der HU/AU-Vorcheck verwendet denselben Parser. Kein neuer
Fahrzeugbefehl wurde freigeschaltet.

Diese neue Funktion ist **noch nicht am Display oder Adapter geprüft**.
[Pi-Simulatortest und Grenzen](ISO-TP-DTC-LAB-TEST.md).

## Phase 2: Geführter Fuel-Trim-Test

Der vom Android-Projekt portierte C++17-Testzustandsautomat befindet sich in
`firmware/ObdDisplay/FuelTrimTest.h` und die native Regression in
`tests/test_fuel_trim.cpp`. Die neue Touchseite `TRIM-TEST` startet,
führt und beendet die passive Messung. Der Pi-Simulator erlaubt eine
**manuelle Test-RPM-Änderung** über `--rpm-file` ohne Fahrzeug und ohne
andere Kommandos. Genaue Zeiten, Voraussetzungen, sichere Pi-Kommandos,
Fehlersimulation: [FUEL-TRIM-LAB-TEST.md](FUEL-TRIM-LAB-TEST.md).

**Noch offen:** GitHub- und lokaler ESP32-Compile sowie der praktische
45+-Sekunden-Zwei-Phasen-Test am Display. Eine alte erfolgreich
geflashte LAB-Firmware enthält diese Funktionen noch nicht.
