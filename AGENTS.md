# Working Guide

This repository is **Komport-Qt6**, a Qt 6 serial terminal (GPL-2.0-or-later):
the maintained successor of the KDE3/Qt3 *Komport* by Mike Sharkey, built as a
pure Qt 6 application with `QSerialPort` and no KDE Frameworks. The product
anchor is the awkward serial case — network and industrial consoles, historical
machines, unusual line endings, raw control characters, diagnostic captures,
macros, device profiles, charset peculiarities — not a generic shell terminal.

`CLAUDE.md` remains the product and architecture reference; this file is the
short working guide for agents. When the two differ, `CLAUDE.md` and the accepted
ADRs win.

## Repository scope

- Work only inside this Git repository; do not write to sibling repositories.
- Never commit or push automatically. `master` (1.0.0) is release-grade; the
  session/transport work lives on `v2/session-platform`. Remotes: `origin`
  (Codeberg) and `github`.
- No KDE/KF6 dependency may be introduced. Qt 6 only, C++17, declared Qt floor
  6.3 (`CMakeLists.txt`); the locally installed Qt is newer.
- No secrets, production data or live devices without explicit user approval.

## Engineering rules

- **Bytes are the truth.** Never normalize, trim or implicitly translate a
  received byte stream. Raw bytes precede emulation; the emulation is a consumer,
  never a data source.
- **Charset translation is interactive-only.** `KomportCharset` applies to
  typing, paste, macro text and RX display. File transfer (`KomportTransfer`)
  stays raw and byte-exact, so an uploaded CNC program cannot be altered by a
  translation table.
- **Byte-exactness has a stated boundary.** RX is exactly the bytes `readAll()`
  returned, TX exactly the bytes `write()` accepted. That is not an electrical
  guarantee; do not describe it as "wire truth".
- **`QSerialPort` stays below the transport boundary.** New code talks to
  `ITransport` / `SessionController` (ADR-003), and the new public session and
  transport contracts stay QtCore-only, without widget or application types
  (ADR-009).
- **Session data never comes from the legacy character signals.** `receivedChar`
  and `sentChar` remain display adapters (terminal view, hex monitor, text
  logger); recorder/decoder/replay code must not consume them.
- **Keep the build warning-free.** The project does not add warning flags itself,
  so verify explicitly (see below). No new warnings.
- **Persisted behaviour is a contract.** Profiles, macros, line endings and
  charsets live in `QSettings`; custom `*.charset` files must stay usable without
  a rebuild. Do not change visible terminal behaviour (including the legacy RX
  buffer and the configuration application path) as a drive-by fix — such a
  change needs its own reviewed slice.

## Process (binding)

`docs/komport-engineering-governance-spec-review-workflow.md` is binding:

- **Spec before code.** An accepted ADR/spec is required for anything touching
  public API, persistent formats, architecture boundaries or cross-module
  behaviour. Design decisions belong in reviewed documents, not inside
  implementation code.
- **Independent review gate.** Reviews run through the Codex CLI
  (`codex exec -s read-only`), the review records live in `docs/reviews/` in
  English, and the fix cycle continues until explicit approval.
- **Verify the reviewer.** A review finding or a reviewer correction is a claim,
  not a fact: check it against the code before adopting it, and keep a corrections
  list for your own statements that turned out wrong.
- **One normative amendment document, not layers of deltas.** Superseded passages
  in earlier documents are how contradictions are born.
- **Count your cycles.** After five review-fix cycles with unresolved critical
  findings, pause, propose re-cutting the milestone or sharpening the spec instead
  of starting another text cycle.
- **Language.** New ADRs, specs, milestone/backlog entries, design and review
  documents, and engineering handoffs are written in English. Existing German
  documents (`CLAUDE.md`, `TODO.md`, `TODO-ARCHIVE.md`, `docs/project-context.md`)
  stay German and are not translated proactively.
- Keep diffs small, local and testable. Do not invent APIs, commands, config
  fields or dependencies. Run the relevant validation and report the result.

## Build and test

```sh
cmake -B build -DCMAKE_CXX_FLAGS="-Wall -Wextra" && cmake --build build
cd build && ctest --output-on-failure
```

Tests are Qt Test targets in `tests/` (run through `ctest`), and they force
`QT_QPA_PLATFORM=offscreen`, so they work without a display. `tst_serial` uses
`openpty()` and needs `libutil`. `-DBUILD_TESTING=OFF` skips the test targets.

## Where things are

- `CLAUDE.md` — product vision, architecture overview, port history (German).
- `TODO.md` — open items, milestone roadmap; `TODO-ARCHIVE.md` — completed work.
- `docs/komport-engineering-governance-spec-review-workflow.md` — the binding
  spec-first and review process.
- `docs/architecture-decisions/` — ADR-001 … ADR-009 (ADR-002 – ADR-009 accepted
  2026-09-18; ADR-001 is a rejected QTermWidget spike).
- `docs/specs/` — implementation specs (`SPEC-M8-session-transport-foundation.md`
  is accepted; its §17 lists the implementation steps).
- `docs/reviews/` — the review record of the M8 foundation gate (pre-review, five
  independent rounds, consolidated amendment package, application verification).
- `docs/project-context.md` — handover for new sessions (German).
- `komport/` — sources; `tests/` — regression tests; `spike/qtermwidget-bridge/`
  — the ADR-001 spike, kept for the record.

Workspace-wide model roles for the local agent setup are documented in the
workspace template (`/home/max/Development/billard-arena/template/AGENTS.md`)
and are not duplicated here.
