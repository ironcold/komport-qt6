# Komport-Qt6 — Modernisierung KDE3/Qt3 → Qt6

> **Projekt umbenannt:** Das Original-Projekt "Komport" (Mike Sharkey, KDE 2/3,
> zuletzt 2003 aktiv) wird upstream nicht mehr gepflegt. Dieser Qt6-Fork läuft
> deshalb eigenständig als **Komport-Qt6** weiter — Verzeichnis
> (`~/Entwicklung/komport-qt6`), CMake-Projekt-/Binary-Name (`komport-qt6`),
> `.desktop`-Anzeigename ("Komport Qt6") und Versionszählung (neu bei `1.0.0`,
> statt der von KDE3 geerbten `0.4.6`) sind entsprechend angepasst. Ältere
> Einträge in `TODO.md` sprechen noch von `./build/komport` — das war zum
> jeweiligen Zeitpunkt der tatsächliche Binary-Name und wird als historisches
> Protokoll nicht nachträglich umgeschrieben. Die Quelldateien in `komport/`
> behalten ihre Dateinamen und den Datei-Header-Titel "Komport Serial Port
> Communicator" als Verweis auf ihren Ursprung.

> **Status:** Die Portierung ist durchgeführt und baut sauber mit CMake/Qt6
> (`cmake -B build && cmake --build build`, auch mit `-Wall -Wextra` ohne Warnungen).
> Dazu gekommen sind vier Admin-Tool-Features: ein zuschaltbarer Hex-Monitor
> (RX/TX, Split-Screen), eine deutlich vollständigere VT100/VT102-Emulation
> (Cursor-Zähler, Insert/Delete Line/Char, DECCKM/DECTCEM, Device-Status-Reports,
> erweiterte SGR-Farben — plus zwei per Code-Review gefundene und gefixte
> Absturz-Bugs bei Cursor-Clamping), programmierbare Makro-Buttons,
> Ein-Klick-Session-Logging und eine Zeilenende-Auswahl (CR/LF/CRLF) für
> Enter-Taste und Makros. Details, was dabei gemacht/entschieden wurde, stehen
> in `TODO.md`. Dieses Dokument bleibt als Ziel-/Architektur-Referenz für
> künftige Änderungen bestehen.

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

`komportserial.h/.cpp` macht aktuell:
- `::open()`/`::close()`/`read()`/`write()` auf `/dev/ttyXX` direkt (POSIX).
- `termios`-Struct manuell für Baudrate/Framing konfigurieren (`tcgetattr`/`tcsetattr`).
- `QSocketNotifier` auf dem File-Descriptor für eingehende Daten, dazu ein eigener
  Ring-Puffer (`KomportQueue`) und ein Timer, der den Puffer per `receivedChar(char)`-Signal
  leert.
- Baudraten als eigenes `enum Baud` mit manueller String-Tabelle.

Soll ersetzt werden durch:
- `QSerialPort` als Member statt rohem `fd`.
- `QSerialPortInfo::availablePorts()` zur Geräteauswahl (ersetzt fest codierte
  Device-Comboboxen/-Listen im Settings-Dialog).
- `QSerialPort::setBaudRate()`, `setDataBits()`, `setStopBits()`, `setParity()`,
  `setFlowControl()` statt manuellem `termios`.
- `QSerialPort::readyRead()`-Signal statt `QSocketNotifier` + eigenem Ring-Puffer;
  `KomportQueue` kann entfallen oder bleibt nur als optionaler Anwendungs-Puffer, falls
  Flush-Rate-Verhalten (`setFlushRate`) bewusst beibehalten werden soll.
- Fehlerbehandlung über `QSerialPort::errorOccurred()` statt `errno`/`perror`.
- Die öffentliche Signal-/Slot-Schnittstelle nach außen (`receivedChar`, `settingsChanged`,
  `settingsFailed`, `putChar`/`putStr`) so weit wie möglich beibehalten, damit
  `KomportView`/`KomportEmulation` nicht mehr als nötig angefasst werden müssen — intern
  aber komplett auf `QSerialPort` umstellen.

## Architektur-Überblick

- `KomportApp` (`komport.h/.cpp`) — Hauptfenster, Menüs/Toolbar/Statusbar, Dateiverwaltung,
  verdrahtet auch die neueren Panels/Leisten unten (Hex-Monitor, Makro-Bar, Recording, s.u.).
  Zentralwidget ist ein `QSplitter` mit `KomportView` und `KomportHexView`.
- `KomportDoc` (`komportdoc.h/.cpp`) — hält `KomportSerial`-Instanz, Document-View-Pattern
  (aus KDevelop-Boilerplate; für ein Terminal eigentlich zu schwergewichtig, aber wird
  strukturell übernommen statt neu designt).
- `KomportView` (`komportview.h/.cpp`) — Zeichen-Grid-Widget, Zeichnen, Maus-/Tastatur-Events,
  Auswahl/Zwischenablage, hält `KomportCellArray`, `KomportScrollBuffer`, `KomportEmulation`.
  **Achtung:** sitzt im Zentral-`QSplitter`, nicht direkt unter `KomportApp` — `getDocument()`
  läuft deshalb über `window()`, nicht `parentWidget()` (siehe `TODO.md` 8.6).
- `KomportEmulation` (`komportemulation.h/.cpp`) — VT100/VT102-Escape-Sequenz-Interpreter.
  **Größtes und wichtigstes Modul.** Inzwischen recht vollständig (Cursor-Bewegung mit
  Zähler, Insert/Delete Line/Char, DECCKM/DECTCEM, Device-Status-Reports, erweiterte
  SGR-Farben, Tab, non-CSI-Escapes) — Details und bekannte Lücken (Scroll-Regionen,
  VT52, Zeichensatz-Umschaltung) in `TODO.md` Abschnitt 8.2. Trägt auch die
  `LineEnding`-Einstellung (CR/LF/CRLF) für die Enter-Taste und die Makro-Bar.
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
  (RX/TX getrennt, 16 Byte/Zeile, roh vor jeder Emulations-Interpretation).
- `KomportMacroBar` (`komportmacrobar.h/.cpp`, **neu**) — 8 programmierbare Quick-Command-Buttons,
  unten angedockt, editierbar per Klick/Rechtsklick, persistiert.
- `KomportSessionLogger` (`komportsessionlogger.h/.cpp`, **neu**) — Ein-Klick-Mitschnitt des
  empfangenen Bytestroms in eine zeitgestempelte Textdatei.

## Build-System

**Erledigt:** das alte Autotools-Setup (`configure.in`, `acinclude.m4`, `admin/*`,
KDevelop-1.2-generiert, KDE-2/3-typisch) wurde komplett durch ein Top-Level
`CMakeLists.txt` ersetzt (`find_package(Qt6 COMPONENTS Widgets PrintSupport
SerialPort REQUIRED)`, `CMAKE_AUTOMOC`/`CMAKE_AUTORCC`, C++17). Alle Autotools-
Dateien und die KDevelop-1.x-Projektdateien sind entfernt (siehe `TODO.md` Abschnitt 3
für die vollständige Liste). Bauen: `cmake -B build && cmake --build build`.

## Was NICHT im Scope ist (sofern nicht anders vom Nutzer gewünscht)

- Keine neue Terminal-Emulation (kein xterm/VT220/256-Farben-Ausbau) — nur VT100/VT102
  wie bisher, nur die Infrastruktur drumherum wird modernisiert.
  **Explizit vom Nutzer gewünschte Ausnahme:** die hellen ANSI-Farbcodes `90–97`/`100–107`
  (`komportemulation.cpp`, `doGraphics()`) sind aixterm/xterm-Herkunft, nicht Teil der
  originalen VT102-Doku im Kopfkommentar dieser Datei — sie wurden bewusst mit
  aufgenommen, weil der Nutzer explizit "korrektes Handling von Farb-Codes (ANSI)"
  als vollständig gefordert hat und reale Zielgeräte (Cisco/Juniper-CLIs, eingefärbte
  `ls`-Ausgaben o.ä.) sie routinemäßig senden. Bewusste, dokumentierte Ausnahme von der
  VT100/VT102-Beschränkung, keine sonstige xterm-Erweiterung (keine 256-Farben, kein
  True-Color, kein sonstiger xterm-Funktionsumfang).
- Keine funktionale Erweiterung des Datei-Transfers (Upload/Download/Script) über das
  bisherige rudimentäre Grundgerüst hinaus.
- Keine Internationalisierung/`.po`-Pflege über das Nötigste hinaus (die alte
  `I18N_NOOP`/KDE-i18n-Kette entfällt, `po/` kann ggf. auf Qt-`.ts`/`lupdate` umgestellt
  werden, ist aber nicht Kernziel).

## Sonstiges

- Lizenz: GPL (siehe `COPYING`), Original-Autor Mike Sharkey — bei Umbenennungen/neuen
  Dateien Copyright-Header-Konvention der bestehenden Dateien beibehalten bzw. sinnvoll
  ergänzen (nicht den ursprünglichen Autor entfernen).
- `komport.desktop`, Icons (`lo16-app-komport.png`, `lo32-app-komport.png`) bleiben
  nutzbar, ggf. Pfade/Kategorien im `.desktop`-File an Nicht-KDE-Umgebungen anpassen.
