# ADR-005: Monotonic source timing and session-time mapping

Status: Accepted
Date: 2026-09-17

## Context

Wall-clock changes must not alter serial timing, while a remote UI must not
mistake network latency for timing at the physical serial port.

## Decision

`SessionEvent::sourceTimestampNs` is the immutable, non-negative monotonic
duration supplied by the physical/event source, in nanoseconds. The header
stores a human-readable wall-clock creation time separately.

The physical/event source timestamps observations. For local serial this is the
local serial transport; for a future remote serial connection it is the remote
agent.

`SessionEvent::timestampNs` is the non-negative duration on the session
timeline, derived from source time by one explicit mapping. Per clock domain the
controller keeps a reference and a last emitted value:

```text
reference.sourceTimestampNs        i64  raw source-clock value of the anchor event
reference.sessionTimestampNs       i64  session time of the same anchor event; exactly 0 in v1
lastEmittedSessionTimestampNs      runtime state; initialized to reference.sessionTimestampNs
                                        when the reference is set, updated after every emission

rawSessionTimestampNs = sourceTimestampNs - reference.sourceTimestampNs
timestampNs           = max(lastEmittedSessionTimestampNs, rawSessionTimestampNs)
lastEmittedSessionTimestampNs = timestampNs        (after emission)
```

The anchor event of a clock domain is the first event the controller accepts
whose source belongs to that domain; its reference is set from that event, so it
has `rawSessionTimestampNs == 0` and `timestampNs == 0`. A failed-open `Error`
event is a legitimate anchor event: it is a real observation of that domain, its
timing is meaningful, and treating it as the anchor removes the ambiguity of
"which event is first" without special cases. Once set for a domain, the
reference never moves for the rest of the session: a later activation, a reopen
or a second source in the same domain does not re-anchor it. `sourceTimestampNs`
is retained raw for the whole life of the session and is never rewritten.

The `max()` term is the explicitly documented non-decreasing normalization of
session time within a clock domain; it is not a correction of stored data. If
the raw term would be lower than the last emitted session time — which
contradicts "one monotonic clock per domain" — the event is still emitted with
its payload unchanged, carries the last emitted session time, and the controller
records exactly one anomaly diagnostic (`Error`, `kind: "runtime"`,
`code: "non_monotonic_observation"`). Byte evidence is never dropped and raw time
is never rewritten.

M8 contains exactly one active local transport source and one clock domain, so
its mapping has a single reference and needs no synchronization. A later
multi-source session may add sources only with a persisted descriptor: sources
sharing a `clockDomainId` are ordered directly on that domain's timeline, while
sources in different domains may not be ordered directly and need a later,
separately specified versioned mapping (reference point, offset, scale,
uncertainty). Multi-host merging is not part of M8 and must visibly report
uncertainty when no trustworthy alignment exists.

This ADR prepares only the data contract. M8 and M9 implement neither a clock
synchronization exchange, offset/drift estimation, a `TimeMapping` runtime
component, nor an alignment UI, and they persist no mapping **between** clock
domains. The per-domain origin reference that ADR-006 requires is capture
metadata describing how this file's own timestamps were produced; it is not a
cross-domain alignment mapping.

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
- ADR-003, ADR-006, ADR-008
