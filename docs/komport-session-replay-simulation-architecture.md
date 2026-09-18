# Komport Qt 6 – Session Recording, Replay, Simulation and Network Architecture

## Status

Design proposal for the Komport Qt 6 project.

The goal is to evolve Komport from a serial terminal into a reusable communication, analysis, replay and simulation tool without coupling the implementation to one transport or one protocol.

The design intentionally keeps the raw communication data authoritative. Decoded or interpreted representations are always derived from the original session data.

---

# 1. Goals

Komport should support the following workflows:

1. Record complete communication sessions.
2. Preserve TX/RX direction, raw bytes, timing and transport metadata.
3. Reload sessions later for offline analysis.
4. Apply different decoders to an existing session without modifying the recording.
5. Export sessions as raw bytes, hex dumps, decoded text or combined reports.
6. Replay recorded traffic locally inside Komport.
7. Replay recorded TX traffic against real hardware.
8. Simulate a previously recorded device.
9. Allow protocol-aware simulation where decoded fields may be changed.
10. Support serial, RS-232, RS-485 and future network transports without redesigning the session format.
11. Support remote/network-connected Komport instances later.
12. Keep the implementation safe: loading a session must never automatically transmit data to real hardware.

---

# 2. Architectural Principle

The central rule is:

> Raw session events are the source of truth. Everything else is a view or interpretation.

A session must not primarily store rendered terminal text.

It should store transport events:

```text
timestamp
direction
raw bytes
transport metadata
event metadata
```

The display pipeline is derived from those events:

```text
Transport
    |
    v
Session Event Stream
    |
    +----> Recorder
    |
    +----> Hex Viewer
    |
    +----> Text Renderer
    |
    +----> Protocol Decoder
    |
    +----> Statistics / Analysis
```

The same event stream must also be usable when no physical device is connected:

```text
Session File
    |
    v
Replay Engine
    |
    v
Session Event Stream
    |
    +----> Viewer
    +----> Decoder
    +----> Simulator
```

This separation is especially important for future network support.

---

# 3. Important Terminology

## 3.1 Transport

A transport moves bytes.

Examples:

- local serial port
- RS-232
- RS-485
- USB-to-serial converter
- TCP client
- TCP server
- remote Komport agent
- virtual PTY
- replay source

The decoder must not need to know whether bytes originally came from `/dev/ttyUSB0`, COM3 or a remote agent.

## 3.2 Session Event

A timestamped unit of communication.

Typical events:

- TX data
- RX data
- transport opened
- transport closed
- serial settings changed
- line state changed
- error
- annotation/bookmark
- replay marker

## 3.3 Decoder

A decoder interprets recorded bytes.

Examples:

- plain text / ASCII
- PETSCII
- Modbus RTU
- Modbus ASCII
- proprietary RS-485 protocol
- C64 BASIC token decoder
- MOS 6502 / 6510 disassembler

A decoder does not own the bytes and must never alter them.

## 3.4 Replay

Previously captured session events are processed again.

Replay may be purely internal or may cause bytes to be transmitted.

## 3.5 Simulation

Komport acts as one side of a recorded communication.

Simulation is therefore not identical to replay. A simulator may wait for input, compare it against recorded or decoded requests and generate a response.

---

# 4. Core Components

Recommended logical components:

```text
komport-core
|
+-- Transport API
|   +-- SerialTransport
|   +-- TcpTransport
|   +-- RemoteTransport
|   +-- ReplayTransport
|   +-- PtyTransport
|
+-- Session Engine
|   +-- SessionEvent
|   +-- SessionRecorder
|   +-- SessionReader
|   +-- SessionWriter
|
+-- Decoder API
|   +-- TextDecoder
|   +-- TranslationTableDecoder
|   +-- ModbusDecoder
|   +-- ...
|
+-- Replay Engine
|
+-- Simulation Engine
|
+-- Export Engine
|
+-- UI
```

These may initially live in one repository and even one library, but the interfaces should be kept separate.

---

# 5. Session Event Model

A session should consist of an ordered stream of events.

A suggested in-memory representation:

```cpp
enum class SessionDirection {
    None,
    Tx,
    Rx
};

enum class SessionEventType {
    Data,
    TransportOpened,
    TransportClosed,
    TransportConfigChanged,
    LineStateChanged,
    Error,
    Annotation,
    Bookmark
};

struct SessionEvent {
    quint64 sequence;
    quint32 sourceId;
    qint64 sourceTimestampNs;
    qint64 timestampNs;
    SessionEventType type;
    SessionDirection direction;
    QByteArray payload;
    QVariantMap metadata;
};
```

`sourceId` identifies a configured capture source and is independent of
direction. `sourceTimestampNs` is the immutable monotonic observation time in
that source's clock domain. `timestampNs` is the derived time on the common
session timeline; with a single local source it can use the same clock.

Source descriptors (name, transport/configuration and optional semantic role)
belong in the session header. A passive dual-port sniffer can therefore retain
`Rx` for both physical receive ports while labeling their roles as
controller-to-device and device-to-controller. Do not encode those roles by
redefining TX/RX.

A separate wall-clock timestamp should be stored in the session header.

This avoids problems when the system clock changes during a recording.

## 5.1 Why relative monotonic timing matters

Communication timing may itself be protocol-relevant.

Examples:

- Modbus RTU frame boundaries
- inter-character delays
- device response time
- bootloader timing
- timeouts
- delays between commands

Therefore a session should preserve actual observed timing as accurately as reasonably possible.

Use a monotonic timer while recording.

Wall-clock timestamps are useful for humans but must not be used as the primary replay clock.

---

# 6. Chunk Preservation

Do not collapse an entire RX or TX direction into one byte stream.

Preserve the chunks as observed by the application.

Example:

```text
+0.000000 TX  01 03 00 10 00 02 C5 CE
+0.008421 RX  01 03 04
+0.009105 RX  00 64 00 32
+0.009801 RX  7A 1B
```

This is useful because:

- timing is preserved,
- framing heuristics remain possible,
- real device behavior can be studied,
- transport buffering remains observable.

However, decoders must not assume that one chunk equals one protocol frame.

A decoder must be able to consume an arbitrary byte stream assembled from several events.

---

# 7. Session File Format

## 7.1 Requirements

The session format should be:

- binary-safe
- versioned
- extensible
- streamable
- reasonably compact
- recoverable after partial writes
- independent of Qt internal serialization versions
- independent of selected decoder
- capable of storing arbitrary NUL bytes
- capable of storing metadata
- usable across platforms

## 7.2 Recommended Format

Use a container with:

1. fixed magic header
2. version
3. JSON metadata header
4. length-prefixed binary event records

Suggested extension:

```text
.kpsession
```

Suggested magic:

```text
KOMPORTSESSION
```

### File layout

```text
+------------------------------+
| Magic                        |
+------------------------------+
| Format version               |
+------------------------------+
| Header length                |
+------------------------------+
| JSON session header          |
+------------------------------+
| Event record                 |
+------------------------------+
| Event record                 |
+------------------------------+
| ...                          |
+------------------------------+
```

## 7.3 Session Header

Example:

```json
{
  "format": "komport-session",
  "version": 1,
  "created": "2026-09-17T14:30:00+02:00",
  "application": {
    "name": "Komport",
    "version": "6.x"
  },
  "sources": [{
    "id": 1,
    "name": "local serial",
    "transport": {
      "type": "serial",
      "endpoint": "/dev/ttyUSB0",
      "settings": {
        "baud": 9600,
        "dataBits": 8,
        "parity": "none",
        "stopBits": 1,
        "flowControl": "none"
      }
    }
  }],
  "capture": {
    "clock": "monotonic",
    "timestampResolution": "ns"
  },
  "decoderHints": [
    "modbus-rtu"
  ],
  "notes": ""
}
```

`decoderHints` are suggestions only.

The session must remain readable without the corresponding decoder.

## 7.4 Event Record

Do not serialize `QVariantMap` directly with `QDataStream` as the long-term public file format.

A stable explicit record format is preferable.

Conceptually:

```text
record length
event type
direction
sequence
source ID
source-local timestamp
aligned session timestamp
metadata length
metadata JSON
payload length
payload bytes
```

Potential fixed header:

```cpp
struct EventRecordHeaderV1 {
    quint32 recordLength;
    quint16 type;
    quint8 direction;
    quint8 flags;
    quint64 sequence;
    quint32 sourceId;
    qint64 sourceTimestampNs;
    qint64 timestampNs;
    quint32 metadataLength;
    quint32 payloadLength;
};
```

All integer byte ordering must be specified by the file format.

Little-endian would be reasonable, but the choice is less important than documenting it.

## 7.5 Crash Recovery

The recorder should append complete records.

A partially written final record should be ignored when loading.

This allows useful session recovery after:

- application crash
- host crash
- power loss
- storage failure

Optional later enhancement:

- per-record CRC
- file index/footer
- compressed event blocks

These are not required for format version 1.

---

# 8. Raw Data Must Remain Authoritative

Decoded content should normally not be permanently embedded as authoritative session content.

Reason:

A decoder may later be improved.

Example:

```text
Session captured in 2026
    |
    +-- decoder v1: unknown command 0x17
    |
    +-- decoder v2: SetMotorCurrent current=2.5 A
```

The same session should benefit from the new decoder.

Decoder output may optionally be cached, but caches must be disposable.

---

# 9. Replay Modes

Replay should be explicitly divided into modes.

## 9.1 Passive Replay

Purpose:

- offline analysis
- demonstrations
- decoder development
- reproducing UI behavior
- debugging

No bytes leave Komport.

The recorded session events are emitted again internally according to replay timing.

Options:

```text
Timing:
- Original timing
- Immediate
- 0.1x
- 0.25x
- 0.5x
- 1x
- 2x
- 5x
- 10x
- Step mode
```

Step mode should allow:

- next event
- next TX event
- next RX event
- next decoded frame

Passive replay is always safe and may be started directly after loading a session.

---

# 10. Active TX Replay

Active TX Replay sends recorded TX data to an actual transport.

Recorded RX data is not transmitted.

Use cases:

- reproduce a command sequence
- retest a real device
- reproduce initialization
- investigate device regressions

Example:

```text
Recorded:
TX request A
RX response A
TX request B
RX response B

TX Replay:
send request A
wait recorded delay
send request B
```

Optional timing policies:

```text
Original TX timing
Fixed delay between TX events
Immediate
Scaled timing
Manual step
```

## Safety

Active replay must require explicit user action.

Loading a session must never enable active replay automatically.

The UI should clearly show:

```text
PASSIVE
```

versus:

```text
ACTIVE REPLAY -> /dev/ttyUSB0
```

Potential future safety controls:

- require confirmation before first transmission
- dry run
- maximum transmitted bytes
- restrict replay to selected event range
- warning if transport settings differ from recorded settings

---

# 11. Full Duplex Blind Replay

A separate mode may replay both directions to two endpoints.

Example use case:

```text
Endpoint A <-> Komport <-> Endpoint B
```

This should not be an MVP feature.

It is more complex because TX/RX semantics depend on viewpoint.

If implemented, session directions must always be defined relative to the original Komport transport:

```text
TX = Komport sent to device
RX = Komport received from device
```

This definition must never change.

---

# 12. Device Simulation Modes

## 12.1 Static Device Simulation

Komport replaces the recorded device.

Assume the original recording contains:

```text
TX request
RX response
TX request
RX response
```

In simulation mode:

```text
External application -> Komport
request
Komport -> External application
recorded response
```

The simulator waits for an incoming request and compares it to the recorded request.

Possible comparison modes:

```text
Exact byte match
Frame match
Decoder-aware match
Masked byte match
```

Exact matching should be implemented first.

---

# 13. Sequential Simulation

The simplest simulator can follow a recorded script.

Example:

```text
State 0:
expect request A
send response A

State 1:
expect request B
send response B

State 2:
expect request C
send response C
```

On mismatch:

```text
- stop
- log mismatch
- optionally allow user to skip
```

This mode is deterministic and useful for automated tests.

---

# 14. Decoder-Aware Simulation

This is a later and more powerful mode.

Instead of requiring exact bytes:

```text
01 03 00 10 00 02 C5 CE
```

the decoder may produce:

```text
Protocol: Modbus RTU
Slave: 1
Function: Read Holding Registers
Start: 16
Count: 2
```

Simulation rules can then match fields.

Example:

```text
when:
    protocol: modbus-rtu
    slave: 1
    function: 3
    register: 16

respond:
    registers:
        16: 100
        17: 50
```

This transforms Komport from a recorder into a lightweight device simulator.

---

# 15. Parameterized Simulation

A protocol-aware simulator should eventually allow users to modify values.

Example:

```text
Recorded:
Temperature = 21.5 C
Pressure = 1012 hPa
```

Simulation:

```text
Temperature = 80 C
Pressure = 850 hPa
```

Useful for:

- testing alarm handling
- fault injection
- UI development without hardware
- regression testing
- training/demo environments

This must be built on decoder-defined semantic fields rather than editing arbitrary offsets whenever possible.

---

# 16. Fault Injection

Potential later feature:

```text
- delay response
- drop response
- corrupt checksum
- truncate frame
- send invalid value
- duplicate frame
- send unexpected frame
- disconnect transport
```

Fault injection should be separate from normal replay.

It must never alter the stored source session.

---

# 17. Export Formats

The session format is for lossless reload.

Exports are for humans or external tools.

## 17.1 Raw Export

Options:

```text
TX only
RX only
interleaved raw stream
selected range
```

Output may be `.bin`.

Important:

An interleaved raw stream loses direction information.

The UI should warn about that.

## 17.2 Hex Export

Example:

```text
00000000  01 03 00 10 00 02 C5 CE
00000008  01 03 04 00 64 00 32 7A
```

Optional timestamps and directions:

```text
+0.000000 TX  01 03 00 10 00 02 C5 CE
+0.008421 RX  01 03 04 00 64 00 32 7A 1B
```

## 17.3 Decoded Text Export

Example:

```text
00:00:00.000 TX
Modbus RTU
  Slave: 1
  Function: Read Holding Registers
  Start register: 16
  Count: 2

00:00:00.008 RX
Modbus RTU Response
  Register 16: 100
  Register 17: 50
  CRC: OK
```

## 17.4 Combined Export

Recommended for bug reports:

```text
[00:00:00.000 TX]
01 03 00 10 00 02 C5 CE

Modbus RTU:
  Slave = 1
  Function = Read Holding Registers
  Start = 16
  Count = 2
```

Potential output formats:

- text
- Markdown
- JSON
- CSV for event metadata

---

# 18. Decoder Architecture

The decoder API should operate on session data independently of the live transport.

Conceptual interface:

```cpp
class ProtocolDecoder
{
public:
    virtual ~ProtocolDecoder() = default;

    virtual QString id() const = 0;
    virtual QString displayName() const = 0;

    virtual void reset() = 0;
    virtual void consume(const SessionEvent &event) = 0;

signals:
    void frameDecoded(const DecodedFrame &frame);
};
```

A decoder may require:

- byte stream reconstruction
- timing information
- direction information
- decoder state

Therefore the decoder must receive session events, not only a `QByteArray`.

---

# 19. Decoder Categories

Suggested categories:

```text
Text / Character Decoders
    ASCII
    UTF-8
    PETSCII
    translation tables

Protocol Decoders
    Modbus RTU
    Modbus ASCII
    custom RS-485
    device-specific protocols

Binary / Code Decoders
    6502
    6510
    7501/8501
    Z80
    ...

Format Decoders
    Commodore BASIC tokenized programs
    firmware/file formats
```

A decoder may expose capabilities:

```text
canDecodeLive
canDecodeOffline
canSimulate
canEditParameters
requiresTiming
```

---

# 20. Network Capability – Architectural Order

The planned network capability affects the architecture, but it should NOT be implemented before the local event model is clean.

The focused terminal, future analyzer and future headless agent are peer
frontends over application-neutral contracts (ADR-009). This is a logical
boundary first: it does not require an immediate CMake or directory rewrite.
The analyzer owns advanced multi-source/timeline workflows; the agent owns
remote physical transports. Neither may depend on terminal UI internals.

Recommended order:

## Phase 1 – Transport-neutral event model

Implement first:

```text
SessionEvent
SessionEngine
Transport interface
Decoder interface
```

Do not let UI widgets communicate directly with `QSerialPort`.

Bad:

```text
MainWindow -> QSerialPort
```

Preferred:

```text
MainWindow
    |
    v
Session / Communication Controller
    |
    v
ITransport
    |
    +-- SerialTransport
```

This is the most important architectural decision for later network support.

---

# 21. Phase 2 – Local Serial Transport

Implement the existing serial functionality through the transport abstraction.

Example interface:

```cpp
class ITransport : public QObject
{
    Q_OBJECT

public:
    virtual void open() = 0;
    virtual void close() = 0;
    virtual void writeBytes(const QByteArray &data) = 0;

signals:
    void bytesReceived(const QByteArray &data);
    void bytesWritten(const QByteArray &data);
    void opened();
    void closed();
    void errorOccurred(const TransportError &error);
};
```

The session layer converts these into `SessionEvent` objects.

---

# 22. Phase 3 – Session Recording

Once all serial communication already passes through the transport/session layer, recording becomes straightforward.

Do this before network support.

Reason:

The network layer can then transport the same event model instead of inventing a second communication model.

---

# 23. Phase 4 – Passive Replay

Implement passive replay before active replay.

A session reader should be able to act as an event source.

Conceptually:

```text
SerialTransport
       \
        -> SessionEngine -> UI / Decoder
       /
ReplaySource
```

This tests whether the architecture is actually transport-independent.

If passive replay requires special cases throughout the UI, the abstractions are not clean enough yet.

---

# 24. Phase 5 – Decoder Framework

Once live and replayed events follow the same pipeline, add protocol decoders.

This is a good point to implement:

```text
translation table decoder
Modbus RTU decoder
```

The Modbus RTU decoder is particularly useful because it tests timing-dependent framing.

---

# 25. Phase 6 – Active Replay

Add an output target to the replay engine.

Important separation:

```text
Replay Source
    |
    +-- Passive -> SessionEngine
    |
    +-- Active -> ITransport
```

Active replay must not bypass the transport abstraction.

This automatically allows future active replay over:

- local serial
- TCP
- remote serial agent

---

# 26. Phase 7 – Basic Simulation

Implement exact request/response simulation.

Start with recorded sequential matching.

Do not begin with protocol-aware simulation.

A minimal simulator already provides substantial value.

---

# 27. Phase 8 – Network Transport

At this point network capability becomes much easier.

Possible initial transports:

```text
TcpClientTransport
TcpServerTransport
```

Then later:

```text
RemoteKomportTransport
```

The rest of the application should not care whether the bytes are local or remote.

---

# 28. Network Architecture Recommendation

There are two different meanings of "network support" and they should be kept separate.

## 28.1 Serial-over-Network

Komport connects to:

```text
TCP <-> serial gateway <-> RS-232/RS-485 device
```

This is simply another transport.

Example:

```text
TcpTransport
```

The session recorder still sees TX and RX bytes normally.

## 28.2 Remote Komport Agent

A remote computer owns the physical serial port.

Architecture:

```text
Komport UI
    |
    | network
    v
Komport Agent
    |
    v
Serial Port
    |
    v
Device
```

This is different from a raw serial-to-TCP converter because the remote agent can preserve richer metadata.

It can report:

- exact RX chunks
- TX chunks
- monotonic timestamps
- serial errors
- modem line changes
- port settings
- device disconnects

This is preferable for high-quality remote recording.

---

# 29. Timestamping in Remote Sessions

For remote recording, timestamps should be created where the physical transport exists.

Bad:

```text
device -> network -> UI -> timestamp
```

Network latency and jitter would become part of the serial timing.

Preferred:

```text
device -> remote serial agent -> timestamp -> network -> UI
```

The agent sends already-timestamped events.

The session remains an ordered event stream.

For a later multi-source session, retain each source-local timestamp and map it
to session time through explicit offset/scale/uncertainty metadata. Sources on
one capture host/agent should share one monotonic clock and can be directly
ordered. Independent hosts require a separate clock-synchronization design;
the UI must expose uncertainty rather than invent an exact cross-host order.

For a single remote serial endpoint, relative timestamps from the agent are sufficient.

---

# 30. Remote Protocol

Do not simply stream terminal text.

The remote protocol should carry structured messages.

Example conceptual message types:

```text
HELLO
OPEN_TRANSPORT
CLOSE_TRANSPORT
SET_CONFIG
TX_DATA
RX_DATA
LINE_STATE
ERROR
PING
SESSION_EVENT
```

Binary payloads must remain binary-safe.

Potential implementation technologies can be decided later.

The important architectural rule is:

> Network serialization is an adapter around the internal event model, not the event model itself.

---

# 31. Network Disconnection Handling

A remote session should record network-related conditions separately from serial conditions.

Example:

```text
Remote link disconnected
```

must not be confused with:

```text
Serial device disconnected
```

If the remote agent keeps running after the UI loses connection, a later enhancement could allow buffered capture on the agent and resynchronization.

This does not need to be implemented initially, but the event model should allow it.

---

# 32. Replay Over Network

Because active replay writes to `ITransport`, no replay-specific network architecture is required.

Example:

```text
Session
    |
Replay Engine
    |
RemoteKomportTransport
    |
Network
    |
Remote Agent
    |
SerialTransport
    |
Device
```

This is one of the main reasons to introduce the transport abstraction early.

---

# 33. Simulation Over Network

The same applies to simulation.

Example:

```text
Real application
    |
serial/network
    |
Komport Simulator
    |
Recorded Session + Decoder
```

Or remotely:

```text
Real application
    |
Remote serial port
    |
Komport Agent
    |
network
    |
Simulation Engine
```

No simulator logic should depend on a local `QSerialPort`.

---

# 34. UI Proposal

A possible toolbar:

```text
Connect
Disconnect

Record
Stop

Load Session
Save Session

Replay
Simulation

Export
```

Replay button behavior:

```text
Replay
    Passive Replay
    TX Replay...
    Step Replay...
```

Simulation:

```text
Simulation
    Recorded Device
    Recorded Host
    Protocol Simulation...
```

Active modes should visually distinguish themselves.

Possible status indicator:

```text
LIVE
RECORDING
OFFLINE SESSION
PASSIVE REPLAY
ACTIVE REPLAY
SIMULATOR
```

---

# 35. Session Timeline

A timeline view would be particularly useful.

Example:

```text
0 ms          TX request
8 ms              RX response
150 ms        TX request
155 ms             RX response
```

Selecting an event should synchronize:

- hex view
- decoded view
- text view
- metadata view

This can be added later without changing the session format.

---

# 36. Bookmarks and Annotations

Allow users to add annotations to a session without changing the raw payload.

Examples:

```text
"Device reset here"
"Unexpected response"
"Firmware 1.2 bug"
```

Annotations may be stored as separate session events.

This makes saved sessions useful as diagnostic artifacts.

---

# 37. Session Editing

Raw recorded events should be immutable by default.

For simulation, modifications should be represented as an overlay or derived scenario.

Example:

```text
Original session:
device-test.kpsession

Scenario:
device-test-high-temperature.kpsim
```

The `.kpsim` file may reference the source session and contain:

- modified fields
- timing overrides
- fault injection rules
- match rules

This preserves forensic integrity.

---

# 38. Suggested Simulation Scenario Format

Later extension:

```yaml
format: komport-simulation
version: 1

source:
  session: pump-startup.kpsession

mode: device

rules:
  - match:
      decoder: modbus-rtu
      slave: 1
      function: 3
      register: 16

    response:
      registers:
        16: 100
        17: 50
```

This should not be required for the first simulator implementation.

---

# 39. Testing Strategy

## 39.1 Session Serialization Tests

Verify:

- NUL bytes survive
- all byte values `00..FF` survive
- timing survives
- directions survive
- metadata survives
- partial final record is recoverable

## 39.2 Replay Tests

Record:

```text
TX A
RX B
TX C
RX D
```

Verify passive replay produces the identical event sequence.

## 39.3 Timing Tests

Use tolerances.

Do not expect exact nanosecond scheduling from a desktop OS.

The stored timestamp may be high resolution even when replay scheduling is less precise.

## 39.4 PTY Tests

PTY tests should verify:

- active TX replay
- device simulation
- fragmented RX
- fragmented TX
- NUL data
- long sessions
- cancellation

## 39.5 Decoder Tests

A decoder test should use recorded `SessionEvent` fixtures, not require actual hardware.

This permits deterministic regression testing.

---

# 40. Modbus as First Reference Decoder

Modbus RTU is a good first full protocol decoder because it exercises:

- binary frames
- direction
- CRC
- request/response semantics
- timing-sensitive framing
- variable payloads
- exception responses
- register decoding

It also provides a natural path from:

```text
decoder
```

to:

```text
protocol-aware simulator
```

---

# 41. Recommended Development Order

Strong recommendation:

```text
1. Transport abstraction
2. Unified SessionEvent model
3. Convert existing serial path to these abstractions
4. Session recorder
5. Session file format v1
6. Session loader
7. Passive replay
8. Exporters
9. Decoder API
10. Translation-table decoder migration
11. Modbus RTU decoder
12. Active TX replay
13. Sequential exact-match device simulator
14. Focused shared-library extraction and analyzer skeleton, when first needed
15. Multi-source recording and local dual-port sniffer event merge in analyzer
16. Interleaved/split/timeline analyzer views
17. TCP transport and remote Komport-agent protocol
18. Cross-host clock alignment with visible uncertainty, when needed
19. Decoder-aware simulation
20. Parameter editing / fault injection
```

The network layer should therefore NOT be implemented first.

But network requirements must influence steps 1 and 2.

Specifically:

- no direct `QSerialPort` dependencies in UI or decoder code
- timestamps belong to transport events
- transport configuration is metadata
- session events must be serializable
- binary data must remain binary
- direction semantics must be globally fixed
- source identity must remain separate from direction and raw source time must
  remain available alongside any aligned session time
- public shared contracts must not depend on terminal, analyzer or agent UI
  code; physical library extraction is incremental and demand-driven

If these rules are followed, network support becomes an additional transport instead of an architectural rewrite.

---

# 42. Recommended MVP Boundary

A realistic first milestone for the new feature set:

## Recording

- start/stop session recording
- RX/TX recording
- relative timestamps
- transport settings
- lossless `.kpsession` storage

## Loading

- load a session
- show it in current text/hex views
- show RX/TX direction
- navigate events

## Replay

- passive original-speed replay
- passive immediate replay
- step replay

## Export

- raw binary
- timestamped hex
- current decoded/text representation

## Architecture

- transport abstraction
- decoder abstraction

No active hardware replay is required for the first milestone.

This gives a safe foundation and tests the design before transmission-capable replay is introduced.

---

# 43. Second Milestone

- active TX replay
- range selection
- timing scaling
- PTY regression tests
- basic exact-match device simulation
- Modbus RTU decoder

This milestone already turns Komport into a useful hardware test and simulation tool.

---

# 44. Third Milestone

- TCP client/server transport
- remote Komport agent
- remote timestamped recording
- remote replay
- decoder-aware simulation
- editable simulation parameters
- fault injection

---

# 45. Security and Safety Notes

Replay can have physical effects.

A recorded command may:

- move an actuator
- switch a relay
- start a motor
- alter configuration
- erase memory
- reset equipment
- control machinery

Therefore:

1. Passive replay is always the default.
2. Loading a session never transmits data.
3. Active replay requires explicit selection.
4. The active target is always shown.
5. Recorded transport settings are not silently forced onto a connected device without user confirmation.
6. Simulation and replay status must be visually obvious.
7. Future automation APIs must distinguish read-only session analysis from active output.

---

# 46. Design Summary

The core concept should be:

```text
                   +----------------+
                   | Physical /     |
                   | Virtual Source |
                   +-------+--------+
                           |
                           v
                     ITransport
                           |
                           v
                    Session Events
                           |
          +----------------+----------------+
          |                |                |
          v                v                v
       Recorder         Decoder           UI
          |
          v
     Session File
          |
          v
     Replay Engine
          |
      +---+-------------------+
      |                       |
      v                       v
 Passive Replay          Active Output
                              |
                        +-----+------+
                        |            |
                        v            v
                    Real Device   Simulator
```

Network support extends the transport side:

```text
ITransport
    |
    +-- SerialTransport
    +-- TcpTransport
    +-- RemoteKomportTransport
    +-- ReplayTransport
```

It does not require a second recording, decoding or replay architecture.

---

# 47. Central Architectural Decision

If only one design decision is adopted from this document, it should be this:

> Introduce a transport-neutral, timestamped, byte-exact SessionEvent stream before implementing recording, replay, simulation or network support.

Everything else can then be implemented as producers, consumers or transformers of that stream.

This is the point that prevents the future network feature from forcing a major redesign.
