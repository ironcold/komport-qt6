# ADR-005: Monotonic source timing and one-source sessions

Status: Proposed
Date: 2026-09-17

## Context

Wall-clock changes must not alter serial timing, while a remote UI must not
mistake network latency for timing at the physical serial port.

## Decision

`SessionEvent::timestampNs` is a monotonic, non-negative duration from the
beginning of the session, with nanoseconds as its storage unit. The header
stores a human-readable wall-clock creation time separately.

The physical/event source timestamps observations. For local serial this is
the local serial transport; for a future remote serial connection it is the
remote agent. `SessionController` creates an explicit mapping from the active
transport's monotonic origin to the session origin. It preserves source order;
equal timestamps are ordered by `sequence`.

Session v1 contains exactly one active transport source. Merging multiple
physical or remote clocks is deliberately out of scope until a separate clock
synchronization design exists.

Scheduling during replay may have lower real-world precision than the stored
timestamp. Stored timing is evidence; replay timing is an explicitly chosen
policy and must not overwrite it.

## Consequences

- Clock adjustments do not corrupt capture timing.
- Remote support has a defined place to preserve physical timing.
- Multi-source capture is rejected rather than silently assigning misleading
  timestamps.

## References

- `docs/komport-session-replay-simulation-architecture.md`, sections 5.1 and 29
- `docs/komport-engineering-governance-spec-review-workflow.md`, section 16.8
