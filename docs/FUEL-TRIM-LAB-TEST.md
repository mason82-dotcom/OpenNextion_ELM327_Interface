# Passiver Fuel-Trim-Test auf OpenNextion (Phase 2)

**Stand 2026-10-11: am Display gegen den Pi-Simulator geprüft
(siehe Boardtest unten). Kein Test an einem echten ELM327 oder Fahrzeug.**
Vor einem erneuten Flash müssen die nativen Tests, die Python-Simulatortests
und der ESP32-S3-LAB-Build erfolgreich sein.

## Ursprung und Ablauf

Die Logik ist von `mason82-dotcom/ELM327`
(`app/src/main/java/de/roman/mercedesobdmonitor/FuelTrimTest.java`)
auf eine unabhängige C++17-Zustandsmaschine portiert:

| Schritt | Zeit | RPM | Voraussetzungen |
| --- | --- | --- | --- |
| Stabilisieren Leerlauf | 3 s | 550–1000 | gültige frische RPM- **und** Tempo-Daten, Tempo unter 3 km/h |
| Leerlaufmessung | 20 s | 500–1100 | mindestens 3 frische Samples von STFT/LTFT B1/B2 |
| Stabilisieren ca. 2500 | 3 s | 2200–2800 | gleiche Fahrzeugstandbedingung |
| Messung ca. 2500 | 15 s | 2100–2900 | mindestens 3 frische Samples von STFT/LTFT B1/B2 |

Die Messdauer wird bei unzureichenden Trim-Samples maximal um 30 s
verlängert, sonst **FAILED** statt eines unvollständigen Ergebnisses.
Unzulässige Drehzahl oder Geschwindigkeit setzt die jeweilige laufende
Messphase zurück. RPM und Geschwindigkeit müssen jünger als 3,5 s
sein; sonst wird der Phasenübergang verweigert bzw. der aktuelle
Messabschnitt verworfen.

Frische Messwerte werden durch **eindeutig erfolgreich beantwortete
Abfragen** gezählt; vorhandene gecachte LTFT-Werte werden nicht mehrfach
eingerechnet. Während der Messung werden acht benötigte/optionale PIDs
priorisiert (`010C`, `010D`, `0106`, `0107`, `0108`, `0109`,
`0110`, `010B`). Dies sind ausschließlich fest erlaubte Leseanfragen.

Das Ergebnis zeigt Durchschnittswerte pro Phase/B1/B2, Gesamttrim
(STFT+LTFT), optionale MAF-/MAP-Mittel und Anzahl der gültigen Samples.
Hinweise bei Bankdifferenz, deutlicher Leerlauf-Überkorrektur,
dauerhaft positiver oder negativer Korrektur sind **Heuristiken**,
keine Mercedes-Werkstattgrenzwerte.

## Displaybedienung

`LIVE → ALLE PIDS → TRIM-TEST` öffnet die zusätzliche Ansicht und
startet bei bestehender LAB-Verbindung den Test. START beginnt erneut,
ABBRUCH stoppt den Test ohne weitere Diagnoseaktion.
Ein neuer DTC-/HU-/Mode06-Scan beendet einen laufenden Trim-Test.
Ein TCP-/ELM-Verbindungsverlust verwirft alle Zwischenergebnisse.
Das Gerät steuert weder Drosselklappe noch Motor noch Fahrzeugbewegung.

## Kontrollierter LAB-Test mit simulierter manueller Drehzahländerung

**Nur mit Pi-Simulator; kein Fahrzeug und kein realer ELM327-Dongle.**
Im Test wird die Pi-Simulatorantwort auf `010C` umgeschaltet,
nicht die Drehzahl eines Motors.

### A. Neue Branchversion prüfen

```bash
cd ~/Projekte/OpenNextion_ELM327_Interface
git status --short
git pull --ff-only
python3 -m unittest discover -s tests -p 'test_*.py' -v
g++ -std=c++17 -Wall -Wextra -Werror -Ifirmware/ObdDisplay \
    tests/test_fuel_trim.cpp -o /tmp/obd-fuel-trim-test
/tmp/obd-fuel-trim-test
test ! -f firmware/ObdDisplay/Config.h
bash scripts/pi/build.sh lab
```

**Erst bei `BUILD OK` und grüner GitHub-CI** in Abstimmung erneut
`build/firmware-lab/ObdDisplay.ino.merged.bin` auf das Board flashen.
Andere Firmwareprofile dürfen dabei nicht verwendet werden.

### B. Pi-Simulator neu starten

Im ersten Terminal die alte Simulatorinstanz mit Strg+C stoppen.
Danach mit dynamischen RPM-Testwerten starten:

```bash
cd ~/Projekte/OpenNextion_ELM327_Interface
rm -f /tmp/elm327-lab-pid-fault
printf '800\n' > /tmp/elm327-lab-rpm
python3 tools/elm327_lab_simulator.py \
    --bind 192.168.178.63 --port 35000 \
    --fault-file /tmp/elm327-lab-pid-fault \
    --rpm-file /tmp/elm327-lab-rpm
```

IP des Pi gegebenenfalls mit `ip -br -4 addr` erneut prüfen.
Die WLAN-Zugangsdaten verbleiben ausschließlich in der ignorierten
`LabConfig.h`.

### C. Zwei Testphasen

1. Auf dem Display `LIVE → ALLE PIDS → TRIM-TEST` öffnen.
   Simulator liefert `800 rpm` und `0 km/h`.
2. Status erst 3 s stabilisieren, dann 20 s Leerlaufwerte sammeln.
3. Sobald das Display `ca. 2500 rpm` fordert, im **zweiten Pi-Terminal**
   den Simulatorwert manuell umschalten:

```bash
printf '2500\n' > /tmp/elm327-lab-rpm
```

4. Auf dem Display sollte nach 3 s Stabilisierung für ca. 15 s die
   zweite Messphase beginnen und anschließend ein Bericht erscheinen.
   Beide Bänke zeigen die bekannten Labor-STFT/LTFT-Werte
   (STFT 0 %, LTFT B1 ca. +7,8 %, B2 ca. +5,5 %).
5. Nach dem Test zur bekannten Simulator-RPM zurück:

```bash
printf '1234\n' > /tmp/elm327-lab-rpm
```

Die Simulator-Protokollausgabe sollte weiter nur `01xx`-PIDs und
die freigegebenen Setup-Anfragen enthalten.

### D. Negative Tests

- `printf '04\n' > /tmp/elm327-lab-rpm` ist **kein** Motorkommando:
  das Labor-Skript akzeptiert nur rein dezimale RPM im Bereich 0–7000.
  Ein nicht gültiger Wert lässt die ursprüngliche 1234-rpm-Testantwort
  erscheinen; es wird kein Fahrzeugbefehl weitergeleitet.
- `printf '0107\n' > /tmp/elm327-lab-pid-fault`: in einer Messphase
  fehlen neue LTFT-B1-Daten. Bereits empfangene Werte dürfen nicht
  durch gecachte Werte doppelt gezählt werden. Ein abgeschlossener
  Bericht ohne mindestens drei gültige LTFT-B1-Samples ist unzulässig.
- Im Simulator-TCP-Terminal `Strg+C`: der Verbindungsverlust muss den
  laufenden Test als ungültig markieren, nicht weiterlaufen lassen.

## Boardtest 2026-10-11

ONX3248G035 mit Lab-Image `a5dca1d`, Pi-Simulator (`--rpm-file`,
800 rpm → 2500 rpm nach der Leerlaufphase umgeschaltet):

| | Leerlauf (20 s) | ca. 2500 rpm (15 s) |
|---|---|---|
| B1 STFT / LTFT / gesamt | 0,0 / 7,8 / 7,8 % | 0,0 / 7,8 / 7,8 % |
| B2 STFT / LTFT / gesamt | 0,0 / 5,5 / 5,5 % | 0,0 / 5,5 / 5,5 % |
| MAF / MAP | 5,00 g/s / 40 kPa | 5,00 g/s / 40 kPa |
| frische Werte je Trim | 29–30 | 21–22 |

Bericht „Messung abgeschlossen“, Hinweis „Keine eindeutige Abweichung
>10% im Schema“ – passend zu den Fixture-Werten. 1223 Antworten,
max. 151 ms Antwortzeit, kein Verbindungsabbruch.

Zwei vorherige Läufe wurden korrekt als ungültig abgebrochen, weil
einzelne Antworten wegen WLAN-Stromsparen (ESP32-Modem-Sleep, Pi
`power_save on`) bis zu 1,6 s brauchten und das alte 1,7-s-Timeout
die Sitzung beendete. Behoben mit `WiFi.setSleep(false)` und 2,5 s
Timeout (`a5dca1d`); Ping danach Ø 8,7 ms / max. 28 ms.

## Was noch offen ist

Ein echter ELM327 kann längere Abfragezeiten, fehlende Bank-2-PIDs,
Formatabweichungen oder Verbindungsstörungen haben. Die konservative
Frischeanforderung kann daher einen Test beenden oder wiederholen.
Das ist sicherer als ein Bericht aus veralteten oder fehlenden Werten.
Die Fahrzeugportierung bleibt ausdrücklich nicht freigegeben.
