# Qt6-Portierung — Arbeitsstand

Verfolgt den Fortschritt der Portierung von KDE3/Qt3 auf reines Qt6 (siehe `CLAUDE.md`
für die Ziele/Klassen-Mappings). Wird laufend aktualisiert.

## 0. Umgebung

- [x] Analyse der Ordnerstruktur (`komport/` — 27 Quelldateien, ~5300 Zeilen,
      Autotools-Build aus KDevelop-1.2-Ära, KDE 2/3 + Qt 3 APIs)
- [x] Git-Repository initialisiert, Ausgangszustand als Baseline committet
      (Commit `Import upstream KDE3/Qt3 Komport 0.4.6`)
- [x] Build-Toolchain (`cmake`, `qt6-base-dev`, `qt6-serialport-dev`, ...) vom Nutzer
      nachinstalliert, seither kein Blocker mehr.
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
  reinen Syntax-Port hinaus — mittlerweile zu großen Teilen erledigt, siehe Abschnitt 8).

## 8. Admin-Tool-Features (zweiter Auftrag, nach der Basis-Portierung)

Drei Feature-Wünsche plus eine nachgereichte Ergänzung um drei weitere. Alle
umgesetzt, neu gebaut (0 Warnungen mit `-Wall -Wextra`) und per Selbsttest
verifiziert (Details unten).

### 8.1 Hex-Monitor (`komporthexview.h/.cpp`, neu)

- Zuschaltbares Split-Screen-Panel: `QSplitter` im Zentralwidget von `KomportApp`
  hält `KomportView` und die neue `KomportHexView` nebeneinander; Hex-Panel ist
  standardmäßig versteckt (`setVisible(false)`), Sichtbarkeit über eine neue
  Toggle-Action "Hex Monitor" (Toolbar-Button + Eintrag im View-Menü) und
  in `QSettings` persistiert.
- Klassisches Format `[Offset] [Hex] [ASCII]`, 16 Bytes pro Zeile, getrennt nach
  Richtung: `RX`-Zeilen (empfangen, eigener Offset-Zähler) und `TX`-Zeilen
  (tatsächlich gesendet, eigener Offset-Zähler) — zeigt explizit auch die
  Steuerzeichen, die im reinen Textfenster unsichtbar sind oder das Layout
  zerschießen.
- Angebunden direkt an `KomportSerial::receivedChar(char)`/das dafür neu
  ergänzte `KomportSerial::sentChar(char)` (siehe unten) — **vor** jeder
  Interpretation durch die VT100-Emulation, zeigt also wirklich die rohen Bytes.
- Unvollständige Zeilen (<16 Byte) werden von einem 300ms-Timer nachgezogen,
  damit auch kleine/vereinzelte Bytes zeitnah sichtbar werden, statt auf eine
  volle Zeile zu warten.
- `KomportSerial` bekam dafür ein neues Signal `sentChar(char)`, emittiert aus
  `putChar()`; `putStr()` wurde von einem einzelnen `write()`-Aufruf zurück auf
  eine Schleife über `putChar()` umgestellt (kostet minimal Performance, dafür
  bekommt jedes gesendete Byte sein Signal — für einen Terminal-Client ohne
  Bulk-Transfers irrelevant).

### 8.2 VT100/VT102-Emulation vervollständigt (`komportemulation.h/.cpp`)

- **Kritischer Bugfix (Absturz):** `doCursorDown()`/`doCursorRight()` hatten im
  Original ein Off-by-one (`pos.y() < arrayHeight()` statt `< arrayHeight()-1`)
  — der Cursor konnte eine Zeile/Spalte außerhalb des gültigen Bereichs landen;
  das nächste gezeichnete Zeichen rief `cell(x,y)` auf einem out-of-range Index
  auf, das `nullptr` liefert, und `drawChar()` dereferenzierte das ungeprüft →
  Absturz. Gefixt, plus Regressionstest (s.u.).
- Cursor-Bewegung (`CSI A/B/C/D`) berücksichtigt jetzt den numerischen
  Wiederholungs-Parameter (`ESC[5C` = 5 Spalten nach rechts), vorher wurde er
  ignoriert und immer nur 1 Schritt gemacht.
- Non-CSI-Zweizeichen-Escapes (`ESC D` Index, `ESC M` Reverse Index, `ESC E`
  Next Line, `ESC 7`/`ESC 8` Save/Restore Cursor, `ESC c` Reset,
  `ESC (`/`ESC )` Zeichensatz-Auswahl) waren im Original **komplett
  unbehandelt** — die Zustandsmaschine fiel ohne `mSawESC` zurückzusetzen in
  den Normalzeichen-Zweig durch, druckte das zweite Escape-Zeichen als
  Klartext-Glyphe und blieb danach in einem inkonsistenten Zustand hängen.
  Neu strukturiert (`shortEscape()`), inklusive Verschlucken des auf
  `ESC (`/`ESC )` folgenden Zeichensatz-Designators statt ihn zu drucken.
- Neue CSI-Sequenzen: `L`/`M`/`P` (Insert/Delete Line, Delete Char), `h`/`l`
  (Set/Reset Mode, inkl. privater Modi `?1`=DECCKM und `?25`=DECTCEM), `n`
  (Device Status Report inkl. Cursor-Position-Report), `c` (Device Attributes,
  antwortet als VT102, wie in der Kopfkommentar-Doku dieser Datei vorgesehen).
- SGR (`CSI m`) ergänzt um die in der eigenen Kopfkommentar-Dokumentation
  bereits aufgeführten, aber nie implementierten Codes `21/22/24/25/27`
  (Attribute gezielt zurücksetzen) sowie `39/49` (Vorder-/Hintergrund auf
  Standard) und die hellen ANSI-Farben `90–97`/`100–107` (nicht in der
  originalen VT102-Doku, aber von realer Hardware/Farb-CLIs routinemäßig
  genutzt).
- Tab (`0x09`) wurde vorher als **druckbares Zeichen gezeichnet** (kein
  Tab-Handling vorhanden) — jetzt springt der Cursor korrekt zum nächsten
  Vielfachen von 8.
- DECCKM (`ESC[?1h`/`l`): Pfeiltasten senden abhängig vom Modus `ESC[A` (normal)
  oder `ESC OA` (Application-Modus) — wichtig für Vollbild-Programme wie `vi`
  oder `htop` auf der Gegenseite.
- DECTCEM (`ESC[?25h`/`l`): Cursor kann jetzt unsichtbar geschaltet werden;
  `KomportCellArray` hat dafür ein neues `cursorVisible`-Flag samt Signal,
  `KomportView::paintCell()` respektiert es.
- Bekannte, bewusst nicht geschlossene Lücken (ehrlich dokumentiert statt
  stillschweigend unvollständig zu lassen): Scroll-Regionen (`DECSTBM`,
  `CSI r`) werden nur konsumiert, nicht angewendet — bräuchte eine
  scroll-region-fähige `scrollUp()`/neue `scrollDown()` in `KomportCellArray`.
  `ESC M` (Reverse Index) scrollt am oberen Rand deshalb nicht rückwärts,
  sondern bleibt stehen (wie es `doCursorUp()` schon immer tat). VT52-Modus,
  echte Zeichensatz-Umschaltung (Linien-Grafikzeichen) und Insert-Mode (`CSI 4h`)
  sind nicht implementiert.

### 8.3 Makro-Buttons (`komportmacrobar.h/.cpp`, neu)

- Leiste mit 8 programmierbaren Buttons, angedockt unten im Hauptfenster
  (eigene `QToolBar` in `Qt::BottomToolBarArea`).
- Klick auf einen konfigurierten Button sendet den hinterlegten Befehl plus
  das aktuell gewählte Zeilenende (s. 8.5); Klick auf einen leeren Button oder
  Rechtsklick auf jeden Button öffnet einen Editor (Label + Befehl) für den
  jeweiligen Slot.
- Drei Slots vorbelegt mit den vom Nutzer genannten Beispielen
  (`show running-config`, `wr mem`, `exit`), fünf leer zur freien Belegung.
- Persistiert unter `QSettings`-Gruppe `Macros`.

### 8.4 Ein-Klick-Session-Logging (`komportsessionlogger.h/.cpp`, neu)

- Toggle-Action "Record Session..." (Toolbar + Session-Menü): beim Aktivieren
  `QFileDialog::getSaveFileName`, dann Aufzeichnung; beim Deaktivieren wird die
  Datei sauber geschlossen (letzte unvollständige Zeile wird noch geflusht).
  Schlägt das Öffnen der Datei fehl, springt der Button-Zustand zurück und es
  gibt eine Fehlermeldung.
- Loggt den **empfangenen** Bytestrom (das, was über den Bildschirm läuft),
  zeilenweise mit Zeitstempel `[HH:mm:ss.zzz]` pro Zeile, angebunden an
  `KomportSerial::receivedChar(char)` — unabhängig vom Hex-Monitor (der zeigt
  Rohbytes inkl. TX, der Logger die für Menschen lesbare Session).

### 8.5 Zeilenende-Auswahl (Toolbar-Dropdown + `KomportEmulation`)

- Neues `KomportEmulation::LineEnding`-Enum (`CR`/`LF`/`CRLF`) plus
  `setLineEnding()`/`lineEndingBytes()`; steuert, was die Enter-Taste
  (`slotKeyPressed()`, vorher immer nur ein hartkodiertes `\r`) und die
  Makro-Buttons an Zeilenende senden.
- Schnelles Dropdown in der Toolbar ("Enter sends: CR/LF/CR+LF"), Auswahl wird
  in `QSettings` persistiert. Bewusst *nicht* zusätzlich in den
  Settings-Dialog dupliziert — der Nutzer wollte explizit ein schnelles
  Toolbar-Dropdown, kein Dialogfeld.

### 8.6 Speicher-Bug beim Reparenting gefunden und gefixt

Beim Einbau des `QSplitter`s für den Hex-Monitor reparentiert
`QSplitter::addWidget()` die `KomportView` weg von `KomportApp` (ihr direkter
Parent ist danach der Splitter, nicht mehr das Hauptfenster). `KomportView::
getDocument()` castete bis dahin `parentWidget()` direkt nach `KomportApp*` —
nach dem Reparenting zeigte das auf den `QSplitter`, der C-Style-Cast war damit
falsch, und der erste Zugriff auf `view->getSerial()` (im `KomportApp`-Konstruktor,
für die neuen Hex-/Logger-Verbindungen) stürzte reproduzierbar ab (per `gdb`
verifiziert). Fix: `getDocument()` benutzt jetzt `window()` statt
`parentWidget()` — läuft die komplette Widget-Hierarchie bis zum Top-Level-Fenster
hoch, unabhängig davon, wie viele Container-Widgets dazwischenliegen.

### 8.7 Verifikation

- Build mit `-Wall -Wextra`: weiterhin 0 Warnungen, 0 Fehler.
- Offscreen-GUI-Smoke-Test (`QT_QPA_PLATFORM=offscreen ./build/komport`): startet,
  keine Abstürze (hat den Splitter/`getDocument()`-Bug oben tatsächlich gefangen).
- Zusätzlich ein temporärer, nicht mit ausgelieferter Integrationstest
  (`komport_selftest`, provisorisches CMake-Target, nach Gebrauch wieder aus
  `CMakeLists.txt` entfernt): über ein echtes `pty`-Geräte-Paar
  (`python3 pty.openpty()`) hat `KomportSerial` einen echten seriellen Port
  geöffnet, ein Python-Treiber hat auf der Gegenseite Bytes geschrieben/gelesen.
  30 Prüfungen, alle grün, u.a.:
  - Klartext, Cursor-Positionierung, SGR-Farben inkl. hell, Mehrfach-Parameter
    bei Cursor-Bewegung;
  - **Regressionstest für den Absturz-Bug** 8.2: Cursor gezielt in die untere
    rechte Ecke bewegt, dann versucht darüber hinaus zu bewegen/zu zeichnen —
    kein Absturz, sauber geklemmt;
  - Tab-Sprung, Insert/Delete-Char, DECTCEM, non-CSI Save/Restore/Index/Reset,
    Zeichensatz-Designator wird verschluckt statt gedruckt;
  - **Echter Byte-Roundtrip über den seriellen Port:** `CSI 6n`
    (Cursor-Position-Report) beantwortet, Antwort auf der Host-Seite des
    `pty`-Paars gelesen und verglichen;
  - DECCKM schaltet die von `slotKeyPressed()` für die Pfeiltasten gesendeten
    Bytes tatsächlich zwischen `ESC[A` und `ESC OA` um;
  - Enter-Taste sendet abhängig von `LineEnding` tatsächlich CR, LF oder CRLF;
  - `KomportSessionLogger`: Datei geschrieben, Zeile + Zeitstempel im
    Dateiinhalt wiedergefunden.

## 9. Code-Review (nach Abschnitt 8) — 10 Findings, alle behoben

Ein `/code-review` über den kompletten Diff der Abschnitte 1–8 hat 10 verifizierte
Findings ergeben. Alle behoben, erneut gebaut (0 Warnungen mit `-Wall -Wextra`) und
per erweitertem PTY-Selbsttest verifiziert (32/32 Checks grün, inkl. zwei neuer
Regressionstests für die beiden Absturz-/Logikfixes unten).

1. **`doCursorTo()` (CSI H/f) klemmte nicht an den Grid-Rand** — anders als die
   bereits gefixten relativen Cursor-Bewegungen (`komportemulation.cpp`). Ein
   `ESC[9999;9999H` gefolgt von `ESC[K`/`ESC[P`/`ESC[L`/`ESC[M` oder einem
   normalen Zeichen hätte `cell(x,y)` außerhalb des gültigen Bereichs aufgerufen
   → `nullptr`-Dereferenzierung → Absturz. Jetzt mit `qBound()` auf
   `[0, arrayHeight()-1]`/`[0, arrayWidth()-1]` geklemmt, wie die anderen
   Cursor-Befehle auch. Nebeneffekt (Verbesserung, kein Funktionsverlust): die
   Einzelparameter-Form `ESC[5H` (nur Zeile, keine Spalte) wurde im Original
   fälschlich wie "kein Parameter" behandelt und sprang immer auf (0,0) —
   funktioniert jetzt korrekt (Spalte defaultet auf 1).
2. **Echtes ESC nach `ESC(`/`ESC)` wurde als Zeichensatz-Designator verschluckt**
   — der `mPendingCharsetChar`-Check lief vor der ESC-Prüfung. Jetzt prüft
   `slotReceivedChar()` zuerst auf ein neues ESC (das immer Vorrang hat und
   jeden anderen Zustand abbricht, auch einen offenen `mPendingCharsetChar`),
   erst danach auf den Designator.
3. **`ctlParam()` ohne Obergrenze → möglicher Integer-Overflow** — ein
   `ESC[2147483647C` hätte `pos.x()+n` überlaufen lassen (UB), bevor geklemmt
   wird. Neue Konstante `MaxCtlParam = 10000` (weit über jeder plausiblen
   Bildschirmgröße) deckelt jeden geparsten CSI-Zähler-Parameter, bevor er in
   irgendeine Arithmetik einfließt — behebt das für alle Aufrufer von
   `ctlParam()` auf einmal (Cursor-Bewegung, Insert/Delete Line/Char, Device
   Status Report).
4. **Neue Dateien ohne Copyright-Header** — `komporthexview.*`,
   `komportmacrobar.*`, `komportsessionlogger.*` haben jetzt denselben
   Header-Aufbau (`begin`/`copyright`/`email`) wie alle bestehenden Dateien,
   zugeschrieben an `Harald Stürmer <ironcold@ironcold.de>` (bereits in
   `AUTHORS` als zweiter Autor neben Mike Sharkey gelistet) für die 2026
   neu hinzugekommenen Dateien der Qt6-Portierung.
5. **Helle ANSI-Farben (90–97/100–107) — Scope-Frage** — technisch xterm/aixterm,
   nicht Teil der VT100/VT102-Doku, die dieser Datei eigentlich zugrunde liegt,
   und `CLAUDE.md` schließt xterm-Erweiterungen explizit aus. Da der Nutzer
   selbst "korrektes Handling von Farb-Codes (ANSI)" als vollständig gefordert
   hatte und die Farben bereits funktionieren, wurde **nicht der Code entfernt**,
   sondern `CLAUDE.md` um eine explizite, begründete Ausnahme für genau diese
   beiden Codebereiche ergänzt (keine sonstige xterm-Erweiterung wie 256-Farben
   o.ä.).
6. **Hex-Monitor arbeitete auch während er versteckt war** — `KomportHexView::
   appendByte()` bricht jetzt sofort ab, wenn das Panel nicht sichtbar ist
   (Default: versteckt). Kein Verlust: der Hex-Monitor ist ein Live-Monitor,
   kein persistentes Log (dafür gibt's den Session-Logger) — es gibt keinen
   Rückstand nachzuholen, wenn das Panel später geöffnet wird.
7. **`putStr()`-Schleife über `putChar()` war ineffizient** — zurückgebaut auf
   einen einzelnen `QSerialPort::write()`-Aufruf; `sentChar()` (für den
   Hex-Monitor) wird weiterhin pro tatsächlich geschriebenem Byte emittiert,
   nur getrennt vom eigentlichen I/O.
8. **`viewHexMonitor` feuerte beim Start doppelt** — hing an `QAction::toggled`
   statt `triggered` wie `viewToolBar`/`viewStatusBar`; `readOptions()`s
   `setChecked()` + expliziter Slot-Aufruf hätte den Slot zweimal ausgelöst.
   Jetzt konsistent auf `triggered` umgestellt.
9. **`doInsertLine`/`doDeleteLine` duplizierten `KomportCellArray::copyRow()`**
   — `copyRow()` war `protected`, jetzt `public` (mit Doku-Kommentar) und wird
   von beiden Funktionen direkt genutzt statt die Pro-Zelle-Kopierschleife
   erneut zu schreiben.
10. **CSI-Parameter-Parsing existierte vierfach** (`ctlParam`, `doGraphics`,
    `doSetMode`, `doCursorTo`) — auf zwei geteilte Datei-lokale Helfer
    konsolidiert: `splitCsiParams()` (`;`-getrennte Felder) und
    `stripPrivatePrefix()` (führendes `?` für private Modi). Alle vier
    Stellen nutzen jetzt dieselbe Parsing-Logik.

## 10. Projekt umbenannt auf Komport-Qt6

Das Original-Projekt "Komport" ist upstream tot (letzte Aktivität von Mike
Sharkey 2003). Auf Wunsch des Nutzers läuft der Qt6-Fork jetzt eigenständig
als **Komport-Qt6** weiter:

- [x] Verzeichnis umbenannt: `~/Entwicklung/komport-0.4.6` → `~/Entwicklung/komport-qt6`.
- [x] `CMakeLists.txt`: `project(komport-qt6 VERSION 1.0.0 ...)` (statt
      `komport`/`0.4.6`), `add_executable(komport-qt6 ...)`, `install()`-Regeln
      und Icon-`RENAME`-Ziele entsprechend angepasst.
- [x] `komport/komport.desktop`: `Exec=komport-qt6 %f`, `Icon=komport-qt6`,
      `Name=Komport Qt6`.
- [x] `main.cpp`: `QCoreApplication`-Organisation/App-Name auf `Komport-Qt6`
      (wirkt sich auf den `QSettings`-Speicherort aus — bestehende
      `~/.config/Komport/Komport.conf`-Einstellungen aus Testläufen unter dem
      alten Namen werden dadurch nicht automatisch übernommen, das ist so
      gewollt bei einer Neuidentität).
- [x] `komport.doxygen`: `PROJECT_NAME`/`PROJECT_NUMBER`/`OUTPUT_DIRECTORY`
      aktualisiert (Letzteres zeigte noch auf einen Pfad auf Mike Sharkeys
      altem Rechner).
- [x] `komport.lsm` entfernt — das alte "Linux Software Map"-Format wird von
      keiner heutigen Distribution mehr genutzt, reine tote Metadaten.
- [x] `README` (war leer) neu geschrieben: Projektbeschreibung, Feature-Liste,
      Verweis auf `CLAUDE.md`/`TODO.md`/`INSTALL`, Lizenz-/Autoren-Hinweis.
- [x] `INSTALL`: Binary-Name in der Kurzanleitung aktualisiert.
- [x] `CLAUDE.md`: neue Status-Notiz oben, erklärt die Umbenennung und warum
      ältere `TODO.md`-Einträge noch `komport` als Binary-Namen nennen
      (historisches Protokoll, nicht rückwirkend umgeschrieben).
- **Bewusst NICHT umbenannt:** die Quelldateien in `komport/` (Dateinamen wie
  `komport.cpp`, `komportview.h`, ...) und deren Datei-Header-Titel "Komport
  Serial Port Communicator" — das sind Verweise auf den historischen Ursprung
  des Codes, kein Produktname. Die Git-Historie (4 Commits, Baseline +
  3 Arbeits-Commits, alle bereits mit `Co-Authored-By: Claude Sonnet 5`
  versehen, siehe Commit-Messages) bleibt unverändert bestehen, wie vom Nutzer
  gewünscht — ab jetzt wird nur regelmäßiger/kleinteiliger committet.
- [x] `cmake --build build` nach der Umbenennung neu verifiziert: sauber mit
      `-Wall -Wextra` (0 Warnungen), Binary heißt jetzt `build/komport-qt6`,
      Offscreen-Smoke-Test läuft weiterhin fehlerfrei.
