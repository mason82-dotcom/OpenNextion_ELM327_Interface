# DTC-Bedienablauf: gezielte Verifikation

Für die inzwischen verfügbaren gezielten Transportfehler und die wiederverwendbare
Ergebnisvorlage siehe [DTC-LAB-Runbook](DTC-LAB-RUNBOOK.md).

Ausgangsbasis: Entwicklungsbranch `feature/elm327-android-function-port-20261011`,
Commit `4ba925443f83f29de18b7ce5ac9bc0c7e9f647ad`. `main` enthält zu diesem
Prüfzeitpunkt nur eine README; PR #1 und der darauf aufbauende PR #2 sind Drafts.

## Befunde und Änderung

Im Ausgangscode setzt jeder weitere Klick auf **DTC lesen** den laufenden
Scan zurück. Ein eigener DTC-Abbruch fehlt. Der bestehende Generationenschutz
verwirft bereits Antworten einer ersetzten Diagnose; der einzige TCP-Sendepfad
und die sequentielle Hauptschleife verhindern paralleles Live-Polling.

- Weitere Start-Klicks werden während eines aktiven DTC-Scans ignoriert.
- **ABBRUCH** auf der DTC-Seite verwirft sämtliche Teilergebnisse und setzt alle drei
  DTC-Zeilen sowie DETAILS sichtbar auf ungültig. Ein Wechsel zu HU/AU,
  Mode06 oder einem erfolgreich gestarteten Trim-Test beendet einen aktiven
  DTC-Scan auf dieselbe Weise.
- Die laufende Antwort wird bis zum Prompt gelesen, aber nicht ausgewertet.
  Erst danach darf die Hauptschleife einen weiteren Befehl senden. Fehlt der
  Prompt, bleibt der vorhandene Timeout-/Disconnect-Pfad wirksam.
- Ein neuer Scan benötigt einen neuen expliziten Start. Zehn Start-Klicks
  während einer Transaktion starten keine zehn Scans.
- Der statische Sendepfad prüft zusätzlich `allowedWire(command)` und gibt
  `OBD_TX,<Millisekunden>,<laufende Nummer>,<statischer Befehl>` über USB-Serial
  aus. Das Protokoll enthält weder WLAN-Zugangsdaten noch rohe Antworten.
  Es belegt den Software-Sendeversuch, nicht die Verarbeitung durch eine ECU.

Die Allowlist bleibt unverändert. Kein Mode 04, keine Codierung, Anpassung,
Stellgliedtests oder ECU-Schreibbefehle. AT-Initialisierung konfiguriert nur den
Adapter. Kein automatisches Flashen, Deployment oder Fahrzeugzugriff.

## Aussagekraft der Prüfungen

`tests/test_dtc_workflow.py` kompiliert und führt die tatsächlichen Funktionen
`onScanButton`, `onDtcCancel`, `cancelDtcScan`, `transact` und `scanOneDtcMode`
aus der Firmware mit C++17 aus. Arduino, LVGL, Uhr und TCP werden durch
Host-Doubles ersetzt; der echte ISO-TP-Parser und die Allowlist werden benutzt.
Das Disconnect-UI ist im Hosttest ein Double und wird ergänzend durch die
bestehenden Quellcodeprüfungen abgesichert. Die Hauptschleife wird dabei nicht
ausgeführt: Pollingpriorität und Ersetzung durch andere Diagnosen werden
ergänzend als Quellcodeverträge geprüft.

Abgedeckt: Mehrfachklick während einer ausstehenden Antwort, Codes in 03 bei
leeren 07/0A, NO DATA ohne falsche Entwarnung, Abbruch nach Teilergebnis,
späte Antwort nach Abbruch, expliziter Neustart während einer alten Antwort,
fehlender Prompt nach Abbruch, ungültiger Command ohne Senden sowie der
gemeinsame Transportpfad für 010C/0105/010D. Die vorhandenen nativen Tests
prüfen PID-Formeln, ISO-TP-Fehlerfälle und verbotene Befehlsformen.

Das sind keine ESP32-Laufzeit-, Display- oder Mercedes-Nachweise. Die im
Repository dokumentierten Boardtests vom 11.10.2026 zu MULTI/TWO/TRUNCATED/
BAD_SEQUENCE sind historische Berichte, keine in diesem Durchlauf neu
beobachtete Hardwareabnahme. Der Parser ist auf das dokumentierte ATH0-
Indexformat begrenzt; keine gesicherte ECU-Zuordnung und keine vollständige
Mercedes-/ABS-/ESP-Diagnose.

## Nächster praktischer LAB-Test

Erst den aktuellen Commit und erfolgreichen ESP32-LAB-Build dokumentieren.
Ein Flashvorgang ist kein Bestandteil dieses Prüfablaufs und erfolgt hier
nicht automatisch. Für den Displaytest muss die passende Firmware bereits
bereitgestellt und der Simulator eindeutig als LAB identifiziert sein.

1. USB-Serial aufzeichnen; 010C/0105/010D im Simulator vorab prüfen.
2. Fixture MULTI wählen, DTC starten und innerhalb der aktiven Abfrage zehnmal
   **DTC lesen** berühren. Falls normale Antworten dafür zu schnell sind,
   den ersten Prompt ausschließlich im Labor gezielt verzögern.
3. Erneut starten und nach dem ersten Modusergebnis **ABBRUCH** berühren.
   Anschließend auch Abbruch während einer verzögerten Antwort prüfen.
4. Einen Prompt im Labor vollständig unterdrücken: Timeout muss den Stream
   schließen. Reconnect ohne DTC-Neustart prüfen. Danach manuell neu lesen.
5. TRUNCATED und BAD_SEQUENCE prüfen; anschließend 010C/0105/010D wiederholen.

Erfolgskriterien: sichtbarer Ladezustand innerhalb 250 ms; genau eine Folge
03/07/0A bei Mehrfachklick; nach Abbruch keine weitere DTC-Anfrage ohne neuen
Start; keine alten Codes als aktuelle Ergebnisse; nach Reconnect kein
automatischer DTC-Neustart; Livewerte nach Ende des Scans innerhalb von zwei
regulären Pollingzyklen wieder aktuell. Ein Timeout beendet die betroffene
DTC-Transaktion nach 3500 ms zuzüglich gemessener Scheduler-/UI-Laufzeit.
Im TX-Protokoll dürfen ausschließlich statisch freigegebene Befehle stehen.
Sperrtests bleiben im Mock/Simulator, niemals am Fahrzeug.

Erst danach kurze Fahrzeugprüfung im Stand: DTC manuell lesen, Rückmeldungen
und Abbruch prüfen; PID 010D muss bei Stillstand plausibel 0 anzeigen.
Das belegt keinen Geschwindigkeitstest während der Fahrt und keine Diagnose
von ABS/ESP. Kein automatisierter Fahrzeugzugriff.

Technische Risiken: UI-Latenz unter LVGL/Serial-Last; langes Warten auf einen
Prompt nach Abbruch; adapterabhängige Antwortformate; verzögerte Antworten
nach Neustart; irreführende Interpretation generischer OBD-DTCs.

Nächster benötigter Nachweis: ein zeitgestempeltes LAB-Protokoll der neuen
Firmware mit Commit-ID, OBD_TX-Zeilen und sichtbaren Lade-/Abbruch-/Reconnect-
Zuständen. Die Sichtbarkeits- und Latenzkriterien bleiben bis dahin offen.

