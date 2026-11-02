# Sicherheitskonzept v0.1

## Garantieumfang / Grenzen

**Ziel:** Keine Fahrzeug-Codierung, keine DTC-Löschung, keine Stellgliedansteuerung und keine beliebigen Rohframes. Dieses Ziel wird durch eine geschlossene Allowlist der *Firmware* abgesichert. Es ist keine Aussage über das Innenleben eines ELM327-Klons oder eine Garantie für jedes Steuergerät.

- Nur Anfragen: Mode 01/PIDs 00, 05, 0C, 0D; Mode 03, 07, 0A.
- Mode 04 (Löschen), Mode 08 (Stellgliedtests), UDS 10/11/14/27/2E/2F/31/34–37 und beliebige CAN-Nachrichten sind **nicht implementiert**.
- Kein Terminal, keine vom Benutzer eingegebenen AT-Kommandos, keine CAN-Header-Umschaltung (ATSH/ATCRA usw.).
- ELM327-Adapterinitialisierung: ATZ/ATE0/ATL0/ATS0/ATH0/ATSP0. Diese ändern die Arbeitsweise des **Adapters**, nicht das Steuergeräte-Coding.
- WLAN wird ausschließlich bei vorhandener lokaler `Config.h` aktiviert; ohne Konfiguration keine Verbindung, kein OBD-Verkehr.
- Antworten sind untrusted input. Timeout ohne ELM-'>'-Prompt erzwingt Abbruch der TCP-Verbindung, damit keine veraltete Antwort einem neuen Befehl zugeordnet wird.
- Eine OBD-Abfrage ist keine passive Busüberwachung. Normale Diagnose-Requests erzeugen Verkehr auf dem Fahrzeugbus.

## Verbindliche Prüfschritte vor Fahrzeugtest

1. Default-Firmware ohne `Config.h` auf dem Display prüfen, Touch und PSRAM kontrollieren.
2. Quellcode und kompiliertes Image überprüfen: **keine** zufälligen Raw-Requests, kein Terminal, keine Schreib-Modi.
3. Mit TCP-ELM327-Simulator ausschließlich die erlaubten Befehle protokollieren, auch bei Timeout, Adapter-Reconnect, Fehldaten und Touch-DTC-Scan.
4. Mode-01-Antworten des Zieladapters auf Echos, `SEARCHING...`, Leerzeichen, Header, Antwortfragmentierung und parallele ECU-Antworten prüfen. Der aktuelle Parser ist absichtlich noch nicht vollständig.
5. Keine ersten Tests während der Fahrt. Bei eingeschalteter Zündung mit gesicherter Spannungsversorgung, angeschlossenem Adapter und stillstehendem Fahrzeug.
6. Vor produktivem Live-Betrieb Latenzen, PID-Bitmap, DTC-Interpretation und Ausfallverhalten hardwareseitig verifizieren.

## Änderungsdisziplin

Fahrzeug-Schreibzugriffe dürfen **niemals** durch bloßes Aktivieren einer UI-Option freigegeben werden. Eine künftige Schreibvariante benötigt gesonderte Firmware-/Build-Profile, separate Tests, eine ausdrückliche Bestätigung je Operation und nachvollziehbare Risiken pro Fahrzeug-/Steuergerätetyp. Read-only bleibt die unveränderte Standardeinstellung.

## Bekannte Lücken des Prototyps

- Keine belastbare ISO-TP-/Multi-ECU-DTC-Dekodierung.
- Statusanzeige zeigt lediglich die jüngste DTC-Rohantwort, vollständige 03/07/0A-Antworten am USB-Serial.
- Keine SD-Protokollierung; Serial-Ausgabe ist keine dauerhafte Datenaufzeichnung.
- PID-Parser ist ein Minimalparser ohne ausführliche Antwortnormalisierung; fehlende Daten nicht als 0 interpretieren.
- UI ruft Netzwerktransaktionen synchron ab; höhere Last kann Touch/Grafik verzögern.
- Keine Fahrzeug-Werkstattdiagnose oder zertifizierte Prüffunktion.
