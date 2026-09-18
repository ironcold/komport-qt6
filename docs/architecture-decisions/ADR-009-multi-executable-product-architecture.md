# ADR-009: Multi-executable product architecture and shared-library boundaries

Status: Accepted
Date: 2026-09-18

## Context

Komport's future session, decoder, replay, simulation, sniffer and remote
capabilities can grow into an engineering workbench. Adding all of them to the
existing terminal UI would make the everyday serial terminal unnecessarily
heavy. The planned remote endpoint also must be useful without a desktop UI.

At the same time, a large refactor into many libraries before a second frontend
exists would add risk without product value. The current build deliberately
contains one executable and an object library that also includes terminal UI
classes.

## Decision

`komport-qt6` remains the existing, focused serial-terminal executable. Future
products are peers, not extensions of its internals:

```text
komport-qt6       terminal frontend
komport-analyzer  analysis, recording, replay and simulation frontend
komport-agent     headless-capable remote transport endpoint
```

All future public session, transport, decoder, replay and simulation contracts
are application-neutral: they may use the appropriate Qt non-UI modules but
must not expose `QWidget`, `KomportApp`, analyzer UI or agent-service types.
Executables may depend on shared libraries; shared libraries may only depend on
lower shared libraries. No executable may depend on another executable's
internals, and no shared library may depend on executable UI code.

This is a logical boundary now, not an immediate physical split. M8 retains
the current build and uses `KomportDoc` only as an incremental owner/adapter.
No analyzer/agent skeleton, CMake option, package split or mass movement of
existing classes is authorized by this ADR. A shared-library extraction occurs
only when a reviewed implementation spec needs a second frontend; it must move
the smallest tested, dependency-clean boundary and keep the terminal working.

## Consequences

- The terminal remains fast and comprehensible for ordinary serial work.
- Advanced multi-source/timeline functionality belongs to the later analyzer;
  remote physical ownership belongs to the later agent.
- Existing `komport_core` is not automatically a reusable platform library,
  because it currently contains terminal UI. New reusable APIs must not inherit
  that dependency.
- M8 through M12 can establish and test logical contracts without a disruptive
  directory or CMake rewrite.
- The first analyzer or agent work must begin with a focused extraction spec,
  including dependency, ownership, install/packaging and regression-test plan.

## References

- `docs/komport-multi-executable-product-architecture.md`
- `docs/komport-multiport-sniffer-time-alignment.md`
- ADR-002 through ADR-008
- `docs/specs/SPEC-M8-session-transport-foundation.md`
