# ADR-002: SessionEvent v1 as the common communication model

Status: Accepted
Date: 2026-09-17

## Context

`KomportSerial` currently exposes individual `receivedChar` and `sentChar`
signals. This is sufficient for terminal rendering, but loses the observed
receive/write chunk boundaries and provides neither a stable sequence nor a
timestamp. The existing text logger also keeps only decoded RX text.

Recording, offline decoding, replay, simulation and remote transports require
one byte-exact representation shared by live and offline paths.

## Decision

Introduce `SessionEvent` v1 as the only model crossing session, recorder,
decoder, replay and future remote boundaries:

```cpp
enum class SessionDirection : quint8 { None = 0, Tx = 1, Rx = 2 };
enum class SessionEventType : quint16 {
    Data = 1, TransportOpened = 2, TransportClosed = 3,
    TransportConfigChanged = 4, LineStateChanged = 5, Error = 6,
    Annotation = 7, Bookmark = 8
};
struct SessionEvent {
    quint64 sequence;
    quint32 sourceId;
    qint64 sourceTimestampNs;
    qint64 timestampNs;
    SessionEventType type;
    SessionDirection direction;
    QByteArray payload;
    QJsonObject metadata;
};
```

`sequence` is strictly increasing within one session and begins at one. A
session may contain several transport activations (open, close and reopen of the
same source); `TransportOpened` and `TransportClosed` events mark them, and
`sequence` continues across them. An activation never resets `sequence`.

`sourceId` identifies the configured capture source and is independent of
`direction`; physical capture sources use non-zero IDs, while zero is reserved
for future session-wide non-data events. `sourceTimestampNs` is the original,
non-negative monotonic observation time in that source's clock domain.
`timestampNs` is the non-negative session-timeline time derived from the source
time by the mapping of ADR-005 (session-origin-relative; for one source in one
clock domain the two values differ only by that domain's reference). A future
alignment must never overwrite `sourceTimestampNs`.

For `Data`, direction is exactly `Tx` or `Rx`; `payload` is the non-empty,
unmodified binary byte chunk observed by the transport. Non-data events
use `None` and an empty payload. Metadata is structured JSON-compatible data;
binary data belongs only in `payload`. Metadata is empty for `Data` events in
M8; an empty metadata object is encoded as a zero length (ADR-006), not as
`null`.

Lossless encoding applies to the values that are 64-bit and appear in JSON:
`sequence`, both timestamps, and any future 64-bit quantity such as an alignment
mapping's reference point or offset. Those are written as canonical decimal
strings, because JSON numbers are IEEE-754 doubles and lose integer precision
beyond 2^53 (about 104 days of nanoseconds). 32-bit quantities keep their
specified widths and remain JSON numbers, which represent them exactly:
`sourceId` is `quint32` in the event model and `u32` in the record, `eventType`
is `u16`, `direction` and `flags` are `u8`, and every length field is `u32`. A
JSON number must never be assumed to carry a full nanosecond value, and a reader
must never silently truncate.

The event is immutable after emission: a consumer must not modify an event and
must not replace stored events with derived data. This is a contract with two
parts, and the producer satisfies both before an event becomes part of the
stream:

- Structural validation is event-local and stateless — `type` is a declared
  enumerator; `direction` is exactly `Tx` or `Rx` if and only if `type == Data`
  and `None` otherwise; `Data` has a non-empty `payload` while every non-data
  event has an empty one; `sourceId` is non-zero for physical/event sources (zero
  is reserved and unused in M8); both timestamps are non-negative; `metadata` is
  JSON-encodable or empty. It is implemented once as a free function and used by
  the loader, replay and tests.
- Stream validation is stateful and belongs to the producing controller —
  `sequence` starts at one and strictly increases within the session; session
  time is non-decreasing **within each clock domain** (ADR-005; different clock
  domains are ordered only by `sequence` and arrival order until a versioned
  cross-domain mapping exists, ADR-008); every event's source resolves to its
  clock domain and uses that domain's single reference.

`SessionController` is the factory for M8's live events and the only place where
an event enters the stream, so both parts are enforced on emission and are
verifiable through the deterministic transport double. `SessionEvent` stays a
plain aggregate; a stricter type-level encoding (private members with accessors)
is not required by M8.

## Consequences

- One model can be serialized, replayed and decoded without knowing the
  original transport.
- Existing character signals become compatibility/UI adapters, not the source
  of recorder truth.
- The session layer must preserve chunks and order even when terminal display
  continues to consume individual characters.
- A source can be described and persisted independently of its generic TX/RX
  direction, allowing passive dual-RX sniffing and later multi-source capture
  without redefining the event model.

## Alternatives considered

- Keep text logging as the session format: rejected; it loses bytes, TX,
  chunks and timing.
- Record only a flattened byte stream: rejected; direction and chunk timing
  are required for analysis and simulation.
- Persist `QVariantMap`/`QDataStream` directly: rejected; this is not a stable
  public format. `QJsonObject` is only the in-memory metadata representation;
  ADR-006 defines the file encoding.

## References

- `docs/komport-session-replay-simulation-architecture.md`, sections 5–8
- `docs/komport-engineering-governance-spec-review-workflow.md`, sections 16.1–16.2
- ADR-005, ADR-006, ADR-008
