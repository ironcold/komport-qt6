# ADR-006: `.kpsession` v1 file-format strategy

Status: Proposed
Date: 2026-09-17

## Context

Saved sessions must retain arbitrary binary payloads, order, direction and
timing across Qt versions and platforms. The existing document save/open stubs
and text logger cannot provide this.

## Decision

Use a new binary-safe, append-only `.kpsession` format. v1 is explicitly
little-endian and consists of:

```text
8 bytes   magic: KPSN 0x1A CR LF NUL
u16       format version: 1
u16       header encoding: 1 (UTF-8 JSON)
u32       header byte length
bytes     UTF-8 JSON header
records... length-prefixed SessionEvent records
```

Each record has a fixed little-endian prefix followed by UTF-8 JSON metadata
and raw payload bytes:

```text
u32 recordLength (bytes following this field)
u16 eventType
u8  direction
u8  flags (must be zero in v1)
u64 sequence
u32 sourceId
i64 sourceTimestampNs
i64 timestampNs
u32 metadataLength
u32 payloadLength
bytes metadata JSON
bytes payload
```

The reader validates every length before allocating: header at most 16 MiB,
record at most 64 MiB, and `metadataLength + payloadLength` must exactly match
the remaining record body. Invalid magic/version/header or a malformed
complete record fails loading. A truncated final record is ignored and reported
as recovered, never treated as a valid complete event.

The JSON header contains format/version, wall-clock creation time, application
version, an array of source descriptors/configuration snapshots, capture clock
domains, optional decoder hints and notes. A one-source M9 file writes an
array with exactly one physical source. Each descriptor carries its non-zero
`sourceId`; its semantic role is metadata and does not redefine the event's
generic TX/RX direction. No M9 alignment mapping is written or interpreted.
A later, separately specified extension may add versioned derived mapping
metadata; it must never replace the raw per-event `sourceTimestampNs`. The
header must never contain decoded output as authoritative data.

## Consequences

- The format has no dependency on `QDataStream` or Qt private serialization.
- Future versions can add flag-gated fields or new event types while v1 readers
  reject unsupported critical versions rather than misread them.
- CRCs, indexes, compression and simulation overlays remain later extensions.
- Sessions captured by one source remain straightforward, while a later
  multi-source recorder need not invent an incompatible second format.

## References

- `docs/komport-session-replay-simulation-architecture.md`, section 7
- ADR-002, ADR-004, ADR-005
- ADR-008
