# Implementation Spec: M8 Session and Transport Foundation

Status: Draft — implementation blocked pending ADR review

Date: 2026-09-17

## 1. Objective

Create the transport-neutral, byte-exact live-event foundation required by
recording, loading, passive replay, decoders and later network transports.
This slice makes the current local serial path produce ordered `SessionEvent`
objects without changing visible terminal behavior or transmitting any new
bytes.

## 2. Scope

Included:

- `SessionEvent`, direction and event-type value types from ADR-002/004/005.
- `ITransport` v1 from ADR-003.
- Migration of `KomportSerial` into the local `ITransport` implementation
  while preserving its existing character-oriented public API as a temporary
  compatibility adapter.
- A `SessionController` owned by `KomportDoc`, which converts current live
  serial observations to ordered events and emits `eventObserved`.
- Unit and PTY tests proving chunk, direction, sequence, timestamp ordering
  and all-byte preservation.

## 3. Non-goals

- No `.kpsession` reader/writer, load/save UI or replacement of the current
  text logger.
- No replay, simulation, TCP, remote agent, decoder framework or UI timeline.
- No change to serial settings, charset translation, macros or file transfer
  semantics.
- No removal of legacy `receivedChar`/`sentChar` signals in M8.

## 4. Architecture References

- `docs/komport-session-replay-simulation-architecture.md`, sections 5–8,
  20–23, 39 and 42.
- `docs/komport-engineering-governance-spec-review-workflow.md`, sections
  7–10 and 16.
- ADR-002 through ADR-007.

## 5. Current State

`KomportSerial` owns `QSerialPort`, reads into `mRxBuffer` and later emits
individual `receivedChar(char)` values. `putStr()` sends a batch but emits
individual `sentChar(char)` values. `KomportView`, emulation, Hex monitor and
the text logger consume those character signals directly. Thus current
terminal behavior is correct for its purpose but is not a session source:
observed chunks, TX batch boundaries, monotonic timing and event order are not
represented.

## 6. Proposed Design

### 6.1 New components

- `sessionevent.h`: `SessionEvent`, `SessionDirection`, `SessionEventType`.
- `itransport.h`: `ITransport` and its binary chunk notifications.
- `sessioncontroller.h/.cpp`: binds one `ITransport` to one live session;
  assigns `sequence`, normalizes the monotonic source timestamp to the
  session origin, emits `SessionEvent`.

### 6.2 Existing components

`KomportSerial` implements `ITransport` and emits exactly one RX observation
for every successful `QSerialPort::readAll()` result, before its existing
buffering/character compatibility path. It emits exactly one TX observation
only after a complete successful `QSerialPort::write()` call; partial/failed
writes remain failures and emit no successful TX data event.

Existing `receivedChar`/`sentChar` signals continue with their present behavior
and remain responsible for legacy terminal/Hex/logger display. They are not
used by `SessionController`.

While live, `KomportSerial` also emits a configuration observation after a
successfully applied settings change. Its metadata captures only the effective
serial settings (endpoint, baud, data bits, parity, stop bits and flow control)
and never credentials. Modem-line events are part of the frozen `ITransport`
surface but remain unimplemented until a transport actually exposes them.

`KomportDoc` owns the transport and `SessionController`; it exposes only the
controller's read-only event signal to new consumers. No Qt widget gains a
`QSerialPort` dependency.

### 6.3 Data flow

```text
QSerialPort
  -> KomportSerial / ITransport (binary RX or successful TX chunk)
  -> SessionController
  -> SessionEvent
  -> future recorder / decoder / passive replay UI

KomportSerial compatibility signals
  -> existing terminal, hex monitor, text logger
```

The two branches observe the same wire data but have different purposes.
M8 does not replace terminal rendering with SessionEvent delivery.

## 7. State model

`SessionController` has `Idle`, `Live`, `Closed`.

- construction: `Idle`
- transport opened: begin a new one-source live session, reset sequence to one
  and capture the transport-origin offset: `Live`
- observations in `Live`: emit ordered events
- transport closed or destruction: emit closing event once, then `Closed`

A failed open produces an error observation but does not enter `Live`.

## 8. Invariants

- `QByteArray` payload is copied byte-for-byte, including NUL and all values
  `00..FF`.
- One transport observation produces one `Data` event; M8 never coalesces or
  splits it.
- `sequence` strictly increases; equal timestamps are still ordered by
  sequence.
- RX and TX use ADR-004 semantics.
- No live event is created from terminal-decoded text or charset-converted
  data.
- M8 does not write through `SessionController`; it cannot create a new
  hardware-output path.

## 9. Error handling

- Partial/failed serial writes keep current `false` return and warning; no
  success TX event is emitted.
- A transport error becomes an `Error` event with structured non-secret
  metadata when a session is live; the existing user-facing error path remains.
- Empty successful chunks are ignored; an empty data event is never emitted.
- A timestamp that would violate non-decreasing order is clamped to the last
  emitted timestamp and logged as a transport/controller anomaly.

## 10. Threading and concurrency

M8 is main-Qt-thread only. `KomportSerial`, `SessionController` and legacy UI
connections share the owning document's thread. Signals are direct within that
thread; no queue, lock, worker or cross-thread lifetime rule is introduced.

## 11. Persistence, replay and network impact

M8 introduces no persistence and no replay. Its events satisfy ADR-006's
future writer input. One active local transport is deliberate; remote adapters
will supply the same observation contract with source-side timestamps. Passive
replay later feeds `SessionEvent` into the same downstream consumer interface,
but is not a transport or writer in this milestone.

## 12. Safety

M8 neither opens a transport automatically nor introduces a write API beyond
the existing user-driven serial calls. It cannot replay or simulate traffic.
ADR-007 remains binding for later milestones.

## 13. Testing strategy

Unit tests:

- value semantics for every event type/direction invariant;
- sequence reset on a new live session and strict increment thereafter;
- timestamp normalization and non-decreasing clamp.

PTY integration tests:

- RX data with embedded NUL and values spanning `00..FF` produces one
  byte-identical RX event per observed read chunk;
- length-aware TX with embedded NUL produces one byte-identical TX event;
- failed/partial writes produce no successful TX event;
- open, close, error and successful effective-setting changes produce the
  corresponding non-data events with `None` direction;
- ordering for interleaved TX/RX is deterministic.

Regression tests:

- all existing `tst_serial`, emulation, charset and UI tests still pass;
- Hex monitor and text logger retain their current character-signal behavior.

## 14. Acceptance criteria

- [ ] Core types and transport contract compile with Qt 6.3.
- [ ] Current serial I/O exposes binary chunk observations without changing
  legacy public calls/signals.
- [ ] A document-owned controller emits ordered, byte-exact live events.
- [ ] Open, close, error and effective serial configuration are observable as
  non-data events without changing their existing UI behavior.
- [ ] NUL/all-byte PTY tests pass.
- [ ] Existing full `ctest` suite passes.
- [ ] No file format, replay UI, decoder or active transmission is introduced.

## 15. Risks

| Risk | Mitigation |
| --- | --- |
| Raw chunk notification subtly changes terminal behavior | It is additive and emitted before the existing buffer/character path; existing tests remain required. |
| `QSerialPort::write()` buffering is mistaken for a physical wire guarantee | Event semantics are "accepted complete write by transport", not electrical delivery; document this in code/API. |
| Two event paths drift | New recorder/decoder code is forbidden to consume legacy character signals. |
| Full UI migration expands scope | M8 explicitly preserves compatibility adapters; migration is a later spec. |

## 16. Decision required

None. The decisions needed for M8 are proposed in ADR-002 through ADR-007.
They must be reviewed and accepted before implementation begins.

## 17. Implementation plan

1. Review and accept ADR-002 through ADR-007 and this spec.
2. Add core value/interface types and CMake entries.
3. Implement additive raw-chunk observations in `KomportSerial`.
4. Implement document-owned `SessionController`.
5. Add focused unit/PTY tests and run the complete test suite.
6. Perform self-review and an independent review against this specification.
