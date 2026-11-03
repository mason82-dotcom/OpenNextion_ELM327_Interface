# DTC-LAB: reproduzierbare Transportfehler und Prüfprotokoll

Priorität: die praktische Abnahme der DTC-Bedienung vorbereiten. Die bereits
gemergten UI-Korrekturen werden nicht neu implementiert. Die Firmware und ihre
Befehls-Allowlist werden durch diese Simulator-Erweiterung nicht geändert.

## Belegstand

- Nutzerbestätigte PID-Tests: 010C, 0105, 010D. Daraus folgt keine vollständige
  Fahrzeugabnahme und kein Nachweis für neue DTC-Bedienzustände.
- Ausgangsbasis dieser Erweiterung: `main` bei
  `f68c52162774ae299e8e3125edb0aba032141066`; die Änderung aus PR #3 ist darin
  enthalten. Der frühere Hinweis „main enthält nur README“ im älteren
  DTC-Prüfbericht beschreibt dessen damaligen Prüfzeitpunkt.
- Bestehende Hosttests prüfen echte Firmwarefunktionen mit Doubles; ältere
  Boardberichte sind historische Hinweise. Neue Displaylatenz, Abbruch und
  Reconnect sind ohne frisches LAB-Protokoll weiterhin **UNBEKANNT**.
- Die neuen TCP-Tests benutzen ausschließlich Loopback, einen flüchtigen Port
  und selbst gestartete Simulatorinstanzen. Keine reale OBD-Verbindung.

## Simulator vorbereiten

Den gewünschten Repository-Stand in einem separaten Checkout bereitstellen.
Commit, Firmware-/LAB-Buildstand und Simulatorstand vor jedem Lauf notieren.
Ein Firmwarebuild aktualisiert kein Board. Dieses Runbook flasht nichts.

```bash
git rev-parse HEAD
python3 tools/elm327_lab_simulator.py --help
python3 -m unittest discover -s tests -p 'test_elm_simulator*.py' -v
```

Für einen lokalen TCP-Test ist Loopback der sichere Standard:

```bash
python3 tools/elm327_lab_simulator.py \
  --bind 127.0.0.1 --port 35000 \
  --dtc-file /tmp/elm327-lab-dtc \
  --dtc-transport-file /tmp/elm327-lab-dtc-transport
```

Für das ESP32-Board stattdessen ausdrücklich die aktuelle **Pi-LAN-IP** an
`--bind` übergeben. IP und Port vorab prüfen; keinen laufenden Simulator oder
anderen Dienst ungeprüft ersetzen. Keine Weiterleitung zu einem echten ELM.
Die LAB-Firmware muss den Simulator anhand `ELM327 LAB SIM v0.1` erkennen.
USB-Serial separat aufzeichnen, einschließlich `OBD_TX` und sichtbarer
Displayzustände per Foto/Video. Bedienereignisse mit Zeitstempel notieren.

Die folgenden Dateien schalten nur den selbst gestarteten LAB-Simulator um:

```bash
printf 'MULTI\n' > /tmp/elm327-lab-dtc
printf 'DELAY_07\n' > /tmp/elm327-lab-dtc-transport
```

Die Transportdatei wird pro Anfrage gelesen (maximal 65 Bytes). Zulässig sind
genau neun Selektoren: je `DELAY`, `NO_PROMPT`, `DISCONNECT` mit Suffix
`_03`, `_07` oder `_0A`. Andere Werte verändern die Antworten nicht und sind
kein gültiger Fehler-Injektionsnachweis. Im Serverlog muss die passende Zeile
`[LAB] DTC TRANSPORT <Aktion> for <Modus>` stehen.

| Beispiel | Wirkung nur für den gewählten Modus |
|---|---|
| `DELAY_07` | Antwortinhalt sofort, Prompt nach weiteren 2 Sekunden; ermöglicht Abbruch nach einem 03-Teilergebnis |
| `NO_PROMPT_07` | Antwortinhalt ohne Prompt; Verbindung wartet auf Client-Timeout/Trennung, Folgekommandos werden ohne Antwort verworfen und die Sitzung geschlossen |
| `DISCONNECT_0A` | TCP-Verbindung vor der 0A-Antwort schließen |

**Persistente Einstellung:** bleibt bis Änderung oder Entfernung aktiv, auch
nach Reconnect. Kein Einmalfehler. Die Datei sendet selbst keine Diagnose und
startet keinen ESP32-Scan. AT-Setup und Live-PIDs werden nicht gezielt gestört;
ein absichtlich blockierter oder geschlossener Stream unterbricht natürlich
auch deren spätere Verarbeitung. Ein bereits verzögerter Prompt läuft nach
Entfernen der Datei noch aus; ein fehlender Prompt erfordert eine neue Sitzung.

Rückkehr zur normalen Transportantwort:

```bash
rm -f /tmp/elm327-lab-dtc-transport
```

## Prüffolge auf dem Board

1. Ohne Transportdatei 010C/0105/010D prüfen und manuell einen MULTI-Scan lesen.
2. `DELAY_03`: Scan starten, während der aktiven Abfrage zehnmal „DTC lesen“
   berühren. TX muss genau eine Folge 03/07/0A enthalten. Ladezustand innerhalb
   250 ms sichtbar; keine Live-Anfragen zwischen den DTC-Modi.
3. `DELAY_07`: nach sichtbarem 03-Ergebnis „ABBRUCH“ drücken. Teilergebnisse
   sofort ungültig; alte 07-Antwort darf sie nicht wiederherstellen. Ohne neuen
   Start kein 0A. Live-Polling erst nach dem alten Prompt wieder aufnehmen.
4. `NO_PROMPT_07`: Scan einmal ohne Abbruch bis zum Timeout laufen lassen,
   anschließend mit Abbruch wiederholen. Jeweils Stream schließen und alle
   DTC-Ergebnisse invalidieren. DTC-Timeout: 3500 ms zuzüglich gemessener
   UI-/Schedulerzeit. Keine Folgeabfrage auf dem unsynchronisierten Stream.
5. `DISCONNECT_0A`: Verbindungsverlust muss auch schon vorhandene 03-/07-
   Teilergebnisse invalidieren. Reconnect darf keinen DTC-Neustart auslösen.
6. Transportdatei entfernen, Reconnect abwarten, manuell neu lesen. Danach
   010C/0105/010D innerhalb von zwei regulären Pollingzyklen wieder aktuell.

Keine Fehlerlöschung, insbesondere kein Mode 04, keine Codierung, Anpassung,
Stellgliedtests oder ECU-Schreibbefehle. Sperrtests ausschließlich im Mock/
Simulator. Kein Deployment, Flashen, Netzwerkumbau oder Fahrzeugzugriff durch
diese Werkzeuge. Erst nach erfolgreicher LAB-Abnahme kurze manuelle Prüfung
im stehenden Fahrzeug planen. Standard-OBD ersetzt keine ABS-/ESP-Diagnose.

## Wiederverwendbares Ergebnisprotokoll

Vorhandene USB-Mitschnitte lassen sich mit
`python3 tools/audit_obd_tx.py ./lab-serial.txt` offline auf die feste
Befehls-Allowlist und Zählerlücken prüfen. Aussagegrenzen, JSON-Felder und
Exit-Codes: [OBD_TX-Audit](OBD-TX-AUDIT.md). Ein PASS dieser Dateiprüfung
setzt keinen Board-Testfall automatisch auf PASS.

Pro Fall die tatsächliche Beobachtung und Logausschnitte eintragen; ein
ausgefüllter Erwartungswert ist kein Testergebnis. PASS nur bei beobachteter
Erfüllung, FAIL bei Abweichung, ansonsten UNBEKANNT.

- Datum/Zeit mit Zeitzone:
- Repository-Commit / aufgespielter Firmwarestand / Simulator-Commit:
- LAB-Kennung gesehen / Board / Port:
- USB-Serial-Datei / Simulatorlog / Foto- oder Videonachweis:

| Fall | Erwartung | Tatsächliches Ergebnis / Logverweis | Status |
|---|---|---|---|
| Baseline | 010C/0105/010D plausibel, MULTI-Codes sichtbar | noch kein neuer Nachweis | UNBEKANNT |
| DELAY_03 / Mehrfachklick | ein Scan, Ladeanzeige <=250 ms, kein Live-Polling dazwischen | noch kein neuer Nachweis | UNBEKANNT |
| DELAY_07 / Abbruch | Teilergebnisse ungültig, kein 0A ohne neuen Start | noch kein neuer Nachweis | UNBEKANNT |
| NO_PROMPT_07 | Timeout, Stream geschlossen, keine veralteten Daten | noch kein neuer Nachweis | UNBEKANNT |
| DISCONNECT_0A / Reconnect | alle Teilergebnisse ungültig, kein DTC-Autostart | noch kein neuer Nachweis | UNBEKANNT |
| Erholung | manueller Scan möglich, drei PIDs wieder aktuell | noch kein neuer Nachweis | UNBEKANNT |

Technische Risiken: Reaktionszeit unter realer LVGL-/Serial-Last, Timing der
Berührung relativ zum Prompt, alte Firmware auf dem Board, falscher Selektor,
weiter aktive Störung nach Reconnect sowie Adapterformate außerhalb des
begrenzten ATH0-Parsers. Das Befehlsprotokoll allein belegt keine UI-Reaktion.

**Nächster benötigter Nachweis:** ein ausgefüllter LAB-Lauf dieser Prüffolge
mit Firmware-Commit, OBD_TX-Zeilen, passender Simulator-Störungszeile und
sichtbarem Abbruch-/Reconnect-Verhalten. Bis dahin keine neue Hardwareabnahme.
