# Inkrementeller DTC-Empfang: Stufe 1

Basis: `main` bei `c087fe6b0379ea2b2f85c62416c31cda1b1242fd`.
Diese Änderung führt keine externe Bibliothek ein. ELMo wird nicht eingebaut.
Die eigene feste Befehls-Allowlist bleibt unverändert.

## Verhalten

`ObdTransaction.h` enthält eine einzelne Transport-Zustandsmaschine ohne
Arduino-Abhängigkeit, Heap-Puffer, Thread oder frei eingebbare Befehle.
`begin()` akzeptiert ausschließlich `obd_policy::Command`. Ein zweiter Start
während einer aktiven oder noch nicht konsumierten Antwort wird abgewiesen.
Der gesamte Befehl einschließlich CR wird in einem `write()` übergeben;
Teilschreiben beendet die Verbindung. Auch dieser Sendeversuch erscheint im
OBD_TX-Protokoll.

`poll()` wartet nicht auf weitere Empfangsdaten und liest pro Aufruf höchstens
64 Bytes. Die normalisierte Antwort ist auf 1024 Zeichen begrenzt. Ein fehlender
Prompt nach Ablauf des jeweiligen Timeouts, Pufferüberlauf, Verbindungsverlust
oder Teilschreiben beendet die Sitzung. Vor einem neuen Befehl werden höchstens
64 alte Bytes verworfen; mehr Restdaten schließen die Verbindung, statt die
Bedienoberfläche mit einer unbeschränkten Drain-Schleife zu blockieren.

Die DTC-Seite startet einen Modus und kehrt sofort nach dem Start zur
Hauptschleife zurück. Weitere Durchläufe bedienen LVGL, den DTC-Empfang,
Statistik und die vorhandenen periodischen Funktionen. Erst nach dem Prompt
wird die vollständige Antwort geparst. Der Abstand zur nächsten Anfrage wird
ab Empfangsende gemessen, nicht nach jedem erfolglosen Poll erneut gestartet.

Abbruch oder Start einer anderen Diagnose verändert die Diagnosegeneration.
Die Hauptschleife bedient die alte DTC-Transaktion weiterhin vorrangig bis zum
Prompt/Fehler; auf diesem Socket startet zwischenzeitlich keine Live-, HU/AU-
oder Mode-06-Anfrage. Alte Antworten werden danach verworfen und Teilergebnisse
bleiben ungültig. Ein ausdrücklich neu gestarteter DTC-Scan beginnt anschließend
bei 03. Reconnect allein startet keinen Scan.

## Bewusste Grenzen dieser Stufe

- DTC-Antworten werden inkrementell empfangen. Adapterinitialisierung, Live-PIDs,
  HU/AU und Mode06 verwenden weiterhin einen kooperativ synchronen Wrapper mit
  LVGL-Aufrufen. Auch sie nutzen denselben Transportbaustein und Puffer.
- Der TCP-Verbindungsaufbau kann weiterhin bis zu 1000 ms blockieren.
- `WiFiClient::write`, SD-Schreiben, Serial-Ausgabe, LVGL und das Parsen einer
  abgeschlossenen Antwort sind keine garantierten Echtzeitoperationen.
- Die neue Architektur belegt deshalb keine feste maximale Touch-Latenz und
  noch keine vollständige Entkopplung aller Netzwerkoperationen.
- Der separate RTC-PR #10 ist nicht Bestandteil dieser Änderung. Gemeinsame
  Änderungen an Sketch/CI müssen beim Zusammenführen beider PRs geprüft werden.

## Software-Nachweise

Der native Transporttest prüft fragmentierten Empfang, leeren RX-Puffer ohne
Warten, 64-Byte-Budget, 1024-Zeichen-Grenze, Überlauf, fehlenden Prompt,
Millis-Überlauf, Verbindungsverlust, alte RX-Daten, Teilschreiben und die Sperre
eines zweiten Befehls. Ungültige Befehle werden ohne Sendeversuch abgewiesen.

Der Hosttest führt die tatsächlichen Firmwarefunktionen mit Display-/TCP-
Doubles aus: DTC-Start ohne Warteschleife, Mehrfachklick, Zusammenfassung über
03/07/0A, NO DATA, wiederholte UI-Durchläufe bei fehlendem Prompt, Abbruch,
späte Antwort, expliziter Neustart und Antwortüberlauf. Die vorhandenen Tests
prüfen weiterhin Parser, PID-Formeln, Allowlist und CSV-Format. Das sind keine
neu ausgeführten Board- oder Fahrzeugtests.

## Nächste praktische LAB-Abnahme

Passenden Firmware- und Simulator-Commit dokumentieren. Kein automatisches
Flashen, Deployment oder Fahrzeugzugriff durch diese Änderung.

1. LAB-Simulator mit `--dtc-file` und `--dtc-transport-file` starten; Einzelheiten
   im [DTC-LAB-Runbook](DTC-LAB-RUNBOOK.md).
2. `PADDED` und `DELAY_07` wählen. DTC starten, während des verzögerten Prompts
   zu INFO wechseln und den Touch-Zähler bedienen. Anzeige und Statistik müssen
   weiter reagieren; anschließend DTC-Ergebnis/DETAILS prüfen.
3. Mit `DELAY_03` zehnmal schnell starten: genau eine Folge 03/07/0A. Mit
   `DELAY_07` nach dem ersten Teilergebnis abbrechen: alle Teilergebnisse ungültig,
   kein 0A ohne neuen Start und kein Live-Polling vor dem noch ausstehenden Prompt.
4. `NO_PROMPT_07`: Stream nach 3500 ms zuzüglich gemessener Loop-Verzögerung
   schließen. UI muss währenddessen bedienbar bleiben. Nach Reconnect kein
   DTC-Autostart. Anschließend die Störungsdatei entfernen und manuell neu lesen.
5. 010C/0105/010D und die unveränderten Mode06-/HU/AU-Abläufe auf Regression
   prüfen. Der vollständige Befehlsmitschnitt darf nur die feste Allowlist enthalten.

Für alle Schritte USB-Serial inklusive OBD_TX, Simulatorlog und sichtbare
UI-Zustände mit Zeitbezug sichern. Bis dahin sind Bedienlatenz und die praktische
Abnahme dieser Revision **UNBEKANNT**. Der Betrieb bleibt strikt lesend: keine
Fehlerlöschung, Codierung, Anpassung, Stellgliedtests oder ECU-Schreibbefehle.
