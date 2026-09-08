# Komport-Qt6 — Migrations-Archiv (abgeschlossene Arbeit)

Append-only Protokoll der abgeschlossenen Portierungs-/Feature-/Review-Arbeit,
ausgelagert aus `TODO.md`, damit die dort verbleibende aktive Liste übersichtlich
bleibt. Abschnittsnummern sind unverändert aus `TODO.md` übernommen (auch aus
älteren Commit-Messages/Dokus zitierbar); Abschnitte 6 und 7 fehlen hier bewusst
— die sind (weiterhin offene Punkte) in `TODO.md` geblieben.

Für den aktuellen Stand siehe `TODO.md`, für Architektur/Ziele `CLAUDE.md`.

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
  `putChar()`.

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
  genutzt — bewusste, in `CLAUDE.md` dokumentierte Ausnahme vom
  VT100/VT102-Scope, siehe Review-Finding 9.5).
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
  sind nicht implementiert. (Diese Lücken stehen auch, als offene Punkte, in
  `TODO.md` Abschnitt 6.)

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
      (Später durch die vom Nutzer eingebrachte `README.md` ersetzt/erweitert,
      siehe Commit-Historie.)
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

## 11. Statusleisten-Fix + Geräteprofile (erste Rückmeldung nach echtem Hardware-Test)

Erster Praxistest mit einem HPE 1920 lief erfolgreich. Zwei Punkte daraus:

### 11.1 "Tooltips im Menü werden abgeschnitten" — Ursache gefunden und gefixt

War kein echter `QToolTip`-Bug, sondern zu lange `QAction::setStatusTip()`-Texte:
die werden beim Hovern über einen Menüeintrag unten in der Statusleiste
angezeigt, und `QStatusBar` bricht nicht um, sondern schneidet einfach ab —
besonders sichtbar, weil das Fenster durch das feste 80×25-Zeichenraster der
`KomportView` recht schmal ist. Alle `setStatusTip()`-Texte in
`komport.cpp::initActions()` auf kurze, prägnante Phrasen gekürzt (z.B.
"Prints out the whole screen or selected section" → "Print the screen").

### 11.2 Geräteprofile (`komport.h/.cpp`, neu)

Vollständige Profilverwaltung über eine `QComboBox` in der Haupt-Toolbar:

- Ein Profil bündelt die komplette Session-Konfiguration: serielle Parameter
  (Device, Baudrate, Databits, Stopbits, Parity, Flow-Control, RX-Queue,
  Flush-Rate, Emulation, Scroll-Buffer), das Zeilenende ("Enter sends" —
  CR/LF/CR+LF) und die komplette Makro-Bar-Belegung (Label + Befehl aller
  8 Slots).
- Storage: `QSettings`-Gruppe `Profiles/<Name>/...`, mit `KomportMacroBar`s
  eigener `Macros`-Gruppe einfach mitgenistet (`beginGroup()`-Verschachtelung
  — `KomportMacroBar` selbst weiß nichts von Profilen, schreibt/liest einfach
  die gerade offene Gruppe). Zusätzlich ein globaler Schlüssel `LastProfile`
  außerhalb der `Profiles`-Gruppe (bewusst nicht `Profiles/LastProfile`, um
  eine Namenskollision mit einem eventuellen Profil namens "LastProfile" zu
  vermeiden).
- UI: `profileCombo` (editierbar) + zwei `QAction`s "Save Profile"/"Delete
  Profile" daneben in der Toolbar. Auswahl eines vorhandenen Eintrags aus dem
  Dropdown lädt ihn sofort (`QComboBox::textActivated`, feuert bewusst nur bei
  echter Nutzerauswahl, nicht beim Tippen eines neuen Namens oder bei
  programmatischem `setCurrentText()`/`setCurrentIndex()`). Tippen + Enter im
  Textfeld speichert genau wie der Save-Button.
- Verhalten beim Profilwechsel (`KomportApp::loadProfile()`): serielle
  Verbindung wird sauber getrennt (`serial->close()`), alle Hardware-Parameter
  neu angewendet, Port neu geöffnet, Zeilenende-Dropdown aktualisiert (löst
  `slotLineEndingChanged()` aus), Makro-Leiste per `macroBar->loadSettings()`
  sofort mit den Befehlen des neuen Profils neu befüllt.
  `applyConnectionSettings()` bündelt die eigentliche Seriell-Anwendung, von
  `loadProfile()` und `initProfiles()` gemeinsam genutzt (nicht von
  `slotShowPreferences()` — das behält bewusst sein bisheriges, sanfteres
  Verhalten: nur neu verbinden, wenn sich das Gerät tatsächlich geändert hat).
- Migration: `readOptions()`/`saveOptions()` verwalten nur noch die
  allgemeinen Fenster-Optionen (Geometrie, Toolbar/Statusbar/Hex-Monitor
  sichtbar, zuletzt genutzte Dateien) — die alten flachen
  `Connection`/`Macros`/`LineEnding`-Schlüssel werden nicht mehr geschrieben.
  Existiert beim ersten Start unter dem neuen Feature noch kein Profil, liest
  `initProfiles()` einmalig die alten flachen Schlüssel (falls vorhanden,
  sonst die eingebauten Defaults) und legt daraus automatisch ein
  `"Default"`-Profil an — bestehende Konfigurationen aus der Zeit vor den
  Profilen gehen dadurch nicht verloren.
- `slotShowPreferences()` (Connection-Settings-Dialog) schreibt eine dort
  vorgenommene Änderung jetzt zusätzlich sofort ins aktive Profil zurück
  (`saveProfile(mCurrentProfile)`), damit sie nicht beim nächsten
  Profilwechsel/Neustart wieder verschwindet.
- `README.md`s Hinweis "not yet implemented: per-connection settings
  profiles" ist damit überholt und wurde entfernt/durch eine echte
  Feature-Beschreibung ersetzt.

### 11.3 Verifikation

- Build mit `-Wall -Wextra`: weiterhin 0 Warnungen, 0 Fehler.
- Offscreen-Smoke-Test gegen ein frisches `XDG_CONFIG_HOME`: Erststart legt
  korrekt ein `"Default"`-Profil mit allen erwarteten Schlüsseln
  (Seriell-Parameter, `LineEnding`, 8 Makro-Slots) an, zweiter Start lädt es
  ohne erneute Migration.
- Zusätzlich ein temporärer, nicht ausgelieferter Integrationstest
  (`komport_profiletest`, provisorisches CMake-Target wie zuvor bei den
  Selbsttests, danach wieder entfernt): zwei echte `pty`-Geräte
  (`python3 pty.openpty()`), zwei vorab per `QSettings` gesetzte Profile mit
  unterschiedlichem Device/Baudrate/Framing/Makro. Eine echte `KomportApp`-
  Instanz gebaut, `loadProfile()` zwischen beiden Profilen hin- und
  hergeschaltet. 10 Prüfungen, alle grün: korrektes Gerät nach Start (aus
  `LastProfile` aufgelöst, nicht das automatisch angelegte `"Default"`),
  Verbindung tatsächlich offen, Baudrate angewendet, `LastProfile` nach
  Wechsel aktualisiert, Wechsel zurück zum ersten Profil funktioniert
  ebenso. (Framing-Werte wie 7 Databits/EVEN-Parity/RTS-CTS auf einem
  virtuellen `pty` liefern erwartungsgemäß "ungültiges Argument"-Warnungen
  von `QSerialPort`, da ein Pseudo-Terminal keine echte UART-Framing-Hardware
  hat — sauber über `settingsFailed()`/`qWarning()` abgefangen, kein Absturz,
  bestätigt den bereits vorhandenen Fehlerpfad.)

## 12. Vorbelegte Herstellerprofile (Cisco, HP 1920, Aruba CX)

`KomportApp::seedBuiltinProfiles()` legt einmalig drei fertig konfigurierte
Profile mit bekannten Konsolen-Defaults an, damit die Profil-Combobox nicht
leer startet:

- **"Cisco (9600 8N1)"** — 9600 8N1, kein Flow-Control (IOS-Standard-Konsole).
  Makros: Show Config (`show running-config`), Show Version (`show version`),
  Save (`write memory`), Exit (`exit`).
- **"HP 1920 (9600 8N1)"** — 9600 8N1, kein Flow-Control. HP-1920 und die
  breitere HPE-Comware/H3C-Familie (ältere 5130/5510 u.ä.) teilen sich diesen
  CLI-Dialekt. Makros: Show Config (`display current-configuration`), Show
  Version (`display version`), Save (`save`), Quit (`quit`).
- **"Aruba CX (115200 8N1)"** — für neuere Aruba-gebrandete HPE-Switches
  (CX-Serie, z.B. 6100/6300/6400/8xxx). ArubaOS-CX nutzt bewusst
  Cisco-ähnliche Befehlssyntax, bootet die Konsole bei vielen Modellen aber
  mit höherer Baudrate als der klassische 9600er-Standard. Makros wie beim
  Cisco-Profil.

Alle drei mit `Device=/dev/ttyUSB0` als plausiblem Platzhalter (über
Dropdown/Settings-Dialog auf das tatsächliche Gerät anzupassen), `Parity=NONE`,
`StopBits=1`, Standard-RX-Queue/Flush-Rate/Scroll-Buffer.

**Wichtig — ehrlich eingeordnet:** Baudrate und exakte Befehlssyntax können je
nach genauem Modell/Firmware-Stand abweichen; das sind bewährte Startpunkte,
keine für jedes Gerät exakt zutreffenden Werte. Trivial anpassbar (Makro-Button
rechtsklicken zum Editieren, dann "Save Profile") oder per "Delete Profile"
wieder entfernbar.

- Läuft **einmalig**, getrackt über den Schlüssel `BuiltinProfilesSeeded`
  (top-level, wie `LastProfile`) — überschreibt nie ein gleichnamiges Profil,
  das schon existiert, und legt ein einmal gelöschtes Preset nicht erneut an.
  Aufruf in `KomportApp`s Konstruktor vor `initProfiles()`, damit die neuen
  Profile sofort in der Combobox erscheinen, ohne dass die
  `"Default"`-Migrationslogik (Abschnitt 11.2) dadurch beeinträchtigt wird —
  für Bestandsnutzer mit bereits vorhandenem `"Default"`-Profil kommen die drei
  Presets einfach zusätzlich dazu.
- Nebenbei einen kleinen, echten Bug in `KomportMacroBar::loadSettings()`
  gefunden und gefixt: fehlte die `Macros`-Gruppe eines Profils komplett (wie
  bei den hier neu angelegten, bevor der Fix da war), blieben die zuvor
  geladenen Makro-Werte des *vorherigen* Profils einfach stehen, statt
  zurückgesetzt zu werden — der `contains()`-Check pro Slot wurde entfernt,
  ein fehlender Slot setzt jetzt sauber auf leer zurück.
- Fresh-Install-Sonderfall: da die drei Presets vor `initProfiles()`s
  `profileNames().isEmpty()`-Prüfung angelegt werden, greift die
  `"Default"`-Migration bei einer wirklich frischen Installation nicht mehr —
  es gibt ja schon drei Profile. Startprofil ist dann alphabetisch das erste
  (aktuell "Aruba CX ..."), nicht mehr ein leeres `"Default"`. Rein kosmetisch,
  jederzeit per Dropdown änderbar.

### Verifikation

- Build mit `-Wall -Wextra`: weiterhin 0 Warnungen, 0 Fehler.
- Smoke-Test gegen frisches `XDG_CONFIG_HOME`: alle drei Presets korrekt mit
  allen erwarteten Schlüsseln (inkl. Makros) angelegt, `BuiltinProfilesSeeded=true`
  gesetzt.
- Zweiter Lauf nach simuliertem Löschen von "Cisco (9600 8N1)" aus der
  Konfigurationsdatei: Profil wird beim nächsten Start **nicht** erneut
  angelegt (Löschung wird respektiert), `BuiltinProfilesSeeded` bleibt `true`.

## 13. Baudraten-Korrektur HP 1920/1950 + echter `QSettings`-Bug gefunden

Rückmeldung direkt von echter Hardware: der HP 1920 (und 1950) läuft mit
**38400**, nicht mit den ursprünglich angenommenen 9600 — Profil korrigiert,
umbenannt zu `"HP 1920 & 1950 (38400 8N1)"` (deckt beide Modelle ab, die
sich dasselbe Comware/H3C-CLI teilen).

### 13.1 Seeding von einem Flag auf Pro-Name-Historie umgestellt

Das ursprüngliche einzelne `BuiltinProfilesSeeded`-Flag hätte diese Korrektur
bei jedem Nutzer blockiert, der die App vorher schon einmal gestartet hatte
(Seeding lief dann ja schon "einmalig" und nie wieder). Umgebaut auf eine
Liste `SeededProfileNames`, die verfolgt, welche *Preset-Namen* schon einmal
angeboten wurden: ein neuer/umbenannter Preset-Name erreicht damit auch
Installationen, die schon einmal gesät haben, während ein vom Nutzer bewusst
gelöschtes Preset (weiterhin, per Name geprüft) nicht zurückkommt.

### 13.2 Echter Bug gefunden: `/` im Profilnamen zerlegt die QSettings-Gruppe

Beim Testen der Umbenennung fiel auf: `QSettings::beginGroup()` behandelt `/`
als Pfadtrenner — *auch innerhalb eines einzelnen Aufrufs*. Der ursprünglich
geplante Name `"HP 1920/1950 (38400 8N1)"` hätte `beginGroup("Profiles")` +
`beginGroup("HP 1920/1950 (38400 8N1)")` intern in **zwei** verschachtelte
Gruppen aufgespalten (`Profiles/HP 1920/1950 (38400 8N1)/...` statt
`Profiles/<ein Name>/...`). Mit einem kleinen Test-Programm gegen die echte
Konfigurationsdatei verifiziert: `childGroups()` unter `"Profiles"` zeigte nur
`"HP 1920"` (abgeschnitten), `loadProfile("HP 1920")` hätte die eigentlichen
Werte (eine Ebene tiefer unter `"1950 (38400 8N1)"`) gar nicht gefunden.

Betrifft nicht nur dieses eine Preset, sondern die gesamte Profilfunktion:
jeder Nutzer, der selbst einen Profilnamen mit `/` eintippt, hätte denselben
stillen Defekt ausgelöst. Zwei Fixes:
- Preset umbenannt auf `"HP 1920 & 1950 (38400 8N1)"` (kein `/`).
- `slotSaveProfile()` weist Namen mit `/` jetzt explizit mit einer Meldung
  zurück, statt sie still zu zerlegen (analog zum bereits vorhandenen
  Leerstring-Check).

### 13.3 Verifikation

- Build mit `-Wall -Wextra`: weiterhin 0 Warnungen, 0 Fehler.
- Kleines Standalone-`QSettings`-Testprogramm gegen die echte Konfigurations-
  datei bestätigt: `"HP 1920 & 1950 (38400 8N1)"` erscheint jetzt korrekt als
  **ein** Eintrag in `childGroups()` unter `"Profiles"`, nicht mehr aufgespalten.
- Smoke-Test gegen frisches `XDG_CONFIG_HOME`: alle drei Presets korrekt
  angelegt, `SeededProfileNames` enthält alle drei aktuellen Namen.

## 14. Statusleisten-Fußzeile mit aktuellen Verbindungseinstellungen

Permanentes `QLabel` rechts in der Statusleiste (`statusBar()->
addPermanentWidget()`, bleibt unabhängig von den kurzlebigen
`slotStatusMsg()`-Texten links sichtbar), zeigt kompakt Gerät, Baudrate,
Framing (`8N1`-Notation) und Zeilenende — z.B. `/dev/ttyUSB0  ·  38400 8N1  ·
Enter: CR`. Tooltip beim Hovern zeigt zusätzlich Profilname und Flow-Control
aus.

Aktualisiert von `updateConnectionStatusLabel()`, aufgerufen aus
`applyConnectionSettings()` (deckt `loadProfile()`/`initProfiles()` ab),
`slotShowPreferences()` (nach angenommenen Änderungen) und
`slotLineEndingChanged()` (Toolbar-Dropdown). Bewusst kurz gehalten — gleiche
Begründung wie bei den gekürzten `setStatusTip()`-Texten (Abschnitt 11.1):
das Fenster ist durch das feste Zeichenraster schmal, ein `QLabel` würde zwar
nicht wie die Statusleisten-Message abgeschnitten (eigenes Tooltip wickelt
korrekt um), aber unnötig lang macht die Fußzeile trotzdem unübersichtlich.

## 15. Meilenstein 2+3: Layout-Fix, Kate-Minimap, RX/TX-Diagnosefilter

Aus zwei Prompts im lokalen `helper/`-Ordner (`2-scrollleiste.txt`,
`3-tx-rx-checkbox.txt`, seither gelöscht, Inhalt in `TODO.md` "Meilensteine"
archiviert), ausgelöst durch zwei Screenshots vom ersten echten Test am HPE
1920 — jetzt archiviert unter `docs/screenshots/2026-09-04_scrollbar-layout-bug_*.png`.

### 15.1 Ursache des Layout-Bugs gefunden

Die Screenshots zeigten eine kaputte, breite, frei schwebende Leiste direkt
über dem Hex-Monitor-Panel. Ursache: `KomportView`s Scrollbar wurde (noch aus
der ursprünglichen Qt3-Portierung übernommen) absichtlich als **Sibling**
angelegt — `new QScrollBar(Qt::Vertical, parent)`, wobei `parent` der
ÄUSSERE Parent von `KomportView` war, nicht `KomportView` selbst — und in
`resizeEvent()`/`moveEvent()` manuell anhand der VIEW-eigenen Breite/Höhe
positioniert. Das funktionierte, solange `KomportView` direkt unter
`KomportApp` saß. Seit dem zentralen `QSplitter` für den Hex-Monitor
(Abschnitt 8.1) sitzt `KomportView` aber im Splitter, nicht mehr direkt
unter `KomportApp` — die Scrollbar (weiterhin Kind von `KomportApp` direkt)
wurde dadurch in einem völlig anderen Koordinatensystem positioniert als
gedacht, sichtbar als die frei schwebende Leiste in den Screenshots.

### 15.2 Fix: Scrollbar wird echtes Kind-Widget + Kate-artige Minimap (`komportminimap.h/.cpp`, neu)

Statt die alte `QScrollBar`-Sibling-Konstruktion zu reparieren, wurde sie
durch eine neue Klasse `KomportMinimapScrollBar` ersetzt, die als **echtes
Kind-Widget von `KomportView` selbst** (`parent == this`) angelegt wird —
dadurch ist ihre Position immer relativ zu ihrem direkten Parent korrekt,
unabhängig davon, in wie vielen Containern `KomportView` von außen
eingebettet ist (behebt die Ursache strukturell, nicht nur das Symptom).

- Drop-in-kompatible API zur alten `QScrollBar` (`value()`/`setValue()`/
  `maximum()`/`setMaximum()`/`setRange()`/Signal `valueChanged(int)`) —
  der komplette umgebende Scroll-Code in `komportview.cpp` (`getCell()`,
  `resetScroll()`, `slotScroll()`, Auto-Scroll im `timerEvent()`, Text-
  Selektion) blieb dadurch unverändert.
- Rendert die komplette Historie (Scrollback + Live-Bildschirm) als
  geschrumpfte Text-Dichte-Silhouette: pro Pixelzeile eine gesampelte
  Content-Zeile, pro nicht-leerer Spalte ein Punkt — kein 1:1-Rendering
  echter Glyphen (das Zeichen-Grid ist nicht an ein `QTextDocument`
  gebunden, das man dafür wiederverwenden könnte), aber ein echtes,
  proportionales Silhouetten-Bild statt eines reinen Balkens.
- Farben ausschließlich über `QPalette`-Rollen (`QPalette::Base`/`Text`/
  `Highlight`) — folgt damit automatisch dem aktiven System-Theme (Breeze
  Light/Dark o.ä.), kein Theme-spezifischer Code nötig.
- Markiert den aktuell sichtbaren Bereich als hervorgehobenes Band
  (`QPalette::Highlight`), mit derselben "scrolled"-Rechnung wie
  `KomportView::getCell()`/`resetScroll()`.
- Klick/Drag auf die Minimap scrollt direkt dorthin; Mausrad funktioniert
  ebenfalls (neu — die alte `QScrollBar` bekam das automatisch vom Widget,
  das musste hier extra nachgebaut werden).
- Hover-Tooltip: `QEvent::ToolTip` abgefangen, zeigt die 7 Zeilen Klartext
  um die Cursor-Position aus der Historie (via die zwei neuen `KomportView`-
  Methoden unten).
- Zwei neue `KomportView`-Methoden für den Zugriff auf die *gesamte*
  Historie unabhängig von der aktuellen Scroll-Position (die alte
  `getCell()` mischt Scrollback/Live nur relativ zur aktuellen
  Scrollbar-Position, das reicht für die Minimap nicht):
  `totalHistoryRows()` (Scrollback-Tiefe + Bildschirmzeilen) und
  `cellAtHistoryRow(col, row)` (0 = älteste Scrollback-Zeile), beide mit
  sauberer Bounds-Prüfung (negativ/zu groß → `nullptr`, kein Absturz).
- `KomportView::resizeEvent()`/`paintEvent()`/`setCellSize()` angepasst:
  das Zeichen-Grid-Pixmap ist jetzt nur noch so breit wie das Grid selbst
  (Widget-Breite minus Minimap-Breite), `paintEvent()` clippt explizit auf
  die Pixmap-Grenzen, damit es die Minimap (malt sich als Kind-Widget
  selbst) nicht überschreibt. `moveEvent()` komplett entfernt — überflüssig,
  da ein echtes Kind-Widget sich automatisch mit seinem Parent mitbewegt.
- **Speicher-Bug vermieden:** da die Scrollbar jetzt ein echtes Kind-Widget
  ist (Qt löscht Kinder automatisch beim Zerstören des Parents), musste das
  bisherige manuelle `delete mScrollBar;` im Destruktor entfernt werden —
  sonst Doppel-Free.
- Toolbar-Politur (kleine Zusatzfrage des Nutzers zwischendurch, ob aktuelle
  Widgets verwendet werden): explizite Icon-Größe 24px und
  `Qt::ToolButtonIconOnly` statt Style-Default — reine `QToolBar`-Styling-
  Anpassung, keine Widget-Typen geändert (es waren schon durchgehend aktuelle
  Qt6-Widgets, der "alte" Eindruck kam nur von Default-Icon-Größe/-Spacing).

### 15.3 RX/TX-Diagnosefilter im Hex-Monitor (`komporthexview.h/.cpp`)

- Zwei `QCheckBox`en ("RX"/"TX", Default: beide an) im Header-Bereich des
  Hex-Monitor-Panels, links vom "Clear"-Button.
- `appendByte()` bricht früh ab, wenn die jeweilige Checkbox aus ist —
  zusätzlich zum bereits vorhandenen Sichtbarkeits-Check (Abschnitt 9.6).
- Neues Methodenpaar `saveSettings(QSettings*)`/`loadSettings(QSettings*)`
  (Gruppe `"HexMonitor"`, genau wie `KomportMacroBar`s `"Macros"`-Gruppe
  genestet unter `Profiles/<Name>/...`), von `KomportApp::saveProfile()`/
  `loadProfile()` direkt neben den bestehenden `macroBar->saveSettings()`/
  `loadSettings()`-Aufrufen mit angebunden.
- `loadSettings()` bewusst **ohne** `contains()`-Guard pro Checkbox (Default
  `true`, wenn der Schlüssel fehlt) — dieselbe Begründung wie beim
  `KomportMacroBar`-Fix in Abschnitt 12: ein Profil ohne eigene
  `HexMonitor`-Gruppe muss auf den Standard zurückfallen, nicht die
  Checkbox-Stellung des zuvor geladenen Profils übernehmen.

### 15.4 Verifikation

- Build mit `-Wall -Wextra`: weiterhin 0 Warnungen, 0 Fehler.
- Offscreen-Smoke-Test: startet weiterhin fehlerfrei.
- Zusätzlicher, nicht ausgelieferter Integrationstest (`komport_minimaptest`,
  provisorisches CMake-Target, danach wieder entfernt) über ein echtes
  `pty`-Gerät: 85 Zeilen als *empfangene* Daten geschickt (direkt auf die
  Master-Seite des `pty`-Paars geschrieben, nicht über `putStr()` — das wäre
  Senderichtung und wird lokal nicht dargestellt), 12 Prüfungen, alle grün:
  - `totalHistoryRows()` wächst korrekt über die Bildschirmhöhe hinaus,
    sobald der Scrollback-Puffer greift;
  - `cellAtHistoryRow()` liefert für jede gültige (Spalte,Zeile)-Kombination
    eine echte Zelle, für negative/zu große Zeilenindizes sauber `nullptr`
    statt Absturz;
  - Fenster mehrfach in der Größe geändert (übt `resizeEvent()` und den
    Minimap-Reflow aus) — kein Absturz;
  - Ein frisch geseedetes Profil hat erwartungsgemäß noch keine
    `HexMonitor`-Filterwerte gespeichert (Default greift).
- Visuelle Kontrolle der Minimap-Silhouette/des Hover-Previews selbst konnte
  in dieser Sandbox nicht erfolgen (kein echtes Display) — auf echter
  Hardware/Display gegenzuprüfen empfohlen.

## 16. Terminalhöhe folgt der Fensterhöhe, Mausrad im Textfenster, Tooltip-Race im Toolbar

Rückmeldung nach den ersten Tests auf echter Hardware.

### 16.1 Terminal-Zeilenzahl folgt jetzt der Fensterhöhe (Breite bleibt fest)

Bug: `KomportView` hatte in `setCellSize()` eine feste Mindest-/Maximalgröße
für Breite **und** Höhe — das Grid wuchs/schrumpfte beim Ziehen am
Fensterrand nie mit, die Minimap-Scrollbar blieb auf ihrer ursprünglichen
Höhe stehen. Nicht beabsichtigt, reine Altlast aus der nie überarbeiteten
Erstportierung.

Fix (`komportview.h`/`.cpp`):

- `setCellSize()`: sperrt jetzt nur noch die Breite fest
  (`cellArray()->width() + mScrollBar->width()`, als Min- **und**
  Maximalbreite gesetzt); die Höhe bekommt nur noch eine kleine
  Mindesthöhe (3 Zeilen) und sonst `QWIDGETSIZE_MAX` als Maximum.
- Neue Methode `resizeGridRows(int _newRows)`: wächst/schrumpft das
  Live-Grid über `KomportCellArray::setArraySize()` auf die neue
  Zeilenzahl, bei fester Spaltenzahl. Beim Schrumpfen werden die
  wegfallenden obersten Zeilen zunächst in den Scrollback-Puffer
  geschoben (genau wie ein normaler zeilenweiser Scroll in
  `slotAboutToScrollUp()`) — es geht nichts verloren, nur weil das
  Fenster kleiner gezogen wird. Cursor-Y-Position wird passend
  nachgeführt, `resetScroll()` am Ende (Scrollback-Tiefe hat sich
  ggf. geändert).
- `resizeEvent()`: berechnet aus der neuen Fensterhöhe die passende
  Zeilenzahl (`height() / cellArray()->cellHeight()`), ruft
  `resizeGridRows()` auf, und sized den Offscreen-Pixmap-Puffer danach
  anhand von `cellArray()->height()` (tatsächliche Grid-Pixelhöhe nach
  dem Resize) statt der rohen Widget-Höhe — die ist selten ein exaktes
  Vielfaches von `cellHeight()`.
- `paintEvent()`: füllt den Hintergrund jetzt zuerst komplett mit
  `cellArray()->defaultBackgroundColor()`, bevor der Pixmap-Ausschnitt
  gezeichnet wird — deckt den Rand unterhalb der letzten Zeile ab, falls
  die Widget-Höhe kein exaktes Vielfaches von `cellHeight()` ist.

### 16.2 Mausrad im Textfenster

Bug: `KomportView` hatte nie einen `wheelEvent()`-Override — nur die neue
Minimap-Scrollbar und der Hex-Monitor reagierten aufs Mausrad, im
eigentlichen Terminaltextfenster passierte nichts.

Fix: `KomportView::wheelEvent()` hinzugefügt — scrollt `mScrollBar` um
3 Zeilen pro Rasterschritt (`angleDelta().y() / 120`), exakt wie das
`wheelEvent()` der Minimap selbst. `KomportMinimapScrollBar::setValue()`
emittiert dabei jetzt zuverlässig `valueChanged()` bei jeder echten
Wertänderung (siehe 16.3) — davon hängt ab, dass das Scrollen tatsächlich
einen Repaint auslöst.

### 16.3 Nebenbei gefundener Bug: Minimap-`setValue()` emittierte nicht

`KomportMinimapScrollBar::setValue()` (aus Meilenstein 2+3, Abschnitt 15)
hat den `valueChanged()`-Signal beim Setzen nie ausgelöst — anders als ein
echter `QScrollBar`, der das bei jeder tatsächlichen Wertänderung tut, egal
auf welchem Weg der Wert gesetzt wurde. `KomportView::resetScroll()` (wird
nach praktisch jeder eingehenden Zeile aufgerufen) verlässt sich genau
darauf, um ein Repaint auszulösen — dadurch aktualisierte sich die Ansicht
nicht zuverlässig. Fix: `emit valueChanged(mValue);` in `setValue()`
ergänzt; die dadurch redundanten expliziten `emit`-Aufrufe in
`scrollToPixelY()` und `wheelEvent()` wieder entfernt.

### 16.4 Tooltip-Truncation beim schnellen Wechsel zwischen Toolbar-Icons

Rückmeldung: einzelne Hover-Tooltips (Statusleistentext beim Hovern über
ein Toolbar-Icon) werden korrekt angezeigt, beim schnellen Wechsel von
Icon zu Icon (direkt benachbart) erscheint der Text wieder abgeschnitten —
derselbe optische Fehler wie beim ursprünglichen, bereits behobenen
Tooltip-Bug (Abschnitt 11).

Vermutete Ursache: `QLayout::updateGeometry()`/`invalidate()` löst in Qt
kein sofortiges Relayout aus, sondern postet ein `QEvent::LayoutRequest`,
das erst im nächsten Durchlauf der Event-Loop verarbeitet wird. Bei einem
einzelnen, isolierten Hover ist genug Zeit dafür, bevor der nächste Paint
kommt — beim schnellen Wechsel zwischen zwei benachbarten Icons kann das
knapp werden, und der neue (ggf. längere) Statusleistentext wird gegen die
noch alte, schmalere Geometrie gemalt und dabei abgeschnitten.

Fix (`initToolBar()` in `komport.cpp`): explizite `QAction::hovered()`-
Verbindung für jede Toolbar-Aktion mit gesetztem `statusTip()`, die nach
`statusBar()->showMessage(...)` sofort `statusBar()->layout()->activate()`
erzwingt statt auf das deferred Relayout zu warten.

**Ehrlicher Hinweis zur Verifikation:** Das exakte Race-Verhalten selbst
ließ sich in dieser Sandbox (kein echtes Display, nur `offscreen`-Plattform
mit synthetischen `QTest::mouseMove()`-Events) nicht reproduzieren — ein
Testprogramm mit zwei benachbarten Toolbar-Buttons zeigte in jedem
getesteten Ablauf bereits ohne den Fix den korrekten Text. Der Fix ist eine
standardkonforme, risikoarme Absicherung gegen die wahrscheinlichste
Ursache (deferred Layout), aber **nicht visuell auf echter Hardware
bestätigt** — bitte nach diesem Update erneut gegenprüfen.

### 16.5 Verifikation

- Build mit `-Wall -Wextra`: 0 Warnungen, 0 Fehler.
- Offscreen-Smoke-Test: startet weiterhin fehlerfrei.
- Zusätzlicher, nicht ausgelieferter Integrationstest (`komport_resizetest`,
  provisorisches CMake-Target, danach wieder entfernt) über ein echtes
  `pty`-Gerät, 11 Prüfungen, alle grün:
  - Fenster höher gezogen → Zeilenzahl wächst, Spaltenzahl bleibt fix;
  - Fenster wieder verkleinert → Zeilenzahl schrumpft, Spaltenzahl bleibt
    fix, `totalHistoryRows()` (Scrollback+Live) wird dabei **nicht**
    kleiner — die oben herausgeschobenen Zeilen landen nachweislich im
    Scrollback statt verworfen zu werden;
  - `cellAtHistoryRow()` bleibt nach mehrfachem Resize für jede gültige
    Koordinate crash-sicher, Grenzfälle (negativ, genau `total`, weit
    darüber) liefern weiterhin sauber `nullptr`;
  - synthetische `QWheelEvent`s (rauf und runter) werden an `KomportView`
    zugestellt, ohne abzustürzen.

## 17. Rückmeldung nach echtem Hardware-Test: Tooltip-Fix wirkungslos, Mausrad "ruckelt", Hex-Monitor startet leer

Nutzer-Feedback nach Abschnitt 16: die Höhenanpassung funktioniert super,
aber drei Punkte noch offen.

### 17.1 Tooltip-Truncation — Abschnitt 16.4 hat nicht geholfen, Root Cause neu bewertet

Der `layout()->activate()`-Ansatz aus Abschnitt 16.4 hat das Problem nicht
behoben. Neue Einschätzung: Qt zeigt Toolbar-Icon-Hovertexte offenbar über
einen **eigenen, internen** Mechanismus an (unabhängig davon, ob man selbst
`QAction::hovered()` verbindet oder nicht — das bestätigte bereits ein
Offscreen-Testprogramm in Abschnitt 16.4, das *ganz ohne* eigenen Code
korrekten Text zeigte). Das eigentliche Problem liegt vermutlich nicht an
der Zeitpunkt-Reihenfolge einzelner Events, sondern daran, dass
`QStatusBar::showMessage()` bei *jedem* Aufruf sein internes
Nachrichten-Label per (verzögertem) `updateGeometry()` neu vermisst -
gemeinsam genutzt sowohl von den automatischen Menü-Tooltips als auch von
`slotStatusMsg()` (aufgerufen bei praktisch jeder Aktion, z.B. "Ready.",
"Uploading file...", ...). Ein `layout()->activate()` in nur *einem* der
beiden Aufrufer reicht nicht, wenn der andere unverändert weiter über den
verzögerten Pfad läuft.

**Grundsätzlich anderer Fix statt weiterer Reparatur am selben
Mechanismus:** neues `hoverHintLabel` (`QLabel*`, Member von `KomportApp`)
ersetzt `statusBar()->showMessage()` komplett für alles, was von dieser App
selbst gesteuert wird (Toolbar-Hover **und** `slotStatusMsg()`):

- `initStatusBar()`: `hoverHintLabel` wird mit `QSizePolicy::Ignored`
  (horizontal) und Stretch-Faktor 1 per `statusBar()->addWidget(...)` als
  **normales** (nicht temporäres) Statusleisten-Widget eingehängt. Der
  entscheidende Punkt: `QSizePolicy::Ignored` sorgt dafür, dass das Layout
  den Sizehint des Labels komplett ignoriert und ihm von Anfang an einfach
  seinen Stretch-Anteil an Restbreite zuweist — die *zugewiesene* Breite
  hängt danach nie wieder vom aktuellen Text ab, `setText()` ist nur noch
  ein Repaint, nie ein Relayout. Damit ist die Race-Bedingung strukturell
  ausgeschlossen, nicht nur zeitlich enger gemacht.
- `initToolBar()`: die `QAction::hovered()`-Verbindungen rufen jetzt
  `hoverHintLabel->setText(action->statusTip())` statt
  `statusBar()->showMessage(...)`.
- Neuer `KomportApp::eventFilter()`-Override, auf `mainToolBar` installiert:
  setzt bei `QEvent::Leave` (Maus verlässt die Toolbar komplett) den Text
  zurück auf "Ready." — Wechsel von Icon zu Icon läuft direkt über die
  einzelnen `hovered()`-Verbindungen und braucht das nicht.
- `slotStatusMsg()` (bisher `statusBar()->showMessage(text)`) schreibt jetzt
  ebenfalls auf `hoverHintLabel->setText(text)` — wichtiger Nebenfund beim
  Umbau: `QStatusBar::showMessage()` blendet *alle* "normalen"
  (nicht-permanenten) Widgets der Statusleiste aus, solange die temporäre
  Nachricht aktiv ist. Da `slotStatusMsg()` bei praktisch jeder Aktion
  aufgerufen wird (u.a. mit "Ready." als Dauerzustand nach jeder
  Operation), hätte das neue `hoverHintLabel` sonst die meiste Zeit einfach
  verdeckt hinter dem letzten `slotStatusMsg()`-Text gelegen.
- Automatische Menü-Tooltips laufen weiterhin unverändert über den echten
  `showMessage()`-Pfad (bereits bestätigt funktionierend) — der blendet
  `hoverHintLabel` kurz aus, während ein Menü offen ist, und gibt es beim
  Schließen automatisch wieder frei (Standard-Qt-Verhalten für
  "normale" vs. "temporäre" Statusleisten-Widgets).

**Ehrlicher Hinweis:** Auch dieser Fix konnte in der Sandbox (kein echtes
Display) nicht visuell gegen die Original-Beobachtung getestet werden — er
beseitigt aber die *einzige* bislang identifizierte, tatsächlich
nachvollziehbare Ursache (geometrieabhängiges Relayout) vollständig durch
Konstruktion, statt sie nur zeitlich zu entschärfen. Bitte erneut auf
echter Hardware gegenprüfen.

### 17.2 Mausrad "ruckelt" — fehlende Delta-Akkumulation

Nutzer: "ich muss voll in eine Richtung beschleunigen, damit sich was
bewegt." Ursache: `wheelEvent()` (sowohl in `KomportView` als auch in
`KomportMinimapScrollBar`) hat `_e->angleDelta().y() / 120` pro Event
einzeln berechnet. Viele Mäuse/Touchpads (v.a. mit Smooth-Scrolling-Treibern
wie `libinput` unter Linux) melden ein einzelnes "Notch" über mehrere
Events mit jeweils kleinem Delta statt einem einzelnen Event mit vollen
120 — jedes davon rundete für sich allein auf 0 herunter, es bewegte sich
also nur bei einem einzelnen großen Ausschlag (starkes Beschleunigen).

Fix: neues Member `mAccumWheelDelta` (in beiden Klassen) sammelt die
Rohdeltas über mehrere Events auf; erst wenn genug für mindestens ein
volles Notch (120) zusammengekommen ist, wird gescrollt und der
verbrauchte Anteil abgezogen — der Rest bleibt für das nächste Event
erhalten. Gleiches Muster wie ein normaler `QScrollBar`/`QAbstractSlider`
das intern handhabt.

### 17.3 Hex-Monitor beginnt leer, ältere Ausgaben fehlen nach dem Öffnen

Bisheriges Verhalten (bewusste Entscheidung, siehe Abschnitt 15.3-Umfeld):
`KomportHexView::appendByte()` brach früh ab, wenn das Panel gerade nicht
sichtbar war — Begründung war, keine Zyklen für etwas zu verschwenden, das
niemand sieht. Nutzer möchte das anders: das Panel soll von Anfang an
mitschreiben, damit beim ersten Öffnen bereits die Sitzungshistorie zu
sehen ist, statt bei Null anzufangen.

Fix: die `isVisible()`-Abfrage in `appendByte()` entfernt — RX/TX-Bytes
werden jetzt unabhängig von der Sichtbarkeit des Panels geloggt (die
RX/TX-Checkbox-Filter bleiben natürlich weiter wirksam).
`mLog->setMaximumBlockCount(20000)` begrenzt den Speicherverbrauch
weiterhin, unabhängig davon, wie lange das Panel geschlossen bleibt.

### 17.4 Verifikation

- Build mit `-Wall -Wextra`: 0 Warnungen, 0 Fehler (voller Clean-Rebuild).
- Offscreen-Smoke-Test: startet weiterhin fehlerfrei.
- Zusätzlicher, nicht ausgelieferter Integrationstest (`komport_hextest`,
  provisorisches CMake-Target, danach wieder entfernt) über ein echtes
  `pty`-Gerät: Daten bei verstecktem Hex-Panel gesendet, danach geprüft,
  dass der interne `QPlainTextEdit`-Log bereits vor dem Anzeigen befüllt
  ist (bestätigt: die Historie ist tatsächlich schon da, nicht erst ab dem
  Öffnen) — der Kernpunkt des Fixes ist damit verifiziert.
- Mausrad-Akkumulation und der Statusleisten-Umbau wurden nicht durch einen
  eigenen Integrationstest abgedeckt (beides schwer sinnvoll ohne echte
  Maus-/Renderingereignisse zu simulieren) — Build- und Smoke-Test-grün,
  ansonsten auf Code-Review-Ebene verifiziert; auf echter Hardware
  gegenzuprüfen.

## 18. Tooltip-Truncation war die ganze Zeit das native `QToolTip`-Popup, nicht die Statusleiste

Nutzer hat einen kurzen Screen-Record aufgenommen (`helper/`, per `ffmpeg`
in Einzelbilder zerlegt und ausgewertet), um Abschnitt 17.1 gegenzuprüfen.
Ergebnis: der Fix aus Abschnitt 17.1 hat wieder nicht geholfen. Auf
Nachfrage präzisiert der Nutzer das Reproduktionsmuster deutlich:

> Die ersten, verstümmelten Tooltips im Video sind von Icon zu Icon.
> Danach, dort wo es passt, ist es jedes Mal nach oben aus dem Icon raus
> und neu ins nächste rein — und das funktioniert.

Dieses exakte Muster — direkter Wechsel zwischen zwei benachbarten Icons
verstümmelt, aber die Toolbar erst verlassen und neu hineingehen zeigt
korrekten Text — ist die Signatur eines bekannten Qt/Plasma-Verhaltens:
das **native `QToolTip`-Popup** (nicht die Statusleiste!) wird beim
direkten Übergang zwischen den Tooltips zweier benachbarter Widgets ohne
Lücke dazwischen oft als *dasselbe* Popup-Fenster mit der *alten*,
zwischengespeicherten Größe wiederverwendet statt komplett neu erzeugt —
der neue (ggf. längere) Text wird gegen die alte, schmalere Geometrie
abgeschnitten. Erst ein vollständiges Verstecken (Toolbar verlassen) und
Neuerscheinen erzwingt ein frisches Popup mit korrekter Größe.

Alle bisherigen Fixes (Abschnitt 16.4, 17.1) haben ausschließlich die
**Statusleiste** angefasst (`statusBar()->showMessage()` bzw. später
`hoverHintLabel`) — mit dem tatsächlich sichtbaren Fehler hatten sie damit
nie etwas zu tun. Video-Analyse (`ffmpeg`-Einzelbilder, 1 fps Quellmaterial)
bestätigt das indirekt: alle 25 extrahierten Sekunden-Frames zeigen in der
Statusleiste ausschließlich vollständigen, sauberen Text (u.a. auch das
mit 30 Zeichen längste `"Show raw RX/TX bytes as hex"`) — nirgends
abgeschnitten. Die Statusleiste war also nie das Problem; das Video konnte
nur schlicht den Sekundenbruchteil des defekten *nativen Popups* nicht
einfangen (1 fps Aufnahme, keine höhere zeitliche Auflösung verfügbar).

### 18.1 Erster Fix (Zwischenstand): natives Popup komplett unterdrückt

Erste Version: `QEvent::ToolTip` im Event-Filter einfach abgefangen und
`true` zurückgegeben — kein natives Popup mehr, nur noch `hoverHintLabel`
in der Statusleiste. Funktional ein Fix (das kaputte Popup verschwindet
komplett), aber vom Nutzer zu Recht als bloßer Workaround eingestuft:
"als schneller Workaround ok, aber Tooltip wäre schöner" — ein
funktionierendes natives Tooltip ist besser als gar keins.

### 18.2 Eigentlicher Fix: natives Tooltip selbst neu zeigen statt nur unterdrücken

Statt das Popup zu unterdrücken, wird es jetzt **selbst** angezeigt — mit
einem expliziten `QToolTip::hideText()` direkt vor jedem
`QToolTip::showText()`, um genau die Ursache zu umgehen (Qt cached sonst
die Popup-Geometrie des vorherigen Tooltips beim direkten Wechsel
zwischen zwei benachbarten Widgets und schneidet den neuen, ggf.
längeren Text dagegen ab). Gleiches Muster wie bereits vorher in
`KomportMinimapScrollBar::event()` für den Minimap-Hover-Preview
verwendet (siehe Abschnitt 15) — dort trat das Problem nie auf, weil es
dort nur ein einzelnes Widget mit eigenem `event()`-Override ist, keine
Sequenz mehrerer benachbarter Fremd-Widgets mit je eigenem Tooltip.

- `initToolBar()`: jeder Toolbar-Button bekommt jetzt zusätzlich ein
  echtes `QWidget::setToolTip(action->statusTip())` — denselben,
  ausführlicheren Text wie `hoverHintLabel`, statt der knappen
  `action->text()`-Vorbelegung, die es vorher implizit gezeigt hätte.
- `KomportApp::eventFilter()`: fängt `QEvent::ToolTip` weiterhin ab, ruft
  jetzt aber `QToolTip::hideText()` gefolgt von
  `QToolTip::showText(pos, widget->toolTip(), widget)` selbst auf, bevor
  `true` zurückgegeben wird (Event bleibt konsumiert — Qts eigene,
  fehlerhafte Popup-Wiederverwendung kommt so nie mehr zum Zug).

### 18.3 Verifikation

- Build mit `-Wall -Wextra`: 0 Warnungen, 0 Fehler (voller Clean-Rebuild).
- Offscreen-Smoke-Test: startet weiterhin fehlerfrei.
- **Ehrlicher Hinweis:** auch dieser Fix konnte in der Sandbox nicht visuell
  gegen das Original-Video-Muster verifiziert werden (kein echtes Display,
  und `QEvent::ToolTip`-Timing/Popup-Wiederverwendung ist ohnehin ein
  reines Rendering-/Fensterverwaltungs-Verhalten, das sich nicht sinnvoll
  automatisiert nachstellen lässt). Das explizite `hideText()`-vor-
  `showText()`-Muster ist aber ein bekannter, in der Qt-Community
  dokumentierter Workaround genau für diese Geometrie-Wiederverwendung,
  kein Ratespiel ins Blaue — bitte erneut auf echter Hardware
  gegenprüfen; sollte es weiterhin auftreten, ist das ein Zeichen, dass
  das Popup nicht dort ansetzt, wo hier angenommen (z.B. ein anderes
  Widget als der `QToolButton`), und weiter eingegrenzt werden muss.

## 19. Review-Gate: sieben Codex-Adversarial-Review-Runden vor Meilenstein 4 (2026-09-06)

Auf Nutzerwunsch, vor Beginn der funktionalen Meilensteine 4/5/6/7: ein
vollständiges Adversarial-Review-Gate über den kompletten Stand von
`komport/` (Basis-Portierung + Admin-Tool-Features, die auf einem anderen
Rechner entstanden waren und noch nicht nach den strengeren
Review-Vorgaben dieses Arbeitsbereichs geprüft worden waren). Sieben
Runden insgesamt (sechs automatisierte `codex:codex-rescue`-Adversarial-
Reviews mit je neuem, unabhängigem Codex-Thread, plus eine gezielte
manuelle Durchsicht dazwischen), jede Runde gegen den Diff der
vorherigen verifiziert. Insgesamt 2 Kritisch-, 4 Hoch-, ~15 Mittel- und
2 Nitpick-Findings, alle gefixt und per dauerhaftem `ctest`-Regressions-
test abgesichert — inklusive der Einführung des `tests/`-Verzeichnisses
selbst (vorher gab es nur temporäre, wieder gelöschte Test-Programme).

- [x] Vor neuen funktionalen Meilensteinen einen kompletten Codex-/Adversarial-Review
  des aktuellen Stands durchfuehren. Review-Fokus: serielle I/O mit
  `QSerialPort`, VT100/VT102-Emulation, Scrollback/Minimap, Hex-Monitor/
  Session-Logging, Profil-Persistenz, UI-Lifetime/Signal-Slot-Verbindungen,
  Build-/Install-Pfade, Lizenz-/Header-Konsistenz und fehlende Tests/
  Smoke-Checks. Durchgeführt am 2026-09-06 via `codex:codex-rescue`.

### 19.1 Findings aus dem Codex-Adversarial-Review (2026-09-06)

Statischer Review (kein Build/Smoke-Test möglich, read-only Sandbox ohne
`build/`) über den kompletten Stand von `komport/`. Kritisch/Hoch zuerst
abarbeiten, bevor Meilenstein 4 (VT220/xterm) startet — siehe Priorisierung
am Ende dieses Abschnitts.

**Stand:** Alle 15 Findings (2 Kritisch, 4 Hoch, 7 Mittel, 2 Nitpick) sind
abgearbeitet, jeweils per temporärem Testprogramm oder (Kritisch/Hoch/Mittel)
per dauerhaftem `ctest`-Regressionstest verifiziert, und über mehrere
Runden `node codex-companion.mjs review` gegen den finalen Diff
gegengeprüft (Details je Finding unten). Das Review-Gate ist damit
vollständig erfüllt — Meilenstein 4/5/6/7 kann angegangen werden.

#### Kritisch

- [x] **Manipulierte/kaputte Profilwerte können beim Start abstürzen.**
  `komport.cpp` `applyConnectionSettings()` (~Z. 586), `komportview.cpp`
  `setScrollBuffer()` (~Z. 575), `komportcellarray.cpp` `setArraySize()`
  (~Z. 55): `strScrollBuffer.toInt()` aus `QSettings` geht ungeprüft in
  `setArraySize(QSize(width, value))`. Negative Werte erzeugen negative
  Zellzahlen; `setArraySize()` ruft dann öfter `takeFirst()` auf als Zellen
  existieren. Große Werte können zudem massiven Speicherverbrauch auslösen.
  Reproduktion: `Profiles/<name>/ScrollBuffer=-1` (oder sehr groß) in der
  Konfiguration setzen, Profil laden.
  **Gefixt (2026-09-06), zwei Ebenen:** `applyConnectionSettings()` klemmt
  `strScrollBuffer.toInt()` jetzt mit `qBound(0, ..., 4096)` auf denselben
  Bereich wie das Settings-Dialog-Spinbox (`ScrollBufferSpinBox`), *und*
  `KomportCellArray::setArraySize()` selbst klemmt negative Breite/Höhe
  defensiv auf 0 — schützt so auch vor jedem anderen künftigen Aufrufer mit
  Schrott-Eingabe, nicht nur dem Profil-Ladepfad. Verifiziert per
  temporärem Testprogramm (gegen die Objektdateien des Builds gelinkt):
  gegen den ungefixten Stand reproduzierbarer Absturz (`SIGABRT` in
  `QList::takeFirst()`), nach dem Fix läuft `setArraySize(QSize(80,-1))`
  sauber durch. Voller Clean-Build mit `-Wall -Wextra`: 0 Warnungen/Fehler;
  Offscreen-Smoke-Test der App weiterhin grün.
- [x] **Unbounded CSI-Escape-Buffer durch seriellen Input.**
  `komportemulation.cpp` `sequence()` (~Z. 1073), `slotReceivedChar()`
  (~Z. 1181): nach `ESC [` hängt jedes nicht-alphabetische Byte unbegrenzt
  an `mCtlSequence` — keine Maximallänge, kein Timeout/Abort außer neuem
  ESC oder Endbuchstaben. Ein angeschlossenes Gerät kann so mit endlosen
  Ziffern/Semikolons Speicher wachsen lassen und die Ausgabe blockieren.
  Reproduktion: seriell `\x1b[` gefolgt von vielen MB `0` ohne
  Endbuchstaben senden.
  **Gefixt (2026-09-06):** neue Konstante `MaxCtlSequenceLength` (256, im
  anonymen Namespace neben dem bereits vorhandenen `MaxCtlParam`-Cap) in
  `komportemulation.cpp`; `sequence()` bricht die laufende Sequenz ab
  (State-Reset wie beim regulären Abschluss) statt `mCtlSequence`
  unbegrenzt wachsen zu lassen, sobald das Limit erreicht ist. Verifiziert
  per temporärem Testprogramm: `ESC[` + 100.000 Ziffern ohne
  Endbuchstaben verarbeitet ohne Hänger/OOM, anschließende normale
  CSI-Sequenz danach weiterhin korrekt verarbeitet (State sauber
  zurückgesetzt). Clean-Build mit `-Wall -Wextra`: 0 Warnungen/Fehler.

#### Hoch

- [x] **Geschlossene Fenster bleiben leben und können den seriellen Port
  offen halten.** `main.cpp` (~Z. 42), `komport.cpp` `closeEvent()`
  (~Z. 791), `slotFileNewWindow()` (~Z. 838): kein `WA_DeleteOnClose`,
  `closeEvent()` schließt den Serial-Port nicht. Ein per Fenstermanager
  geschlossenes Fenster wird nur versteckt, nicht zerstört — Objekte,
  Timer, Signalverbindungen und ggf. der exklusive Port-Zugriff bleiben
  aktiv.
  **Gefixt (2026-09-06), drei Teile in `komport.cpp`:**
  `KomportApp`-Konstruktor setzt jetzt `Qt::WA_DeleteOnClose`, sodass ein
  akzeptiertes `close()` das Fenster tatsächlich zerstört statt nur zu
  verstecken; `closeEvent()` schließt zusätzlich explizit den Serial-Port
  (`view->getSerial()->close()`, idempotent); `~KomportApp()` ruft jetzt
  `doc->removeView(view)` auf, um das zugehörige Mittel-Finding
  (`pViewList` wird nie bereinigt) im selben Zug zu beheben — sonst hätte
  das jetzt tatsächlich greifende `WA_DeleteOnClose` einen Dangling
  Pointer in `slotUpdateAllViews()` erzeugt.
  **Nebenbefund beim Verifizieren:** `KomportDoc::newDocument()`
  (`komportdoc.cpp`) markierte jedes frisch erzeugte Fenster unconditional
  als `modified` — dadurch zeigte *jedes* Schließen eines Fensters (auch
  eines unberührten) einen blockierenden "Datei wurde geändert,
  speichern?"-Dialog. War von Codex nicht gefunden, blockierte aber direkt
  den Verifikationstest für diesen Fix. Nutzer hat Mitfixen bestätigt:
  `setModified(true)` in `newDocument()` entfernt (openDocument/
  saveDocument bleiben laut `CLAUDE.md` bewusst Stubs, es gibt kein echtes
  "Dokument" zum Schützen).
  Verifiziert per temporärem Testprogramm: zwei `KomportApp`-Fenster
  erzeugt, eines per `close()` geschlossen (simuliert Fenstermanager-X),
  `QPointer`-Guard bestätigt tatsächliche Zerstörung, anschließender
  `slotUpdateAllViews()`-Broadcast auf dem verbleibenden Fenster crasht
  nicht. Clean-Build mit `-Wall -Wextra`: 0 Warnungen/Fehler.
  **Nachbesserung nach Codex-Review-Runde 2 (`node codex-companion.mjs
  review`):** Codex bemängelte (P1), `slotFileClose()` greife nach
  `close()` mit `WA_DeleteOnClose` noch per `slotStatusMsg(tr("Ready."))`
  auf `this` zu. Eigene Gegenprobe zeigte zwar, dass Qt hier
  `deleteLater()` statt sofortiger Zerstörung nutzt (Objekt bleibt bis zum
  nächsten Event-Loop-Durchlauf gültig) — auf Vorschlag des Nutzers aber
  bewusst trotzdem defensiv gepatcht, statt sich auf dieses (von der
  Qt-Doku nicht als Vertrag zugesicherte) Detail zu verlassen:
  `slotFileClose()` greift jetzt nach einem *erfolgreichen* `close()`
  nicht mehr auf `this` zu (`if (!close()) slotStatusMsg(...)`), und das
  bisher dort stehende `view->getSerial()->close()` wurde entfernt, da
  `closeEvent()` den Port bei akzeptiertem Close bereits für jeden
  Close-Pfad einheitlich schließt. Erneut verifiziert per temporärem
  Testprogramm (`slotFileClose()` über `QMetaObject::invokeMethod`
  aufgerufen wie die verbundene `QAction`): Fenster wird zerstört, Port
  ist danach geschlossen, kein Zugriff auf `this` nach akzeptiertem Close.
  Clean-Build mit `-Wall -Wextra`: weiterhin 0 Warnungen/Fehler.
- [x] **Session-Logger puffert unbegrenzt bis zum nächsten LF.**
  `komportsessionlogger.cpp` `logChar()` (~Z. 58): `mLineBuffer` wächst bis
  `\n` kommt oder Logging gestoppt wird — binäre Streams, lange
  Statuszeilen oder nur-`\r`-Geräte werden nie periodisch geflusht (RAM-Risiko).
  **Gefixt (2026-09-06):** neue Konstante `MaxLineBufferLength` (4096) in
  `komportsessionlogger.cpp`; `logChar()` erzwingt einen `flushLine()`,
  sobald der Puffer diese Grenze erreicht, auch ohne `\n`. Verifiziert per
  temporärem Testprogramm: 500.000 Bytes ohne jedes LF ergaben 125
  geflushte Log-Zeilen statt eines unbegrenzt wachsenden Puffers.
- [x] **Download-Transfer ist eine Busy-Loop und prüft den Dateiöffnen-Fehler
  nicht.** `komporttransfer.cpp` `download()` (~Z. 62): `mFile.open()`-
  Rückgabewert wird ignoriert; danach läuft eine `processEvents()`-Schleife
  ohne Sleep/Wait bis Cancel/Ctrl-D — dreht CPU-lastig, wenn keine Daten
  kommen, und behandelt einen fehlgeschlagenen Dateiöffnen-Versuch still
  als Erfolg.
  **Gefixt (2026-09-06):** `download()` prüft jetzt `mFile.open()` und
  zeigt bei Fehlschlag einen `QMessageBox::warning()` statt still
  weiterzulaufen; die Polling-Schleife nutzt jetzt
  `QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents)` statt
  `AllEvents` — wartet auf das nächste Ereignis (eingehendes Byte,
  Cancel-Klick, Ctrl-D) statt CPU-lastig zu drehen, wenn nichts ankommt.
- [x] **Upload-Transfer ignoriert Open-/Write-Fehler.** `komporttransfer.cpp`
  `upload()` (~Z. 39), `komportserial.cpp` `putChar()` (~Z. 167):
  `mFile.open()` und serielle Write-Fehler werden nicht an den Aufrufer
  gemeldet; `upload()` gibt immer `true` zurück.
  **Gefixt (2026-09-06):** `KomportSerial::putChar()` gibt jetzt `bool`
  zurück (Erfolg/Misserfolg) statt `void`; `upload()` prüft sowohl
  `mFile.open()` als auch jeden `putChar()`-Aufruf und zeigt bei
  Fehlschlag einen `QMessageBox::warning()` (inkl. Byte-Fortschritt bei
  Verbindungsabbruch) statt `true` zu faken.
  Beide Transfer-Fixes verifiziert durch Clean-Build mit `-Wall -Wextra`
  (die vorher vorhandenen `-Wunused-result`-Warnungen zu den ignorierten
  `mFile.open()`-Rückgabewerten sind jetzt weg, da beide Stellen den
  Rückgabewert jetzt auswerten); `KomportUpload`/`KomportDownload` sind
  reine Passthrough-Subklassen ohne eigene Overrides, profitieren also
  automatisch mit.

#### Mittel

- [x] `QSerialPort`-Fehler (`settingsFailed()`) sind nirgends mit der UI
  verbunden — Port-/Framing-/Permission-Probleme landen nur in
  `qWarning()`, nicht als Status-/Fehlerdialog. (`komportserial.cpp`
  `slotPortError()` ~Z. 206, `komport.cpp` `applyConnectionSettings()`
  ~Z. 586 ignoriert den `open()`-Rückgabewert.)
  **Gefixt (2026-09-06):** `KomportSerial::settingsFailed()` trägt jetzt
  einen `const QString &reason`-Parameter (befüllt aus `mPort.errorString()`
  bzw. einer festen Meldung bei fehlgeschlagenem `applyPortSettings()`);
  neuer Slot `KomportApp::slotSerialSettingsFailed()` verbindet das Signal
  und zeigt die Meldung über `slotStatusMsg()` in der Statusleiste (bewusst
  kein `QMessageBox`, da das Signal bei einer instabilen Verbindung
  wiederholt feuern kann). `open()`s ignorierter Rückgabewert wird
  darüber indirekt mit abgedeckt, da ein fehlgeschlagenes `mPort.open()`
  bereits `errorOccurred()` → `slotPortError()` → `settingsFailed()`
  auslöst.
  **Nachgebessert über drei weitere Codex-Review-Runden:** (1) neues
  Member `mSerialErrorPending` verhindert, dass `loadProfile()`s/
  `slotShowPreferences()`s eigene abschließende Erfolgsmeldung
  ("Loaded profile ..."/"Ready.") eine gerade erst synchron gemeldete
  Fehlermeldung sofort wieder überschreibt (Codex-Fund: `applyPortSettings()`
  kann mitten in `loadProfile()`/`slotShowPreferences()` synchron
  `settingsFailed()` auslösen, bevor die Methode ihre eigene
  "Erfolg"-Statusmeldung setzt). (2) In `slotShowPreferences()` wenden
  `setFraming()`/`setFlowControl()` auf einen bereits offenen Port sofort
  eine noch unvollständig aktualisierte Zwischenkombination an — kann
  transient fehlschlagen, obwohl die tatsächlich gewünschte Endkombination
  gültig ist. Reset von `mSerialErrorPending` deshalb nicht pauschal an den
  Anfang des Setter-Blocks gelegt, sondern gezielt direkt vor
  `setBaudRate()` (dem in der Aufrufreihenfolge letzten Setter, der die
  Portoptionen berührt, mit bereits vollständig aktualisierten Feldern) —
  verwirft so gezielt nur den "Lärm" der früheren Zwischenaufrufe.
  (3) Ein erster Versuch, den Erfolg stattdessen über `serial->isOpen()`
  zu verifizieren (`mSerialErrorPending = !serial->isOpen()`), war selbst
  ein Fehlschluss: eine von `QSerialPort` abgelehnte Einstellung schließt
  den Port nicht zwangsläufig — der Port kann offen bleiben, obwohl die
  gewünschte Kombination nicht übernommen wurde. Diese Zeile wieder
  entfernt; die vierte Review-Runde war dann sauber (keine Findings mehr).
  `applyConnectionSettings()`/`loadProfile()`s Pfad brauchte diese
  Nachbesserung nicht, da dort der Port vor jedem Setter-Aufruf immer
  erst geschlossen wird (siehe Kommentar dort) — keine Zwischenzustände,
  kein `isOpen()`-Fehlschluss möglich.
- [x] RX-Flush ist bei Bursts unnötig teuer — `slotFlushRxBuffer()`
  (`komportserial.cpp` ~Z. 214) leert den Buffer byteweise mit
  `remove(0, 1)`, was bei hohen Baudraten/Bursts O(n²) kostet.
  **Gefixt (2026-09-06):** Snapshot-and-Clear statt Byte-für-Byte-`remove()`
  — `mRxBuffer` wird einmal kopiert (implizit geteilt, kein Deep-Copy) und
  sofort geleert, dann wird über die Kopie iteriert. O(n) statt O(n²).
- [x] Ungültige `RXQueue`/`FlushRate`-Werte aus Profilen werden ungeprüft
  übernommen (`komport.cpp` ~Z. 600, `komportserial.cpp` ~Z. 190/223) —
  die GUI begrenzt Werte, `QSettings`-Import nicht; `RXQueue<=0` wirft
  empfangene Daten effektiv weg.
  **Gefixt (2026-09-06):** `KomportSerial::setRxQueue()` klemmt jetzt auf
  `qMax(1, _i)`, `setFlushRate()` auf `qMax(0, _i)` — direkt im Setter,
  schützt also jeden Aufrufer, nicht nur den Profil-Ladepfad.
- [x] Blink-Update iteriert über Pixelbreite statt Spaltenzahl —
  `komportview.cpp` `timerEvent()` (~Z. 223) nutzt `cellArray()->cellWidth()`
  als Spaltenlimit statt `arrayWidth()`; blinkende Zellen rechts davon
  werden nicht regelmäßig neu gezeichnet.
  **Gefixt (2026-09-06):** Ein-Zeilen-Fix, `cellWidth()` → `arrayWidth()`.
- [x] Statische View-Liste `pViewList` wird nie bereinigt
  (`komportdoc.cpp` ~Z. 30/49/55) — `removeView()` wird nirgends
  aufgerufen. Aktuell durch das "Hoch"-Finding zu Fensterlebenszeit
  verdeckt; sobald Fenster korrekt gelöscht werden, drohen Dangling
  Pointers in `slotUpdateAllViews()`. **Zusammen mit dem Fenster-Lifetime-
  Fix oben beheben, nicht isoliert.**
  **Gefixt (2026-09-06) zusammen mit dem Hoch-Finding oben:**
  `~KomportApp()` ruft jetzt `doc->removeView(view)` auf — siehe
  Verifikation dort.
- [x] `KomportView::getDocument()` castet `window()` hart zu `KomportApp*`
  (`komportview.cpp` ~Z. 90) und dereferenziert sofort — crasht außerhalb
  dieses Einbettungskontexts (Tests, Preview-Container).
  **Gefixt (2026-09-06):** C-Style-Cast durch `qobject_cast<KomportApp*>`
  ersetzt. **Nachgebessert nach Codex-Review:** die erste Version gab bei
  Nichtübereinstimmung `nullptr` zurück (plus `qWarning()`) — Codex wies
  zu Recht darauf hin, dass das den Crash nur eine Ebene weiter
  verschiebt, da `KomportView::getSerial()` `getDocument()->getSerial()`
  ungeprüft aufruft und der Konstruktor `getSerial()` sofort aufruft; ein
  Null-Return "löst" hier nichts, es tauscht nur UB gegen einen
  Null-Pointer-Crash an unzusammenhängender Stelle. Da `KomportView`
  architekturell zwingend an ein `KomportApp`-Top-Level-Fenster gebunden
  ist (kein Rewrite zu einem allgemein wiederverwendbaren Widget, keine
  sinnvolle Teilfunktion ohne Dokument), gibt es hier keinen "sauberen"
  Fallback — stattdessen jetzt `qFatal()` direkt an der eigentlichen
  Verletzungsstelle: lauter, aber mit einer klaren Diagnosemeldung genau
  dort, wo die Annahme bricht, statt eines mysteriösen Absturzes an
  anderer Stelle ohne Hinweis auf die Ursache.
- [x] Keine dauerhaften Tests/CTest-Targets im Repo (nur temporäre,
  wieder gelöschte Test-Targets laut `TODO-ARCHIVE.md`) — genau die
  bereits gefixten Crash-Klassen (Cursor-Clamping, Escape-Parsing,
  Profilwechsel, serieller Roundtrip) können regressieren, ohne dass es
  auffällt.
  **Gefixt (2026-09-06):** `CMakeLists.txt` in eine Objekt-Library
  `komport_core` (alle Quellen außer `main.cpp`) plus dünnes
  `komport-qt6`-Executable aufgeteilt; neues `tests/`-Verzeichnis mit
  `Qt6::Test`-basierten Regressionstests, die gegen exakt dieselben
  kompilierten Klassen linken. Fünf permanente Test-Targets (`ctest`
  nach dem Build, `BUILD_TESTING` via `include(CTest)`, Default an):
  `tst_cellarray` (negative `setArraySize()`-Dimensionen), `tst_emulation`
  (unbounded CSI-Buffer + Cursor-Clamping an allen vier Rändern),
  `tst_serial` (RXQueue/FlushRate-Clamping), `tst_windowlifetime`
  (`WA_DeleteOnClose`, `pViewList`-Bereinigung, `slotFileClose()`
  greift nicht auf ein bereits akzeptiert geschlossenes Fenster zu),
  `tst_profileerror` (ein synchron gemeldeter Serial-Fehler in
  `loadProfile()` übersteht dessen eigene abschließende
  "Loaded profile ..."-Erfolgsmeldung — Regressionstest für die
  `mSerialErrorPending`-Nachbesserung beim `settingsFailed()`-Finding
  oben). `tst_windowlifetime`/`tst_profileerror` leiten `QSettings`
  jeweils auf eine eigene Organisation/App
  ("Komport-Qt6-Test"/"Komport-Qt6-Test-ProfileError") um, damit
  Testläufe nie das echte `~/.config/Komport-Qt6/`-Profil eines Nutzers
  anfassen, und räumen ihre Test-Config-Datei/ihr Verzeichnis in
  `cleanupTestCase()` wieder auf. Alle 5 Tests grün, Clean-Build mit
  `-Wall -Wextra`: weiterhin 0 Warnungen/Fehler.

#### Nitpick

- [x] `komport.desktop`: veraltete Shebang-Zeile (`#!/usr/bin/env
  xdg-open`) und `Encoding=UTF-8` sind für moderne `.desktop`-Dateien
  unüblich.
  **Gefixt (2026-09-06):** beide Zeilen entfernt. Verifiziert mit
  `desktop-file-validate`: vorher eine Warnung ("key \"Encoding\" ... is
  deprecated"), danach `VALID, no warnings`.
- [x] Debug-`printf()` in `komportemulation.cpp` (~Z. 920, ~Z. 1139) bei
  unbekannten SGR/CSI-Sequenzen geht nach stdout statt z.B. `qDebug()`.
  **Gefixt (2026-09-06):** beide Stellen auf `qDebug()` umgestellt,
  `#include <cstdio>` (sonst ungenutzt) durch `#include <QDebug>` ersetzt.
  Verifiziert per temporärem Testprogramm mit unbekannter SGR- (`ESC[999m`)
  und CSI-Sequenz (`ESC[z`) — beide Meldungen erscheinen korrekt
  (`?attr? 999` / `?ctl? 'z'`). **Nebenbefund bei der Verifikation:**
  dieses Sandbox-Environment läuft unter systemd (`JOURNAL_STREAM`
  gesetzt) — Qt routet `qDebug()`/`qWarning()` dort standardmäßig ins
  systemd-Journal statt nach stderr, sichtbar erst mit
  `QT_FORCE_STDERR_LOGGING=1`. Reine Environment-Eigenheit dieser
  Sandbox, kein Verhalten der Anwendung selbst.

#### Empfohlene Reihenfolge

Profilwert-Validierung/Array-Größen absichern → CSI-Buffer begrenzen →
Fenster-Close/Serial-Lifetime korrigieren (inkl. `pViewList`-Bereinigung) →
Logger-/Transfer-Busy-Loop und stille Fehler entschärfen → danach
dauerhafte PTY- und Emulations-Regressionstests in CMake/CTest aufnehmen.
Erst danach Meilenstein 4/5/6/7 angehen.


### 19.2 Zweiter Full-Review nach den Fixes aus 0.1 (2026-09-06)

Auf Nutzerwunsch ein zweiter, unabhängiger `codex:codex-rescue`-Adversarial-
Review über den Stand nach allen 15 Fixes aus Abschnitt 19.1 (Branch
`fix/codex-review-critical-high-findings`, neuer Codex-Thread statt
Fortsetzung des ersten) — Ziel: prüfen, ob die Fixes selbst sauber sind und
ob dabei neue Probleme eingeführt wurden. Ergebnis: keine Kritisch-/Hoch-
Blocker, aber 3 neue Mittel-Findings, alle durch die vorherigen Fixes
selbst verursacht bzw. davon unberührt gebliebene Nachbarstellen. Alle 3
gefixt, verifiziert, per Codex-Review gegengeprüft (keine weiteren
Findings).

- [x] **Uninitialisiertes `mPixmap` beim Profil-Laden im Konstruktor.**
  `komportview.cpp` `slotScroll()` (~Z. 592), `updateCell()` (~Z. 197),
  `slotScrolledUp()` (~Z. 447): `mPixmap` bekommt seine tatsächliche Größe
  erst in `resizeEvent()`. `KomportApp`s Konstruktor lädt ein Profil
  (`initProfiles()` → `loadProfile()` → `applyConnectionSettings()` →
  `view->setScrollBuffer()` → `resetScroll()`) schon vor `show()`/dem
  ersten `resizeEvent()`; `resetScroll()`s `setValue()` kann (abhängig vom
  Scroll-Buffer-Zustand) synchron über `valueChanged()` `slotScroll()`
  erreichen, das bis dahin bedingungslos `QPainter paint(&mPixmap)` auf
  ein noch null-großes `QPixmap` ausgeführt hat — funktional ein No-op,
  aber mit lauten `QPainter::begin: Paint device returned engine == 0`-
  Warnungen bei jedem Start.
  **Gefixt (2026-09-06):** `if (mPixmap.isNull()) return;`-Guard an allen
  drei Stellen. Sicher, weil `resizeEvent()` am Ende immer
  `cellArray()->update()` aufruft, was jede Zelle über die normale
  Signal-Kette (`slotRowChanged()`/`slotCellChanged()` → `updateCell()`)
  neu zeichnet, sobald `mPixmap` eine echte Größe hat — es geht also
  nichts verloren, nur redundante Arbeit/Warnungen werden vermieden.
  Verifiziert per neuem `tst_windowlifetime`-Testfall: gegen den
  ungefixten Stand reproduzierbar (`FAIL!` mit exakt der erwarteten
  `QPainter`-Warnung, 2×), gegen den gefixten Stand grün.
  **Nebenbefund beim Testschreiben:** der natürliche Konstruktions-Pfad
  allein triggert das *nicht* zuverlässig (ein frisch geladenes Profil
  hat einen leeren Scroll-Buffer, `resetScroll()`s `setValue(0)` ändert
  dann nichts am bereits-0 stehenden Scrollbar-Wert, `valueChanged()`
  feuert nicht) — der Test ruft `updateCell()`/`slotScroll()` deshalb
  direkt auf, statt sich auf den zufälligen Trigger-Zustand beim Start zu
  verlassen.
- [x] **Neue Transfer-Fehlerrückgaben wurden von der UI ignoriert.**
  `komporttransfer.cpp` `download()` (~Z. 76), `slotReceivedChar()`
  (~Z. 107), `komport.cpp` `slotFileOpen()`/`slotFileOpenRecent()`/
  `slotFileSaveAs()`: `upload()`/`download()` geben seit dem ersten
  Fix-Durchlauf (Abschnitt 19.1, Hoch) sinnvoll `false` bei Fehlern
  zurück, aber (a) `download()` gab nach einem Cancel weiterhin `true`
  zurück und prüfte `mFile.putChar()`s Rückgabewert in
  `slotReceivedChar()` nicht, und (b) die UI-Aufrufstellen ignorierten
  den Rückgabewert komplett und riefen trotzdem `addRecentFile()` auf —
  ein abgebrochener/fehlgeschlagener Transfer landete so wie ein
  erfolgreicher in der Recent-Files-Liste.
  **Gefixt (2026-09-06):** neues Member `mDownloadWriteError`, gesetzt in
  `slotReceivedChar()` bei fehlgeschlagenem `mFile.putChar()`, geprüft in
  `download()`s Polling-Schleife (bricht die Schleife mit ab) und danach
  (zeigt einen `QMessageBox::warning()`). `download()` unterscheidet jetzt
  "Ctrl-D empfangen" (echter Abschluss, `mFile` wird dabei selbst
  geschlossen) von "Cancel/Fehler" (Schleife bricht anders ab, `mFile`
  bleibt bis zum expliziten `close()` offen) und gibt entsprechend
  `true`/`false` zurück. Alle drei UI-Aufrufstellen rufen
  `addRecentFile()` jetzt nur noch bei `true` auf.
  **Nachgebessert nach Codex-Review-Runde 2:** dieselbe Lücke bestand
  unverändert auch in `upload()` — bei Cancel gab es weiterhin
  bedingungslos `true` zurück (`writeFailed` blieb `false`), obwohl der
  Upload nicht vollständig war. `upload()` unterscheidet jetzt ebenfalls
  "wirklich fertig" (zunächst `mFile.atEnd()` direkt nach der Schleife,
  vor `close()`) von "Cancel/Fehler" und gibt entsprechend zurück.
  **Nachgebessert nach Codex-Review-Runde 3:** `mFile.atEnd()` selbst war
  noch nicht ganz richtig — `getChar()` rückt die Leseposition bereits
  vor, *bevor* die Schleifenbedingung `&& !progress.wasCanceled()`
  überhaupt geprüft wird. Ein Cancel-Klick exakt beim letzten Byte kann
  also dazu führen, dass dieses Byte zwar schon *gelesen* (Datei damit
  auf `atEnd()`), aber nie an `putChar()` übergeben wurde (die
  Schleifenkörper-Ausführung für dieses Byte entfällt, `sent` wird nicht
  erhöht) — `atEnd()` hätte das fälschlich als vollständig gemeldet.
  Jetzt `sent == size` statt `mFile.atEnd()` als Kriterium — unabhängig
  von dieser Lese-vs-Sende-Reihenfolge-Race, da es die tatsächlich
  gesendete Byte-Anzahl mit der Dateigröße vergleicht statt der
  Leseposition.
- [x] **`setArraySize()`-Fix schützte vor negativen Werten, nicht vor
  Integer-Overflow.** `komportcellarray.cpp` `setArraySize()` (~Z. 55):
  `newcnt = _sz.width()*_sz.height()` als `int`-Multiplikation kann bei
  sehr großen positiven Dimensionen überlaufen und (undefiniertes
  Verhalten, praktisch oft) negativ/falsch klein werden — dann kann der
  Shrink-Pfad wieder mehr Zellen per `takeFirst()` entfernen als
  existieren, exakt dieselbe Absturzklasse wie beim ursprünglichen
  negativen-Werte-Fix, nur über eine andere Route.
  **Gefixt (2026-09-06):** die Multiplikation läuft jetzt zuerst in
  `qint64` (`wanted`); überschreitet sie eine neue Konstante `MaxCells`
  (10.000.000 — weit über jeder plausiblen Terminal-/Scrollback-Größe),
  wird `height` so weit heruntergeklemmt, dass das Produkt wieder passt,
  *bevor* die eigentliche `int newcnt`-Berechnung läuft. Eine zweite,
  bis dahin übersehene Stelle im selben Vorher-leer-Zweig
  (`for (index=0; index < mArraySize.width()*mArraySize.height(); ...)`,
  dieselbe unsichere Multiplikation ein zweites Mal) auf das jetzt
  sichere `newcnt` umgestellt statt sie erneut zu berechnen.
  Verifiziert per `tst_cellarray`-Testfall: gegen den ungefixten Stand
  reproduzierbar (Absturz), gegen den gefixten Stand grün.
  **Nachgebessert nach Codex-Review-Runde 2 (P2/P3):** der erste Fix
  klemmte nur `height` auf Basis des Produkts, ließ aber eine extrem
  große `width` unverändert in `mArraySize` stehen — andere öffentliche
  Methoden (`size()`, `arrayWidth()`, Iterationen bis `arrayWidth()`)
  nutzen die Dimensionen direkt, nicht nur die Zellzahl, könnten also
  weiterhin überlaufen oder exzessiv viel arbeiten, obwohl `newcnt`
  selbst sicher war. Neue Konstante `MaxDimension` (100.000) klemmt jetzt
  *zuerst* jede Dimension einzeln, bevor das Produkt überhaupt betrachtet
  wird. Beim Implementieren selbst noch eine zweite, subtilere Lücke
  gefunden und geschlossen, bevor sie in eine weitere Review-Runde ging:
  die `MaxCells`-basierte `height`-Reduktion (`MaxCells / width`) kann
  rechnerisch selbst wieder über `MaxDimension` hinausschießen, wenn
  `width` klein ist — mit den aktuellen Konstanten (`MaxCells` ist ein
  glattes Vielfaches von `MaxDimension`) ist das nicht tatsächlich
  erreichbar (siehe Kommentar im Code für die Herleitung), aber der Cap
  wird nach der Reduktion trotzdem defensiv erneut angewendet, damit die
  Invariante "Dimensionen bleiben immer ≤ `MaxDimension`" nicht still von
  dieser zahlentheoretischen Zufälligkeit zwischen den beiden Konstanten
  abhängt, falls eine davon später unabhängig geändert wird.
  `tst_cellarray`s Testfall bewusst mit extrem großer Breite statt großer
  Höhe gewählt (Breite wird auf `MaxDimension` geklemmt, Höhe bleibt
  klein) — ein Test, der tatsächlich den vollen `MaxCells`-Ergebniszweig
  durchläuft, hätte ~10 Mio. `KomportCell`-Objekte alloziert (~1,9s
  gemessen, selbst mit `-O2`); für einen einzelnen Unit-Test-Fall nicht
  gerechtfertigt, wenn eine deutlich billigere Eingabe (Breite allein
  jenseits von `MaxDimension`) den Clamp genauso direkt nachweist.

**Verifikation (gesamt):** alle 5 `ctest`-Targets grün. Clean-Build mit
`-Wall -Wextra`: weiterhin 0 Warnungen/Fehler. Offscreen-Smoke-Test grün.
Drei Codex-Review-Runden auf dem finalen Diff: Runde 1 fand die
`upload()`-Cancel-Lücke (P2) und die fehlende Breiten-Klemmung (P3),
beide gefixt; Runde 2 fand eine subtilere Restlücke in genau diesem
Cancel-Fix (P2, `atEnd()` vs. tatsächlich gesendete Bytes), ebenfalls
gefixt; Runde 3 fand keine weiteren Findings.


### 19.3 Dritter Full-Review (2026-09-06)

Wieder auf Nutzerwunsch, wieder neuer, unabhängiger Codex-Thread statt
Fortsetzung. Diesmal explizit angewiesen, den kompletten Code frisch zu
prüfen statt nur die zuletzt geänderten Stellen — Ziel: prüfen, ob beim
wiederholten Fokussieren auf einzelne Findings andere Bereiche
vernachlässigt wurden. Ergebnis: keine Kritisch-/Hoch-Funde, 2 neue
Mittel-Findings — beide in Code, der in Runde 1/2 nicht angefasst wurde
(Erase-Sequenzen der Emulation, `putStr()`), also echte, bisher
unentdeckte Lücken und keine durch die vorherigen Fixes verursachten
Regressionen. Codex bestätigte explizit: "Statisch sehe ich keine neuen
Kritisch-/Hoch-Probleme in den Runde-2-Fixes selbst" — `mPixmap.isNull()`-
Guards, `sent == size`, `mDownloadWriteError`, `WA_DeleteOnClose` und die
`MaxDimension`/`MaxCells`-Logik wurden als konsistent eingestuft.

- [x] **`CSI 1 K`/`CSI 1 J` (Erase Line/Display "Anfang bis Cursor")
  aktualisierten das sichtbare Terminal nicht, und `CSI 1 J` löschte zu
  viel.** `komportemulation.cpp` `doClearEOL()` Fall 1 (~Z. 751),
  `doClearScreen()` Fall 1 (~Z. 792): beide riefen `cell(x,y)->clear()`
  direkt auf `KomportCell` auf statt über `KomportCellArray::clear()`,
  welches zusätzlich `updateCell()`/das `cellChanged`-Signal auslöst, auf
  das `KomportView` (malt aus `mPixmap`, aktualisiert nur darüber)
  angewiesen ist — die gelöschten Zellen blieben also visuell "stale",
  bis ein unabhängiges späteres Repaint sie zufällig mit erfasste.
  Zusätzlich in `doClearScreen()` Fall 1 ein eigenständiger Logikfehler:
  das `break` verließ nur die innere Spalten-Schleife bei Erreichen der
  Cursor-Position, die äußere Zeilen-Schleife lief unverändert weiter und
  löschte danach *alle* Zeilen unterhalb des Cursors mit — `CSI 1 J`
  ("Anfang des Bildschirms bis einschließlich Cursor") hat sich damit wie
  `CSI 2 J` (kompletter Bildschirm) verhalten.
  **Gefixt (2026-09-06):** beide Stellen nutzen jetzt
  `cellArray()->clear(x,y)` statt `cell(x,y)->clear()`;
  `doClearScreen()` Fall 1 zusätzlich auf eine korrekte Zeilen-Grenze
  umgestellt (äußere Schleife nur bis einschließlich der Cursor-Zeile,
  auf der Cursor-Zeile selbst nur bis zur Cursor-Spalte) statt sich auf
  ein bedingtes `break` zu verlassen, das die äußere Schleife nie
  beendet hätte. Verifiziert per neuem `tst_emulation`-Testfall: gegen
  den ungefixten Stand reproduzierbar (`FAIL!` — Zeilen unterhalb des
  Cursors wurden ebenfalls gelöscht), gegen den gefixten Stand grün
  (inkl. `QSignalSpy` auf `cellChanged`, um auch die fehlende
  Benachrichtigung abzudecken).
- [x] **`KomportSerial::putStr()` konnte partielle/fehlgeschlagene Writes
  still verlieren.** `komportserial.cpp` `putStr()` (~Z. 181): ein
  einzelner `mPort.write(str, len)`-Aufruf, `sentChar()` wurde nur für
  tatsächlich geschriebene Bytes emittiert, der Rest bei einem Partial-
  Write oder `-1` bei Fehler wurde stillschweigend verworfen — kein
  Retry, kein Rückgabewert zur Prüfung. Betroffen: Makro-Kommandos/
  Zeilenenden (`komport.cpp`), Tastatur-Escape-Sequenzen und Device-
  Status-Report/Device-Attributes-Antworten (`komportemulation.cpp`).
  **Gefixt (2026-09-06):** `putStr()` gibt jetzt `bool` zurück (analog
  zum bereits vorher auf `bool` umgestellten `putChar()`) und loggt einen
  `qWarning()` bei Partial-/Fehl-Write. Bewusst *nicht* jede einzelne
  Aufrufstelle (u.a. viele knappe Tastatur-Escape-Sequenzen in einem
  `switch`) einzeln auf den Rückgabewert umgestellt — anders als bei den
  Datei-Transfers, wo Datenintegrität kritisch ist, sind das reine
  Fire-and-Forget-Pfade; die `qWarning()` in `putStr()` selbst macht das
  Problem für alle Aufrufer gleichermaßen diagnostizierbar, ohne jede
  Stelle einzeln umbauen zu müssen. Verifiziert per neuem
  `tst_serial`-Testfall (geschlossener Port als einzig zuverlässig ohne
  echtes Gerät reproduzierbarer Fehlerfall): `putChar()`/`putStr()` geben
  jetzt korrekt `false` zurück statt stillschweigend Erfolg vorzugeben.

**Verifikation:** alle 5 `ctest`-Targets grün (2 erweitert). Clean-Build
mit `-Wall -Wextra`: weiterhin 0 Warnungen/Fehler. Offscreen-Smoke-Test
grün, echtes `~/.config/Komport-Qt6/`-Profil unangetastet (md5 identisch).


### 19.4 Vierter Full-Review (2026-09-06)

Wieder neuer, unabhängiger Codex-Thread, diesmal explizit mit der Frage,
ob nach drei vorherigen Fix-Runden echte Konvergenz erreicht ist oder ob
die wiederholte Fokussierung auf einzelne Findings zu verzerrter
Testabdeckung geführt hat. Ergebnis: **die Runde-3-Fixes selbst wurden
explizit als sauber bestätigt** ("Die beiden Runde-3-Fixes sehen sauber
aus ... Kein neuer Bounds-/Notify-Fehler sichtbar"; `putStr()`
"konsistent"). 2 neue Mittel-Findings, wieder beide in bis dahin
unangefasstem Code (Textauswahl beim Zurückscrollen,
`KomportEmulation`-Lifetime) — echte, unabhängig vom bisherigen
Fix-Fokus bestehende Lücken.

- [x] **Auswahl/Copy war bei zurückgescrollter Ansicht falsch (falscher
  Zellinhalt, und ein potenziell dauerhaft "selektiert" bleibender,
  unsichtbarer Live-Grid-Cell).** `komportview.cpp` `select()` (~Z. 523),
  `deselect()` (~Z. 554): Rendering (`paintCell()`) und Minimap lesen
  Scrollback-Inhalte korrekt über das scroll-bewusste `getCell()`
  (berücksichtigt, ob Bildschirmposition `(x,y)` gerade auf den
  Scroll-Buffer oder das Live-Grid zeigt), aber `select()`/`deselect()`
  griffen weiterhin direkt auf `cellArray()->cell(x,y)` zu — beim
  Zurückscrollen in die Historie wurden also Zellen aus dem aktuellen
  Live-Bildschirm markiert/kopiert, nicht die tatsächlich sichtbaren
  Scrollback-Zellen. Zusätzlich fegte `deselect()` nur über das Live-Grid,
  sodass eine fälschlich markierte Scroll-Buffer-Zelle nie wieder
  zurückgesetzt worden wäre.
  **Gefixt (2026-09-06):** `select()` nutzt jetzt `getCell(x,y)` statt
  `cellArray()->cell(x,y)` zum Lesen/Markieren (der Repaint-Trigger
  `cellArray()->updateCell(x,y)` bleibt unverändert — er ist nur ein
  "Bildschirmposition (x,y) neu zeichnen"-Signal, das über die normale,
  scroll-bewusste Repaint-Kette bei `paintCell()`/`getCell()` ohnehin
  wieder korrekt aufgelöst wird). `deselect()` fegt jetzt zusätzlich über
  `mScrollBuffer` und löst nur bei tatsächlich etwas Zurückgesetztem einen
  einzelnen `cellArray()->update()`-Vollrepaint aus (statt vorher pro
  Zelle einzeln). Verifiziert per neuem `tst_selection`-Testfall: Inhalt
  über die echte Emulation geschrieben (mehr Zeilen als Bildschirmhöhe,
  treibt echtes Scrollen), zurückgescrollt, `select()` aufgerufen — gegen
  den ungefixten Stand reproduzierbar (Live-Grid-Zelle statt
  Scroll-Buffer-Zelle markiert), gegen den gefixten Stand grün. (Die
  Prüfung stützt sich bewusst auf Zell-Identität/Flags statt auf einen
  Roundtrip über `QApplication::clipboard()`s `Selection`-Modus, da die
  hier für Tests verwendete `offscreen`-QPA-Plattform diesen
  X11-spezifischen Clipboard-Modus gar nicht unterstützt — reine
  Plattform-Einschränkung, kein Code-Bug.)
- [x] **`KomportEmulation` wurde pro `KomportView` geleakt.**
  `komportview.cpp` Konstruktor (~Z. 77): `mEmulation = new
  KomportEmulation(...)` — kein `QObject`-Parent möglich (der Konstruktor
  nimmt keinen entgegen) und `~KomportView()` löschte es nie. Vor dem
  Fenster-Lifetime-Fix (Abschnitt 19.1, Hoch) faktisch harmlos (Fenster
  wurden ohnehin nie zerstört, der Leak lebte nur bis Prozessende); seit
  `Qt::WA_DeleteOnClose` Fenster wirklich zerstört, wird bei jedem
  geschlossenen Fenster real Speicher verloren.
  **Gefixt (2026-09-06):** `delete mEmulation;` in `~KomportView()`
  ergänzt.

**Verifikation:** alle 6 `ctest`-Targets grün (1 neu). Clean-Build mit
`-Wall -Wextra`: weiterhin 0 Warnungen/Fehler. Offscreen-Smoke-Test grün,
echtes `~/.config/Komport-Qt6/`-Profil unangetastet.


### 19.5 Fünfter Full-Review (2026-09-06)

Wieder neuer, unabhängiger Codex-Thread, diesmal explizit mit der Frage,
ob sich das Muster der letzten beiden Runden (vorherige Runde bestätigt
sauber, aber 1-2 neue Findings in bislang wenig fokussierten
Randbereichen) fortsetzt, oder ob jetzt Konvergenz erreicht ist —
inklusive der ausdrücklichen Anweisung, trotz mehrerer sauberer Runden
nicht nachlässiger zu werden. Ergebnis: die Runde-4-Fixes (Scrollback-
Auswahl, `KomportEmulation`-Lifetime) wurden als sauber bestätigt, keine
neuen Kritisch-/Hoch-Funde — aber wieder 4 neue Mittel-Findings, diesmal
in der Emulation, dem Zell-Bounds-Checking und der seriellen
Konfiguration. **Konvergenz ist damit weiterhin nicht erreicht**, das
Muster setzt sich fort — siehe Anmerkung am Ende dieses Abschnitts.

- [x] **CSI-Abschlusserkennung akzeptierte nur Buchstaben als Finalbyte.**
  `komportemulation.cpp` `sequence()` (~Z. 1103): `completed = (_ch>='a'
  && _ch<='z') || (_ch>='A' && _ch<='Z')` — nach ECMA-48/ANSI X3.64 (dem
  Standard, dem VT100/VT102-CSI-Sequenzen folgen) ist das Finalbyte einer
  CSI-Sequenz aber jedes Byte im Bereich `0x40`–`0x7E` (`@` bis `~`), nicht
  nur Buchstaben — z.B. ist `CSI Pn @` (Insert Character) eine echte
  VT102-Sequenz. Traf ein solches Sonderzeichen als Finalbyte ein, blieb
  die Sequenz "offen" und verschluckte das nächste vom Host gesendete
  alphabetische Nutzzeichen als vermeintliches Finalbyte — echte
  Protokoll-Korrektheitslücke, kein reiner Edge-Case.
  **Gefixt (2026-09-06):** `completed` prüft jetzt `_ch >= 0x40 && _ch <=
  0x7E`. Nicht implementierte Finalbytes (z.B. `@` — Insert Character ist
  in dieser Emulation nicht umgesetzt) fallen weiterhin harmlos in den
  bereits vorhandenen `default:`-Zweig (verworfen, geloggt via `qDebug()`)
  statt die Sequenz offen zu lassen. Verifiziert per neuem
  `tst_emulation`-Testfall: `CSI 5 @` gefolgt von `A` — gegen den
  ungefixten Stand reproduzierbar (`A` wurde als Finalbyte verschluckt,
  Cursor bewegte sich statt den Buchstaben zu drucken), gegen den
  gefixten Stand grün.
- [x] **`KomportCellArray::cell(x,y)` prüfte nur den linearen Index, nicht
  `x`/`y` einzeln — Zellzugriffe außerhalb der Spalten-/Zeilengrenzen
  konnten in eine Nachbarzeile "wrappen" statt `nullptr` zu liefern.**
  `komportcellarray.cpp` `cell(int,int)` (~Z. 184): `index =
  arrayWidth()*_y + _x` wurde nur als Ganzes gegen `[0, mCells.count())`
  geprüft. Bei 80 Spalten lieferte z.B. `cell(-1, 1)` (`index = 80*1-1 =
  79`) fälschlich die letzte Zelle von Zeile 0 statt `nullptr`, und
  `cell(80, 0)` (`index = 80*0+80 = 80`) fälschlich Zeile 1, Spalte 0.
  Erreichbar über `KomportView::selectStart()`/`selectEnd()` (~Z. 617/622
  vor dem Fix), die rohe, ungeclampte Maus-Pixelpositionen in
  Zellkoordinaten umrechnen — eine Drag-Selektion knapp außerhalb des
  Textbereichs konnte dadurch falsche Zellen markieren/kopieren.
  **Gefixt (2026-09-06), zwei Ebenen:** `cell(int,int)` validiert jetzt
  `_x`/`_y` einzeln gegen die tatsächlichen Grid-Grenzen, *bevor* der
  flache Index berechnet wird (schützt alle Aufrufer, nicht nur die
  Maus-Selektion). Zusätzlich neue `KomportView::clampToGrid()`-Methode,
  von `selectStart()`/`selectEnd()` genutzt, um Mauspositionen direkt auf
  gültige Zellkoordinaten zu klemmen (verhindert, dass ungültige
  Koordinaten überhaupt erst entstehen). Verifiziert per neuem
  `tst_cellarray`-Testfall: `cell(-1,1)`/`cell(80,0)` — gegen den
  ungefixten Stand reproduzierbar (lieferten fälschlich Nachbarzellen
  statt `nullptr`), gegen den gefixten Stand grün. Kein dedizierter Test
  für `clampToGrid()`/`selectStart()`/`selectEnd()` selbst (`protected`,
  nur über echte `QMouseEvent`-Drag-Simulation testbar) — der
  `cell()`-Fix ist die tiefere, wichtigere Schutzebene und deckt die
  Regression bereits ab, unabhängig davon, wie ungültige Koordinaten
  entstehen.
- [x] **`FlushRate=0` war über den Einstellungsdialog direkt wählbar und
  erzeugte einen dauerfeuernden 0-ms-Timer.** `komportserial.cpp`
  `setFlushRate()` (~Z. 256), `settingsdialog.cpp`
  `FlushRateSpinBox->setRange(0, 4096)` (~Z. 94): der vorherige Fix
  klemmte negative Werte auf `qMax(0, ...)`, aber `0` selbst ist ein
  gültiger, aber unsinniger `QTimer`-Intervall — `QTimer::start(0)`
  feuert bei jedem einzelnen Durchlauf der Event-Loop erneut, ein
  waschechtes Idle-Busy-Polling (`slotFlushRxBuffer()` bricht zwar bei
  leerem Puffer früh ab, aber der Timer-Dispatch selbst läuft trotzdem
  ununterbrochen). Anders als bei den meisten anderen Findings war das
  hier nicht nur über ein hand-editiertes Profil erreichbar, sondern
  direkt über den normalen Einstellungsdialog wählbar.
  **Gefixt (2026-09-06):** `setFlushRate()` klemmt jetzt auf `qMax(1,
  ...)`; `FlushRateSpinBox`s Minimum auf `1` gesetzt, damit der Dialog
  gar nicht erst einen Wert anbietet, den er ohnehin nur stillschweigend
  hochklemmen würde. Neuer Getter `KomportSerial::flushRate()` ergänzt
  (rein für Testbarkeit — `setFlushRate()` hatte vorher keinen
  Rückgabewert und keinen Weg, den geklemmten Wert von außen zu prüfen).
  `tst_serial`s bestehender Test entsprechend erweitert/umbenannt
  (`flushRateClampsToNonNegative` → `flushRateClampsToPositive`) und
  gegen den ungefixten Stand gegengeprüft.
- [x] **`ESC Z` (klassische VT100-Geräteidentifikation) war dokumentiert,
  aber nicht implementiert.** `komportemulation.cpp` `shortEscape()`
  (~Z. 1195 vor dem Fix): der VT102-Referenz-Header dieser Datei
  dokumentiert `ESC Z` selbst als Alternative zu `CSI c`/`CSI 0c` (mit
  dem zeitgenössischen Hinweis "esc Z may not be supported in future") —
  `CSI c`/`CSI 0c` sind über `doDeviceAttributes()` bereits implementiert,
  `ESC Z` fiel aber im `default:`-Zweig von `shortEscape()` durch und tat
  nichts. Hosts, die noch die ältere Identifikationsform senden, bekamen
  keine Antwort.
  **Gefixt (2026-09-06):** `case 'Z': doDeviceAttributes(); break;`
  ergänzt — dieselbe Antwort wie `CSI c`/`CSI 0c`. Bewusst umgesetzt statt
  nur dokumentiert: klein, sicher, passt direkt in den dokumentierten
  VT100/VT102-Scope dieser Emulation (`CLAUDE.md`), und der Referenz-Text
  selbst deutet `ESC Z` nur als möglicherweise *künftig* wegfallend an,
  nicht als bereits obsolet. Verifiziert per neuem `tst_emulation`-Testfall
  (kann die tatsächliche Antwort nicht direkt beobachten, da der
  Serial-Port im Test nie geöffnet ist und `putStr()` auf einem
  geschlossenen Port ein No-op ist — prüft stattdessen, dass `ESC Z` als
  vollständige, erkannte Sequenz behandelt wird und die Emulation danach
  wieder normal auf Eingaben reagiert, statt in einem "wartet auf mehr"
  -Zustand hängen zu bleiben).

**Verifikation:** alle 6 `ctest`-Targets grün (2 erweitert). Clean-Build
mit `-Wall -Wextra`: weiterhin 0 Warnungen/Fehler. Offscreen-Smoke-Test
grün, echtes `~/.config/Komport-Qt6/`-Profil unangetastet.

**Anmerkung zur Konvergenz:** Nach jetzt 5 Runden ist das wiederkehrende
Muster (vorherige Runde bestätigt sauber, 1-2-4 neue Mittel-Findings in
Randbereichen) noch nicht abgerissen. Alle Findings seit Runde 1 waren
aber Mittel oder niedriger (keine neuen Kritisch/Hoch seit Runde 1), und
jede Runde bestätigt explizit, dass die *vorherige* Runde sauber war —
d.h. die Fixes selbst sind stabil, nur die Review-Abdeckung selbst
erweitert sich noch mit jeder Runde auf neue Codebereiche. Das ist beim
nächsten Review-Lauf im Auge zu behalten: wenn sich das Muster fortsetzt,
könnte statt weiterer Einzel-Runden eine gezielte, vollständige manuelle
Durchsicht der noch nie im Fokus gestandenen Bereiche
(`komporthexview.cpp`, `komportsessionlogger.cpp`, `komportmacrobar.cpp`,
`komportfilescrollbuffer.cpp`, `komportminimap.cpp`) sinnvoller sein als
ein weiterer vollautomatischer Review-Durchlauf.


### 19.6 Gezielte manuelle Durchsicht der bislang unfokussierten Bereiche (2026-09-06)

Wie in der Anmerkung zu Abschnitt 19.5 vorgeschlagen: statt einer weiteren
automatisierten Full-Review-Runde eine gezielte manuelle Durchsicht der
Dateien, die in keiner der 5 bisherigen Runden im Fokus standen —
`komporthexview.{h,cpp}`, `komportsessionlogger.{h,cpp}`,
`komportmacrobar.{h,cpp}`, `komportfilescrollbuffer.{h,cpp}`,
`komportminimap.{h,cpp}`, dazu `komportdoc.{h,cpp}`, `komportcell.{h,cpp}`,
`komportscript.h`, `settingsdialog.h` und ein systematischer Scan über
alle nicht-geparenteten `new`-Allokationen im gesamten Projekt (auf der
Suche nach weiteren Lecks wie dem `KomportEmulation`-Fund aus
Abschnitt 19.4).

**Ergebnis: deutlich ruhiger als die letzten 5 automatisierten Runden —
nur ein Fund, und der ist rein defensiv (kein aktueller Reproduktionspfad),
anders als alle bisherigen Findings.** Ein gutes Konvergenz-Signal.

- [x] **`KomportCell::copy(KomportCell* _other)` prüfte `_other` nicht auf
  `nullptr`.** `komportcell.cpp` `copy()` (~Z. 75): griff direkt auf
  `_other->select()`/`_other->character()` etc. zu. Die beiden aktuellen
  Aufrufstellen (`KomportView::slotAboutToScrollUp()`/`resizeGridRows()`,
  jeweils beim Verschieben einer Zeile in den Scroll-Buffer) übergeben
  nach Prüfung **aktuell nie** einen Null-Pointer — ihre Schleifengrenzen
  bleiben innerhalb der tatsächlichen `cellArray()`-Größe. Kein konkreter
  Reproduktionspfad wie bei allen bisherigen Findings, aber `cell()`
  (siehe Abschnitt 19.5) liefert für jede außerhalb liegende Koordinate
  `nullptr` zurück — `copy()` selbst hatte dagegen keine eigene
  Absicherung, wäre also nur einen Aufruf von einem Crash entfernt, sollte
  sich das je ändern.
  **Gefixt (2026-09-06):** `if (!_other) return;` ergänzt, passend zum
  bereits etablierten Verteidigungsmuster dieses Projekts (`cell()`
  selbst, `setArraySize()`s Clamps, `getDocument()`s `qFatal()`, ...).
  Verifiziert per neuem `tst_cellarray`-Testfall: gegen den ungefixten
  Stand reproduzierbar (Absturz), gegen den gefixten Stand grün.
- Sonst nichts Nennenswertes: `komporthexview`, `komportsessionlogger`,
  `komportmacrobar`, `komportminimap` — sauber, korrekt geparentet, keine
  Bounds-/Lifetime-Probleme gefunden. `komportfilescrollbuffer` bleibt
  bewusster, nirgends instanziierter Stub (bereits in `CLAUDE.md`/oben in TODO.md
  TODO.md Abschnitt 6 dokumentiert). Kein weiteres un-geparentetes `new` ohne
  zugehöriges `delete` gefunden — der `KomportEmulation`-Leak aus
  Abschnitt 19.4 war der einzige.
- **Nebenbefund, kein Fix:** `komportdoc.cpp` `saveModified()`s gesamter
  "Datei geändert?"-Zweig ist seit dem `newDocument()`-Fix (Abschnitt 19.1)
  faktisch unerreichbarer Code, da nichts mehr `setModified(true)`
  aufruft. Harmlos (keine Fehlfunktion), aber erwähnenswert für eine
  künftige Aufräumrunde — bewusst nicht angefasst, da kein Bug, nur totes
  Gerüst.

**Verifikation:** alle 6 `ctest`-Targets grün (1 neu). Clean-Build mit
`-Wall -Wextra`: weiterhin 0 Warnungen/Fehler. Offscreen-Smoke-Test grün,
echtes `~/.config/Komport-Qt6/`-Profil unangetastet.


### 19.7 Sechster Full-Review (2026-09-06)

Wieder neuer, unabhängiger Codex-Thread, diesmal mit der Frage, ob nach
der gezielten manuellen Ergänzungsrunde (Abschnitt 19.6) tatsächlich
Konvergenz erreicht ist. Ergebnis: **keine Kritisch-/Hoch-Funde**, aber 2
neue Mittel-Findings — beide von Codex selbst ausdrücklich als **keine
Regression** einer vorherigen Fix-Runde eingeordnet, sondern als
bestehende Alt-/Feature-Fehler, die erst durch die Minimap-/History-API
bzw. den Copy-Pfad sichtbar werden. Konvergenz damit weiterhin nicht
vollständig erreicht, aber die Art der Findings hat sich sichtbar
verschoben: keine Regressionen mehr, nur noch unabhängige Alt-Bugs.

- [x] **`cellAtHistoryRow()` adressierte den Scrollback um eine Zeile
  falsch — übersprang die tatsächlich älteste Zeile und lieferte am
  neuen Ende stattdessen eine dauerhaft leere Zeile.**
  `komportview.cpp` `cellAtHistoryRow()` (~Z. 327): nutzte `(mScrollBuffer
  .arrayHeight() - depth) + _row`, während `getCell()` (die bereits
  mehrfach verifizierte, korrekte Referenz) `(arrayHeight()-1-scrolled)
  +_y` nutzt — eine fehlende `-1`. Durch Nachrechnen bestätigt (und exakt
  mit `KomportView::slotAboutToScrollUp()`s Kopier-Reihenfolge
  hergeleitet): nach `N` Scroll-Ereignissen sitzt die zuletzt
  hinzugekommene Zeile immer bei `arrayHeight()-2`, nicht `-1` (die
  frisch angehängte, nie beschriebene letzte Zeile) — `cellAtHistoryRow()`
  hat diese Verschiebung nicht mitgemacht. Sichtbar direkt in der
  Minimap-Silhouette und deren Hover-Vorschau, die History-Zeilen
  ausschließlich über diese Funktion lesen. Kein durch vorherige Fixes
  verursachtes Problem, sondern ein eigenständiger Alt-Fehler in der in
  Runde 4 eingeführten `cellAtHistoryRow()`-API selbst (diese exakte
  Formel-Diskrepanz war mir bei meiner eigenen manuellen Durchsicht in
  Abschnitt 19.6 bereits aufgefallen, ich hatte sie dort aber nicht zu
  Ende verfolgt — von Codex jetzt konkret bestätigt und lokalisiert).
  **Gefixt (2026-09-06):** zusätzliches `-1` ergänzt:
  `(mScrollBuffer.arrayHeight() - depth - 1) + _row`. Verifiziert per
  neuem `tst_selection`-Testfall: bei komplett zurückgescrolltem
  Scrollbar (`value=0`) muss `cellAtHistoryRow(x, row)` für jede Zeile
  `row < depth` exakt denselben Inhalt liefern wie `getCell(x, row)` (bei
  `value=0` bezeichnen View-relatives `_y` und absolutes `_row` dieselbe
  Bildschirmposition) — gegen den ungefixten Stand reproduzierbar (`'A'`
  statt erwartetem `' '`), gegen den gefixten Stand grün.
- [x] **Copy (Ctrl+C/Edit-Menü) hing ausschließlich am X11-
  Selection-Clipboard und kopierte auf anderen Plattformen still gar
  nichts.** `komport.cpp` `slotEditCopy()` (~Z. 1020): las
  `QClipboard::Selection` (die X11 "primary selection", auch bekannt vom
  Mittelklick-Einfügen) und schrieb den Inhalt nach `QClipboard::
  Clipboard`. `KomportView::select(..., clip=true)` (~Z. 585) schrieb nur
  nach `QClipboard::Selection`. Auf Plattformen ohne Selection-Clipboard-
  Unterstützung (Wayland ohne das Primary-Selection-Protokoll, Windows,
  macOS, und — wie in dieser Session mehrfach direkt beobachtet — die für
  die eigene Testsuite verwendete `offscreen`-QPA-Plattform) blieb Copy
  damit funktionslos, obwohl `hasSelection()` `true` war und Zellen
  sichtbar markiert waren. Kein durch vorherige Fixes verursachtes
  Problem, sondern bestehendes Design aus dem ursprünglichen
  Auswahl-Pfad — bei der Portierung von KDE3 (wo dasselbe X11-spezifische
  Verhalten unauffällig war, da Zielplattform ohnehin X11) unverändert
  übernommen, aber ein echter Portabilitäts-Rückschritt für das
  Qt6-Ziel dieses Projekts (`CLAUDE.md`: "reines Qt6-Programm").
  **Gefixt (2026-09-06):** neues `KomportView`-Member `mSelectedText` plus
  öffentlicher Getter `selectedText()` — `select(..., clip=true)` befüllt
  es zusätzlich zum weiterhin bestehenden `QClipboard::Selection`-Schreiben
  (das bleibt als Bonus für Mittelklick-Einfügen auf Plattformen, die es
  unterstützen), `deselect()` leert es. `slotEditCopy()` liest jetzt
  `view->selectedText()` direkt statt über `QClipboard::Selection` zu
  gehen — funktioniert damit plattformunabhängig. Verifiziert per
  erweitertem `tst_selection`-Testfall: `selectedText()` nach `select()`
  exakt geprüft (nicht mehr nur indirekt über Zell-Flags wie in Runde 4,
  weil jetzt eine plattformunabhängige Prüfmöglichkeit existiert), sowie
  dass `deselect()` es wieder leert.

- [x] **Scrollback-Puffer konnte nach einem Profilwechsel/Settings-Resize
  auf eine Tiefe landen, die exakt der neuen Kapazität entsprach —
  `getCell()`/`cellAtHistoryRow()` griffen dann mit einem
  Out-of-Range-Index auf die älteste Zeile zu und lieferten `nullptr`
  statt des tatsächlich noch vorhandenen Inhalts.**
  `KomportScrollBuffer::scrollUp()`/`setArraySize()` (und die gespiegelte,
  aktuell ungenutzte `KomportFileScrollBuffer`-Fassung) deckelten
  `mDepth` bislang auf `arrayHeight()` statt `arrayHeight()-1` — das
  überstieg die tatsächlich nutzbare Historie um genau eine Zeile (siehe
  `scrollUp()`-Kommentar: die letzte Zeile ist nach jedem Scroll-Schritt
  immer frisch leer, es gibt nie mehr als `arrayHeight()-1` Zeilen mit
  echtem Inhalt). **Wichtige Präzisierung gegenüber der ursprünglichen
  Einschätzung:** über fortlaufendes Scrollen allein ist dieser Zustand
  gar nicht erreichbar — `KomportCellArray::scrollUp()` (Basisklasse)
  schrumpft das Array um eine Zeile und vergrößert es sofort wieder, und
  beide `setArraySize()`-Aufrufe dispatchen virtuell zurück in die
  überschriebene Fassung, die `mDepth` dabei zweimal pro Scroll-Schritt
  neu deckelt — dadurch bleibt `mDepth` beim fortlaufenden Scrollen immer
  bei `arrayHeight()-1` hängen, unabhängig davon, welche Konstante hier
  stand. Real erreichbar ist der Fehlerzustand über einen *direkten*,
  ungepaarten `setArraySize()`-Aufruf — z.B. `KomportView::
  setScrollBuffer()` bei einem Profilwechsel: trifft die neue,
  konfigurierte Puffergröße exakt die aktuelle Tiefe, ließ die alte
  `> arrayHeight()`-Prüfung `mDepth` unverändert (weil `mDepth` bereits
  gleich der neuen `arrayHeight()` war, die Prüfung also falsch war) —
  `depth() == arrayHeight()` exakt, der eine Zustand, den die
  Index-Arithmetik nicht abbilden kann. Von Codex als eigenständiger
  Alt-Fehler eingestuft, keine Regression einer vorherigen Fix-Runde.
  **Gefixt (2026-09-06):** Deckel in beiden Klassen (`scrollUp()` und
  `setArraySize()`) auf `arrayHeight()-1` geändert. Verifiziert per neuem
  `tst_selection`-Testfall (`oldestRowStillReadableOnceScrollBufferIsSaturated`):
  sättigt den Puffer durch Scrollen, ruft dann `setScrollBuffer()` mit der
  aktuellen Tiefe als neuer Größe auf (simuliert den Profilwechsel-Pfad)
  und prüft, dass `cellAtHistoryRow(0,0)`/`getCell(0,0)` danach nicht
  `nullptr` liefern — gegen den ungefixten Stand reproduzierbar
  (`nullptr`), gegen den gefixten Stand grün. Die erste Testfassung prüfte
  nur reines Dauerscrollen und schlug dadurch nie fehl, unabhängig vom
  Fix-Stand — beim Nachrechnen des self-correcting Shrink/Grow-Verhaltens
  von `KomportCellArray::scrollUp()` aufgefallen und auf den echten,
  profilwechsel-basierten Auslöser umgestellt.
- [x] **`select()` konnte die soeben gesetzten Zell-Selektions-Flags
  (und den kopierten Text) im selben Aufruf sofort wieder verlieren, wenn
  es außerhalb eines Maus-Drags aufgerufen wurde** — z.B. direkt aus
  einem Test, oder von einem künftigen, nicht-Maus-getriebenen Aufrufer.
  Beim Schreiben des `selectedText()`-Regressionstests oben aufgefallen:
  `select(..., clip=true)` schreibt am Ende auch nach
  `QApplication::clipboard()->setText(str, QClipboard::Selection)` —
  das kann synchron (gleicher Thread, Direktverbindung) `QClipboard::
  selectionChanged()` auslösen, worauf `KomportView::
  slotSelectionChanged()` reagiert und `deselect()` aufruft, *außer*
  `mInSelection` ist gerade `true`. Im echten Maus-Pfad ist das immer der
  Fall (`mouseReleaseEvent()` setzt `mInSelection=true` schon in
  `mousePressEvent()` und löscht es erst *nach* dem `select(...,true)`-
  Aufruf), weshalb dieser Rückkopplungspfad über die UI nie sichtbar
  wurde — ein direkter `select()`-Aufruf ohne diesen Kontext (wie im
  neuen Test) triggerte ihn aber sofort und löschte alle gerade gesetzten
  Zell-Flags, noch bevor `select()` zurückkehrte. Kein Codex-Finding,
  sondern beim eigenen Testschreiben in dieser Runde entdeckt.
  **Gefixt (2026-09-06):** `select()` setzt `mInSelection` für die Dauer
  des `clipboard()->setText()`-Aufrufs defensiv selbst auf `true` (und
  stellt den vorherigen Wert danach wieder her) — macht `select()` als
  eigenständigen Aufruf sicher, statt sich stillschweigend auf die
  Aufrufreihenfolge in `mouseReleaseEvent()` zu verlassen. Verifiziert:
  `selectWhileScrolledBackReadsScrollBufferNotLiveGrid` reproduzierbar
  rot gegen den ungefixten Stand (Zell-Flags blieben `false`), grün nach
  dem Fix.

**Verifikation:** alle 6 `ctest`-Targets grün (3 erweitert/neu). Clean-Build
mit `-Wall -Wextra`: weiterhin 0 neue Warnungen/Fehler (die eine
vorbestehende `-Wsfinae-incomplete=`-Warnung aus `komportview.h` in einer
MOC-generierten Datei ist nachweislich unabhängig von dieser Runde — per
Vergleichs-Build auf unverändertem `HEAD` bestätigt). Offscreen-Smoke-Test
grün, echtes `~/.config/Komport-Qt6/`-Profil unangetastet (md5-Vergleich
vor/nach identisch). Ein Codex-Review auf dem finalen Diff fand **keine
bestätigten Bugs** (explizit gegen den `arrayHeight()-1`-Deckel getestet:
leerer Puffer, Höhe 1, exakte Kapazität, Wachsen/Schrumpfen,
`setScrollBuffer(depth)`-Resize — kein erreichbarer Out-of-Range-Zustand
gefunden). Drei unbestätigte Low-Severity-Hypothesen zum
`mInSelection`-Guard (kein RAII, schützt nur vor der eigenen
`slotSelectionChanged()`, setzt synchrone Signal-Zustellung voraus) sowie
ein Kommentar-Nit im neuen Test wurden bewusst nicht als defensive Fixes
übernommen — ohne Reproduktionspfad und laut Codex selbst bei aktueller
Verdrahtung "moot"/"unconfirmed"; der Kommentar-Nit (irreführender
4096-Bezug) wurde korrigiert.


## 20. Meilenstein 4 — VT220/xterm-Erweiterungen + Review-Zyklus (2026-09-08)

Nach Abschluss des Architektur-Gates (Abschnitt 1, `ADR-001` auf `Rejected`)
direkt Meilenstein 4 umgesetzt: 256-Farben-SGR (`CSI 38;5;N`/`48;5;N`,
xterm-Palette: 16 Basisfarben + 6x6x6-Würfel + 24-stufige Graurampe),
Scroll-Regionen (DECSTBM, `CSI Pt;Pb r`, inkl. neuer `KomportCellArray::
scrollUpRegion()`/`scrollDownRegion()`), Insert-Mode (`CSI 4h`/`4l`) und
VT220-Geräte-Identifikation (`CSI c`/`CSI 0c`/`ESC Z` antworten jetzt
`\x1b[?62c` statt `\x1b[?6c`). Anders als bei den vorherigen Review-Runden
(Abschnitt 19.1–19.7, die den *bestehenden* Code durchleuchtet haben) hier:
das Feature direkt gegen mehrere Codex-Adversarial-Review-Runden auf dem
*entstehenden* Diff entwickelt (`/codex:adversarial-review`, `--background`
mit `AskUserQuestion`-Bestätigung vor jedem Lauf), zusätzlich mehrfach über
`hermes-gemma-review` (Gemma 4, lokal über `hermes`) gegengeprüft — siehe
Stolperstein unten zur Codex-Infra-Instabilität während dieser Runde.

**Runde 1+2** (zwei separate `codex:codex-rescue`-Läufe auf dem ersten
Entwurf, letztere nach Nachbesserung): 5 Findings, alle bestätigt und
gefixt:
- `doIndex()`/`doReverseIndex()` scrollten fälschlich auch, wenn der Cursor
  außerhalb der Region stand (`>=`/`<=` statt exaktem `==` am Rand).
- `doGraphics()`s `38;5;N`/`38;2;r;g;b`-Sub-Parameter-Parsing rückte bei
  unvollständigen Sequenzen den Feld-Index nicht vor — ein übrig gebliebenes
  Feld wurde als eigenständiger SGR-Code fehlinterpretiert (z.B. Blink).
- `doSetScrollRegion()` behandelte `0` nicht wie `ctlParam()`s
  Default-Konvention — `CSI 0;0 r` setzte die Region fälschlich nicht
  zurück.
- `scrollUpRegion()`/`scrollDownRegion()` konnten bei `arrayHeight()==0`
  über `clearRow()` einen Null-Pointer-Crash auslösen (aktuell über die
  echte UI nicht erreichbar, aber öffentliche API ohne diese Garantie).
- `CSI 38;5;N` mit leerem/nicht-numerischem Index wandte stillschweigend
  Farbindex 0 (Schwarz) an statt die Farbe unverändert zu lassen
  (`toInt()` statt `toInt(&ok)`, inkonsistent zu `ctlParam()`s Muster).

**Runde 3** (`/codex:adversarial-review`): 2 weitere Findings, beide
gefixt:
- Zeichendruck am rechten Rand (`KomportCellArray::advanceCursor()`) kannte
  keine Scroll-Region — Auto-Wrap konnte die Region verlassen bzw. bei
  `top>0` fälschlich in den Scrollback-Puffer scrollen. Gefixt durch neue
  `KomportEmulation::advanceCursorWithWrap()`, die den Y-Schritt an
  `doIndex()` delegiert (identische Region-Logik wie bei LF/Index).
- `doInsertLine()`/`doDeleteLine()` (`CSI L`/`M`) stammen aus der Zeit vor
  DECSTBM und arbeiteten immer auf dem ganzen Bildschirm — ein Programm mit
  aktiver Scroll-Region hätte Zeilen unterhalb der Region verschoben/
  gelöscht. Jetzt auf `scrollTop()`/`scrollBottom()` begrenzt, No-Op bei
  Cursor außerhalb der Region.

**Runde 4**: 1 Finding gefixt, 1 bewusste Produktentscheidung:
- **Gefixt:** `doSetScrollRegion()` speicherte bei implizitem
  Vollbild-Reset (`CSI r`/`CSI 0;0 r`) die *aktuelle* Bildschirmhöhe als
  festen Wert statt den dynamischen `INT_MAX`-Sentinel — nach einem
  späteren Fenster-Resize (Wachsen) blieb die untere Marge auf der alten,
  jetzt zu kleinen Zeile hängen, wodurch Scrollen an der *neuen* letzten
  Zeile dauerhaft ausblieb. Hoch eingestuft, da über einen ganz normalen
  Fenster-Resize nach jedem expliziten Region-Reset erreichbar.
- **Bewusst nicht geändert:** die neue VT220-Kennung (`\x1b[?62c`) wurde
  von Codex kritisiert, da nicht alle VT220-Fähigkeiten implementiert
  sind. War aber ein expliziter Punkt aus diesem Meilenstein (s.o.), kein
  Alleingang — Kommentar im Code dokumentiert bereits bewusst, dass keine
  Feature-Codes mitgeclaimt werden. Nutzerentscheidung: wie spezifiziert
  belassen.

**Runde 5** (nach mehreren an einem Codex-Infra-Problem gescheiterten
Versuchen, s.u., schließlich erfolgreich): 1 weiteres Finding, bewusst
**nicht gefixt, sondern dokumentiert** (siehe TODO.md Abschnitt 6): DECOM/
Origin-Mode (`CSI ?6h/l`) ist nicht implementiert — Cursor-Adressierung
bleibt immer physisch-bildschirmbezogen, auch bei aktivem Origin-Mode.
War schon vor diesem Meilenstein in `doSetMode()` bewusst ignoriert
(Kommentar dort), wird aber erst durch aktive Scroll-Regionen überhaupt
beobachtbar. Nutzerentscheidung: als bekannte Lücke dokumentiert statt
implementiert — moderne Curses-Programme (vim, nano, htop) adressieren
üblicherweise absolut statt sich auf Origin-Mode zu verlassen, reale
Praxisrelevanz gering.

**Stolperstein — Codex-Infra-Instabilität:** über den Verlauf dieser Runde
schlugen mehrere `/codex:adversarial-review`-Läufe mit identischem Fehler
fehl (`unexpected status 404 Not Found: The model \`gpt-5.5\` does not
exist or you do not have access to it.`) — ein Modell-Konfigurationsproblem
auf Codex-Seite, intermittierend (in Summe lief etwa jeder zweite bis
dritte Versuch durch). Nicht durch Code-Änderungen lösbar; als
`hermes-gemma-review` (Gemma 4 über `hermes`) genutzt, um in der
Zwischenzeit trotzdem eine unabhängige zweite Meinung zu bekommen (dreimal
"PASS"/"Approved", einmal mit einem falsch-negativen Hinweis, `doSetScrollRegion()`
sei im Diff nicht sichtbar — Artefakt der begrenzten Diff-Sicht, Methode
war vorhanden und getestet).

**Verifikation (gesamt):** alle 6 `ctest`-Targets grün (`tst_emulation`
und `tst_cellarray` erweitert, u.a. `printableAutoWrapRespectsScrollRegion`,
`insertAndDeleteLineRespectScrollRegion`, `fullScreenResetSurvivesLaterResize`,
`scrollRegionHelpersDoNotCrashOnZeroHeightArray`). Jeder der zehn Fixes
einzeln per gezieltem Revert-und-Wiederherstellen (statt Git-Stash — ein
`git stash push --keep-index` hatte in dieser Runde einmal versehentlich
zu weit zurückgesetzt, per `git checkout <pfad>` aus dem Index sauber
wiederhergestellt, siehe Session-Historie) rot/grün verifiziert. Clean-Build
mit `-Wall -Wextra`: 0 neue Warnungen. Offscreen-Smoke-Test grün, echtes
`~/.config/Komport-Qt6/`-Profil unangetastet (md5 identisch vor/nach).

- [x] Produktvision festhalten und bei kuenftigen Features gegenpruefen: Komport-Qt6
  soll ein spezialisiertes serielles Werkstatt-Terminal werden, nicht noch ein
  allgemeines Shell-Terminal. Kernnutzen: die schwierigen Faelle, fuer die man
  sonst mehrere halb passende Tools sucht: Netzwerk-/Industriegeraete, alte
  Rechner, ungewoehnliche Zeilenenden, rohe Steuerzeichen, Hex-Diagnose,
  Mitschnitte, Makros, Profile und Retro-/Industrie-Zeichensaetze.
  Festgehalten im Abschnitt "Produktvision / Daseinsberechtigung" in
  `CLAUDE.md`.
- [x] Terminal-Engine-ADR vor Meilenstein 4 finalisieren: eigene Emulation weiter
  ausbauen vs. `QTermWidget` als optionales Backend. Entscheidungsfrage war
  nicht "koennen wir etwas wiederverwenden?", sondern ob Wiederverwendung die
  seriellen Spezialfeatures einfacher, stabiler und wartbarer macht.
  **Ergebnis (2026-09-07): `ADR-001` auf `Rejected` entschieden — bei der
  eigenen Emulation bleiben.** Der Spike (siehe nächster Punkt) hat gezeigt,
  dass ein `QTermWidget`-Backend technisch ginge, aber der Nutzer hat sich
  bewusst dagegen entschieden: die eigene Emulation ist nach sechs
  Review-Runden (Abschnitt 19.1–19.7) bereits ausgereift und exakt auf die
  seriellen Spezialfälle dieses Projekts zugeschnitten; ein Backend-Wechsel
  wäre ein großer, riskanter Eingriff für Vorteile (fertige Farbschemata,
  geschenktes Copy/Paste), die sich günstiger direkt in der bestehenden
  Architektur nachbauen lassen (Meilenstein 5 plant ohnehin einen
  Appearance-Tab). Details/Begründung in
  `docs/architecture-decisions/ADR-001-terminal-engine-strategy.md`
  ("Spike Results"/"Final decision"). Meilenstein 4 baut damit direkt auf
  `komportemulation.cpp` weiter, kein Backend-Wechsel nötig.
- [x] Kleinen Spike fuer `QTermWidget` planen, bevor VT220/xterm-Komfort tief in
  die eigene Emulation eingebaut wird: RX/TX-Bridge, Tastatureingaben,
  Scrollback, Farbschemata, Cursor, Copy/Paste und Performance pruefen. Die
  Zeichensatz-Uebersetzung muss dabei weiterhin vor der Terminal-Interpretation
  auf Byte-Ebene moeglich bleiben.
  **Durchgeführt (2026-09-07):** eigenständiges, per `-DKOMPORT_BUILD_SPIKES=ON`
  optional baubares CMake-Target `spike/qtermwidget-bridge/` (Paket
  `qtermwidget-devel` 2.4.0, Qt6-Build von `QTermWidget`). Bridged einen
  `QSerialPort` mit einem `QTermWidget` im `startTerminalTeletype()`-Modus
  (kein Shell-Kindprozess) über dessen internen Pty-Slave-Fd
  (`getPtySlaveFd()`) und das `sendData()`-Signal, mit Stub-Hookpunkten an
  exakt der Stelle, die `CLAUDE.md`s Datenfluss-Vorgabe verlangt
  (Diagnose/Zeichensatz-Übersetzung vor bzw. nach dem Terminal-Backend).
  Verifiziert end-to-end gegen ein echtes `socat`-Pty-Paar (derselbe
  Trick wie in mehreren bestehenden `tests/tst_*.cpp`): RX-Bytes an die
  simulierte Gegenstelle geschickt landeten korrekt im Bildschirmbild
  (`selectedText()` geprüft); von der Emulation verarbeiteter Text kam
  korrekt an der simulierten Gegenstelle an. 14 Farbschemata und
  konfigurierbare Scrollback-Größe sofort nutzbar, Copy/Paste
  funktioniert ohne Zusatzcode. Ein bislang unbekannter Stolperstein
  gefunden: neue `connect(sender, &Class::signal, ...)`-Syntax schlägt zur
  Laufzeit ("signal not found") speziell bei `QTermWidget`s eigenen
  Signalen gegen dieses Distro-Paket fehl, alte `SIGNAL()/SLOT()`-Syntax
  funktioniert. Font-Rendering, Cursor-Optik, Maus-Drag-Auswahl und echte
  Performance unter Last blieben in dieser Headless-Sandbox ungeprüft.
  Vollständiger Befund in `spike/qtermwidget-bridge/README.md`, in
  `ADR-001` übernommen. Haupt-Build/Tests bewusst unberührt (Spike-Target
  nur mit explizitem CMake-Flag gebaut, `qtermwidget-devel` keine neue
  Pflicht-Abhängigkeit). **Nicht durch das Review-Gate aus Abschnitt 0
  gelaufen** (anders als jeder Commit dort) — bewusst ausgelassen, auf
  Nutzernachfrage bestätigt: reiner, standardmäßig nicht gebauter
  Wegwerf-Spike ohne Berührung des Kern-Codepfads, kein Kernfeature. Falls
  `spike/qtermwidget-bridge/` später doch als Ausgangspunkt für echte
  Arbeit dient (z.B. bei einer künftigen Neubewertung von `ADR-001`),
  sollte es dann alsbald nachgeholt werden.
- [x] `KonsolePart` nur als dokumentierte Gegenprobe aufnehmen, nicht als
  bevorzugten Pfad: zu viele KDE-/KF6-Abhaengigkeiten und PTY-/Shell-Fokus fuer
  ein schlankes Qt6-Serial-Tool. Bereits im "Decision"-Abschnitt von
  `ADR-001` dokumentiert; durch den Spike unverändert (wurde bewusst nicht
  prototypisiert, da explizit die nicht bevorzugte Vergleichsoption).


## 21. Meilenstein 5 — Appearance-Tab (Schrift/Farbschemata) + Review-Zyklus (2026-09-08)

Umgesetzt: konfigurierbare Terminal-Schrift (Familie/Größe/Laufweite) und
Vordergrund-/Hintergrundfarben statt der bisherigen, aus `QApplication::
palette()` abgeleiteten festen Default-Farben. Kern der Änderung ist eine
neue **Farb-Herkunfts-Verfolgung** (`mForegroundIsDefault`/
`mBackgroundIsDefault` auf `KomportCell`/`KomportCellArray`): ein
Farbschema-Wechsel darf nur Zellen umfärben, die tatsächlich noch beim
Default stehen, nicht solche, die zufällig explizit auf eine Farbe gesetzt
wurden, die mit dem *alten* Default numerisch übereinstimmt (z.B. bei
"Green on Black": SGR 40 Schwarz == Default-Hintergrund). Neu:
`KomportView::setDefaultColors()`/`setTerminalFont()` (einziger Eintrittspunkt
von außen, hält `mCellArray` und `mScrollBuffer` synchron), `saveSettings()`/
`loadSettings()` (Persistenz unter `Profiles/<Name>/Appearance`), sowie ein
neuer Appearance-Tab in `SettingsDialog` (Schriftauswahl, Laufweiten-Spinbox,
Farbschema-Presets "Breeze Light/Dark", "Green on Black", "Black on Light
Yellow", Custom-Farbwahl). Ab dieser Runde: Gemma 4 (`hermes-gemma-review`)
lief vor **jeder** Codex-Runde als erstes Gate (Nutzervorgabe), Codex selbst
über `codex-companion.mjs task --model gpt-5.6-sol` (Standardmodell `gpt-5.5`
zu dieser Zeit intermittierend mit 404-Fehlern, s. Abschnitt 20).

**Runde 1** (Gemma 4: Pass; Codex `gpt-5.6-sol`): 5 Findings, 4 gefixt:
- Farb-Umfärbung erkannte "noch beim Default" per Farbwert-Gleichheit statt
  Herkunft — bei einer Kollision (z.B. "Green on Black") wurden explizit
  gesetzte Farben fälschlich mit umgefärbt. Gefixt durch die oben genannte
  Herkunfts-Verfolgung.
- Farbschema-Wechsel färbte nur `mCellArray` um, nicht `mScrollBuffer`
  (eigene, zweite `KomportCellArray`-Instanz) — Scrollback zeigte nach einem
  Wechsel weiter das alte Schema. Gefixt durch `KomportView::setDefaultColors()`
  als einzigen Eintrittspunkt für beide Arrays.
- `loadSettings()`s Fallback bei fehlender "Appearance"-Gruppe (Alt-/
  Built-in-Profile) war "aktuell gesetzter Wert" — dadurch leakte das
  Erscheinungsbild eines zuvor geladenen Profils nicht-deterministisch in
  ein anderes, das gar keins hatte. Gefixt: fester Baseline-Fallback
  (`QFontDatabase::systemFont(FixedFont)`, `QApplication::palette()`-Farben).
- Laufweiten-Kontrolle fehlte trotz TODO-Vorgabe ("Spacing"-Regler) im
  ersten Entwurf des Appearance-Tabs — ergänzt (`FontSpacingSpinBox`, 50–300%,
  `QFont::setLetterSpacing(PercentageSpacing, ...)`).
- **Bewusst nicht gefixt, dokumentiert:** Schriftgrößen-Wechsel verschiebt
  die Fensterbreite (Fixed-Width-Design, architektonische Grenze, kein
  Meilenstein-5-Regression) — siehe TODO.md Abschnitt 6.2.

**Runde 2** (Gemma 4 auf dem Diff nach Runde 1: Pass, keine Blocker; Codex
`gpt-5.6-sol`): 5 Findings, 4 gefixt:
- **[Hoch]** Die gewählte Schrift wurde nie tatsächlich zum Zeichnen benutzt:
  `paintCell()` ging von `_paint->font()` aus, aber jeder reale Aufrufer
  zeichnet in `mPixmap` (ein `QPixmap`, das keine eigene Schrift trägt) — der
  Painter startete also immer beim Anwendungs-Default. `setTerminalFont()`
  änderte dadurch nur die Zellgeometrie (`fontMetrics()`), nie die
  tatsächlich gezeichneten Glyphen. Gefixt: `paintCell()` geht jetzt von
  `this->font()` aus.
- **[Mittel]** Ein Farbwechsel bei bereits aktivem Scrollback machte die
  Historie nicht sofort sichtbar neu: `setDefaultColors()` färbte zuerst
  `mCellArray` um (löst den einzigen verbundenen Repaint aus, der beim
  Scrollen aus `mScrollBuffer` liest — noch mit alten Farben), erst danach
  `mScrollBuffer` (dessen eigene Signale mit nichts verbunden sind). Gefixt
  durch Vertauschen der Reihenfolge (`mScrollBuffer` zuerst).
- **[Mittel]** `KomportCellArray::drawChar()` übersprang `setCellAttributes()`
  (Farben **und** deren Herkunfts-Flag) komplett, wenn sich das gezeichnete
  Zeichen nicht änderte — ein Host, der Cursor zurücksetzt, Farbe ändert und
  dasselbe Zeichen erneut schreibt (z.B. Statuszeilen-Muster), bekam weder
  die neue Farbe noch die korrekte Herkunfts-Markierung aus Runde 1. Gefixt:
  Attribute werden jetzt bei jedem `drawChar()`-Aufruf angewendet, unabhängig
  davon, ob sich das Zeichen ändert (entspricht echtem Terminal-Verhalten).
- **[Niedrig]** `tst_appearance.cpp`s Profil-Rundlauf-Test testete nicht den
  echten `KomportApp::saveProfile()`-Pfad, sondern rief `KomportView::
  saveSettings()` direkt auf (`saveProfile()` ist `protected`, UI-only) — der
  Datei-Kopfkommentar hatte das fälschlich als vollständige Integration
  dargestellt. Kommentar korrigiert, Lücke (die Ein-Zeilen-Integration in
  `komport.cpp` muss beim Review von Auge geprüft werden) offen dokumentiert.
- **Erneut aufgeworfen, keine neue Aktion:** die Fensterbreiten-Verschiebung
  bei Schriftgrößen-Wechsel (s.o., Runde 1) — bereits als bewusste, dokumentierte
  Grenze bestätigt (TODO.md Abschnitt 6.2), keine Regression dieser Runde.

**Testbarkeit:** `KomportView` gewährt `tests/tst_appearance.cpp` jetzt per
`friend class TstAppearance` Zugriff auf das (bewusst weiterhin `protected`
bleibende, da überschreibbare Rendering-Primitive) `paintCell()` sowie auf
`mPixmap` — damit lässt sich der tatsächlich verwendete Painter-Font bzw. das
tatsächlich gezeichnete Pixel deterministisch prüfen, ohne auf fragile
Font-Rendering-/Pixel-Vergleiche über mehrere Systeme hinweg angewiesen zu
sein. Neue Tests (`tst_appearance.cpp`, jetzt 7 Testfunktionen):
`paintCellUsesTerminalFontNotPainterDefault`,
`colorChangeRepaintsAlreadyScrolledBackHistory` (prüft explizit das
gerenderte `mPixmap`-Pixel, nicht nur die – in beiden Reihenfolgen korrekte –
Zellendaten über `getCell()`), `sameCharacterRedrawPicksUpNewExplicitColor`.
Alle drei zunächst per gezieltem `// TEMP:`-Revert rot verifiziert (inkl.
eines Fehlversuchs bei `colorChangeRepaintsAlreadyScrolledBackHistory`, der
zunächst fälschlich grün blieb, weil er nur `getCell()` statt des
tatsächlichen Pixels prüfte — korrigiert, siehe Kommentar im Test).

**Verifikation (gesamt, beide Runden):** alle 7 `ctest`-Targets grün, Clean
Build mit `-Wall -Wextra` ohne *neue* Warnungen (eine bereits vor dieser
Review-Runde bestehende `-Wsfinae-incomplete=`-Warnung auf `komportview.h:47`
gefunden und gegen den unveränderten Runde-1-Stand verifiziert — keine
Regression dieser Runde, dem Nutzer separat gemeldet). Offscreen-Smoke-Test
grün, echtes `~/.config/Komport-Qt6/`-Profil unangetastet (md5 identisch
vor/nach).

**Nachtrag (2026-09-08, auf Nutzerwunsch "ja, bitte fixen und gut
dokumentieren"):** die oben gemeldete `-Wsfinae-incomplete=`-Warnung
zurückverfolgt und behoben. Ursache war *keine* Regression aus Meilenstein 5
— per Vergleichs-Build gegen den unveränderten Stand von Abschnitt 19.7
(2026-09-06, dort bereits als "vorbestehend, unabhängig von dieser Runde"
vermerkt) bestätigt reproduzierbar, die Warnung existierte also schon
mindestens seit vor Meilenstein 4. Root Cause: `komport.h` deklariert
`KomportView` nur vorwärts (`class KomportView;`), enthält aber eine
`Q_OBJECT`-Klasse (`KomportApp`) mit einem Slot, der `KomportView*` als
Parameter nimmt (`slotViewModified(KomportView*)`). CMakes `AUTOMOC` bündelt
den generierten Code aller Header einer Target in einer einzigen
`mocs_compilation.cpp`-Übersetzungseinheit — darin lief der für `komport.h`
generierte Moc-Code (der nur die Vorwärtsdeklaration von `KomportView` sieht)
*vor* dem für `komportview.h` generierten Moc-Code, der in **derselben**
Übersetzungseinheit später die vollständige Klassendefinition liefert. Ein
`QMetaType`-Traits-Check in Qt selbst (`qmetatype.h:344`) lieferte für
`KomportView` dadurch an zwei Stellen derselben Übersetzungseinheit
potenziell unterschiedliche Antworten.

**Erster Fix-Versuch (verworfen) und Korrektur per Review (2026-09-08):**
der erste Versuch band `komportview.h` in `komport.h` einfach vollständig
statt vorwärts ein. Kompilierte sauber (0 Warnungen) und wurde committet/
gepusht — anschließend nachgeholte Gemma4+Codex-Review (`gpt-5.6-sol`, auf
Nutzerhinweis "hatten wir einen approval von codex?", da dieser einzelne
Fix ohne die sonst übliche Review-Runde durchgerutscht war) deckte mehrere
Ungenauigkeiten auf:
- **Echter, bis dahin nur zufällig kaschierter Bug:** die Behauptung
  "`komportdoc.h`/`komportminimap.h` lösen die Warnung aktuell nicht aus,
  vermutlich günstige Moc-Datei-Reihenfolge in ihren jeweiligen
  Übersetzungseinheiten" war falsch für `komportdoc.h` — Codex hat per
  gezielter manueller Moc-Reihenfolge-Vertauschung bewiesen, dass
  `komportdoc.h` (deklariert `KomportView*` ebenfalls in mehreren
  Slots/Signals, u.a. `slotViewModified()`/`viewModified()`) exakt dieselbe
  Warnung reproduziert, wenn sein Moc-Code vor dem von `komport.h`
  prozessiert wird. Es gibt keine "jeweiligen" (=separaten)
  Übersetzungseinheiten — alle Moc-Dateien einer Target landen in
  *derselben* `mocs_compilation.cpp`. Sauber war der Build nur, weil der
  erste Fix `komport.h`s `KomportView` zufällig früh genug vervollständigt
  hat, um auch `komportdoc.h`s Nutzung mit abzudecken — reine
  Bündelungs-Reihenfolge-Glückssache, kein echter Fix für `komportdoc.h`.
- **Falscher/gefährlicher Hinweis für `komportminimap.h`:** die Notiz
  "falls die Warnung dort künftig auftaucht, gilt derselbe Fix" war falsch.
  `komportminimap.h` exponiert `KomportView*` nirgends über Signal/Slot
  (nur gewöhnlicher Konstruktor-Parameter/Member) — löst den
  Vollständigkeits-Check gar nicht aus und ist als Vorwärtsdeklaration
  tatsächlich korrekt. Ein volles `#include "komportview.h"` dort hätte
  sogar eine echte Zirkularität erzeugt (`komportview.h` bindet bereits
  `komportminimap.h` ein).
- **Kleinere Ungenauigkeiten:** "GCC 13+" war falsch (die Warnung existiert
  laut GCC-Dokumentation erst ab GCC 16, Juni 2025 zum Trunk hinzugefügt);
  "ODR-Risiko" war zu stark formuliert (Qt isoliert diese Checks bewusst
  über einen distincten `Unique`-Typ pro Nutzungsstelle, um echte
  ODR-Verletzungen zu vermeiden — die Warnung meldet ordnungs-abhängiges
  Template-Verhalten, nicht per se einen ODR-Verstoß); der Fix war auch
  nicht rein kosmetisch, da die generierten statischen Metadaten jetzt das
  `KomportView*`-Metatype-Interface tragen können, statt auf einen
  Null-/Incomplete-Type-Eintrag zurückzufallen (Korrektur nach
  Codex-Review auf den revidierten Fix, s.u. — ursprünglich hier fälschlich
  als "generiert zusätzlich einen `QMetaType::fromType<KomportView*>()`-
  Aufruf" formuliert; dieser Aufruf taucht so in den generierten Dateien
  nicht auf).

**Revidierter Fix:** `komport.h` zurück auf Vorwärtsdeklaration +
`Q_MOC_INCLUDE("komportview.h")` (Qts offizieller Mechanismus für genau
diesen Fall: weist *moc selbst* an, das Include in die von ihm generierte
Datei aufzunehmen — ordnungsunabhängig by construction, da jede
betroffene generierte Moc-Datei ihre eigene, durch Include-Guards
geschützte Kopie des Includes trägt, statt sich auf die zufällige
Bündelungs-Reihenfolge in `mocs_compilation.cpp` zu verlassen). Gleicher
Fix zusätzlich auf `komportdoc.h` angewendet (der echte, oben gefundene
Bug). `komportminimap.h` bewusst unverändert gelassen (dort besteht das
Problem nachweislich nicht). Vorteil gegenüber dem ersten Versuch: kein
unnötiges Aufblähen von `komport.h`s Makro-Oberfläche/Präprozessor-Output
durch den vollen transitiven Include.

**Verifikation (revidierter Fix):** per gezieltem `// TEMP:`-Revert beider
`Q_MOC_INCLUDE`-Zeilen rot reproduziert (Warnung erscheint exakt wie
vorher), Fix zurückgesetzt, grün bestätigt (0 Warnungen im kompletten
Clean-Build) — zusätzlich per Grep in den generierten `moc_komport.cpp`/
`moc_komportdoc.cpp`-Dateien direkt bestätigt, dass beide jetzt tatsächlich
`#include "komportview.h"` enthalten. Alle 7 `ctest`-Targets grün,
Offscreen-Smoke-Test grün, echtes `~/.config/Komport-Qt6/`-Profil
unangetastet (md5 identisch vor/nach). Gemma4 (auf den ersten Fix-Versuch,
Commit `03ba91a`): Pass, keine Blocker. Codex (`gpt-5.6-sol`, auf denselben
Commit): die drei oben aufgeführten Findings, alle bestätigt und in den
revidierten Fix eingearbeitet.


### 21.1 Nacharbeit: Settings-Menüpunkt umbenannt (Appearance-Tab unauffindbar)

Nutzerbericht nach dem Merge (PR #3): der neue Appearance-Tab war
unauffindbar, weil der zugehörige Dialog nur unter `Settings > Connection
Settings...` zu finden war — ein Namensrest von vor Meilenstein 5, als
der Dialog tatsächlich nur Verbindungseinstellungen enthielt. Zusätzlich
fiel dabei auf, dass der `build/`-Ordner zum Zeitpunkt der Nachfrage
veraltet war (letzter Build vor dem PR-#3-Merge) — frisch gebaut, bevor
der eigentliche Fix begann.

**Gefixt (2026-09-08):** Menüpunkt (`komport.cpp`, `showPreferences`-Action)
von `"&Connection Settings..."` zu `"&Settings..."` umbenannt, Status-Tip
entsprechend erweitert ("Connection, terminal and appearance settings").
Reine Label-Änderung, kein Test hing am alten Text. Verifiziert: Clean-Build
0 Warnungen, alle 7 `ctest`-Targets grün, Offscreen-Smoke-Test grün. PR #4
auf Codeberg, vom Nutzer gemergt, `master` auf beiden Remotes synchronisiert.
