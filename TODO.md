# Komport-Qt6 — TODO

Aktive Liste offener Punkte. Die komplette, abgeschlossene Portierungs-/
Feature-/Review-Historie (Abschnitte 0–5, 8–10) ist nach **`TODO-ARCHIVE.md`**
ausgelagert, damit diese Datei übersichtlich bleibt — dort stehen alle Details
zu bereits erledigter Arbeit. Architektur/Ziele stehen in `CLAUDE.md`.

Stand: alle bisherigen Aufträge (Basis-Portierung, Admin-Tool-Features,
Code-Review-Fixes, Umbenennung) sind abgearbeitet — siehe `TODO-ARCHIVE.md`.
Unten stehen nur die bewusst offen gelassenen/verschobenen Punkte.

## 0. Review-Gate vor weiterer Feature-Arbeit

- [ ] Vor neuen funktionalen Meilensteinen einen kompletten Codex-/Adversarial-Review
  des aktuellen Stands durchfuehren. Hintergrund: Die Qt6-Portierung und die
  Admin-Tool-Features sind auf einem anderen Rechner entstanden und noch nicht
  nach den strengeren Template-Vorgaben dieses Arbeitsbereichs gesteuert
  worden. Review-Fokus: serielle I/O mit `QSerialPort`, VT100/VT102-
  Emulation, Scrollback/Minimap, Hex-Monitor/Session-Logging, Profil-
  Persistenz, UI-Lifetime/Signal-Slot-Verbindungen, Build-/Install-Pfade,
  Lizenz-/Header-Konsistenz und fehlende Tests/Smoke-Checks. Findings vor
  Meilenstein 4/5/6/7 priorisieren und als konkrete TODOs schneiden.


## 1. Produktvision und Architektur-Gate

- [ ] Produktvision festhalten und bei kuenftigen Features gegenpruefen: Komport-Qt6
  soll ein spezialisiertes serielles Werkstatt-Terminal werden, nicht noch ein
  allgemeines Shell-Terminal. Kernnutzen: die schwierigen Faelle, fuer die man
  sonst mehrere halb passende Tools sucht: Netzwerk-/Industriegeraete, alte
  Rechner, ungewoehnliche Zeilenenden, rohe Steuerzeichen, Hex-Diagnose,
  Mitschnitte, Makros, Profile und Retro-/Industrie-Zeichensaetze.
- [ ] Terminal-Engine-ADR vor Meilenstein 4 finalisieren: eigene Emulation weiter
  ausbauen vs. `QTermWidget` als optionales Backend. Entwurf liegt in
  `docs/architecture-decisions/ADR-001-terminal-engine-strategy.md`.
  Entscheidungsfrage ist nicht "koennen wir etwas wiederverwenden?", sondern
  ob Wiederverwendung die
  seriellen Spezialfeatures einfacher, stabiler und wartbarer macht.
- [ ] Kleinen Spike fuer `QTermWidget` planen, bevor VT220/xterm-Komfort tief in
  die eigene Emulation eingebaut wird: RX/TX-Bridge, Tastatureingaben,
  Scrollback, Farbschemata, Cursor, Copy/Paste und Performance pruefen. Die
  Zeichensatz-Uebersetzung muss dabei weiterhin vor der Terminal-Interpretation
  auf Byte-Ebene moeglich bleiben.
- [ ] `KonsolePart` nur als dokumentierte Gegenprobe aufnehmen, nicht als
  bevorzugten Pfad: zu viele KDE-/KF6-Abhaengigkeiten und PTY-/Shell-Fokus fuer
  ein schlankes Qt6-Serial-Tool.

## 6. Bekannte, bewusst nicht behobene Altlasten (vom Original übernommen)

- `KomportDoc::openDocument/saveDocument` waren im Original bereits reine
  TODO-Stubs ohne echte Dateiverarbeitung — bleiben es auch nach der Portierung.
- `KomportFileScrollBuffer::cell()` liefert immer `NULL` (Datei-Scrollback war im
  Original nie fertig implementiert) — unverändert übernommen.
- VT100/VT102-Emulation: Scroll-Regionen (`DECSTBM`/`CSI r`) werden nur
  konsumiert, nicht angewendet; `ESC M` (Reverse Index) scrollt am oberen Rand
  nicht rückwärts; VT52-Modus, echte Zeichensatz-Umschaltung
  (Linien-Grafikzeichen) und Insert-Mode (`CSI 4h`) sind nicht implementiert.
  Details siehe `TODO-ARCHIVE.md` Abschnitt 8.2.

## 6.1 Restpunkt: natives Toolbar-Tooltip verschwindet gelegentlich statt zu wechseln

Stand nach `TODO-ARCHIVE.md` Abschnitt 18.2 (`QToolTip::hideText()` direkt
vor `QToolTip::showText()`, um die Qt-Popup-Geometrie-Wiederverwendung zu
umgehen): die Abschneide-Truncation ist behoben, aber laut Nutzer-Test
"fast korrekt" — beim Wechsel zwischen zwei Icons blendet sich das native
Tooltip inzwischen manchmal komplett aus, statt sofort mit dem neuen Text
wieder zu erscheinen (vermutlich `hideText()` gefolgt zu schnell von
`showText()` wird von Qt intern nicht immer als "neu zeigen", sondern als
"kommentarlos verwerfen" behandelt). Nutzer stuft das ausdrücklich als
tolerierbar ein ("ist aber erst mal ok") — bewusst zurückgestellt statt
sofort weiter zu patchen. `hoverHintLabel` in der Statusleiste zeigt den
Hover-Text in der Zwischenzeit ohnehin zuverlässig an. Falls das später
angegangen wird: evtl. mit einem kurzen `QTimer::singleShot(0, ...)`
zwischen `hideText()` und `showText()` experimentieren, oder ganz auf ein
eigenes, immer neu erzeugtes Tooltip-Widget umsteigen statt den
`QToolTip`-Singleton zu nutzen.

## 7. Wunschliste / mögliche nächste Schritte

Ursprünglich aus der alten `TODO`-Datei des Original-Autors (2003) übernommen,
seither ergänzt:

- VT100-Emulation weiter finalisieren, insbesondere die oben unter Abschnitt 6
  genannten Lücken (Scroll-Regionen wären der aufwändigste Brocken — bräuchte
  eine scroll-region-fähige `scrollUp()`/neue `scrollDown()` in
  `KomportCellArray`).
- Mögliche Erweiterungen der Profilverwaltung (Abschnitt 11 in
  `TODO-ARCHIVE.md`), falls gewünscht: Umbenennen bestehender Profile,
  Import/Export als Datei, ein "Profil wechseln" ohne den Kombinationsfeld-Text
  erst manuell zu tippen bei sehr vielen Profilen (z.B. Sortierung/Filter).
- Ideen aus der `README.md`, die (noch) nicht umgesetzt sind, bei Bedarf hier
  eintragen, bevor sie als Feature versprochen werden.

## Meilensteine (Roadmap)

Ursprünglich als einzelne Prompt-Textdateien im lokalen (gitignorten) `helper/`-
Ordner gesammelt, hier dauerhaft dokumentiert. Nummerierung entspricht den
Dateinamen dort (`2-scrollleiste.txt` usw.); die Rohdateien wurden nach dem
Übertragen hierher gelöscht, zwei zugehörige Screenshots liegen archiviert in
`docs/screenshots/` (zeigen den in Meilenstein 2 gefixten Scrollbar-Bug).

### Meilenstein 2+3 — ✅ erledigt

Layout-Fix, Kate-artige Minimap-Scrollbar und RX/TX-Diagnosefilter im
Hex-Monitor. Vollständig umgesetzt und verifiziert — Details in
`TODO-ARCHIVE.md` Abschnitt 15.

### Meilenstein 4 — Terminal-Upgrade (VT220 / erweiterter xterm-Farbraum) — offen

> Vollständige VT220-Kompatibilität sowie Integration moderner
> xterm-Erweiterungen, damit komplexe CLI-Tools wie htop, tmux und farbige
> Shell-Skripte perfekt gerendert werden. Im Einzelnen:
> - 256-Farben-Protokoll: `CSI 38;5;Xm` (Vordergrund) / `CSI 48;5;Xm`
>   (Hintergrund) korrekt parsen und im Zeichen-Grid darstellen. (Die
>   16 erweiterten Farben `CSI 90–97m`/`100–107m` sind bereits umgesetzt,
>   siehe `TODO-ARCHIVE.md` Abschnitt 8.2/9.5.)
> - Scroll-Regionen (`DECSTBM`, `CSI Pt;Pb r`) vollständig anwenden, damit
>   Bildschirmsplits (z.B. `vi`/`nano`) nicht den Rest des Terminal-Layouts
>   zerreißen — bereits als Lücke in Abschnitt 6 oben notiert, hier
>   konkretisiert: braucht eine scroll-region-fähige `scrollUp()`/neue
>   `scrollDown()` in `KomportCellArray`.
> - Reverse Index (`ESC M`) innerhalb der Scroll-Region rückwärts scrollen
>   lassen (aktuell: stoppt einfach am oberen Rand, siehe Abschnitt 6).
> - Insert-Mode (`CSI 4h` an, `CSI 4l` aus).
> - Terminal-Identifikation: `CSI 0c`/`CSI ?6c`-Anfragen mit einem
>   passenden VT220-String beantworten (aktuell antwortet
>   `doDeviceAttributes()` immer als VT102, siehe `TODO-ARCHIVE.md` 8.2).
> - Performance im Blick behalten, weiterhin 0 Warnungen bei `-Wall -Wextra`.

Kein Automatismus, der ohne Weiteres "einfach mehr" macht — bewusst als
eigener, noch nicht begonnener Auftrag stehen gelassen (größerer Eingriff in
`komportemulation.cpp`, siehe auch die Scope-Diskussion zu VT100/VT102 vs.
xterm-Erweiterungen in `CLAUDE.md`).

### Meilenstein 5 — Konsolenkomfort (Aussehen-Tab, Farbschemata) — offen

> Optische Konfiguration des Terminal-Ausgabefelds, angelehnt an KDE
> Konsole-Profile:
> - Neuer Tab "Aussehen"/"Appearance" im Settings-Dialog: Schriftart-Auswahl
>   (Monospace-gefiltert) inkl. Spacing/Größe, Farbauswahl (Hintergrund,
>   Standard-Schriftfarbe) über `QColorDialog`.
> - Live-Anwendung im Terminal-Grid ohne Layout-Sprung.
> - Vollständig in die Profilverwaltung integriert: Profil laden lädt auch
>   Schriftgröße/Farbschema mit (analog zu Baudrate/Makros/Zeilenende, die
>   das schon tun).
> - Vorgefertigte, anpassbare Farbschema-Vorlagen wie in KDE Konsole,
>   mindestens: "Breeze Light", "Breeze Dark", "Green on Black" (klassisches
>   Retro-Terminal), "Black on Light Yellow" (augenschonend). Dropdown zur
>   Auswahl, Farb-Buttons passen sich automatisch an, bleiben aber vor dem
>   Speichern individuell überschreibbar.

Baut direkt auf der bestehenden Profilverwaltung auf (Abschnitt 11 in
`TODO-ARCHIVE.md`) — vermutlich der nächstliegende Kandidat nach Meilenstein 4,
da die Profil-Infrastruktur dafür schon steht.

### Meilenstein 6 — Internationalisierung (i18n) mit Qt6 Linguist — offen

> Mehrsprachigkeit (mind. Englisch/Deutsch) für Menüs, Tooltips, Buttons,
> Dialoge:
> - Alle sichtbaren String-Literale im C++-Code durch `tr()` ersetzen
>   (Quellcode bleibt englisch, z.B. `tr("Connect")`).
> - `CMakeLists.txt`: Qt6-Modul `LinguistTools` einbinden, `qt_add_translations()`
>   für automatische `.ts`/`.qm`-Generierung.
> - `komport_de.ts` mit vollständiger deutscher Übersetzung (File→Datei,
>   Edit→Bearbeiten, Settings→Einstellungen, ...).
> - `main.cpp`: `QTranslator` einbinden, der beim Start automatisch per
>   `QLocale` die Systemsprache abfragt und bei Bedarf die deutsche
>   Übersetzung lädt.

Größerer, mechanischer Umbau über sehr viele Dateien (praktisch jede `.cpp`
mit sichtbarem Text) — eigener, in sich abgeschlossener Auftrag, am besten
NACH den funktionalen Meilensteinen 4/5, damit nicht doppelt an neu
hinzukommenden Strings gearbeitet werden muss.

### Meilenstein 7 — Retro-Computing- & Industrie-Zeichensatz-Übersetzung — offen

> Option zur Zeichensatz-Übersetzung zwischen `QSerialPort` und der
> Emulation, um die Kommunikation mit historischen Systemen zu
> ermöglichen:
> - GUI: ein Dropdown "Zeichensatz" (Standard, IBM CP437, Amiga, PETSCII).
> - Logik: zweiseitige Lookup-Tabellen (RX/TX), die historische
>   Zeichensätze dynamisch in modernes UTF-8 (und umgekehrt) umrechnen,
>   damit Sonder- und Grafikzeichen auf Retro-Plattformen fehlerfrei
>   dargestellt werden.
> - Integration: das Mapping muss ebenfalls im aktuellen Geräteprofil
>   speicherbar sein (wie Baudrate/Makros/Zeilenende, siehe Abschnitt 11
>   in `TODO-ARCHIVE.md`).

Eigenständiges Feature, unabhängig von den VT220/Farbschema-Meilensteinen
4/5 — reine Byte-Ebene (vor der Terminal-Emulation), keine Überschneidung.
Sitzt an der gleichen Stelle im Datenfluss wie der Hex-Monitor (roher
RX/TX-Bytestrom), bevor `KomportEmulation` die Escape-Sequenzen
interpretiert.

### Vision (nicht 1.x-Sprint): Netzwerk-Erweiterungen

Architektonischer Leitfaden für später, explizit **nicht** für den aktuellen
1.x-Sprint gedacht — nur als Hinweis, die bestehende Modularität
(`KomportSerial`, `KomportEmulation`) so zu belassen, dass sie später
wiederverwendbar bleibt:

- **Modus A — abgesetzter Dienst (Serial-over-TCP / RFC 2217):** ein neues,
  leichtgewichtiges Headless-`komport-daemon`-Target (systemd-Dienst auf
  einem entfernten Linux-Knoten oder Arduino/ESP32), das serielle Rohdaten
  transparent per RFC 2217 (Telnet Com Port Control Protocol) in TCP-Pakete
  verpackt. GUI bekäme im Settings-Dialog neben lokalen Ports eine
  "Remote TCP Connection"-Option (IP + Port); Profilverwaltung, Makros und
  Hex-Monitor blieben dabei vollständig nutzbar, nur die Baudrate des
  entfernten Geräts würde über RFC 2217 gesteuert.
- **Modus B — Web-Terminal (HTTP/HTTPS + WebSocket):** derselbe Daemon,
  erweitert um `QtHttpServer`/`QtWebSockets`, liefert eine minimale
  HTML5/JS-Seite mit eingebettetem `xterm.js` als Terminal-Frontend;
  WebSocket-Verbindung reicht Tastatureingaben an `QSerialPort` durch und
  empfangene Bytes zurück in den Browser. Für später: HTTPS/WSS via
  `QSslConfiguration`, einfaches HTTP-Basic-Auth/Token-Verfahren.

## Sonstiges

- Remote-Repositories: `origin` zeigt auf Codeberg
  (`ssh://git@codeberg.org/ironcold/komport-qt6.git`), `github` zeigt auf
  GitHub (`git@github.com:ironcold/komport-qt6.git`). Vor Pushes bewusst
  entscheiden, ob nur Codeberg oder Codeberg plus GitHub bedient werden soll.
