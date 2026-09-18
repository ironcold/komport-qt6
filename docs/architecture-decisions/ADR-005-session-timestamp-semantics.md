# ADR-005: Monotonic source timing and session-time mapping

Status: Proposed
Date: 2026-09-17

## Context

Wall-clock changes must not alter serial timing, while a remote UI must not
mistake network latency for timing at the physical serial port.

## Decision

`SessionEvent::sourceTimestampNs` is the immutable, non-negative monotonic
duration supplied by the physical/event source, in nanoseconds. The header
stores a human-readable wall-clock creation time separately.

The physical/event source timestamps observations. For local serial this is
the local serial transport; for a future remote serial connection it is the
remote agent. `SessionEvent::timestampNs` is an aligned, non-negative duration
on the session timeline, derived by an explicit mapping from source time. The
mapping preserves source order; equal aligned timestamps are ordered by the
session-wide `sequence`.

M8 contains exactly one active local transport source. For that source the
mapping uses the common process clock and needs no synchronization. A later
multi-source session may add sources only with a persisted descriptor and a
versioned mapping containing a reference point, offset, scale and uncertainty.
The raw source timestamp remains authoritative. Multi-host merging is not part
of M8 and must visibly report uncertainty when no trustworthy alignment exists.

This ADR prepares only the data contract. M8 and M9 do not implement a clock
synchronization exchange, offset/drift estimation, a `TimeMapping` runtime
component, persisted alignment mappings, or an alignment UI. For their single
local source, `timestampNs` is simply derived in the existing local clock
domain.

Scheduling during replay may have lower real-world precision than the stored
timestamp. Stored timing is evidence; replay timing is an explicitly chosen
policy and must not overwrite it.

## Consequences

- Clock adjustments do not corrupt capture timing.
- Remote support has a defined place to preserve physical timing.
- Future multi-source capture has an explicit alignment seam without silently
  rewriting raw timing or assigning misleading precision.

## References

- `docs/komport-session-replay-simulation-architecture.md`, sections 5.1 and 29
- `docs/komport-engineering-governance-spec-review-workflow.md`, section 16.8
- ADR-008
