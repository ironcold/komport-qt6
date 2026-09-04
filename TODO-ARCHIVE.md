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

### 18.1 Fix: natives Tooltip-Popup für Toolbar-Icons komplett unterdrückt

`hoverHintLabel` bleibt bestehen (funktioniert und ist strukturell robust,
schadet nicht) — zusätzlich wird das native Popup für die Toolbar-Buttons
jetzt vollständig unterdrückt, damit der fehlerhafte Mechanismus gar nicht
mehr zur Anzeige kommt:

- In `initToolBar()`: für jede Aktion mit gesetztem `statusTip()` wird
  zusätzlich zur bestehenden `hovered()`-Verbindung ein Event-Filter auf
  dem zugehörigen Button-Widget installiert
  (`mainToolBar->widgetForAction(action)->installEventFilter(this)`).
- `KomportApp::eventFilter()`: fängt `QEvent::ToolTip` ab und gibt `true`
  zurück (Event konsumiert, kein natives Popup) — sicher unconditional,
  da dieser Filter ausschließlich auf `mainToolBar` selbst und dessen
  Icon-Buttons installiert ist, nirgends sonst.
- `hoverHintLabel` bleibt die einzige verbleibende Hover-Hinweis-Anzeige
  für diese Buttons.

### 18.2 Verifikation

- Build mit `-Wall -Wextra`: 0 Warnungen, 0 Fehler (voller Clean-Rebuild).
- Offscreen-Smoke-Test: startet weiterhin fehlerfrei.
- **Ehrlicher Hinweis:** auch dieser Fix konnte in der Sandbox nicht visuell
  gegen das Original-Video-Muster verifiziert werden (kein echtes Display,
  und `QEvent::ToolTip`-Timing/Popup-Wiederverwendung ist ohnehin ein
  reines Rendering-/Fensterverwaltungs-Verhalten, das sich nicht sinnvoll
  automatisiert nachstellen lässt) — dafür ist die Diagnose diesmal deutlich
  besser durch das tatsächliche Reproduktionsmuster des Nutzers gestützt
  als die vorherigen zwei Versuche. Bitte erneut auf echter Hardware
  gegenprüfen; falls das native Popup selbst (nicht die Statusleiste)
  weiterhin irgendwo auftaucht, ist das ein klares Zeichen, dass die
  Unterdrückung an der falschen Stelle ansetzt (z.B. weil ein anderes
  Widget als der `QToolButton` das Popup zeigt) und weiter eingegrenzt
  werden muss.
