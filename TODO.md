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

- [ ] `komport.desktop`: veraltete Shebang-Zeile (`#!/usr/bin/env
  xdg-open`) und `Encoding=UTF-8` sind für moderne `.desktop`-Dateien
  unüblich.
- [ ] Debug-`printf()` in `komportemulation.cpp` (~Z. 920, ~Z. 1139) bei
  unbekannten SGR/CSI-Sequenzen geht nach stdout statt z.B. `qDebug()`.

### Empfohlene Reihenfolge

Profilwert-Validierung/Array-Größen absichern → CSI-Buffer begrenzen →
Fenster-Close/Serial-Lifetime korrigieren (inkl. `pViewList`-Bereinigung) →
Logger-/Transfer-Busy-Loop und stille Fehler entschärfen → danach
dauerhafte PTY- und Emulations-Regressionstests in CMake/CTest aufnehmen.
Erst danach Meilenstein 4/5/6/7 angehen.


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
