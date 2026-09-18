# ADR-003: ITransport v1 and the session-controller boundary

Status: Proposed
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
    virtual bool writeBytes(const QByteArray &bytes) = 0;
signals:
    void bytesReceived(const QByteArray &bytes, qint64 sourceTimestampNs);
    void bytesWritten(const QByteArray &bytes, qint64 sourceTimestampNs);
    void opened(qint64 sourceTimestampNs, const QJsonObject &metadata);
    void closed(qint64 sourceTimestampNs, const QJsonObject &metadata);
    void configurationChanged(qint64 sourceTimestampNs, const QJsonObject &metadata);
    void lineStateChanged(qint64 sourceTimestampNs, const QJsonObject &metadata);
    void transportError(qint64 sourceTimestampNs, const QJsonObject &metadata);
};
```

Timestamps are monotonic and relative to a single transport activation. The
source that observes bytes creates them: local `SerialTransport` on its Qt
thread, a future remote adapter at the remote agent. `ITransport` neither
stores sessions nor decodes bytes.

`SessionController` is the sole adapter from transport signals to
`SessionEvent`: it assigns sequence numbers, associates the controller-owned
source descriptor/ID, maps the transport-relative clock to its session-relative
clock, and emits events to recorder/UI/decoders. `ITransport` deliberately has
no semantic source role: a passive sniffer source may emit only generic `Rx`
while its controller-owned descriptor says `controller_to_device`. M8 has one
active transport source; this is an implementation boundary, not an implicit
constraint of `SessionEvent` or `.kpsession` v1 (ADR-008).

`KomportSerial` is migrated incrementally into the local serial
implementation and may keep its legacy character methods/signals during the
transition. No new feature may bypass `ITransport` or `SessionController`.

## Consequences

- `QSerialPort` stays below the transport boundary.
- Replay and future TCP/remote transports can feed the same controller.
- Existing UI migration can be staged through compatibility adapters rather
  than a flag-day rewrite.
- A future multi-source controller can attach additional transports without
  making source identity a transport-specific or direction-specific concept.

## Non-goals

- TCP, remote agents, PTYs and replay transports are not part of v1.
- This ADR does not change serial configuration UX or file-transfer behavior.

## References

- `docs/komport-session-replay-simulation-architecture.md`, sections 20–32
- ADR-002
