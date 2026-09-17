# ADR-007: Passive-by-default replay safety model

Status: Proposed
Date: 2026-09-17

## Context

Recorded serial traffic can have physical effects. Loading a file or exploring
it for analysis must therefore not create any output path to real hardware.

## Decision

Loading a `.kpsession` creates an offline session only. Passive replay emits
events through `SessionController` to views and decoders, and has no
`ITransport` output target. It is safe to start without a confirmation.

Active TX replay is a separate future feature. It may send only recorded TX
data through an explicitly selected, open `ITransport`; it requires a visible
target and an explicit confirmation for each start. Recorded transport settings
are informational and are never silently applied to an active target.

Simulation is also separate from passive replay. It must not mutate the source
session and will require its own matching/mismatch policy before implementation.

## Consequences

- File loading and offline decoder work are intrinsically safe.
- Future active output cannot be accidentally enabled by a replay checkbox or
  by restoring a previous UI state.
- The first milestone can deliver useful replay without hardware-risk code.

## References

- `docs/komport-session-replay-simulation-architecture.md`, sections 9–16 and 45
- `docs/komport-engineering-governance-spec-review-workflow.md`, section 16.6
