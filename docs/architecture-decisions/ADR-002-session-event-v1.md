# ADR-002: SessionEvent v1 as the common communication model

Status: Proposed
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

`sequence` is strictly increasing within one session and begins at one.
`sourceId` identifies the configured capture source and is independent of
`direction`; physical capture sources use non-zero IDs, while zero is reserved
for future session-wide non-data events. `sourceTimestampNs` is the original,
non-negative monotonic observation time in that source's clock domain.
`timestampNs` is the non-negative session-timeline time derived from the source
time; for M8's sole local source both values use the same common monotonic
clock. Future alignment must never overwrite `sourceTimestampNs`.
For `Data`, direction is exactly `Tx` or `Rx`; `payload` is the non-empty,
unmodified binary byte chunk observed by the transport. Non-data events
use `None` and an empty payload. Metadata is structured JSON-compatible data;
binary data belongs only in `payload`.

The event is immutable after emission. Views and decoders may derive data but
must not modify the event or use derived data as its replacement.

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
- ADR-008
