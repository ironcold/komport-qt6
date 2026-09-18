# Projektkontext und Vereinbarungen

Stand: 2026-09-18

Diese Datei ist die zentrale Uebergabe fuer neue Chats und Entwicklungs-Sessions.
Sie soll bei neuen Zielen, fachlichen Entscheidungen, Architekturaenderungen und
wichtigen Betriebsabsprachen aktuell gehalten werden. Die kurze agentenbezogene
Arbeitsanweisung steht in `AGENTS.md`, die ausfuehrliche Produkt-/Architektur-
referenz in `CLAUDE.md`.

## Workspace-Struktur

```text
/home/max/Development/misc/komport-qt6/
```

Ein eigenstaendiges Projekt; es gehoert nicht zum Billard-Arena-Arbeitsbereich,
und aus anderen Repositories duerfen keine Annahmen uebernommen werden. Remotes:
`origin` (Codeberg) und `github`. Branch `master` traegt den Stand `1.0.0`, die
Session-/Transport-Arbeit laeuft auf `v2/session-platform`.

## Architecture Decision Records

Tiefere Begruendungen liegen in `docs/architecture-decisions/`:

- `ADR-001` — Terminal-Engine-Strategie: QTermWidget als Backend geprueft und
  verworfen, Status: rejected (Spike in `spike/qtermwidget-bridge/`).
- `ADR-002` … `ADR-009` — Session-Event-Modell, Transportschnittstelle,
  TX/RX-Semantik, Zeitstempel-Semantik, `.kpsession`-Format, Replay-Sicherheit,
  Mehrquellen-/Zeitausrichtung, Multi-Executable-Produktarchitektur.
  Status: accepted (2026-09-18, nach unabhaengigem Review-Gate).

Konvention: jede ADR traegt eine Statuszeile (`Proposed`/`Accepted`/`Rejected`)
und ein Datum. Neue ADRs werden auf Englisch geschrieben. Der verbindliche
Prozess steht in `docs/komport-engineering-governance-spec-review-workflow.md`
(Spec vor Code, unabhaengiges Review, Fix-Zyklus bis Freigabe).

## Aktive Roadmap

- `TODO.md` — offene Punkte, Wunschliste und Meilenstein-Roadmap (deutsch);
  `TODO-ARCHIVE.md` — abgeschlossene Arbeit, Reviews, Fixes.
- `docs/specs/SPEC-M8-session-transport-foundation.md` — erster
  Session-/Transport-Schnitt, Status: accepted. Das Gate ist am 2026-09-18
  geschlossen; als naechstes folgt die Umsetzung nach Spec-Abschnitt 17.
- Danach: M9 Recorder/`.kpsession`, M10 Loader und passive Replay, M11 Decoder,
  M12 aktive Wiedergabe/Simulation, M13 Bibliotheks-Extraktion plus Analyzer,
  M14 Mehrquellen-Capture/Sniffer, M15 TCP/Remote-Agent.
- `docs/reviews/` — der komplette Review-Verlauf des M8-Gates (Pre-Review, fuenf
  unabhaengige Codex-Runden, konsolidiertes Amendment-Paket,
  Anwendungs-Verifikation).

## Ziel des Projekts

Ein serielles Terminal fuer die Spezialfaelle, in denen generische Terminals
unzureichend sind: Konsolen von Netzwerk- und Industriegeraeten, historische
Rechner, ungewoehnliche Zeilenenden, rohe Steuerzeichen, Diagnose-Mitschnitte,
Makros, Geraeteprofile und Zeichensatz-Eigenheiten. Bewusst kein weiteres
generisches Shell-Terminal: der serielle Spezialfall ist die Daseinsberechtigung,
Retro-Kompatibilitaet ist der Produktanker, nicht der Selbstzweck.

## Technischer Rahmen

- Sprache/Framework: C++17, Qt 6 (Untergrenze 6.3 laut `CMakeLists.txt`, lokal
  installiert 6.11.1), kein KDE/KF6. Module: Widgets, PrintSupport, SerialPort,
  LinguistTools, im Test zusaetzlich Test.
- Build: CMake (`cmake_minimum_required 3.16`). Objekt-Library `komport_core`
  (alles ausser `main.cpp`) plus Executable `komport-qt6`; Tests linken gegen
  `komport_core` und `Qt6::Test`.
- Tests: `ctest` im Build-Verzeichnis, Targets in `tests/` (Qt Test), erzwungenes
  `QT_QPA_PLATFORM=offscreen`; `tst_serial` braucht `openpty()` aus `libutil`.
- Persistenz: `QSettings` (`~/.config/Komport-Qt6/Komport-Qt6.conf`), Geraete-
  profile unter `Profiles/<Name>`, benutzerdefinierte Zeichensaetze unter
  `~/.config/Komport-Qt6/charsets/`.
- Externe Schnittstelle: serielle Geraete ueber `QSerialPort`/`QSerialPortInfo`;
  PTYs in Tests. Keine Netzwerkfunktionalitaet in 1.x.
- Es gibt keine CI (nur Issue-/PR-Templates): die Qt-6.3-Untergrenze ist
  dokumentiert, aber nicht automatisiert verifiziert. Warnungsfreiheit muss
  explizit geprueft werden, weil das Projekt selbst keine Warnflags setzt:
  `cmake -B build -DCMAKE_CXX_FLAGS="-Wall -Wextra" && cmake --build build`.

## Zentrale Architektur

Datenfluss (verbindliche Leitlinie aus `CLAUDE.md`):

```text
QSerialPort -> KomportSerial/ITransport -> RX/TX-Diagnose, Logging,
Zeichensatz-Uebersetzung -> Terminal-Backend (KomportEmulation)

Terminal-Backend -> Zeichensatz-Uebersetzung / Line-Ending / Makros -> QSerialPort
```

Session-Schicht (M8, akzeptiert, noch nicht implementiert): `ITransport` (reine
Byte-Bewegung, `activationId` pro Open-Versuch, QtCore-only) -> `SessionController`
(Sequenz, Quell-ID 1, Session-Origin-Mapping nach ADR-005, Aktivierungsfilter,
FIFO-Zustellung) -> `SessionEvent` (ADR-002) -> spaeter Recorder/Replay/Decoder.
Eine Beobachtung wird genau ein Event; Chunks sind Schreib-/Lese-Grenzen der
Anwendung, keine Draht-Frames, und duerfen von Decodern zu groesseren Streams
zusammengesetzt werden.

Legacy-Pfad: `receivedChar`/`sentChar` bleiben Anzeige-Adapter fuer
Terminal-View, Hex-Monitor und Text-Logger und sind ausdruecklich keine
Session-Quelle. Der Anzeigepfad ist bewusst verlustbehaftet und verzoegert
(RX-Puffer-Trimming, Flush-Timer); der Session-Strom ist verlustfrei an der
Anwendungs-Beobachtungsgrenze. Byte-Exaktheit heisst: RX ist exakt das, was
`readAll()` geliefert hat, TX exakt das, was `write()` akzeptiert hat -- keine
elektrische Garantie.

Zentrale Klassen: `KomportApp` (Fenster, Menues/Toolbar, Profile, Makros,
Recording), `KomportDoc` (Dokument; Eigentuemer von Transport und Controller),
`KomportView` + `KomportMinimapScrollBar`, `KomportEmulation` (VT100/VT102-nah
mit bewusst aufgenommenen xterm-Erweiterungen; groesstes Modul),
`KomportCell`/`KomportCellArray`, Scroll-Buffer, `KomportCharset`,
`KomportHexView`, `KomportMacroBar`, `KomportSessionLogger`, `SettingsDialog`.

Lose Kopplung ist Grundprinzip: die neuen Session-/Transportvertraege bleiben
widget- und frontend-neutral (ADR-009), damit ein spaeterer Analyzer oder
Headless-Agent ohne Umbau des Terminals moeglich bleibt. Fail-Policy: serielle
und Konfigurationsfehler bleiben sichtbar (Legacy `settingsFailed`, kuenftig
`Error`-Events mit `kind`/`code`); die Zeichensatz-Uebersetzung darf niemals
still falsche Bytes auf die Leitung legen.

## Entwicklungsvereinbarungen

- Spec vor Code: vor Implementierung relevanter Architektur- oder
  Verhaltensaenderungen eine ADR oder Spezifikation schreiben und reviewen lassen.
- Review-Gate ueber die Codex-CLI ausloesen, solange kein Companion-Befehl
  fehlt; Review-Artefakte liegen englischsprachig in `docs/reviews/`. Findings
  iterativ beheben, erneut testen und erneut Review anfordern, bis ausdruecklich
  freigegeben wurde. Nach fuenf Fix-Zyklen mit weiterhin kritischen Findings
  kontrolliert pausieren und den Meilenstein neu schneiden.
- Reviewer-Aussagen sind Behauptungen: vor Uebernahme im Code verifizieren, und
  eigene Fehlaussagen offen korrigieren.
- Ein normatives Amendment-/Aenderungsdokument statt Delta-Schichten; ersetzte
  Passagen in aelteren Dokumenten sind die Quelle spaeterer Widersprueche.
- Neue technische Planungsartefakte (ADRs, Specs, Reviews, Handoffs) auf
  Englisch; bestehende deutsche Dokumente bleiben deutsch.
- Keine produktiven Secrets, Tokens, Geraetepfade mit Zugangsdaten oder
  Runtime-Daten versionieren. Kein automatischer Commit oder Push; nach
  abgeschlossener Arbeit Review-Stopp einhalten.
- Repository-Grenzen respektieren: keine Schreibzugriffe auf Nachbar-Repos.
- Verhalten an der Draht- oder Anzeige-Grenze nicht stillschweigend aendern
  (Legacy-RX-Puffer, doppelte Konfigurationsanwendung, Zeilenende-Semantik);
  solche Aenderungen brauchen einen eigenen, gereviewten Schnitt.
- Validierung immer ausfuehren und das Ergebnis berichten (`ctest`,
  Warnungsfreiheit unter `-Wall -Wextra`).

## Offene Punkte

- [ ] M8 implementieren: `SPEC-M8` Abschnitt 17, Schritte 2–7 (Core-Typen,
      Raw-Chunk-Observations, Konfigurations-Einstiegspunkt, `SessionController`,
      Migration der beiden App-Aufrufstellen, Tests, anschliessend Selbst- und
      Fremdreview).
- [ ] Qt-6.3-CI oder Release-Gate einrichten, damit die deklarierte Untergrenze
      tatsaechlich verifiziert wird.
- [ ] Follow-ups ausserhalb M8: M9 Streaming-Writer, M10 Replay-Player-Interface,
      Cross-Domain-Synchronisation/Uncertainty-UI, separat gereviewte Aenderung
      des Legacy-RX-Puffers.
- [ ] Bekannte Emulationsluecken aus `TODO.md` Abschnitt 6 (u. a. DECOM/
      Origin-Mode, VT52) bei Bedarf schliessen.
- [ ] Dieses Dokument bei Architektur- oder Prozessaenderungen aktualisieren.
