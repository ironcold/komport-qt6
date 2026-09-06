# Komport-Qt6 — TODO

Aktive Liste offener Punkte. Die komplette, abgeschlossene Portierungs-/
Feature-/Review-Historie (Abschnitte 0–5, 8–10) ist nach **`TODO-ARCHIVE.md`**
ausgelagert, damit diese Datei übersichtlich bleibt — dort stehen alle Details
zu bereits erledigter Arbeit. Architektur/Ziele stehen in `CLAUDE.md`.

Stand: alle bisherigen Aufträge (Basis-Portierung, Admin-Tool-Features,
Code-Review-Fixes, Umbenennung) sind abgearbeitet — siehe `TODO-ARCHIVE.md`.
Unten stehen nur die bewusst offen gelassenen/verschobenen Punkte.

## 0. Review-Gate vor weiterer Feature-Arbeit

- [x] Vor neuen funktionalen Meilensteinen einen kompletten Codex-/Adversarial-Review
  des aktuellen Stands durchfuehren. Hintergrund: Die Qt6-Portierung und die
  Admin-Tool-Features sind auf einem anderen Rechner entstanden und noch nicht
  nach den strengeren Template-Vorgaben dieses Arbeitsbereichs gesteuert
  worden. Review-Fokus: serielle I/O mit `QSerialPort`, VT100/VT102-
  Emulation, Scrollback/Minimap, Hex-Monitor/Session-Logging, Profil-
  Persistenz, UI-Lifetime/Signal-Slot-Verbindungen, Build-/Install-Pfade,
  Lizenz-/Header-Konsistenz und fehlende Tests/Smoke-Checks. Findings vor
  Meilenstein 4/5/6/7 priorisieren und als konkrete TODOs schneiden.
  Durchgeführt am 2026-09-06 via `codex:codex-rescue` (Codex-Adversarial-Review,
  rein lesend, keine Code-Änderungen) — Ergebnis siehe Abschnitt 0.1.

## 0.1 Findings aus dem Codex-Adversarial-Review (2026-09-06)

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

### Kritisch

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

### Hoch

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

### Mittel

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

### Nitpick

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

### Empfohlene Reihenfolge

Profilwert-Validierung/Array-Größen absichern → CSI-Buffer begrenzen →
Fenster-Close/Serial-Lifetime korrigieren (inkl. `pViewList`-Bereinigung) →
Logger-/Transfer-Busy-Loop und stille Fehler entschärfen → danach
dauerhafte PTY- und Emulations-Regressionstests in CMake/CTest aufnehmen.
Erst danach Meilenstein 4/5/6/7 angehen.

## 0.2 Zweiter Full-Review nach den Fixes aus 0.1 (2026-09-06)

Auf Nutzerwunsch ein zweiter, unabhängiger `codex:codex-rescue`-Adversarial-
Review über den Stand nach allen 15 Fixes aus Abschnitt 0.1 (Branch
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
  Fix-Durchlauf (Abschnitt 0.1, Hoch) sinnvoll `false` bei Fehlern
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

## 0.3 Dritter Full-Review (2026-09-06)

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

## 0.4 Vierter Full-Review (2026-09-06)

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
  Fenster-Lifetime-Fix (Abschnitt 0.1, Hoch) faktisch harmlos (Fenster
  wurden ohnehin nie zerstört, der Leak lebte nur bis Prozessende); seit
  `Qt::WA_DeleteOnClose` Fenster wirklich zerstört, wird bei jedem
  geschlossenen Fenster real Speicher verloren.
  **Gefixt (2026-09-06):** `delete mEmulation;` in `~KomportView()`
  ergänzt.

**Verifikation:** alle 6 `ctest`-Targets grün (1 neu). Clean-Build mit
`-Wall -Wextra`: weiterhin 0 Warnungen/Fehler. Offscreen-Smoke-Test grün,
echtes `~/.config/Komport-Qt6/`-Profil unangetastet.

## 0.5 Fünfter Full-Review (2026-09-06)

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
