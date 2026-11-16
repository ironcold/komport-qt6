# Project Context and Agreements

As of: 2026-09-18

This file is the central handoff for new chats and development sessions. It shall be kept up to date with new goals, technical decisions, architectural changes, and important operational agreements. Brief agent-specific instructions are in `AGENTS.md`, and the detailed product/architecture reference is in `CLAUDE.md`.

## Workspace Structure

```text
/home/max/Development/misc/komport-qt6/
```

This is an independent project; it does not belong to the Billard-Arena workspace, and no assumptions from other repositories may be adopted. The remotes are `origin` (Codeberg) and `github`. Branch `master` is at version `1.0.0`; work on the session/transport layer is on `v2/session-platform`.

## Architecture Decision Records

Deeper justifications are located in `docs/architecture-decisions/`:

- `ADR-001` — Terminal Engine Strategy: QTermWidget examined as a backend and rejected, status: rejected (Spike in `spike/qtermwidget-bridge/`).
- `ADR-002` … `ADR-009` — Session event model, transport interface, TX/RX semantics, timestamp semantics, `.kpsession` format, replay safety, multi-source/time alignment, multi-executable product architecture. Status: accepted (2026-09-18, after independent review gate).

Convention: each ADR carries a status line (`Proposed`/`Accepted`/`Rejected`) and a date. New ADRs are written in English. The binding process is described in `docs/komport-engineering-governance-spec-review-workflow.md` (Spec before code, independent review, fix cycle until approval).

## Active Roadmap

- `TODO.md` — open points, wish list and milestone roadmap (German); `TODO-ARCHIVE.md` — completed work, reviews, fixes.
- `docs/specs/SPEC-M8-session-transport-foundation.md` — first session/transport interface, status: accepted. The gate closed on 2026-09-18; the next step is implementation according to Spec section 17.
- Afterwards: M9 Recorder/`.kpsession`, M10 Loader and passive replay, M11 Decoder, M12 active playback/simulation, M13 library extraction plus Analyzer, M14 multi-source capture/sniffer, M15 TCP/Remote Agent.
- `docs/reviews/` — the complete review history of the M8 gate (Pre-Review, five independent Codex rounds, consolidated amendment package, application verification).

## Project Goal

A serial terminal for special cases where generic terminals are insufficient: consoles of network and industrial devices, historical computers, unusual line endings, raw control characters, diagnostic recordings, macros, device profiles, and charset peculiarities. Deliberately not another generic shell terminal: the serial special case is its reason for existing; retro compatibility is the product anchor, not an end in itself.

## Technical Framework

- Language/Framework: C++17, Qt 6 (lower bound 6.3 according to `CMakeLists.txt`, locally installed 6.11.1), no KDE/KF6. Modules: Widgets, PrintSupport, SerialPort, LinguistTools, and Test for testing.
- Build: CMake (`cmake_minimum_required 3.16`). Object library `komport_core` (everything except `main.cpp`) plus executable `komport-qt6`; tests link against `komport_core` and `Qt6::Test`.
- Tests: `ctest` in the build directory, targets in `tests/` (Qt Test), forced `QT_QPA_PLATFORM=offscreen`; `tst_serial` requires `openpty()` from `libutil`.
- Persistence: `QSettings` (`~/.config/Komport-Qt6/Komport-Qt6.conf`), device profiles under `Profiles/<Name>`, custom charsets under `~/.config/Komport-Qt6/charsets/`.
- External interface: serial devices via `QSerialPort`/`QSerialPortInfo`; PTYs in tests. No network functionality in 1.x.
- There is no CI (only issue/PR templates): the Qt 6.3 lower bound is documented but not automatically verified. Warning-freedom must be explicitly checked because the project itself does not set warning flags: `cmake -B build -DCMAKE_CXX_FLAGS="-Wall -Wextra" && cmake --build build`.

## Central Architecture

Data flow (binding guideline from `CLAUDE.md`):

```text
QSerialPort -> KomportSerial/ITransport -> RX/TX diagnostics, logging,
charset translation -> Terminal backend (KomportEmulation)

Terminal backend -> charset translation / Line-Ending / Macros -> QSerialPort
```

Session layer (M8, accepted, not yet implemented): `ITransport` (pure byte movement, `activationId` per open attempt, QtCore-only) -> `SessionController` (sequence, source ID 1, session-origin mapping according to ADR-005, activation filter, FIFO delivery) -> `SessionEvent` (ADR-002) -> later Recorder/Replay/Decoder. One observation is exactly one event; chunks are write/read boundaries of the application, not wire frames, and may be assembled by decoders into larger streams.

Legacy path: `receivedChar`/`sentChar` remain display adapters for terminal view, hex monitor and text logger and are explicitly not session sources. The display path is deliberately lossy and delayed (RX buffer trimming, flush timer); the session stream is lossless at the application observation boundary. Byte-exactness means: RX is exactly what `readAll()` delivered, TX exactly what `write()` accepted -- no electrical guarantee.

Central classes: `KomportApp` (window, menus/toolbar, profiles, macros, recording), `KomportDoc` (document; owner of transport and controller), `KomportView` + `KomportMinimapScrollBar`, `KomportEmulation` (VT100/VT102-like with deliberately included xterm extensions; largest module), `KomportCell`/`KomportCellArray`, scroll buffer, `KomportCharset`, `KomportHexView`, `KomportMacroBar`, `KomportSessionLogger`, `SettingsDialog`.

Loose coupling is a basic principle: the new session/transport contracts remain widget- and frontend-neutral (ADR-009), so that a later analyzer or headless agent can be added without modifying the terminal. Fail policy: serial and configuration errors remain visible (legacy `settingsFailed`, future `Error` events with `kind`/`code`); charset translation must never silently put wrong bytes on the wire.

## Development Agreements

- Spec before code: write an ADR or specification for relevant architectural or behavioral changes and have it reviewed before implementation.
- Trigger review gate via Codex CLI, as long as no companion command is missing; review artifacts are in English in `docs/reviews/`. Fix findings iteratively, test again and request review again until explicitly approved. After five fix cycles with continuing critical findings, pause in a controlled manner and redefine the milestone.
- Reviewer statements are assertions: verify them before adopting them in code, and openly correct your own misstatements.
- A normative amendment/change document instead of delta layers; replaced passages in older documents are the source of later contradictions.
- New technical planning artifacts (ADRs, Specs, Reviews, Handoffs) are in English; existing German documents are translated to English in controlled, reviewed batches.
- Do not version production secrets or tokens, device paths with credentials, or runtime data. No automatic commit or push; observe the review stop after completed work.
- Respect repository boundaries: no write access to neighboring repos.
- Do not silently change behavior at the wire or display boundary (legacy RX buffer, double configuration application, line-ending semantics); such changes require their own separate, reviewed change.
- Always execute validation and report the result (`ctest`, warning-freedom under `-Wall -Wextra`).

## Open Points

- [ ] Implement M8: `SPEC-M8` section 17, steps 2–7 (core types, raw chunk observations, configuration entry point, `SessionController`, migration of the two app call sites, tests, followed by self-review and independent review).
- [ ] Set up Qt 6.3 CI or release gate so that the declared lower bound is actually verified.
- [ ] Follow-ups outside M8: M9 streaming writer, M10 replay player interface, cross-domain synchronization/uncertainty UI, separately reviewed change of the legacy RX buffer.
- [ ] Close known emulation gaps from `TODO.md` section 6 (e.g. DECOM/origin mode, VT52) if necessary.
- [ ] Update this document upon architectural or process changes.
