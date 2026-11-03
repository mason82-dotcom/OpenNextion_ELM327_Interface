# Vorhandene OBD_TX-Mitschnitte offline prüfen

`tools/audit_obd_tx.py` liest ausschließlich eine vorhandene Datei und gibt
einen JSON-Bericht nach stdout aus. Keine Netzwerk-/Serial-Verbindung, kein
Versand von Befehlen, keine Änderung an Eingabedatei, Firmware oder Fahrzeug.
Es werden keine Pakete oder Bibliotheken nachinstalliert; Python-Standardbibliothek.

## Aufruf

Den eigenen USB-Serial-Mitschnitt als `lab-serial.txt` im aktuellen Ordner
bereitstellen. Darin müssen die Firmwarezeilen unverändert enthalten sein:

```text
OBD_TX,1250,1,ATZ
OBD_TX,1300,2,ATE0
```

Die beiden Zeilen oben sind lediglich ein **synthetisches Formatbeispiel**,
kein tatsächlicher Testlauf. Für die Auswertung:

```bash
python3 tools/audit_obd_tx.py ./lab-serial.txt
```

Der Prüfer erzeugt selbst keine Dateien. Der JSON-Bericht enthält SHA-256
und Größe der eingelesenen Bytes für die Zuordnung zum Originalmitschnitt,
feste Statusfelder und Befehlszähler. Fremde Logzeilen und unbekannte
Befehlsinhalte werden nicht in den Bericht kopiert. Der Originalmitschnitt
kann weiterhin private Informationen enthalten und muss separat geprüft werden.

## Was tatsächlich geprüft wird

| Feld | Bedeutung |
|---|---|
| `checks.txAllowlist` | PASS nur bei mindestens einem gültigen TX-Eintrag und ausschließlich exakt freigegebenen Befehlen; unbekannte Befehle oder fehlerhafte Einträge ergeben FAIL |
| `checks.captureContinuity` | fortlaufende 32-Bit-Zähler und plausible Zeitfolge im gelieferten Mitschnitt; Lücken, Duplikate und nicht markierte Rücksprünge ergeben FAIL |
| `observed.pidRequests` | gezählte Sendeeinträge für 010C/0105/010D, **keine** Bestätigung gültiger PID-Antworten |
| `observed.dtcRequests` | gezählte Sendeeinträge für 03/07/0A, **keine** Bestätigung gültiger DTC-Ergebnisse oder einer vollständigen Scanfolge |
| `unverified` | PID-Antworten, exklusiver Adapterzugriff, DTC-Bedienablauf und Hardwareabnahme bleiben UNBEKANNT |

Die Allowlist ist bewusst eine feste Kopie der tatsächlich statisch auf
Fahrzeug und Adapter abgebildeten Firmwarebefehle. Ein Regressionstest
vergleicht beide Listen. Ein zusätzliches syntaktisch gültiges, aber nicht
freigegebenes `01FF` scheitert ebenso wie `04`, UDS-Schreibdienste oder `ATSH`.
Die sieben AT-Setupbefehle sind Adapterkonfiguration, keine ECU-Schreibbefehle.

Leere Mitschnitte erhalten UNBEKANNT. Beginnt ein Abschnitt nicht mit
TX-Zähler 1, ist seine Vollständigkeit UNBEKANNT. Nur die drei exakten
Firmware-Bootmeldungen trennen Abschnitte; ein Zählerrücksprung allein genügt
nicht. Auch Bootmeldungen sind nur Loginhalt und kein Echtheitsnachweis.
Ein Boot ohne folgenden TX-Eintrag bleibt unvollständig. `millis()`- und
Zählerüberlauf werden berücksichtigt. Zeitabstände ab 2^31 Millisekunden
sind für diese kurzen LAB-Mitschnitte mehrdeutig und werden beanstandet.

Die maximale Eingabegröße beträgt 8 MiB, die maximale Zeilenlänge 4096 Bytes.
Terminal-Präfixe vor `OBD_TX`, Kleinbuchstaben, zusätzliche Felder und
beschädigte Datensätze gelten als Fehler. Deshalb den rohen Serial-Text
aufzeichnen, nicht nur ausgewählte Zeilen aus einer Bildschirmansicht.

| Exit-Code | Aussage |
|---|---|
| 0 | die beiden lokalen TX-Prüfungen sind PASS |
| 1 | mindestens eine lokale TX-Prüfung ist FAIL |
| 2 | mindestens eine TX-Prüfung ist UNBEKANNT oder die Eingabe nicht lesbar/zu groß |

**Exit 0 ist keine LAB-, Hardware- oder Fahrzeugfreigabe.** Ein Mitschnitt
kann gefiltert, abgeschnitten oder erfunden sein; SHA-256 macht ihn lediglich
zuordenbar. Nicht protokollierte Übertragungen sind daraus nicht erkennbar.
Ohne Antwort-, Bedien- und Displaynachweis lassen sich Parallelzugriffe,
die 250-ms-Ladeanzeige, ein korrekter Abbruch und der Reconnect nicht
abschließend beurteilen. Auch ein Simulator-Test ist keine Mercedes-/ABS-/ESP-
Abnahme.

Für die praktische Abnahme den Bericht zusammen mit unverändertem Original,
Firmware-Commit, Simulator-Störungszeile und Foto/Video der Bedienzustände
der Vorlage im [DTC-LAB-Runbook](DTC-LAB-RUNBOOK.md) zuordnen. Bestehende
Annahmen bleiben UNBEKANNT, bis diese Nachweise vorliegen.
