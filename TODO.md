# Qt6-Portierung — Arbeitsstand

Verfolgt den Fortschritt der Portierung von KDE3/Qt3 auf reines Qt6 (siehe `CLAUDE.md`
für die Ziele/Klassen-Mappings). Wird laufend aktualisiert.

## 0. Umgebung

- [x] Analyse der Ordnerstruktur (`komport/` — 27 Quelldateien, ~5300 Zeilen,
      Autotools-Build aus KDevelop-1.2-Ära, KDE 2/3 + Qt 3 APIs)
- [x] Git-Repository initialisiert, Ausgangszustand als Baseline committet
      (Commit `Import upstream KDE3/Qt3 Komport 0.4.6`)
- [ ] **Blocker: Build-Toolchain fehlt auf diesem System.** `cmake` und die Qt6-Dev-Pakete
      (Header/CMake-Configs) sind nicht installiert, nur Qt6-Runtime-Libs. Nutzer wurde
      gebeten auszuführen:
      `sudo apt install -y cmake qt6-base-dev qt6-serialport-dev qt6-tools-dev qt6-l10n-tools`
      Ohne das kann nicht gebaut/iteriert werden — Code-Portierung läuft parallel weiter.

## 1. Quellcode-Analyse (abgeschlossen)

Alle 27 Dateien in `komport/` gelesen und Abhängigkeiten katalogisiert (siehe `CLAUDE.md`
für die vollständigen Mapping-Tabellen KDE→Qt6 und Qt3→Qt6).

## 2. Portierung der Klassen

- [x] `komportcell.h/.cpp` — Zellattribute; `QApplication::palette().active().foreground()`
      → `QPalette::Text`/`QPalette::Base` (Qt3 `QPalette::active()` entfällt in Qt6)
- [x] `komportcellarray.h/.cpp` — `QPtrList<KomportCell>` → `QList<KomportCell*>` +
      manuelles Speichermanagement (kein `setAutoDelete` mehr in Qt6)
- [x] `komportscrollbuffer.h/.cpp` — reiner Syntax-Port, keine KDE-Abhängigkeit
- [x] `komportfilescrollbuffer.h/.cpp` — `QFile::setName()` → `setFileName()`;
      Klasse bleibt funktional unvollständig wie im Original (`cell()` liefert immer
      `NULL` — vorbestehende Einschränkung, nicht Teil dieser Migration)
- [x] `komportqueue.h/.cpp` — **entfernt.** War Hilfsklasse nur für den alten
      `QSocketNotifier`-basierten RX-Puffer in `komportserial.cpp`; durch
      `QSerialPort::readyRead()` + internen `QByteArray`-Puffer ersetzt, dadurch
      überflüssig.
- [x] `komportserial.h/.cpp` — **Kernstück:** komplett auf `QSerialPort` umgestellt
      (statt rohem `open()/read()/write()`/`termios`/`QSocketNotifier`).
      `QSerialPortInfo` wird im Settings-Dialog zur Geräteerkennung genutzt.
      Baudrate als `qint32` statt eigenem `enum Baud`. Framing (Databits/Stopbits/
      Parity) wird jetzt tatsächlich über `QSerialPort::setDataBits/setStopBits/
      setParity` angewendet — im KDE3-Original wurden diese Werte zwar im Dialog
      abgefragt, aber nie an die Hardware durchgereicht (nur Baudrate wirkte via
      `termios`); das ist mit `QSerialPort` jetzt vollständig, keine Funktions­erweiterung
      über den ursprünglich vorgesehenen Einstellungsdialog hinaus.
      Ebenso neu angewendet: Flow-Control (`XON/XOFF`/`RTS/CTS`/`NONE`) — im Original
      im Dialog vorhanden, aber nirgends an `KomportSerial` durchgereicht.
- [x] `komportemulation.h/.cpp` — VT100/VT102-Interpreter, **Verhalten unverändert**,
      nur Syntax-Port: `QCString` → `QByteArray` (`find`→`indexOf`), `Key_*` → `Qt::Key_*`,
      `QKeyEvent::ascii()` (entfernt in Qt6) → `QKeyEvent::text()`.
- [x] `komportview.h/.cpp` — Zeichen-Grid-Widget: `bitBlt()` (Qt3-Global, entfernt) →
      `QPainter::drawPixmap()`; `QPixmap::resize()` (entfernt) → neues Pixmap +
      übertragen; `QFontMetrics::width()` (entfernt) → `horizontalAdvance()`;
      `setBackgroundMode()` (entfernt) → `Qt::WA_OpaquePaintEvent`; alte
      Konstruktor-Signatur `(parent,name)` → `(parent, Qt::WindowFlags)`.
      Kleine Vereinfachung: `QPrinter::numCopies()`-Kopierschleife in `print()`
      entfernt (in Qt6 entfernt; Kopienanzahl steuert der Druckdialog/das
      Betriebssystem über `QPrinter::setCopyCount()`, nicht mehr die App).
- [x] `komportdoc.h/.cpp` — `KURL` → `QUrl`; `KIO::NetAccess` entfernt (Original hat
      Dateiinhalte ohnehin nie tatsächlich geladen/gespeichert — reine
      KDevelop-Boilerplate-Stubs); `QList`-Iterationsprotokoll (Qt3 `first()/next()`) →
      Range-based-for über `QList<KomportView*>`; `KMessageBox` → `QMessageBox`.
- [x] `komporttransfer.h/.cpp`, `komportupload.h/.cpp`, `komportdownload.h/.cpp` —
      `KURL`/`KFileDialog`/`KIO::NetAccess` entfernt zugunsten von direktem lokalem
      Dateipfad (`QString`) — Dateiauswahl läuft jetzt über `QFileDialog`, das lokale
      Pfade liefert, der KIO-Remote-Download-Umweg war für den bisherigen
      Anwendungsfall (Datei lokal hoch-/runterladen) ohnehin unnötig.
      `KProgressDialog` → `QProgressDialog`. `QFile::getch/putch` (entfernt) →
      `QFile::read/write` mit 1 Byte. `QApplication::eventLoop()` (entfernt) →
      `QCoreApplication::processEvents()`.
- [x] `komportscript.h/.cpp` — reiner Syntax-Port, keine Änderungen nötig
- [x] `settingsdialog.h/.cpp` — **komplett neu geschrieben** statt Qt3-Designer-`.ui`
      zu konvertieren (`settingsdialog.ui` ist Qt3-`.ui`-Format, von `uic` in Qt6 nicht
      lesbar). Gleiche Felder/Tabs wie zuvor (Device/Baudrate/RX-Queue/Framing/
      Flow-Control, History-Buffer, Emulation/Bell/Echo), jetzt mit Qt6-Layouts statt
      Pixel-Koordinaten. Geräte-Combobox wird jetzt über `QSerialPortInfo::availablePorts()`
      befüllt statt hart codierter `/dev/ttyS0..3`-Liste. `KURLRequester` entfällt
      (History-Buffer-Datei-Funktion ist im Original ohnehin ein nicht funktionierender
      Stub, siehe `komportfilescrollbuffer.cpp`).
- [x] `komport.h/.cpp` — Hauptfenster: `KMainWindow`→`QMainWindow`,
      `KAction`/`KStdAction`→`QAction`, `KConfig`→`QSettings`, `KMessageBox`→
      `QMessageBox`, `KFileDialog`→`QFileDialog`, `createGUI()`/`komportui.rc`
      (KDE-XML-UI-Framework) entfällt zugunsten von Menü/Toolbar, die direkt in
      C++ aufgebaut werden. `memberList` (KMainWindow-Session-Feature) →
      `QApplication::topLevelWidgets()`.
      **Bewusst weggelassen:** die KDE-Session-Crash-Recovery
      (`kapp->tempSaveName`/`checkRecoverFile`, `saveProperties`/`readProperties`
      mit Sitzungs-`KConfig`) — das hing komplett an `KMainWindow`s
      Session-Management, hat kein direktes Qt6-Äquivalent, und im Original wurde
      dabei ohnehin nur eine leere Dokument-Boilerplate "gerettet" (kein echter
      Dateiinhalt). Verbindungseinstellungen und Fenstergeometrie werden weiterhin
      persistiert (jetzt über `QSettings`).
- [x] `main.cpp` — `KApplication`/`KCmdLineArgs`/`KAboutData` → `QApplication` +
      `QCommandLineParser`; Org-/App-Name für `QSettings` gesetzt.
- [x] `komportui.rc`, `settingsdialog.ui` — entfernt (KDE-XML-UI bzw. Qt3-Designer-Format,
      beide durch reinen C++-Aufbau ersetzt).

## 3. Build-System

- [x] `CMakeLists.txt` (Top-Level) erstellt: `find_package(Qt6 COMPONENTS Widgets
      PrintSupport SerialPort)`, `CMAKE_AUTOMOC`/`CMAKE_AUTORCC`, C++17.
      App-Icon über ein neues `komport/komport.qrc` eingebettet (`lo16`/`lo32`-app-komport.png).
- [x] Alte Autotools-/KDevelop-1.x-Dateien entfernt, nachdem der CMake-Build
      erfolgreich verifiziert wurde (s. Abschnitt 4): `admin/`, `po/` (enthielt nur
      leeres Autotools-Boilerplate, keine echten Übersetzungen), `Makefile.am/.in/.dist`
      (Top-Level und `komport/`), `configure`, `configure.in`, `configure.in.in`,
      `config.h.in`, `aclocal.m4`, `acinclude.m4`, `stamp-h.in`, `subdirs`,
      `configure.files`, `komport.kdevprj`, `komport.kdevses`, `messages.log`.
      `komportui.rc` (KDE-XML-UI) und `settingsdialog.ui`/`settings.ui`
      (Qt3-Designer-Format, letzteres ein bereits zuvor unbenutzter Altdatei-Rest)
      wurden schon in Schritt 2 entfernt.

## 4. Build verifiziert

- [x] `cmake -B build && cmake --build build -j$(nproc)` — **läuft fehlerfrei durch,
      auch mit `-Wall -Wextra` (0 Warnungen, 0 Fehler).**
- [x] Compiler-Fehler iterativ behoben (siehe unten, alle beim ersten Build-Versuch
      gefunden und in einem Durchgang gefixt):
      - `QSize` fehlte in `komportcellarray.h` (nur transitiv über `qmetatype.h`
        vorwärtsdeklariert) → `#include <QSize>` ergänzt.
      - `KomportEmulation::slotSimKeyPressed(QChar)` reichte `QChar` direkt an
        `KomportSerial::putChar(char)` durch → `.toLatin1()` ergänzt.
      - `komport.cpp` griff auf `QComboBox`/`QSpinBox`-Methoden zu, während
        `settingsdialog.h` diese nur vorwärtsdeklariert (für schnellere Kompilierung) —
        `#include <QComboBox>`/`<QSpinBox>` in `komport.cpp` ergänzt.
- [x] Smoke-Test (`QT_QPA_PLATFORM=offscreen ./build/komport`): Programm startet,
      Hauptfenster wird erzeugt, `QSerialPort` versucht `/dev/ttyS0` zu öffnen und
      meldet sauber abgefangen "Eingabe-/Ausgabefehler" (Gerät existiert in dieser
      Sandbox nicht) statt abzustürzen, Event-Loop läuft weiter bis zum Timeout.
      Auf echter Hardware mit vorhandenem Gerät entsprechend zu wiederholen.

## 5. Beim Portieren gefundene Alt-Bugs (mitgefixt, keine neue Funktionalität)

- `KomportTransfer::download()` verband im Original `KomportSerial::receivedChar(char)`
  mit einem Slot `slotReceivedChar(unsigned char)` — unterschiedliche Parametertypen,
  Qt's Signal/Slot-Typprüfung hätte das schon in Qt3 zur Laufzeit klanglos verworfen
  (kein Fehler, aber der Slot wäre nie aufgerufen worden). Slot-Signatur auf `char`
  angeglichen, damit Download tatsächlich funktioniert.
- `KomportUpload`/`KomportDownload` waren nie Teil des Autotools-Builds
  (fehlten in `komport/Makefile.am`s `SOURCES`) und hätten mangels
  Default-Konstruktor der Basisklasse `KomportTransfer` auch gar nicht kompiliert.
  Für die Vollständigkeit portiert und kompilierbar gemacht, aber weiterhin
  ungenutzt — `komport.cpp` spricht wie im Original direkt `KomportTransfer` an.

## 6. Bekannte, bewusst nicht behobene Altlasten (vom Original übernommen)

- `KomportDoc::openDocument/saveDocument` waren im Original bereits reine
  TODO-Stubs ohne echte Dateiverarbeitung — bleiben es auch nach der Portierung.
- `KomportFileScrollBuffer::cell()` liefert immer `NULL` (Datei-Scrollback war im
  Original nie fertig implementiert) — unverändert übernommen.
- Emulation ist bewusst nur VT100/VT102-Teilmenge wie im Original, keine Erweiterung.

## 7. Aus der alten `TODO`-Datei übernommene Wunschliste (Original-Autor, 2003)

Stand vor der Migration, nicht Teil dieser Portierung, aber als Ausblick festgehalten
(die alte `TODO`-Datei wurde entfernt, ihr Inhalt lebt hier weiter):

- Settings-Formular vollständig fertigstellen (war schon damals als unvollständig
  markiert — mit dieser Portierung wird die Formular-*Anwendung* auf die Hardware
  über `QSerialPort` jetzt immerhin vollständig, s. Abschnitt 2 zu `komportserial.cpp`).
- Pro-Verbindung eigene Settings-Datei (aktuell: eine gemeinsame `QSettings`-Instanz
  je Prozess/Fenster).
- VT100-Emulation weiter finalisieren (über den in Abschnitt 2 beschriebenen
  reinen Syntax-Port hinaus — nicht Teil dieser Migration).
