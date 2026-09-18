# ADR-003: ITransport v1 and the session-controller boundary

Status: Accepted
Date: 2026-09-17

## Context

The current UI, emulation, transfer code and diagnostics all reach
`KomportSerial` directly. Although this already isolates `QSerialPort` from
most widgets, it cannot make replay, TCP or a remote agent look like the same
source without special paths.

## Decision

Introduce a `QObject`-based `ITransport` v1. It owns byte movement only:

```cpp
class ITransport : public QObject {
    Q_OBJECT
public:
    virtual ~ITransport() = default;
    virtual bool open() = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
    // Sends bytes and returns the number of bytes the transport accepted
    // (0..bytes.size()); a negative value means the write was refused.
    virtual qint64 writeBytes(const QByteArray &bytes) = 0;
signals:
    void bytesReceived(quint64 activationId, const QByteArray &bytes, qint64 sourceTimestampNs);
    void bytesWritten(quint64 activationId, const QByteArray &bytes, qint64 sourceTimestampNs);
    void opened(quint64 activationId, qint64 sourceTimestampNs, const QJsonObject &metadata);
    void closed(quint64 activationId, qint64 sourceTimestampNs, const QJsonObject &metadata);
    void configurationChanged(quint64 activationId, qint64 sourceTimestampNs, const QJsonObject &metadata);
    void lineStateChanged(quint64 activationId, qint64 sourceTimestampNs, const QJsonObject &metadata);
    void transportError(quint64 activationId, qint64 sourceTimestampNs, const QJsonObject &metadata);
};
```

`bytesReceived`/`bytesWritten` report bytes observed/accepted by the transport
API; for a partially accepted write, `bytesWritten` carries exactly the accepted
prefix, and "accepted" never means "physically delivered". `KomportSerial`'s
`bool` compatibility methods keep their signatures and return `true` only when
all requested bytes were accepted.

Timestamps are non-negative monotonic nanoseconds in the observing source's
clock domain. A source uses one monotonic clock for the lifetime of its clock
domain (process or agent) and never a per-activation zero; a clock domain is
identified by a stable identifier (ADR-006). The source that observes bytes
creates the timestamps: local serial uses the process-wide clock defined in
ADR-005, a future remote adapter uses its own agent-wide clock, and observations
within one clock domain remain directly comparable across activations.
`ITransport` neither stores sessions nor decodes bytes and does not assign
session time — that is the controller's mapping (ADR-005).

**Activation identity.** `activationId` starts at 1 for a transport instance and
strictly increases on every `open()` attempt; a failed attempt consumes an id and
never produces an `opened` for it. Every observation of an activation carries
that activation's id, and an attempt's failure is reported exactly once, by the
transport, using an internal in-progress/result guard rather than any text
comparison.

**Emission order versus delivery.** A conforming transport emits `closed()` as
the last signal it emits for an activation and never emits a **non-error**
observation carrying an activation id for which no `opened()` was emitted. The
single `transportError` of a failed `open()` attempt is the only observation that
may carry such an id. Signal emission order and signal delivery order are
different things, however: a consumer must not rely on arrival order, because a
connection may be queued, an event loop may delay delivery, or a transport may be
faulty. The controller therefore filters every observation by `activationId`
(SPEC-M8) instead of trusting order.

**Synchronous open result.** `ITransport` v1 requires `open()` to report its own
result synchronously, as the local serial transport does and as the profile-error
regression depends on. A transport whose open completes asynchronously must
define its own correlation and is not part of v1.

`SessionController` is the sole adapter from **live transport observations** to
`SessionEvent`: it assigns sequence numbers, associates the controller-owned
source descriptor/ID, maps source time to session time (ADR-005) and emits events
to recorder, views and decoders. `ITransport` deliberately has no semantic source
role: a passive sniffer source may emit only generic `Rx` while its
controller-owned descriptor says `controller_to_device`.

Passive replay is not a live transport and does not use `ITransport`: it
distributes stored, immutable `SessionEvent` values — including their stored
sequence numbers, source identity, event types, annotations and times — through a
read-only replay-player interface whose exact shape is specified in M10. This ADR
and ADR-007 state only the constraints that interface must respect (live/replay
separation and safety); neither ADR defines the interface.

M8 has one active transport source; that is an implementation boundary, not an
implicit constraint of `SessionEvent` or `.kpsession` v1 (ADR-008).

Every byte-transmitting entry point of an `ITransport` implementation — the
legacy `putChar()` and both `putStr()` overloads as well as `writeBytes()` — is
implemented on one internal write primitive, and that primitive alone creates a
TX observation. A compatibility method that writes bytes without producing an
observation would be exactly the bypass this ADR forbids.

`KomportSerial` is migrated incrementally into the local serial implementation
and may keep its legacy character methods and signals during the transition. M8
legacy callers may continue to use them directly, provided every such call routes
through the single observed write primitive. M8 adds no new UI-side transport
API, and no new consumer may derive session data from character signals; those
signals remain display adapters (ADR-002). No new feature may bypass `ITransport`
or `SessionController`.

One accepted API write produces exactly one transport-observation chunk. This
records the application/API acceptance boundary, not physical wire framing:
`QSerialPort::write()` acceptance is not electrical delivery. A single logical
action may therefore produce several events (the emulation sends a cursor or
insert key as `putChar(ESC)` followed by one or more `putStr()` calls). The bytes
stored in an event are never rewritten, and an emitted or persisted `SessionEvent`
is immutable. Decoders and views may derive reassembled byte streams and protocol
frames for their own display; such derived framing is a view of the stored
events, must never be written back, and must never replace what the stored events
contain.

Observation is byte-exact at the application transport-observation boundary: RX
is exactly the bytes `readAll()` returned, TX exactly the bytes `write()`
accepted. It is not an electrical or logic-analyzer record: no
`waitForBytesWritten()` acknowledgement is implied, and bytes still buffered
below the application layer when a port closes are never observed. (ADR-008
already states that bit-level electrical order is outside Komport's observation
model.)

A controller must not outlive its transport, must never be destroyed after it,
and must not emit events or call into the transport from its destructor. Where a
transport is a by-value member of its owner (as `KomportSerial` is in
`KomportDoc`), the controller's destruction order relative to that member must be
explicit; relying on `~QObject` child destruction is not acceptable, because Qt
destroys children after the owner's own members.

## Consequences

- `QSerialPort` stays below the transport boundary.
- Future TCP and remote transports can feed the same controller as live
  transports, because they are live byte-movement transports. Passive replay is
  not a transport (see above).
- Existing UI migration can be staged through compatibility adapters rather
  than a flag-day rewrite.
- A future multi-source controller can attach additional transports without
  making source identity a transport-specific or direction-specific concept.

## Non-goals

- TCP, remote agents, PTYs and replay transports are not part of v1.
- This ADR does not change serial configuration UX or file-transfer behavior.

## References

- `docs/komport-session-replay-simulation-architecture.md`, sections 20–32
- ADR-002, ADR-005, ADR-006, ADR-007
