# Komport-Qt6 — Modernisierung KDE3/Qt3 → Qt6

> **Projekt umbenannt:** Das Original-Projekt "Komport" (Mike Sharkey, KDE 2/3,
> zuletzt 2003 aktiv) wird upstream nicht mehr gepflegt. Dieser Qt6-Fork läuft
> deshalb eigenständig als **Komport-Qt6** weiter — Verzeichnis
> (`~/Entwicklung/komport-qt6`), CMake-Projekt-/Binary-Name (`komport-qt6`),
> `.desktop`-Anzeigename ("Komport Qt6") und Versionszählung (neu bei `1.0.0`,
> statt der von KDE3 geerbten `0.4.6`) sind entsprechend angepasst. Ältere
> Einträge in `TODO-ARCHIVE.md` sprechen noch von `./build/komport` — das war
> zum jeweiligen Zeitpunkt der tatsächliche Binary-Name und wird als
> historisches Protokoll nicht nachträglich umgeschrieben. Die Quelldateien in
> `komport/` behalten ihre Dateinamen und den Datei-Header-Titel "Komport
> Serial Port Communicator" als Verweis auf ihren Ursprung.

> **Status:** Die Portierung ist durchgeführt und baut sauber mit CMake/Qt6
> (`cmake -B build && cmake --build build`, auch mit `-Wall -Wextra` ohne Warnungen).
> Dazu gekommen sind vier Admin-Tool-Features: ein zuschaltbarer Hex-Monitor
> (RX/TX, Split-Screen), eine deutlich vollständigere VT100/VT102-Emulation
> (Cursor-Zähler, Insert/Delete Line/Char, DECCKM/DECTCEM, Device-Status-Reports,
> erweiterte SGR-Farben — plus zwei per Code-Review gefundene und gefixte
> Absturz-Bugs bei Cursor-Clamping), programmierbare Makro-Buttons,
> Ein-Klick-Session-Logging und eine Zeilenende-Auswahl (CR/LF/CRLF) für
> Enter-Taste und Makros. Was dabei gemacht/entschieden wurde, steht im Detail
> in `TODO-ARCHIVE.md`; offene Punkte/bekannte Lücken in `TODO.md`. Dieses
> Dokument bleibt als Ziel-/Architektur-Referenz für künftige Änderungen
> bestehen.

## Was ist Komport?

Ein serielles Terminalprogramm (VT100/VT102-ähnliche Emulation) aus 2003, ursprünglich
für KDE 2/3 geschrieben (Mike Sharkey). Autor der KDE-Portierung: `komport/` enthält
Doc/View/App-Struktur im klassischen KDevelop-1.2-Stil (KMainWindow + KAction-Menüs +
Session-Management). Build-System ist reines Autotools (`configure.in`, `Makefile.am`,
`admin/`) — kein CMake, keine `.pro`-Datei.

Kernfunktionen, die erhalten bleiben müssen:
- Serielle Verbindung öffnen/konfigurieren (Device, Baudrate, Datenbits, Stopbits,
  Parität, Flow Control) und Terminal-I/O in Echtzeit.
- Zeichen-Grid-Darstellung (`KomportCell`/`KomportCellArray`) mit Scrollback
  (`KomportScrollBuffer`, optional dateibasiert via `KomportFileScrollBuffer`).
- VT100/VT102-Emulation (`komportemulation.cpp`, ~910 Zeilen — der mit Abstand größte
  und wertvollste Teil des Codes; Escape-Sequenz-Logik selbst bleibt inhaltlich
  unverändert, nur die Qt/KDE-Abhängigkeiten drumherum werden ersetzt).
- Datei-Transfer-Grundgerüst (`KomportTransfer`/`KomportUpload`/`KomportDownload`,
  `KomportScript`, `KomportQueue`) — aktuell rudimentär, bei der Portierung nicht
  funktional erweitern, nur lauffähig halten.
- Settings-Dialog (`settingsdialog.ui`/`.cpp`/`.h`) für Verbindungsparameter.

## Produktvision / Daseinsberechtigung

Komport-Qt6 soll bewusst kein weiteres generisches Terminal fuer normale Shell-Arbeit werden. Die Daseinsberechtigung liegt in den Spezialfaellen, bei denen man heute oft mehrere halb passende Tools ausprobiert und am Ende trotzdem unzufrieden ist: serielle Konsolen von Netzwerk- und Industriegeraeten, historische Rechner, ungewoehnliche Zeilenenden, rohe Steuerzeichen, Diagnose-Mitschnitte, Makros, Geraeteprofile und Zeichensatz- oder Grafikzeichen-Eigenheiten.

Die Retroidee ist dabei kein Selbstzweck, sondern ein guter Produktanker: ein modernes Qt6-Werkzeug, das alte und widerspenstige serielle Welten ernst nimmt. Komfortfeatures duerfen ruhig an KDE Konsole, Kate oder klassische Terminalprogramme erinnern, muessen aber immer dem seriellen Spezialfall dienen. Wiederverwendung ist willkommen, solange sie diese Ziele nicht verdeckt oder das Projekt in unnoetige Framework-Abhaengigkeiten zieht.

## Terminal-Engine-Leitlinie

Die eigene `QSerialPort`-Pipeline bleibt vorerst die Referenzarchitektur, weil Hex-Monitor, Logging, Makros, Profile, Line-Endings und die geplante Zeichensatz-Uebersetzung direkt am seriellen Rohdatenstrom ansetzen. Eine fremde Terminal-Engine darf nur hinter einer klaren Bridge sitzen:

`QSerialPort -> RX/TX-Diagnose/Logging -> Zeichensatz-Uebersetzung -> Terminal-Backend`

und in Gegenrichtung:

`Terminal-Backend -> Zeichensatz-Uebersetzung/Line-Ending/Makros -> QSerialPort`

KDE `KonsolePart` ist technisch interessant, aber langfristig wahrscheinlich zu schwer fuer dieses Projekt: KF6/KParts/XmlGui-Abhaengigkeiten, PTY-/Shell-Fokus und zusaetzliche Bridge-Komplexitaet passen nur schlecht zu einem schlanken, reinen Qt6-Serial-Tool. `QTermWidget` ist der sinnvollere Wiederverwendungs-Kandidat fuer einen Spike, weil es als Qt-Widget einbettbar ist und weniger KDE-Ballast mitbringt. Trotzdem gilt: Wenn Spezialfeatures wie Zeichensatz-Tabellen, Hex-Sicht, Profile oder Rohdatenkontrolle dadurch schlechter werden, bleibt die eigene Emulation die bessere Wahl.

## Ziel dieser Migration

1. **KDE-Klassen raus, Standard-Qt6-Klassen rein.** Es soll am Ende ein reines Qt6-Programm
   sein, keine KDE-Frameworks-/KF6-Abhängigkeit mehr.
2. **Serielles Backend auf `QSerialPort` + `QSerialPortInfo` umstellen**
   (Qt SerialPort-Modul), als Ersatz für die aktuelle rohe POSIX/`termios`-Implementierung
   in `komportserial.cpp`.
3. Funktionsumfang und Bedienung so weit wie sinnvoll erhalten — das ist eine
   **Technologie-Migration, kein Rewrite der Funktionalität**. UI darf im Zuge dessen
   modernisiert werden (z.B. Qt Designer `.ui`-Dateien statt handgeschriebener
   Widget-Aufbau), aber Emulationslogik/Verhalten soll sich nicht ändern.

## Klassen-Mapping (KDE/Qt3 → Qt6)

| Alt (KDE/Qt3)                          | Neu (Qt6)                                          | Betroffene Dateien |
|-----------------------------------------|----------------------------------------------------|---------------------|
| `KApplication`, `kapp.h`                | `QApplication`                                      | `main.cpp`, `komport.h/.cpp` |
| `KCmdLineArgs`, `KCmdLineOptions`       | `QCommandLineParser`/`QCommandLineOption`           | `main.cpp` |
| `KAboutData`, `I18N_NOOP`               | Direkt in `QApplication`-Setup / ggf. weglassen; `QCoreApplication::translate` für i18n | `main.cpp` |
| `KMainWindow`                           | `QMainWindow`                                       | `komport.h/.cpp` |
| `KAction`, `KToggleAction`, `KActionCollection`, `KStdAction` | `QAction`, `QAction::setCheckable(true)` | `komport.h/.cpp` |
| `KRecentFilesAction`                    | eigene Recent-Files-Logik via `QAction`-Liste + `QSettings` (kein direktes Qt6-Äquivalent) | `komport.h/.cpp` |
| `KMenuBar`                              | `QMenuBar` (in `QMainWindow` bereits enthalten)     | `komport.cpp` |
| `KStatusBar`                            | `QStatusBar` (`QMainWindow::statusBar()`)           | `komport.cpp` |
| `KConfig`                               | `QSettings`                                          | `komport.h/.cpp` |
| `KMessageBox`                           | `QMessageBox`                                        | `komport.cpp`, `komportdoc.cpp` |
| `KIconLoader`, KDE-Icon-Theme            | `QIcon` (Ressourcen/Theme-Icons via `QIcon::fromTheme`) | `komport.cpp` |
| `KProgress`                              | `QProgressBar`/`QProgressDialog`                     | Transfer-Dialoge |
| `KURL`                                   | `QUrl`                                                | `komportdoc.h/.cpp`, `komport.h/.cpp`, `komporttransfer.h/.cpp`, `settingsdialog.h/.cpp` |
| `KURLRequester`                          | `QLineEdit` + `QToolButton` mit `QFileDialog::getOpenFileName`, oder eigenes Composite-Widget | `settingsdialog.ui/.h/.cpp` |
| `kfiledialog.h` (`KFileDialog`)          | `QFileDialog`                                         | `komport.cpp` |
| `komportui.rc` (KDE XML-UI-Framework)    | Menüs/Toolbars programmatisch in `QMainWindow` oder via Qt Designer `.ui` aufbauen | `komport.cpp`, `komportui.rc` entfällt |
| `KAccel`                                 | `QShortcut` / `QAction::setShortcut`                  | `komport.h/.cpp` |

## Klassen-Mapping (Qt3 → Qt6, unabhängig von KDE)

| Alt (Qt3)          | Neu (Qt6)                          | Hinweis |
|---------------------|--------------------------------------|---------|
| `QCString`          | `QByteArray`                        | v.a. `komportserial.h/.cpp` (Device-Name), `komportemulation.h` |
| `QPtrList<T>`        | `QList<T>`                          | `komportdoc.h/.cpp`, `komportcellarray.h` |
| `QSocketNotifier` für serielle Daten | entfällt — `QSerialPort::readyRead()` übernimmt das | `komportserial.cpp` |
| `local8Bit()`/`QString::local8Bit()` | `toLocal8Bit()` | überall wo verwendet |
| Altes `SIGNAL()/SLOT()`-Makro | funktioniert weiter in Qt6, aber neue Function-Pointer-Syntax (`connect(a, &A::sig, b, &B::slot)`) bevorzugen bei neu geschriebenem/portiertem Code | v.a. `komport.cpp`, `komportserial.cpp` |
| `WFlags`             | `Qt::WindowFlags`                    | `settingsdialog.h` |
| `QT_VERSION`-Constructor-Signatur `(parent, name)` | Qt6-Constructor `(parent, Qt::WindowFlags)`, `name` via `setObjectName()` | überall |

## Serielles Backend — Kernstück der Migration

**Erledigt:** `komportserial.h/.cpp` ist vollständig auf `QSerialPort` umgestellt (kein
rohes POSIX/`termios`, kein `QSocketNotifier`, `KomportQueue` entfällt) — Details zum
ursprünglichen Zustand und zur Umstellung in `TODO-ARCHIVE.md` Abschnitt 2.

## Architektur-Überblick

- `KomportApp` (`komport.h/.cpp`) — Hauptfenster, Menüs/Toolbar/Statusbar, Dateiverwaltung,
  verdrahtet auch die neueren Panels/Leisten unten (Hex-Monitor, Makro-Bar, Recording, s.u.).
  Zentralwidget ist ein `QSplitter` mit `KomportView` und `KomportHexView`.
  Trägt außerdem die **Geräteprofil-Verwaltung** (`initProfiles()`/`loadProfile()`/
  `saveProfile()`, Toolbar-`profileCombo`): ein Profil bündelt serielle Parameter,
  Zeilenende und die Makro-Bar-Belegung unter `QSettings`-Gruppe `Profiles/<Name>`;
  `readOptions()`/`saveOptions()` kümmern sich nur noch um Fenster-Chrome
  (Geometrie, Bar-Sichtbarkeit, zuletzt genutzte Dateien), nicht mehr um
  Verbindungseinstellungen. Details in `TODO-ARCHIVE.md` Abschnitt 11.2.
- `KomportDoc` (`komportdoc.h/.cpp`) — hält `KomportSerial`-Instanz, Document-View-Pattern
  (aus KDevelop-Boilerplate; für ein Terminal eigentlich zu schwergewichtig, aber wird
  strukturell übernommen statt neu designt).
- `KomportView` (`komportview.h/.cpp`) — Zeichen-Grid-Widget, Zeichnen, Maus-/Tastatur-Events,
  Auswahl/Zwischenablage, hält `KomportCellArray`, `KomportScrollBuffer`, `KomportEmulation`.
  **Achtung:** sitzt im Zentral-`QSplitter`, nicht direkt unter `KomportApp` — `getDocument()`
  läuft deshalb über `window()`, nicht `parentWidget()` (siehe `TODO-ARCHIVE.md` 8.6).
  Scrollbar ist ein echtes Kind-Widget (`KomportMinimapScrollBar`, s.u.), nicht mehr ein
  manuell positioniertes Sibling — siehe `TODO-ARCHIVE.md` Abschnitt 15 für die Bug-Historie.
- `KomportMinimapScrollBar` (`komportminimap.h/.cpp`, **neu**) — Kate-artige Minimap statt
  einer normalen `QScrollBar`: Text-Dichte-Silhouette der kompletten Historie
  (Scrollback + Live), Hover-Textvorschau, `QPalette`-basiert (folgt automatisch dem
  System-Theme). API-kompatibel zur alten `QScrollBar` (`value`/`setValue`/`maximum`/
  `setMaximum`/`valueChanged`), daher drop-in in `KomportView` integriert.
- `KomportEmulation` (`komportemulation.h/.cpp`) — VT100/VT102-Escape-Sequenz-Interpreter.
  **Größtes und wichtigstes Modul.** Inzwischen recht vollständig (Cursor-Bewegung mit
  Zähler, Insert/Delete Line/Char, DECCKM/DECTCEM, Device-Status-Reports, erweiterte
  SGR-Farben inkl. 256-Farben, Scroll-Regionen/DECSTBM, Tab, non-CSI-Escapes) —
  Details in `TODO-ARCHIVE.md` Abschnitt 8.2 und `TODO-ARCHIVE.md` Abschnitt 20
  (Meilenstein 4), bekannte Lücken (DECOM/Origin-Mode, VT52) in `TODO.md`
  Abschnitt 6. Trägt auch die
  `LineEnding`-Einstellung (CR/LF/CRLF) für die Enter-Taste und die Makro-Bar.
- `KomportCharset` (`komportcharset.h/.cpp`, **neu**, Meilenstein 7) — byte-basierte
  Zeichensatz-Übersetzung zwischen rohem seriellem Bytestrom und `KomportEmulation`
  (RX/TX, s. `CLAUDE.md`s Datenfluss-Vorgabe oben). Eingebaut: Standard (Identität),
  IBM CP437 (0x80-0xFF), PETSCII (ASCII-kompatibler Bereich + £/↑/←). Zusätzlich lädt
  `reloadCustomCharsets()` beliebig viele benutzerdefinierte `*.charset`-Dateien aus
  `customCharsetsDirectory()` (`~/.config/Komport-Qt6/charsets/`) — neue Zeichensätze
  lassen sich so ohne Code-Änderung/Neubau hinzufügen, siehe `TODO.md` Abschnitt 1
  für das Dateiformat.
- `KomportCell`/`KomportCellArray` — Zeichen-Zellen-Modell des sichtbaren Bildschirms;
  `KomportCellArray` trägt seit der Feature-Erweiterung auch das DECTCEM-Sichtbarkeits-Flag
  für den Cursor (`cursorVisible()`/`setCursorVisible()`/Signal `cursorVisibilityChanged`).
- `KomportScrollBuffer`/`KomportFileScrollBuffer` — Scrollback (Speicher bzw. Datei).
- `KomportSerial` (`komportserial.h/.cpp`) — auf `QSerialPort` umgestellt (s.o.); trägt
  neben `receivedChar(char)` inzwischen auch `sentChar(char)` (für den Hex-Monitor).
- `KomportTransfer`/`KomportUpload`/`KomportDownload`/`KomportScript` — Datei-Transfer-Grundgerüst.
- `SettingsDialog` (`settingsdialog.h/.cpp`) — Verbindungseinstellungen, handgeschrieben mit
  Qt6-Layouts (die alte Qt3-`.ui` ist entfernt, `uic` von Qt6 kann sie nicht lesen).
- `KomportHexView` (`komporthexview.h/.cpp`, **neu**) — zuschaltbares Split-Screen-Hexdump-Panel
  (RX/TX getrennt, 16 Byte/Zeile, roh vor jeder Emulations-Interpretation), mit zwei
  RX/TX-Filter-Checkboxen im Panel-Header, deren Zustand pro Profil persistiert wird.
- `KomportMacroBar` (`komportmacrobar.h/.cpp`, **neu**) — 8 programmierbare Quick-Command-Buttons,
  unten angedockt, editierbar per Klick/Rechtsklick, persistiert.
- `KomportSessionLogger` (`komportsessionlogger.h/.cpp`, **neu**) — Ein-Klick-Mitschnitt des
  empfangenen Bytestroms in eine zeitgestempelte Textdatei.

## Build-System

**Erledigt:** das alte Autotools-Setup (`configure.in`, `acinclude.m4`, `admin/*`,
KDevelop-1.2-generiert, KDE-2/3-typisch) wurde komplett durch ein Top-Level
`CMakeLists.txt` ersetzt (`find_package(Qt6 COMPONENTS Widgets PrintSupport
SerialPort REQUIRED)`, `CMAKE_AUTOMOC`/`CMAKE_AUTORCC`, C++17). Alle Autotools-
Dateien und die KDevelop-1.x-Projektdateien sind entfernt (siehe `TODO-ARCHIVE.md`
Abschnitt 3 für die vollständige Liste). Bauen: `cmake -B build && cmake --build build`.
Der Hauptcode liegt in einer Objekt-Library `komport_core` (alles außer
`main.cpp`), gegen die sowohl das `komport-qt6`-Executable als auch die
`Qt6::Test`-basierten Regressionstests in `tests/` linken (`ctest` im
Build-Verzeichnis nach dem Build; `BUILD_TESTING` via `include(CTest)`,
Default an). Siehe `TODO.md` Abschnitt 0.1 für den Anlass.

## Was NICHT im Scope ist (sofern nicht anders vom Nutzer gewünscht)

- Kein unkontrollierter Wechsel auf eine fremde Terminal-Engine. Terminal-Engine-Arbeit
  muss als eigener Spike/ADR entschieden werden; `QTermWidget` ist der bevorzugte
  Kandidat fuer Wiederverwendung, `KonsolePart` nur als bewusst dokumentierte
  Gegenprobe. Bis dahin bleibt die eigene VT100/VT102-nahe Emulation der
  stabile Pfad.
  **Explizit vom Nutzer gewünschte Ausnahme:** die hellen ANSI-Farbcodes `90–97`/`100–107`
  (`komportemulation.cpp`, `doGraphics()`) sind aixterm/xterm-Herkunft, nicht Teil der
  originalen VT102-Doku im Kopfkommentar dieser Datei — sie wurden bewusst mit
  aufgenommen, weil der Nutzer explizit "korrektes Handling von Farb-Codes (ANSI)"
  als vollständig gefordert hat und reale Zielgeräte (Cisco/Juniper-CLIs, eingefärbte
  `ls`-Ausgaben o.ä.) sie routinemäßig senden. Bewusste, dokumentierte Ausnahme von der
  VT100/VT102-Beschränkung. **Update Meilenstein 4 (siehe `TODO.md` Abschnitt 0.8):**
  das 256-Farben-Protokoll (`CSI 38;5;N`/`48;5;N`) wurde nachträglich ebenfalls
  bewusst aufgenommen (explizit als Meilenstein-4-Ziel spezifiziert, nicht nur
  aixterm-Kompatibilität) — bleibt aber die einzige weitere Ausnahme: kein
  True-Color (`CSI 38;2;r;g;b` wird geparst/konsumiert, aber nicht angewendet),
  kein sonstiger xterm-Funktionsumfang.
- Keine funktionale Erweiterung des Datei-Transfers (Upload/Download/Script) über das
  bisherige rudimentäre Grundgerüst hinaus.
- **Überholt seit Meilenstein 6 (Internationalisierung):** diese Zeile
  schätzte i18n ursprünglich als "nicht Kernziel" ein — inzwischen ist es
  ein vollwertiger, umgesetzter Meilenstein (siehe `TODO.md` Abschnitt 0,
  "Meilensteine (Roadmap)"): alle sichtbaren String-Literale nutzen `tr()`,
  Qt6-`LinguistTools` sind eingebunden (`CMakeLists.txt`), eine deutsche
  Übersetzung (`komport/translations/komport_de.ts`) wird zur Build-Zeit
  zu `.qm` kompiliert und per Qt-Resource-System eingebettet, `main.cpp`
  lädt sie automatisch anhand der Systemsprache (`QLocale::system()`),
  inklusive Qt's eigener Basis-Übersetzungen (Standard-Dialogtexte wie
  OK/Abbrechen). Die alte KDE-`I18N_NOOP`/`.po`-Kette (aus der
  KDE3-Portierung geerbt) ist komplett durch die moderne Qt6-`.ts`/`.qm`-
  Kette ersetzt.

## Sonstiges

- Lizenz: GPL (siehe `COPYING`), Original-Autor Mike Sharkey — bei Umbenennungen/neuen
  Dateien Copyright-Header-Konvention der bestehenden Dateien beibehalten bzw. sinnvoll
  ergänzen (nicht den ursprünglichen Autor entfernen).
- `komport.desktop`, Icons (`lo16-app-komport.png`, `lo32-app-komport.png`) bleiben
  nutzbar, ggf. Pfade/Kategorien im `.desktop`-File an Nicht-KDE-Umgebungen anpassen.
