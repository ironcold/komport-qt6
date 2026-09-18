# ADR-008: Source identity and time alignment for future multi-source sessions

Status: Accepted
Date: 2026-09-18

## Context

The planned multi-port sniffer observes both directions of a serial link with
separate, receive-only ports. In that topology both physical observations are
generic `Rx`, yet one semantically represents controller-to-device traffic and
the other device-to-controller traffic. Remote capture also introduces separate
monotonic clock domains. Encoding either fact by changing TX/RX semantics or by
rewriting the observed timestamp would make recording, replay and decoding
ambiguous.

## Decision

Every `SessionEvent` carries a stable physical `sourceId`, independent of
`SessionDirection`. Source descriptors are session-header data and contain the
transport/configuration snapshot plus a human-readable name and optional
semantic role. A semantic role such as `controller_to_device` is presentation
and analysis metadata; it never changes ADR-004's generic Tx/Rx meaning.

Every event preserves `sourceTimestampNs`, the source-local monotonic capture
time. `timestampNs` is a derived session-timeline time. A source-to-session
mapping may use a source/session reference point, offset, scale and an explicit
uncertainty. It is versioned metadata, never a destructive rewrite of source
time. Because a mapping's reference points, offsets, scales and uncertainties are
nanosecond-scale integer values, they are encoded losslessly (ADR-006): never as
JSON numbers.

The preparation is deliberately limited to `sourceId`, `sourceTimestampNs`,
`timestampNs` and source descriptors. No synchronization algorithm, clock
probe, offset/drift estimator, `TimeMapping` runtime object, cross-domain mapping
or alignment UI is introduced by this ADR, M8 or M9. Those are separate future
work, started only when the multi-host use case is actually needed. The
per-domain origin reference of ADR-006 is mandatory capture metadata for this
file's own timeline and is not such a mapping.

M8 deliberately instantiates one local source only, using the process's common
monotonic clock. It does not add multi-port UI, a multi-transport controller,
remote clock synchronization or active sniffing hardware control. A later
multi-source implementation may directly merge sources from one process or one
remote agent; it must use shared-clock ordering. Independent hosts require a
separate reviewed synchronization protocol and must expose alignment quality or
uncertainty when their clocks cannot be aligned reliably.

All hex, text, decoder, split and interleaved views consume the same ordered
event stream. They must not synchronize separate terminal/display buffers or
make decoder output authoritative.

## Consequences

- `.kpsession` v1 is source-aware from its first writer, even when M9 records
  only one source.
- Passive dual-port capture can model both ports as Rx without losing their
  semantic communication roles.
- Network jitter is excluded from the captured per-source timing; cross-host
  order remains explicitly uncertain until aligned.
- Bit-level electrical order is outside Komport's observation model; a logic
  analyzer remains appropriate for that requirement.

## References

- `docs/komport-multiport-sniffer-time-alignment.md`
- ADR-002 through ADR-006
- ADR-009
- `docs/specs/SPEC-M8-session-transport-foundation.md`
