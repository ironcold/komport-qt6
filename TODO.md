# Komport-Qt6 — TODO

Aktive Liste offener Punkte. Die komplette, abgeschlossene Portierungs-/
Feature-/Review-Historie ist nach **`TODO-ARCHIVE.md`** ausgelagert, damit
diese Datei übersichtlich bleibt — dort stehen alle Details zu bereits
erledigter Arbeit (Basis-Portierung, Admin-Tool-Features, sieben
Review-Gate-Runden, Meilenstein 4, Meilenstein 5: Archiv-Abschnitte 1–21).
Architektur/Ziele stehen in `CLAUDE.md`.

Stand (2026-09-09): Meilenstein 4 (VT220/xterm) und Meilenstein 5
(Appearance-Tab) sind abgeschlossen, gemergt und auf beiden Remotes
(Codeberg/GitHub) synchron. Meilenstein 7 (Retro-/Industrie-Zeichensatz-
Übersetzung) ist implementiert und review-verifiziert, aber noch nicht
gemergt (Branch `milestone-7-charset`). Unten stehen nur die bewusst
offen gelassenen Punkte (Abschnitt 6), die Wunschliste (Abschnitt 7) und
die Roadmap.

## 0. Review- und Meilenstein-Historie (archiviert)

Kompakte Zusammenfassung — volle Details, Findings, Fixes und
Verifikationsprotokolle in `TODO-ARCHIVE.md`:

- **Review-Gate vor Meilenstein 4** (`TODO-ARCHIVE.md` Abschnitt 19,
  Unterabschnitte 19.1–19.7, 2026-09-06): sieben Codex-Adversarial-Review-
  Runden (sechs automatisiert + eine gezielte manuelle Durchsicht) über den
  kompletten Bestandscode. 2 Kritisch-, 4 Hoch-, ~15 Mittel- und
  2 Nitpick-Findings gefunden und gefixt (u.a. Absturz bei manipulierten
  Profilwerten, unbounded CSI-Buffer, Fenster-/Serial-Lifetime-Lecks,
  Busy-Loops in Transfer/Logger, mehrere Zell-Bounds-/Scrollback-Bugs).
  Dabei entstanden `tests/` und die ersten sechs `ctest`-Targets.
- **Architektur-Gate** (`ADR-001`, `docs/architecture-decisions/
  ADR-001-terminal-engine-strategy.md`, 2026-09-07): `QTermWidget` als
  optionales Terminal-Backend geprüft (`spike/qtermwidget-bridge/`,
  funktionierender Spike). Entscheidung: **Rejected** — bei der eigenen
  VT100/VT102-Emulation bleiben.
- **Meilenstein 4 — VT220/xterm-Erweiterungen** (`TODO-ARCHIVE.md`
  Abschnitt 20, 2026-09-08): 256-Farben-SGR, Scroll-Regionen (DECSTBM),
  Insert-Mode, VT220-Geräte-ID. Fünf Review-Runden (10 Findings, alle
  gefixt bis auf eine bewusst dokumentierte Lücke, DECOM/Origin-Mode,
  siehe Abschnitt 6). PR #2 auf Codeberg, gemergt.
- **Meilenstein 5 — Appearance-Tab** (`TODO-ARCHIVE.md` Abschnitt 21,
  2026-09-08): konfigurierbare Terminal-Schrift + Farbschemata,
  Farb-Herkunfts-Verfolgung, Profil-Integration. Zwei Codex/Gemma4-
  Review-Runden auf dem Appearance-Feature selbst (9 Findings, 8 gefixt),
  danach zwei weitere Nachbesserungsrunden auf einen dabei entdeckten,
  vorbestehenden GCC-`-Wsfinae-incomplete=`-Build-Warnung (Root-Cause-Fix
  über `Q_MOC_INCLUDE`, selbst nochmal per Review korrigiert) sowie ein
  Usability-Nachtrag (Settings-Menüpunkt umbenannt, da der Appearance-Tab
  sonst unauffindbar war). PR #3 + #4 auf Codeberg, beide gemergt,
  `master` auf Codeberg und GitHub synchron.
- **Meilenstein 7 — Retro-/Industrie-Zeichensatz-Übersetzung** (2026-09-09,
  PR #6, gemergt): neue `KomportCharset`-Klasse, byte-basierte
  Übersetzung zwischen rohem seriellem Bytestrom und Terminal-Emulation
  (RX in `KomportEmulation::slotReceivedChar()`s `default:`-Zweig, TX in
  `slotKeyPressed()`/`slotSimKeyPressed()`/Makro-Text), vollständig in
  die Profilverwaltung integriert. Umgesetzt: CP437 (0x80-0xFF-Bereich)
  und PETSCII (ASCII-kompatibler Bereich + £/↑/← + C0-Steuerbereich als
  Identität). Bewusst nicht umgesetzt: "Amiga" (keine belastbare Quelle
  für die früher eigenständige Amiga-1.x-Zeichenbelegung) und CP437s
  ikonischer 0x00-0x1F-Grafikbereich (echter Konflikt mit
  VT100-Steuercodes, siehe Abschnitt 6.3). Vier Codex-Review-Runden
  (`gpt-5.6-sol`, erste zusätzlich mit Gemma4-Gate) bis zur bestätigten
  Konvergenz ("no Medium or High issues found"), 14 Findings insgesamt,
  alle gefixt und verifiziert — u.a. eine Leerzeichen/NUL-Byte-Kollision
  im CP437-Rückwärts-Mapping, ein Profil-Leck-Bug derselben Klasse wie
  bei Meilenstein 5, mehrere stillschweigend-falsche-Byte-Fallbacks bei
  CP437/PETSCII, eine neue längenbasierte `KomportSerial::putStr()`-
  Überladung für korrekt übertragene eingebettete NUL-Bytes (echter
  Pty-Paar-Regressionstest via `openpty()`). `tests/tst_charset.cpp` neu.
  **Nachtrag — benutzerdefinierte Zeichensätze ohne Code (2026-09-09,
  Nutzerwunsch):** `KomportCharset` lädt zusätzlich beliebig viele
  `*.charset`-Dateien aus `customCharsetsDirectory()`
  (`~/.config/Komport-Qt6/charsets/` — der "Custom Charsets Folder..."-
  Button im Settings-Dialog öffnet ihn direkt), ohne Neubau nötig — siehe
  Abschnitt 1 unten für Format und Details.

## 1. Referenz: Benutzerdefinierte Zeichensätze (`*.charset`-Dateien)

Seit Meilenstein 7 (siehe Abschnitt 0 oben) unterstützt der Zeichensatz-
Dropdown im Settings-Dialog (Terminal-Tab) neben "Standard"/"IBM CP437"/
"PETSCII" beliebig viele selbst hinzugefügte Zeichensätze — **ohne Code
zu ändern oder neu zu bauen.** Eine Datei ablegen, Settings-Dialog neu
öffnen, fertig.

**Verzeichnis:** `KomportCharset::customCharsetsDirectory()` — praktisch
immer `~/.config/Komport-Qt6/charsets/` (direkt neben der eigentlichen
`Komport-Qt6.conf`), wird beim ersten Programmstart automatisch
angelegt. Der Button "Custom Charsets Folder..." im Settings-Dialog
(Terminal-Tab, unter dem Zeichensatz-Dropdown) öffnet ihn direkt im
Dateimanager.

**Dateiformat** (Klartext, eine `.charset`-Datei = ein Zeichensatz,
Dateiname ohne Endung = interner Name = Wert in `Profiles/<Name>/
Charset`):

```
# Name: Mein Zeichensatz
#
# Eine Zeile pro Abweichung: <Byte hex, 00-FF>=<Unicode-Codepoint hex, 0000-FFFF>
# Nicht aufgeführte Bytes bleiben automatisch Identität (Byte == Codepoint) -
# das macht Steuercodes (0x00-0x1F, 0x7F) sicher, ohne dass man beim
# Schreiben der Datei an VT100-Semantik denken muss.
DB=2588
41=03B1
```

- `# Name: ...` (optional) setzt den im Dropdown angezeigten Namen —
  fehlt die Zeile, wird der Dateiname selbst verwendet.
- Andere `#`-Zeilen sind reine Kommentare.
- Fehlerhafte einzelne Zeilen (kein `=`, ungültiges Hex, Codepoint über
  `FFFF`) werden übersprungen und geloggt (`qWarning()`), nicht die ganze
  Datei verworfen.
- Die Rückrichtung (Tastatur/Einfügen/Makro → Draht) wird automatisch aus
  derselben Tabelle abgeleitet — keine zweite Tabelle nötig. Ein Zeichen,
  das der Zeichensatz nicht abbilden kann, wird beim Senden als `?`
  markiert statt stillschweigend falsch/als NUL gesendet.
- Ein Dateiname, der (Groß-/Kleinschreibung egal) mit einem eingebauten
  Namen kollidiert ("Standard"/"CP437"/"PETSCII"), wird übersprungen und
  geloggt — kein stilles Überschatten der eingebauten, bereits mehrfach
  review-verifizierten Implementierungen.
- Neu eingeladen wird beim Programmstart und jedes Mal, wenn der
  Settings-Dialog geöffnet wird — kein Neustart nötig, um eine gerade
  abgelegte Datei nutzen zu können.

**Absicherung gegen fehlerhafte/pathologische Eingaben** (Codex-Review, drei
Runden, alle 16 Funde behoben — s. `TODO-ARCHIVE.md`; **wichtig für den
CNC-Übertragungs-Anwendungsfall**: die Übersetzungstabellen laufen im
tatsächlichen RX/TX-Datenpfad, sobald ein Custom-Zeichensatz auf einem für
die Übertragung genutzten Profil aktiv ist — ein Parsing-Bug hier landet
dann als falsches Byte auf der Leitung, nicht nur als Anzeigefehler):
- Datei-Format ist bewusst **nur UTF-8** (inkl. reinem ASCII) — nicht
  UTF-16/UTF-32. Eine Datei mit eingebettetem NUL-Byte gilt als binär/
  falsch kodiert und wird komplett abgelehnt (der praktikable Ansatz, da
  `QTextStream`/`QString::fromUtf8()` ungültige Bytes stillschweigend durch
  U+FFFD ersetzen statt einen Fehler zu melden).
- Einzelne Datei max. 1 MiB (erneut nach dem Einlesen geprüft, nicht nur
  vorher — eine zwischen Größenprüfung und Lesevorgang gewachsene Datei
  wird nicht mehr übersehen; ein Lesefehler mittendrin wird ebenfalls
  erkannt statt eine unvollständige Datei als vollständig zu behandeln),
  Zeilen max. 4096 Zeichen (exakt 4096 sind noch erlaubt — geprüft anhand
  der tatsächlichen, eindeutigen Zeilenlänge aus dem bereits eingelesenen
  Byte-Puffer, nicht mehr über `QTextStream::readLine(maxlen)`s
  Chunk-Splitting, das eine exakt grenzwertige Zeile nicht von einer
  abgeschnittenen unterscheiden konnte), max. 100000 gelesene Zeilen pro
  Datei.
- Max. 256 `*.charset`-Dateien pro Verzeichnis (alphabetisch), sowohl was
  tatsächlich geladen wird als auch was überhaupt erst geöffnet/untersucht
  wird — die Verzeichnisauflistung selbst nutzt einen unsortierten,
  speicherbegrenzten Scan (nie mehr als 256 Dateinamen gleichzeitig im
  Speicher) statt das komplette Verzeichnis erst vollständig aufzulisten
  und zu sortieren, bevor die Grenze greift.
- Unicode-Surrogate (`D800`-`DFFF`) werden als Codepoint abgelehnt wie
  jeder andere ungültige Wert — kein gültiger eigenständiger Unicode-
  Skalarwert. Die direkt angrenzenden gültigen Werte (`D7FF`/`E000`)
  bleiben erlaubt.
- Der angezeigte Name (`# Name: ...`) wird gegen Kollisionen mit
  eingebauten Namen und bereits geladenen anderen Custom-Zeichensätzen
  geprüft (Groß-/Kleinschreibung egal) — bei Kollision wird er um
  " (<Datei-ID>)" ergänzt, die Datei bleibt aber ganz normal nutzbar
  (anders als die reine ID-Kollision oben, die eine Datei komplett
  ablehnt — hier geht es nur um die Anzeige, nicht um die Funktion). Der
  erzeugte, disambiguierte Name wird dabei erneut auf Kollision geprüft
  (nicht nur der ursprüngliche) und bei Bedarf weiter durchnummeriert, da
  zwei Dateien rein zufällig denselben disambiguierten String erzeugen
  könnten.
- Verschwindet die Datei eines *aktuell ausgewählten* Custom-Zeichensatzes
  (gelöscht/umbenannt), wird die aktive Auswahl automatisch auf "Standard"
  zurückgesetzt — und zwar in **jedem** offenen Fenster
  (`KomportApp::reconcileCharsetSelectionAfterReloadForAllWindows()`),
  ausgelöst sowohl durch das Öffnen des Settings-Dialogs als auch durch das
  Erzeugen eines neuen Fensters ("Neues Fenster"), da beide die geteilte,
  prozessweite Registry neu laden. Ein Profil, das direkt (ohne vorheriges
  Live-Auswählen) auf eine bereits fehlende Custom-Zeichensatz-ID
  verweist, wird ebenfalls vollständig normalisiert — nicht nur die aktive
  Emulation, sondern auch der intern gemerkte Wert, der beim erneuten
  Speichern des Profils sonst die hängende Referenz weitergeschrieben
  hätte.
- Kann das `charsets`-Verzeichnis nicht angelegt werden (z. B. Dateisystem
  read-only, oder es liegt bereits eine reguläre Datei an dieser Stelle),
  wird das jetzt geloggt (`qWarning()`) statt stillschweigend einen
  unbrauchbaren Pfad zurückzugeben.

Vollständige technische Details/Rationale im Code-Kommentar von
`KomportCharset::reloadCustomCharsets()`/`loadCustomCharsetFile()`
(`komport/komportcharset.h`/`.cpp`) sowie
`KomportApp::reconcileCharsetSelectionAfterReload()`/
`reconcileCharsetSelectionAfterReloadForAllWindows()` (`komport/komport.h`).

## 6. Bekannte, bewusst nicht behobene Altlasten (vom Original übernommen)

- `KomportDoc::openDocument/saveDocument` waren im Original bereits reine
  TODO-Stubs ohne echte Dateiverarbeitung — bleiben es auch nach der Portierung.
- `KomportFileScrollBuffer::cell()` liefert immer `NULL` (Datei-Scrollback war im
  Original nie fertig implementiert) — unverändert übernommen.
- VT100/VT102-Emulation: Scroll-Regionen (`DECSTBM`/`CSI Pt;Pb r`), Reverse
  Index (`ESC M`, inkl. Rückwärts-Scrollen am oberen Rand der Region) und
  Insert-Mode (`CSI 4h`) sind seit Meilenstein 4 implementiert (`TODO-ARCHIVE.md`
  Abschnitt 20).
  Weiterhin nicht implementiert: VT52-Modus, echte Zeichensatz-Umschaltung
  (Linien-Grafikzeichen) und **DECOM/Origin-Mode** (`CSI ?6h/l`) — Cursor-
  Adressierung (`CSI H`/`f` und alle relativen Cursor-Bewegungen) bleibt immer
  physisch-bildschirmbezogen, auch wenn ein Host Origin-Mode explizit
  aktiviert; `doSetMode()` ignoriert `?6` bereits seit vor Meilenstein 4
  bewusst (Kommentar dort). Erst durch aktive Scroll-Regionen überhaupt
  beobachtbar (ohne Region gibt es nichts, worauf sich "relativ" beziehen
  könnte) — von Codex in der Meilenstein-4-Review gefunden, bewusst nicht
  gefixt: moderne Curses-Programme (vim, nano, htop) adressieren
  üblicherweise absolut statt sich auf Origin-Mode zu verlassen, reale
  Praxisrelevanz gering. Details siehe `TODO-ARCHIVE.md` Abschnitt 8.2 und
  Abschnitt 20.

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

## 6.2 Restpunkt: Schriftgrößen-Wechsel (Appearance-Tab) ändert die Fensterbreite

Aus einer Codex-Review-Runde zu Meilenstein 5 (siehe `TODO-ARCHIVE.md`
Abschnitt 21): der Appearance-Tab wendet Font-/Farbänderungen bewusst "ohne Layout-Sprung" an
(TODO.md-Vorgabe) — für Farben stimmt das exakt (reines Neuzeichnen, keine
Geometrieänderung). Bei einer *Schriftgröße* ist das aber architektonisch
nicht vollständig erreichbar: `KomportView` hält die Spaltenzahl fest
(`setMinimumSize()`/`setMaximumSize()` mit identischer Breite, "klassisches
Fixed-Width-Terminal", schon vor Meilenstein 5 so) — ändert sich die
Zeichenbreite der gewählten Schrift, ändert sich zwangsläufig die für
dieselbe Spaltenzahl benötigte Pixelbreite, was Qt's Layout dazu bringt,
das Fenster/den Splitter entsprechend zu verschieben. Keine Regression von
Meilenstein 5, sondern eine direkte Konsequenz des schon vorher bestehenden
Fixed-Width-Designs — bewusst nicht "gefixt": eine echte Lösung bräuchte
horizontales Scrollen/Clipping statt fester Breite, ein größeres Redesign
außerhalb des Meilenstein-5-Umfangs. Falls das später relevant wird: die
Spaltenzahl vom sichtbaren Ausschnitt entkoppeln (horizontale Scrollbar
oder Clipping), statt die Fensterbreite an die Zeichenbreite zu koppeln.

## 6.3 Restpunkt: CP437s ikonischer 0x00-0x1F-Grafikbereich nicht umgesetzt

Aus einer Codex-Review-Runde zu Meilenstein 7: die erste Fassung von
`KomportCharset` bildete CP437s komplette 256-Byte-Tabelle ab, inklusive
der bekannten "Steuerzeichen-Bereich als Grafik"-Glyphen (☺♥♦♣♠ etc. bei
0x01-0x06 usw. — ein echtes, bekanntes Merkmal der originalen IBM-PC-
Bildschirmschriftart). Codex fand einen echten Konflikt: diese Emulation
ist in erster Linie ein VT100/VT102-Interpreter, und mehrere reale
C0-Steuercodes, die `KomportEmulation::slotReceivedChar()` nicht explizit
behandelt (nur BEL/BS/HT/LF/CR/ESC sind es), fielen dadurch als CP437-
Grafikzeichen in den `default:`-Zweig statt als Steuercode erkannt zu
werden — z.B. hätte VT/FF (0x0B/0x0C, von vielen realen Hosts wie LF
genutzt) ein ♂/♀-Symbol gezeichnet und den Cursor nur um eine Spalte
verschoben statt einen Zeilenumbruch auszuführen. Ein nativer DOS-Textmodus
hatte dieses Problem nie (keine gleichzeitige ANSI-Escape-Interpretation
über denselben Bytebereich) — für ein VT100-Terminal ist es aber ein
echter Korrektheits-Rückschritt.

**Gefixt durch Scope-Reduktion (2026-09-09, Formulierung nach zweiter
Codex-Review-Runde präzisiert):** `KomportCharset::CP437` deckt jetzt nur
noch den unzweideutigen 0x80-0xFF-Bereich ab (Akzent-Buchstaben, Box-
Drawing/Block-Zeichen); 0x00-0x7F ist bewusst Identität (wie "Standard").
**Präzisierung:** das behebt nur die durch CP437 selbst neu eingeführte
Regression (falsche Glyphen für nicht behandelte Steuercodes) — die
zugrundeliegende Lücke selbst (VT/FF & Co. werden von
`slotReceivedChar()` nicht wie LF behandelt) ist eine vorbestehende,
von "Standard" geerbte Einschränkung dieser Emulation und bleibt
unverändert bestehen, wird durch diesen Fix nicht gelöst — nur nicht
mehr durch CP437 zusätzlich sichtbar verschlimmert. Opfert dafür die
ikonischen Grafikzeichen im unteren Bereich.

**Backlog-Vermerk (Nutzerwunsch: "gut dokumentieren und evtl. im
Backlog vermerken, dass hier u.U. noch nachgebessert werden muss"):**
falls die 0x00-0x1F-Grafikzeichen später gewünscht sind, bräuchte das
entweder (a) einen expliziten, vom Nutzer zuschaltbaren "Raw-Grafik-
Modus"-Schalter, der VT100-Steuercode-Interpretation für diese Bytes
bewusst abschaltet (Trade-off klar an den Nutzer kommuniziert), oder
(b) chirurgisches Einzelfall-Handling nur für die tatsächlich relevanten
VT100-Codes (mindestens 0x0B/0x0C wie LF behandeln, 0x18/0x1A auch
außerhalb einer CSI-Sequenz Escape-Sequenzen abbrechen lassen) bei
gleichzeitigem Belassen der übrigen Bytes als CP437-Grafik — beide
Optionen bewusst nicht in Meilenstein 7 umgesetzt (Aufwand/Risiko vs.
Nutzen für den seriellen Werkstatt-Terminal-Anwendungsfall dieses
Projekts), aber hier als möglicher künftiger Auftrag festgehalten statt
stillschweigend verworfen.

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

### Meilenstein 4 — Terminal-Upgrade (VT220 / erweiterter xterm-Farbraum) — ✅ erledigt (2026-09-08)

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

Alle Punkte umgesetzt und über mehrere Codex-Review-Runden verifiziert —
Details, Findings und Fixes in `TODO-ARCHIVE.md` Abschnitt 20. Eine Lücke
bewusst offen gelassen (DECOM/Origin-Mode, `CSI ?6h/l`), siehe Abschnitt 6.

### Meilenstein 5 — Konsolenkomfort (Aussehen-Tab, Farbschemata) — ✅ erledigt (2026-09-08)

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

Baute direkt auf der bestehenden Profilverwaltung auf (Abschnitt 11 in
`TODO-ARCHIVE.md`). Alle Punkte umgesetzt (Farb-Herkunfts-Verfolgung,
Profil-Integration, vier Farbschema-Presets) und über vier Codex/Gemma4-
Review-Runden verifiziert — Details, Findings und Fixes in
`TODO-ARCHIVE.md` Abschnitt 21. PR #3 + #4 auf Codeberg, beide gemergt.

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

### Meilenstein 7 — Retro-Computing- & Industrie-Zeichensatz-Übersetzung — ✅ erledigt (2026-09-09)

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

CP437 und PETSCII umgesetzt (siehe Abschnitt 0 oben), "Amiga" bewusst
nicht umgesetzt (keine belastbare Quelle für die eigenständige
Amiga-1.x-Zeichenbelegung, per Web-Recherche verifiziert statt geraten),
CP437s 0x00-0x1F-Grafikbereich bewusst zurückgestellt (Abschnitt 6.3) —
beide Abweichungen vom ursprünglichen Spec-Umfang sind bewusste,
dokumentierte Entscheidungen, keine übersehenen Lücken. Nachtrag
(Nutzerwunsch): benutzerdefinierte `*.charset`-Dateien ohne Code/Neubau
möglich, siehe Abschnitt 1.

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
