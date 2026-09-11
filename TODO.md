# Komport-Qt6 — TODO

Aktive Liste offener Punkte. Die komplette, abgeschlossene Portierungs-/
Feature-/Review-Historie ist nach **`TODO-ARCHIVE.md`** ausgelagert, damit
diese Datei übersichtlich bleibt — dort stehen alle Details zu bereits
erledigter Arbeit (Basis-Portierung, Admin-Tool-Features, sieben
Review-Gate-Runden, Meilenstein 4, Meilenstein 5: Archiv-Abschnitte 1–21).
Architektur/Ziele stehen in `CLAUDE.md`.

Stand (2026-09-12): Meilenstein 4 (VT220/xterm), Meilenstein 5
(Appearance-Tab), Meilenstein 6 (Internationalisierung, inkl. Nachtrag
alle 24 EU-Amtssprachen) und Meilenstein 7 (Retro-/Industrie-
Zeichensatz-Übersetzung, inkl. Custom-Charset-Nachtrag) sind
abgeschlossen, gemergt und auf beiden Remotes (Codeberg/GitHub)
synchron. Damit sind alle bisher geplanten funktionalen Meilensteine
umgesetzt — die verbleibenden offenen Punkte (Abschnitt 6) sind bewusst
akzeptierte, dokumentierte Trade-offs, keine Lücken; das erste
`1.0.0`-Release steht jetzt unmittelbar bevor. Unten stehen nur die
bewusst offen gelassenen Punkte (Abschnitt 6), die Wunschliste
(Abschnitt 7) und die Roadmap.

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
  Abschnitt 1 unten für Format und Details. Acht Codex-Review-Runden
  (`gpt-5.6-sol`) auf diesen Nachtrag, ~30 Findings insgesamt (u.a. zwei
  echte High-Bugs auf dem interaktiven RX/TX-Pfad: mehrdeutige
  Byte-zu-Zeichen-Zuordnungen in Custom-Tabellen, ein "?"-Platzhalter-
  Fallback, der bei manchen Tabellen selbst ein falsches Byte gesendet
  hätte), Runde 8 ohne neue Funde (Konvergenz bestätigt). PR #7 auf
  Codeberg, gemergt.
- **Meilenstein 6 — Internationalisierung (i18n)** (2026-09-11): alle
  sichtbaren String-Literale nutzen `tr()` (war bei näherer Prüfung schon
  fast vollständig der Fall), Qt6-`LinguistTools` eingebunden
  (`CMakeLists.txt`), `komport/translations/komport_de.ts` (154 Strings
  per `lupdate` extrahiert, 152 übersetzt, 2 bewusst offen gelassen -
  "Visual Bell"/"Framing", keine belastbare Übersetzungskonvention
  gefunden), zur Build-Zeit zu `.qm` kompiliert und per Qt-Resource-System
  eingebettet. `main.cpp` lädt automatisch per `QLocale::system()`, inkl.
  Qt's eigener Basis-Übersetzungen. **Nachtrag** (2026-09-11): alle 24
  EU-Amtssprachen per lokalem Gemma4 vorbereitet, 23 davon vollständig
  (162/162), Irisch bewusst als unübersetztes Skeleton zurückgestellt
  (durchgehend fehlerhafte Modellausgabe, dem Nutzer vorgelegt statt
  geraten) — Details siehe Abschnitt "Meilensteine (Detail)" unten.

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

**Absicherung gegen fehlerhafte/pathologische Eingaben** (Codex-Review, vier
Runden, alle 21 Funde behoben — s. `TODO-ARCHIVE.md`; **Reichweite für den
CNC-Übertragungs-Anwendungsfall, per Codex-Review präzisiert**: die
Zeichensatz-Übersetzung läuft ausschließlich im *interaktiven* Pfad
(Tippen, Einfügen, Makro-Text, RX-Anzeige) — ein Datei-Upload/-Download
(`KomportTransfer`) ist bewusst *roh*, byte-exakt, ohne jede
Zeichensatz-Übersetzung, genau damit ein per Upload gesendetes CNC-Programm
nicht durch eine (ggf. fehlerhafte) Übersetzungstabelle verändert werden
kann. Ein Parsing-Bug in einer Custom-Zeichensatz-Datei kann trotzdem
relevant werden, wenn während einer Sitzung interaktiv getippt/eingefügt
oder ein Makro gesendet wird, während dieser Zeichensatz aktiv ist — dann
landet ein falsches Byte auf der Leitung, nicht nur ein Anzeigefehler):
- Datei-Format ist bewusst **nur UTF-8** (inkl. reinem ASCII) — nicht
  UTF-16/UTF-32, und das wird jetzt auch strikt geprüft (`QStringDecoder`
  mit Fehlererkennung, einmal über die ganze Datei), nicht nur per
  NUL-Byte-Heuristik: eine Datei mit *irgendeiner* ungültigen UTF-8-Sequenz
  wird komplett abgelehnt, statt dass `QString::fromUtf8()` sie
  stillschweigend mit U+FFFD "repariert".
- Einzelne Datei max. 1 MiB (erneut nach dem Einlesen geprüft, nicht nur
  vorher — eine zwischen Größenprüfung und Lesevorgang gewachsene Datei
  wird nicht mehr übersehen, und der Lesevorgang selbst liest nie mehr als
  das Limit+1 Byte ein; ein Lesefehler mittendrin wird ebenfalls erkannt
  statt eine unvollständige Datei als vollständig zu behandeln), Zeilen
  max. 4096 **Unicode-Zeichen** (nicht UTF-16-Einheiten — ein Emoji
  o. ä. zählt als ein Zeichen, nicht zwei; exakt 4096 sind noch erlaubt),
  max. 100000 Zeilen pro Datei (eine gewöhnliche Datei mit genau 100000
  Zeilen und einem einzelnen abschließenden Zeilenumbruch löst dabei
  *nicht* fälschlich die "mehr als 100000 Zeilen"-Warnung aus). Das
  Zeilen-Einlesen selbst verarbeitet dabei eine Zeile nach der anderen und
  hält nie mehr als eine Zeile gleichzeitig im Speicher, unabhängig davon,
  wie viele Leerzeilen eine erlaubte 1-MiB-Datei enthält.
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
- Mapped eine Datei mehrere verschiedene Bytes auf denselben angezeigten
  Buchstaben (z. B. sowohl `01=0041` als auch `41=0041`), wird das jetzt
  geloggt (`qWarning()`): welches der beiden Bytes beim Tippen/Einfügen/
  per Makro tatsächlich gesendet wird (das zahlenmäßig niedrigere), war
  vorher stillschweigend mehrdeutig — direkt relevant für den
  Anwendungsfall oben, da genau das den falschen Steuercode auf die
  Leitung legen könnte.
- Ein Zeichen, das ein Custom-Zeichensatz nicht darstellen kann, sendet
  jetzt das Byte, das **diese Tabelle selbst** für ein literales "?"
  benutzt (statt immer starr Byte `0x3F`) — eine Tabelle, die `0x3F` z. B.
  auf "█" umlegt, hätte sonst genau dieses Byte als "kann ich nicht
  darstellen"-Platzhalter gesendet, obwohl es unter dieser Tabelle etwas
  ganz anderes bedeutet. Hat eine Tabelle *gar kein* Byte für "?" (auch
  nicht mehr über die Identitäts-Vorgabe von `0x3F` selbst, weil die
  Tabelle genau das umdefiniert), bleibt Byte `0x3F` als letzter Ausweg
  bestehen — es gibt dann buchstäblich kein "freies" Byte mehr, jedes der
  256 ist bereits belegt —, aber jetzt mit einer Laufzeit-Warnung
  (`qWarning()`), damit das zumindest sichtbar/diagnostizierbar ist statt
  still falsch.
- Die "nur UTF-8"-Prüfung erkennt jetzt auch eine mitten im letzten
  Zeichen abgeschnittene Datei (z. B. ein einzelnes UTF-8-Einleitungsbyte
  ganz am Dateiende ohne Folgebyte) als ungültig — vorher hätte
  `QStringDecoder`s Standardverhalten (auf eine evtl. noch folgende
  weitere Chunk-Übergabe ausgelegt) diesen Fall nicht als Fehler erkannt.
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

## 6.4 Restpunkt: astrale Zeichen (z. B. Emoji) verdoppeln sich beim Einfügen/Makro-Senden

Aus einer Codex-Review-Runde zum Custom-Charset-Mechanismus (Meilenstein
7 Nachtrag), aber **kein durch diesen Mechanismus verursachtes Problem**
— eine vorbestehende, architektonische Eigenschaft der Tasteneingabe-
Verarbeitung, die schon lange vor Meilenstein 7 so war: `KomportApp::
slotEditPaste()` (Einfügen aus der Zwischenablage) und `KomportApp::
slotMacroTriggered()` (Makro-Text senden) iterieren den zu sendenden
`QString` je Element as UTF-16-Einheit (`QChar`), nicht als echten
Unicode-Codepoint. Ein astrales Zeichen außerhalb der Basic Multilingual
Plane (z. B. die meisten Emoji, `U+1F600` "😀" u. ä.) wird intern als
Ersatzzeichenpaar (zwei `QChar`, ein "Surrogate Pair") dargestellt — jede
Hälfte für sich ist kein gültiger, eigenständiger Unicode-Codepoint und
kann von keiner Zeichensatz-Tabelle (eingebaut oder Custom) sinnvoll
abgebildet werden. Ergebnis: ein einzelnes eingefügtes/per Makro
gesendetes astrales Zeichen erzeugt zwei aufeinanderfolgende
"?"-Platzhalter-Bytes auf der Leitung statt (bestenfalls) eines.

**Bewusst nicht in dieser Runde behoben** (Nutzer-Entscheidung): eine
echte Lösung bräuchte entweder eine codepoint-bewusste Iteration in
beiden Aufrufstellen (inkl. Erkennung/Zusammenfügen von Ersatzzeichen-
Paaren vor dem Aufruf) oder eine Erweiterung von `KomportView::
slotSimKeyPressed(QChar)`, das aktuell nur einzelne `QChar` entgegennimmt
— beides größere Eingriffe in die Kern-Tasteneingabe-Architektur, nicht
nur den Custom-Charset-Lademechanismus. Betrifft *alle* Zeichensätze
gleichermaßen (Standard/CP437/PETSCII/Custom), nicht nur Custom-Tabellen,
und nur den interaktiven Einfüge-/Makro-Pfad (direktes Tippen einzelner
Tasten kann ohnehin nur ein `QChar` pro Tastendruck liefern, echte
Tastaturen erzeugen keine astralen Zeichen einzeln). Für den
CNC-Übertragungs-Anwendungsfall ein schmaler Randfall (Emoji in
G-Code-Sitzungen), aber hier festgehalten statt stillschweigend
übergangen.

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

### Meilenstein 6 — Internationalisierung (i18n) mit Qt6 Linguist — ✅ erledigt (2026-09-11)

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

Die meisten sichtbaren String-Literale waren bei näherer Prüfung bereits
mit `tr()` umschlossen (frühere Entwicklung hatte das schon weitgehend
mitgemacht) — ein erster Grep-Abgleich (Konstruktionsaufrufe von
`QLabel`/`QCheckBox`/`QMessageBox`/etc. gegen `tr(`) übersah aber zwei
echte Lücken, die eine anschließende Codex-Review fand: die Paritäts-
(NONE/EVEN/ODD) und Flusskontroll-Werte (XON/XOFF/RTS/CTS/NONE) im
Settings-Dialog waren als reine `QComboBox::addItems({...})`-Aufrufe gar
nicht erst mit `tr()` versehen — und die vier Farbschema-Namen
("Breeze Light" usw.) wurden zwar mit `tr(scheme.name)` übersetzt, aber
`lupdate` kann ein `tr()` mit einem *Laufzeit*-Argument (statt einem
Literal) nicht extrahieren, sodass keiner dieser vier Namen je in der
`.ts`-Datei gelandet wäre. Beide gefixt, siehe unten.

**Wichtiger Fallstrick bei der Paritäts-/Flusskontroll-Fix:**
`KomportSerial::applyPortSettings()` vergleicht `strParity`/
`strFlowControl` gegen feste englische Bezeichner (`"EVEN"`/`"ODD"`/
`"XON/XOFF"`/`"RTS/CTS"`, `"NONE"` als Standard für beide), und
`KomportApp` persistiert genau das, was die ComboBox zurückgibt, direkt in
`QSettings`. Ein naives `tr()`-Umschließen der Item-*Texte* hätte
`currentText()` die *übersetzte* deutsche Zeichenkette zurückgeben lassen
— stillschweigend als `strParity` gespeichert, gegen die englischen
Literale verglichen, und damit die falsche Parität angewendet (oder ein
Profil erzeugt, das bei Sprachwechsel nicht mehr korrekt geparst wird) —
ein Funktions-/Datenintegritätsbug, keine bloße Übersetzungslücke.
Behoben mit demselben `Qt::UserRole`-Entkopplungsmuster, das schon für
`CharsetComboBox` existierte: die Anzeige ist übersetzbar, der
gespeicherte/verglichene Wert (`UserRole`-Daten) bleibt unabhängig von der
Sprache der feste englische Bezeichner. `komport.cpp`s
`FlowControlComboBox`/`ParityComboBox`-Zugriffe entsprechend auf
`findData()`/`currentData()` umgestellt (statt `setCurrentText()`/
`currentText()`), inklusive derselben "unbekannter Wert lässt die
aktuelle Auswahl unangetastet"-Absicherung wie bei `CharsetComboBox`.
Die vier Farbschema-Namen brauchten keine solche Entkopplung (Auswahl ist
dort index-basiert, kein String-Vergleich) — nur `QT_TR_NOOP(...)` direkt
in der Tabellen-Definition, damit `lupdate` sie überhaupt findet.

Der eigentliche Aufwand lag daher im Tooling und der Übersetzung selbst:

- `CMakeLists.txt`: `Qt6::LinguistTools`-Komponente ergänzt;
  `qt6_add_translation(... OPTIONS -nounfinished)` kompiliert
  `komport/translations/*.ts` zur Build-Zeit zu `.qm` (niedrigere-Level-
  API statt `qt_add_translations()` — letztere existiert entgegen einer
  ersten, von Codex korrigierten Annahme bereits seit Qt 6.2, allerdings
  als CMake-Technology-Preview mit Schnittstellen-Überarbeitung erst mit
  Qt 6.7 stabilisiert; die niedrigere API ist seit 6.2 stabil und deshalb
  die robustere Wahl, nicht wegen einer harten Versionsgrenze);
  `qt6_add_resources()` bettet die `.qm`-Datei über das Qt-Resource-System
  ein (wie schon `komport.qrc` für die Icons) — funktioniert identisch aus
  dem Build-Verzeichnis wie aus einer Installation, kein Such-/Install-Pfad
  nötig. **Nebenbefund (vorbestehend, nicht durch diesen Meilenstein
  verursacht):** die deklarierte Mindestversion `find_package(Qt6 6.2 ...)`
  war schon vorher unerreichbar, da `qt_standard_project_setup()`
  tatsächlich Qt 6.3 voraussetzt — jetzt auf `6.3` korrigiert.
- `komport/translations/komport_de.ts`: per `lupdate` aus dem Quellcode
  extrahiert (154 Strings), 152 davon übersetzt. **2 bewusst
  unübersetzt gelassen** (Nutzervorgabe: "im Zweifel unübersetzt lassen,
  damit klar ist, da ist was offen") — "Visual Bell" und "Framing": für
  beide ließ sich keine belastbare, eindeutige deutsche Konvention
  verifizieren (Web-Recherche zu "Visual Bell" ergebnislos), `-nounfinished`
  sorgt dafür, dass diese beim Kompilieren zu `.qm` ausgelassen werden und
  zur Laufzeit sauber auf den englischen Quelltext zurückfallen, statt
  eine leere/geratene Übersetzung auszuliefern — bleiben in der `.ts`
  selbst als `unfinished` sichtbar für eine spätere Vervollständigung.
- `main.cpp`: zwei `QTranslator`-Instanzen — die eigene
  (`:/translations/komport_de.qm`, eingebettet) und Qt's eigene
  Basis-Übersetzung (`qtbase_de.qm`, aus der System-Qt-Installation über
  `QLibraryInfo::path(QLibraryInfo::TranslationsPath)`, deckt
  Standard-Dialogtexte wie OK/Abbrechen/Dateiauswahl ab) — beide über
  `QLocale::system()` geladen, *bevor* der erste `tr()`-Aufruf (die
  `--help`-Beschreibung) läuft. Beide sind optional: schlägt das Laden
  fehl (z.B. englisches System, oder eine Sprache ohne eigene `.ts`-Datei),
  bleibt es beim englischen Quelltext — kein Fehler, kein Absturz.
- Verifiziert per Offscreen-Smoketest mit `LANG=de_DE.UTF-8`/`LANG=en_US.UTF-8`:
  `--help`-Ausgabe korrekt lokalisiert (inkl. Qt's eigener Basis-Strings
  wie "Aufruf:"/"Optionen:"), englischer Fallback funktioniert
  unverändert, kein Absturz, echte `~/.config/Komport-Qt6/Komport-Qt6.conf`
  per md5sum unverändert.

Weitere Sprachen (über Deutsch hinaus) sind mit demselben Mechanismus
jederzeit ergänzbar (weitere `komport/translations/komport_<sprache>.ts`
+ ein Eintrag in `KOMPORT_TS_FILES` in `CMakeLists.txt`) — bei Bedarf
später, ggf. mit Gemma/Qwen auf lokaler Hardware für die
Rohübersetzung vorbereitet (spart Cloud-Tokens), nach demselben engen
Zuschnitt (bei Unsicherheit unübersetzt lassen).

#### Nachtrag: alle 24 EU-Amtssprachen (2026-09-11)

Nutzerwunsch, in zwei Schritten gewachsen: zunächst eine handvoll
"üblicher Verdächtiger" (Französisch, Spanisch, Italienisch,
Portugiesisch PT+BR) plus für die Retro-Szene interessante
osteuropäische Sprachen (Ungarisch, Polnisch, Tschechisch, dazu bonusweise
Russisch), dann nach durchweg guter Qualität die Entscheidung, gleich
alle 24 EU-Amtssprachen komplett vorzubereiten. Rohübersetzung komplett
lokal per Gemma4 (`gemma4:31b` auf dem M5-Server via `hermes chat`,
strikt sequentiell — nur ein Gemma-Thread gleichzeitig möglich, so vom
Nutzer vorgegeben), damit kein Cloud-Token-Budget für reine
Wörterbucharbeit verbraucht wird. Eigenes Tooling (nicht Teil des
Repos, nur Wegwerf-Skripte unter `/tmp`) extrahierte alle 162
Quell-Strings einmal geordnet, baute daraus pro Sprache einen
wiederverwendbaren Prompt (inkl. expliziter Tastenkürzel-Eindeutigkeits-
Gruppen pro Menü und einer Nicht-übersetzen-Liste für RX/TX,
"Komport-Qt6", "(wr mem)", VT100/VT102/ASCII/Latin-1) und spielte die
Antwort anhand der Dokumentreihenfolge (nicht Inhalt, wegen doppelter
Quell-Strings) in die jeweilige `.ts`-Datei zurück.

Jede Sprache durchlief dieselbe Prüfkette: Tastenkürzel-Kollisionen
automatisiert erkannt (Skript prüft die vier riskanten Menügruppen),
Korruptions-Scan auf fremde Schriftzeichen (nach dem
Ungarisch-Vorfall unten ergänzt), Stichproben der Schlüsseleinträge
(Menü-Akzeleratoren, RX/TX, Produktbeschreibung, "wr mem"),
abschließend erneuter `lupdate`-Lauf zur Bestätigung
"0 new and 162 already existing". Tastenkürzel-Kollisionen kamen in
9 von 24 Sprachen vor (Französisch, Italienisch, Portugiesisch PT/BR,
Tschechisch, Russisch, Bulgarisch, Kroatisch, Schwedisch, Rumänisch,
Litauisch/Lettisch) und wurden jeweils durch einen anderen, noch freien
Buchstaben aus demselben übersetzten Wort behoben.

**Qualitätsprobleme, nach demselben Prinzip behandelt wie beim
Ungarisch-Fund** (Web-Recherche bei Unsicherheit, im Zweifel
unübersetzt lassen):
- **Ungarisch:** ein Vietnamesisch-Schriftfragment ("cổngal" statt
  "soros port", echte Modell-Halluzination, per Regex-Scan auf
  vietnamesische Diakritika gefunden), ein falscher Fachbegriff für
  "Toolbar" (korrigiert auf "eszköztár", per Web-Recherche verifiziert),
  ein unklarer Begriff für "Flow control" (Recherche zeigte: in der
  ungarischen PuTTY-Community bleibt dieser Begriff meist unübersetzt —
  übernommen). Qwen3.8 als Vergleichs-Referenz getestet (Nutzer-Vorschlag
  für den Fall schwacher Gemma4-Qualität) — kompletter Fehlschlag (30 Min
  Timeout, keine Ausgabe); auf Nutzerentscheidung hin blieb die manuell
  korrigierte Gemma4-Fassung.
- **Estnisch:** derselbe Halluzinations-Fehlertyp wie Ungarisch — "seire-"
  (Überwachung) statt "jada-" (seriell) in drei Vorkommen von
  "serieller Port", per Web-Recherche auf "jadaport" korrigiert.
- **Maltesisch:** "Fenster" durchgehend mit einem erfundenen Wort
  ("tielet") statt dem korrekten "tieqa" übersetzt (4 Vorkommen), "Ready."
  als "Priest." fehlübersetzt, ein Wortgemisch ("għall-q lettura" statt
  "għall-qari") — alle per Web-Recherche verifiziert und einzeln
  korrigiert.
- **Irisch:** durchgehend fehlerhafte/erfundene Begriffe über das ganze
  Dokument verteilt (File/Edit/View/Quit/Settings falsch,
  widersprüchliche Schreibweisen desselben Worts an verschiedenen
  Stellen) — deutlich größerer Umfang als bei den anderen Sprachen, kein
  punktuell behebbarer Einzelfund mehr. Dem Nutzer vorgelegt statt
  eigenmächtig zu raten; Entscheidung: `komport_ga_IE.ts` bleibt
  vollständig unübersetzt (Skeleton, 162/162 `unfinished`) — fällt zur
  Laufzeit sauber auf den englischen Quelltext zurück, keine
  Funktionseinbuße, spätere Vervollständigung (idealerweise mit
  Muttersprachler-Prüfung) bleibt offen.

Ergebnis: 23 von 24 EU-Amtssprachen vollständig übersetzt (162/162,
Deutsch weiterhin mit 1 bewusst offenem Eintrag wie oben), Irisch
bewusst als Skeleton zurückgestellt. Vollständige Liste in
`CMakeLists.txt`s `KOMPORT_TS_FILES`. Build (`-Wall -Wextra`, keine
neuen Warnungen), alle 9 `ctest`-Ziele grün, Offscreen-Smoketest über
mehrere neue Locales (u.a. Griechisch als anderes Schriftsystem,
Maltesisch als am stärksten manuell korrigierte Sprache, Irisch als
unvollständige Sprache — überall sauberer Fallback, kein Absturz),
echte `~/.config/Komport-Qt6/Komport-Qt6.conf` per md5sum unverändert.

**Codex-Review-Funde (2 von 2, beide gefixt):**
- **Tooling-Artefakte in 26 Übersetzungen:** der letzte Eintrag
  ("Scheme:"/"Schema:"/... je nach Sprache) jeder betroffenen `.ts`-Datei
  enthielt sichtbaren Text aus dem eigenen Übersetzungs-Tooling
  (`EXIT:0` in 23 Dateien, `session_id: ...` in 3 weiteren) — ein
  Regex-Parsing-Fehler im (nicht versionierten) `/tmp`-Hilfsskript, das
  beim letzten Eintrag einer Gemma4-Antwort alles bis Dateiende statt
  nur bis zum nächsten nummerierten Marker erfasste. Vor dem Review
  unentdeckt, da die automatisierten Checks (Tastenkürzel, Fremdschrift-
  Scan) genau diesen Fall nicht abdeckten. Direkt als eindeutiger,
  ermessensfreier Datenfehler behoben (kein Übersetzungs-Ermessen
  beteiligt, reine Artefakt-Entfernung) — abweichend vom sonst üblichen
  "erst vorlegen, dann fixen"; dem Nutzer transparent gemeldet und auf
  Bestätigung hin beibehalten.
- **Locale-Fallback-Lücke:** `QTranslator::load()`s Dateinamen-Fallback
  streicht bei einer Locale wie `fr_BE` schrittweise Endungen ab
  (`komport_fr_BE` → `komport_fr`) — ein nur länderspezifisch benanntes
  Katalog wie `komport_fr_FR.ts` ist für gleichsprachige Nutzer außerhalb
  dieses einen Landes (fr_BE, nl_BE, sv_FI, de_AT, ...) unsichtbar,
  obwohl die Übersetzung für ihre Sprache vollständig vorliegt. Gefixt
  durch 22 zusätzliche sprachweite Alias-`.ts`-Dateien (`komport_fr.ts`,
  `komport_nl.ts`, ...) — reine Kopien der jeweils einzigen
  Länder-Variante mit angepasstem `<TS language="...">`-Attribut, keine
  eigenständig gepflegte Übersetzung. Portugiesisch (zwei echte
  Varianten: `pt_PT`/`pt_BR`) bekommt den Alias `komport_pt.ts` nach
  Linux/KDE-Konvention von `pt_PT` (europäisches Portugiesisch) kopiert;
  `pt_BR` bleibt eigenständig, da bewusst angelegt, keine Fallback-Lücke.
  Irisch bekommt keinen Alias (Skeleton ohne Inhalt). Verifiziert per
  Offscreen-Smoketest mit `fr_BE`/`nl_BE`/`sv_FI`/`pt_AO` — alle laden
  jetzt die jeweilige Sprachübersetzung statt auf Englisch
  zurückzufallen.

Nach beiden Fixes erneut: Build (`-Wall -Wextra`, keine neuen
Warnungen), alle 9 `ctest`-Ziele grün, echte
`~/.config/Komport-Qt6/Komport-Qt6.conf` per md5sum unverändert.

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
