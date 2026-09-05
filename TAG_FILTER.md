# Zentraler Tag-Filter und UID-Korrektur

Diese Änderung basiert auf **teddyCloud 0.7.0**, Backend-Tag `tc_v0.7.0`
(`439225a`) und Frontend-Tag `tcw_v0.7.0` (`e2f40c2`). Sie liegt in beiden
AlpineNomad-Forks im Branch `fix/uid-tag-filter-0.7.0`.

Das Backend bindet den passenden Commit des Frontend-Forks als Submodul ein.
Die erzeugten Webdateien in `contrib/data/www/web` gehören ebenfalls zu diesem
Branch, damit auch der normale Docker-Build die neuen Bedienelemente enthält.

## Bedienung

In der Weboberfläche unter **Einstellungen → core → tag_filter** stehen bereits
in der Ansicht **Basic** zwei globale Einstellungen zur Verfügung:

- **Tag-Filter aktivieren**: standardmäßig eingeschaltet, entsprechend dem
  bisherigen lokalen Filter.
- **Gesperrter Dateinamen-ID-Teil**: standardmäßig `FFFFFFFF`; genau acht
  Hex-Zeichen, Groß-/Kleinschreibung beliebig.

Mit **Speichern** übernehmen. Die Regel gilt unmittelbar für alle Boxen.
Die normale Speicherfunktion der Oberfläche schreibt sie in `config.ini`.
Eine Einstellung pro Box ist nicht vorgesehen und wird vom Backend abgewiesen.

```ini
core.tag_filter.enabled=true
core.tag_filter.content_id=FFFFFFFF
```

Verglichen wird der **Dateiname**, also die letzten acht Zeichen der rUID:

| rUID / Content-Pfad | Bei `FFFFFFFF` |
|---|---|
| `78563412/FFFFFFFF.json` | gesperrt |
| `FFFFFFFF/000304E0.json` | erlaubt |
| `78563492/000304E0.json` | erlaubt |

Die numerische UID und die rUID haben unterschiedliche Byte-Reihenfolgen.
Die Audio-ID ist ein separater Wert und wird hier nicht verglichen.

## Verhalten

Die RTNL-Umwandlung verbreitert jetzt beide 32-Bit-Wörter ohne Vorzeichenerweiterung.
Dadurch bleibt zum Beispiel `E004030092345678` erhalten, statt zu
`FFFFFFFF92345678` zu werden. Diese Fehlerkorrektur ist immer aktiv, unabhängig
vom optionalen Filter. Die bestehende Wort-Reihenfolge im RTNL-Protokoll bleibt erhalten.

Der Filter greift vor Tag-Anlage, Änderung des zuletzt erkannten Tags,
automatischer Audio-Zuordnung und Cloud-Weiterleitung in Claim- und
Content-Anfragen. Er berücksichtigt auch RTNL-Tag-Ereignisse, Freshness-Anfragen
und vorhandene Freshness-Cache-Einträge. Abgewiesene Claim-/Content-Anfragen
erhalten HTTP 404.

Der bisherige lokale feste Sonderfall in `save_content_json` wird nicht benötigt.
Bei ausgeschaltetem Filter kann auch eine `FFFFFFFF.json` regulär geschrieben werden.
Vorhandene Dateien werden nicht gelöscht oder umbenannt. Falsch angelegte ältere
Einträge werden durch diese Änderung nicht automatisch einer anderen UID zugeordnet.

Der Server kann RFID-Erkennung, Fehleransagen und Offline-Wiedergabe bereits auf
der Box gespeicherter Dateien nicht unterbinden. Die Änderung wurde noch nicht auf
eine laufende Installation ausgerollt oder mit einer echten Box getestet.

## Prüfung und Build

Die C-Regressionstests linken die tatsächlichen Backend-Funktionen. Sie prüfen
UID-Grenzwerte, korrekte UID-Hälften, globale Gültigkeit, Validierung, Persistenz,
Claim/Content v1/v2/v3, RTNL-Zustand und Freshness-Filterung. AddressSanitizer und
UBSan bleiben eingeschaltet. Die Korrekturen für das TB2-Helligkeitsfeld und das
Speichern leerer Konfigurationsabschnitte sind bereits in 0.7.0 enthalten.

```sh
docker build -t teddycloud-tag-filter-build:local -f tests/Dockerfile.tag-filter tests
docker run --rm --network none -v "$PWD:/workspace" \
  teddycloud-tag-filter-build:local \
  make -f Makefile -f tests/tag_filter.mk build test-tag-filter -j6
```

Die Weboberfläche wird im Unterordner `teddycloud_web` mit `npm ci` und
`npm run build` gebaut. Ein Release mit dem bestehenden Dockerfile benötigt
vorher `make web`, damit die neuen Webdateien auch in `contrib/data/www/web`
liegen; ein reiner Backend-Build aktualisiert diese mitgelieferten Dateien nicht.

Nach dem Web-Build prüft folgender Test die echte HTTP-API mit temporären Daten
und frisch erzeugten Testzertifikaten. Es werden keine Box-Zertifikate verwendet:

```sh
docker run --rm --network none -v "$PWD:/workspace:ro" \
  teddycloud-tag-filter-build:local node tests/test_tag_filter_api.mjs
```

Backend-Build, C-Regressionstests, HTTP-Tests und TypeScript/Vite-Build erfolgreich.
Der Frontend-Formatcheck gehört ebenfalls zur Prüfung. Der Server wurde für die
HTTP-Tests mit temporären Daten und ohne Cloud-Verbindung gestartet.

## Docker-Datenverzeichnis

Für eine Installation mit einem einzelnen Volume unter `/data` kann in Docker
die Umgebungsvariable `TEDDYCLOUD_BASE_PATH=/data` gesetzt werden. Sie legt sowohl
das Arbeitsverzeichnis als auch den `--base_path` des Servers fest. Auch die
Test-/Strace-Startmodi und `PUID`/`PGID` berücksichtigen diesen Pfad. Ohne diese
Variable bleibt das Standardverzeichnis von 0.7.0 `/teddycloud` bestehen.
