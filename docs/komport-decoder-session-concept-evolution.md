# Komport Qt 6 – Evolution of the Decoder, Session, Replay and Simulation Concept

## Purpose

This document records the design path that led from Komport's existing translation table to the proposed session recording, replay, protocol analysis and device simulation architecture.

It is intentionally complementary to:

```text
komport-session-replay-simulation-architecture.md
```

That document describes the target architecture.

This document explains **why the architecture evolved in that direction**, which ideas came first, and which design consequences followed from them.

---

# 1. Starting Point: Existing Translation Table

The discussion started with the existing Komport translation table.

Originally, such a table mainly maps characters or byte values to readable representations.

Conceptually:

```text
0x00 -> NUL
0x0D -> CR
0x1B -> ESC
```

or:

```text
AA 55 -> START
03    -> ETX
```

The question was whether this mechanism could also be used to interpret hexadecimal byte sequences from devices connected through RS-232 or RS-485.

The answer is:

Yes, but only up to a point.

A simple translation table works well when a byte or a fixed byte sequence has a direct symbolic meaning.

Example:

```text
01 10 -> SetSpeed
01 11 -> GetSpeed
```

However, real device protocols usually contain structure:

```text
AA 01 10 00 64 3F
```

which might mean:

```text
AA       Start
01       Device address
10       Command: SetSpeed
00 64    Speed = 100
3F       CRC
```

At that point, a direct byte-to-name translation is no longer sufficient.

A structured protocol decoder is needed.

---

# 2. From Translation Table to Protocol Decoder

This led to the idea of separating two levels:

```text
Level 1:
Byte / byte sequence -> symbolic name

Level 2:
Protocol frame -> fields, parameters and semantics
```

The translation table can remain useful as a low-level mechanism.

On top of it, Komport can provide a protocol decoder layer.

Example conceptual protocol definition:

```yaml
protocol: ExampleDevice

frame:
  start:
    value: 0xAA

  address:
    type: uint8

  command:
    type: uint8

commands:
  0x10:
    name: SetSpeed
    parameters:
      - name: speed
        type: uint16
        endian: big
        unit: rpm
```

A received frame:

```text
AA 01 10 00 64 3F
```

could then be displayed as:

```text
Device: 1
Command: SetSpeed
Speed: 100 rpm
CRC: OK
```

This suggested that Komport could evolve from a terminal with text/hex translation into a generic protocol analysis tool.

---

# 3. Decoder Instead of "Disassembler" for RS-485

For serial device communication, the correct term is normally:

- protocol decoder
- frame decoder
- protocol analyzer

rather than "disassembler".

A disassembler normally converts processor machine code into assembly instructions.

RS-485 itself is only the electrical transport layer.

The bytes transported over RS-485 may implement:

- Modbus RTU
- a proprietary industrial protocol
- a bootloader protocol
- a simple command/response protocol
- arbitrary binary data

Therefore Komport should not bind interpretation to RS-485 itself.

The interpretation belongs to a decoder selected independently of the transport.

---

# 4. Built-in Protocol Modules

The next idea was to ship ready-made decoder modules.

The most obvious first candidate is Modbus.

For example:

```text
01 03 00 10 00 02 C5 CE
```

could become:

```text
Modbus RTU

Slave: 1
Function: 0x03 Read Holding Registers
Start register: 16
Count: 2
CRC: OK
```

Responses could similarly decode:

- register values
- exception responses
- CRC validity
- addresses
- function codes

Modbus is particularly suitable as a reference decoder because it exercises:

- binary data
- request/response semantics
- CRC
- variable payloads
- timing-sensitive framing
- slave addresses
- direction awareness

This makes Modbus useful not only as a feature, but also as an architectural test case.

---

# 5. Binary Code Decoders and Retro Systems

The discussion then broadened beyond communication protocols.

The same decoder infrastructure could also interpret machine code.

Examples:

- MOS 6502
- MOS 6510
- MOS 7501/8501
- Z80
- later other CPUs

For the C64:

```text
A9 01
8D 20 D0
60
```

could be shown as:

```asm
LDA #$01
STA $D020
RTS
```

A richer decoder could also know symbolic hardware addresses:

```asm
STA $D020    ; VIC-II border color
```

This is genuinely a disassembler use case.

It suggested that Komport's decoder infrastructure should not be limited to communication protocols.

---

# 6. PETSCII and Tokenized BASIC

A third class of decoder emerged from the C64/C16 idea.

Besides processor machine code, Komport could decode data formats such as:

- PETSCII text
- tokenized Commodore BASIC
- binary file formats

A Commodore BASIC program is not stored simply as ASCII source text.

A format decoder could reconstruct readable BASIC statements from tokenized program bytes.

This led to a broader decoder taxonomy:

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
    proprietary device protocols

Binary / Code Decoders
    6502
    6510
    7501/8501
    Z80
    ...

Format Decoders
    Commodore BASIC
    firmware/file formats
```

The important conclusion was:

> Komport should have a generic decoder framework rather than hard-coding every interpretation into the terminal view.

---

# 7. Reinterpreting the Existing Load/Save Function

Once decoders exist, Komport's Load/Save functionality becomes much more useful.

Instead of only saving visible terminal text, Komport can save the entire communication session.

The key idea became:

> Store the raw communication once and interpret it later in different ways.

This means the same recording can later be viewed as:

- plain text
- hex
- PETSCII
- Modbus
- a custom protocol
- processor machine code
- another future decoder

without recording the communication again.

This directly led to the requirement for a session file format.

---

# 8. Raw Bytes Are the Source of Truth

A fundamental design decision followed:

The stored session must preserve the original bytes.

Decoded output must be derived.

Example:

```text
Recorded session
    |
    +-- today: decoder says "unknown command 0x17"
    |
    +-- later: improved decoder says
               "SetMotorCurrent = 2.5 A"
```

If only interpreted text had been stored, improved decoding would be impossible.

Therefore:

```text
Raw bytes = authoritative data

Decoded text = derived representation
```

This is one of the central principles of the whole design.

---

# 9. TX and RX Direction Must Be Preserved

A simple binary dump is not sufficient.

Communication analysis needs to know who sent which data.

Example:

```text
TX  01 03 00 10 00 02 C5 CE
RX  01 03 04 00 64 00 32 ...
```

If these bytes were flattened into one stream, request/response relationships would be lost.

Therefore the recording needs at minimum:

```text
direction
payload
```

per recorded event.

---

# 10. Timing Must Also Be Preserved

The next consequence was that timing may itself be part of the protocol.

This is especially relevant for:

- Modbus RTU
- device response times
- inter-character gaps
- bootloaders
- timeout analysis
- unusual legacy devices

A useful recording therefore looks more like:

```text
+0.000000 TX  01 03 00 10 00 02 C5 CE
+0.008421 RX  01 03 04
+0.009105 RX  00 64 00 32
```

rather than:

```text
TX: ...
RX: ...
```

This led to the SessionEvent concept:

```text
timestamp
direction
raw bytes
metadata
```

and ultimately to the recommendation to use monotonic relative timestamps.

---

# 11. Preserve Chunk Boundaries

Another important detail followed from serial receive behavior.

The application may receive one logical protocol frame in several chunks:

```text
RX chunk 1: 01 03 04
RX chunk 2: 00 64 00 32
RX chunk 3: 7A 1B
```

These chunks are not guaranteed to match protocol frames.

However, preserving them is useful because they reveal:

- transport buffering
- timing
- fragmentation
- driver behavior
- device behavior

Therefore the session format should preserve application-observed chunks, while decoders must remain capable of reassembling protocol frames across multiple chunks.

---

# 12. Exporters Become a Natural Companion

Once raw sessions and decoders exist, several export modes become useful.

## Raw export

```text
.bin
```

with options such as:

- TX only
- RX only
- selected range

## Hex export

```text
+0.000000 TX  01 03 00 10 00 02 C5 CE
+0.008421 RX  01 03 04 00 64 00 32 7A 1B
```

## Decoded export

```text
TX:
Modbus Read Holding Registers
Slave = 1
Start = 16
Count = 2

RX:
Register 16 = 100
Register 17 = 50
CRC = OK
```

## Combined export

```text
[TX]
01 03 00 10 00 02 C5 CE

Modbus:
Slave = 1
Function = Read Holding Registers
Start = 16
Count = 2
```

This makes saved sessions useful for:

- bug reports
- documentation
- support cases
- protocol reverse engineering
- regression testing

---

# 13. Replay as the Next Logical Step

Once a session can be saved and loaded, the next question was:

Why only display it?

Why not replay it?

That led to the first replay mode:

## Passive Replay

The session is played back internally.

No hardware is accessed.

The UI behaves as if communication were happening live.

Use cases:

- offline analysis
- demonstrations
- decoder development
- reproducing display bugs
- training

Possible modes:

```text
Original timing
Immediate
Slower/faster timing
Step mode
```

This is safe because no bytes leave Komport.

---

# 14. Active TX Replay

The next step is active replay.

Komport can resend only the originally transmitted data to a real device.

Example recording:

```text
TX request A
RX response A

TX request B
RX response B
```

Active TX replay:

```text
send request A
wait
send request B
```

This is useful for:

- reproducing device behavior
- debugging
- firmware regression tests
- repeatable initialization sequences

This also introduced an important safety rule:

> Loading a session must never automatically transmit data.

Active replay must always require explicit user action.

---

# 15. Replay Led Directly to Device Simulation

The most significant conceptual step followed from replay.

If a session contains:

```text
Host -> Device request
Device -> Host response
```

then Komport could potentially replace the device.

Example original session:

```text
PC -> Device
01 03 00 10 00 02 C5 CE

Device -> PC
01 03 04 00 64 00 32 ...
```

Later:

```text
Test application -> Komport
01 03 00 10 00 02 C5 CE

Komport -> Test application
01 03 04 00 64 00 32 ...
```

This transforms Komport from a passive serial terminal into a device simulator.

This was identified as one of the strongest potential features of the new architecture.

---

# 16. First Simulation Mode: Exact Recorded Sequence

The simplest simulator should not initially try to understand the protocol.

It can work sequentially:

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

Request matching can initially be:

```text
exact byte match
```

On mismatch:

```text
stop
log mismatch
optionally allow user intervention
```

This mode is simple, deterministic and already extremely useful.

---

# 17. Decoder-Aware Simulation

Once protocol decoders exist, simulation can become semantic.

Instead of matching:

```text
01 03 00 10 00 02 C5 CE
```

the simulator can match:

```text
Protocol: Modbus RTU
Slave: 1
Function: Read Holding Registers
Start: 16
Count: 2
```

This allows rules such as:

```text
When:
  Slave 1
  Function 3
  Register 16

Respond:
  Register 16 = 100
  Register 17 = 50
```

This is much more flexible than exact byte matching.

It also makes protocol-aware simulation possible.

---

# 18. Editable Simulation Parameters

The next logical extension is to modify values.

A recorded device might return:

```text
Temperature = 21.5 C
Pressure = 1012 hPa
```

The simulator could instead return:

```text
Temperature = 80 C
Pressure = 850 hPa
```

without changing the original recording.

This is useful for:

- UI testing
- alarm testing
- development without hardware
- edge cases
- demos
- automated regression tests

This also led to the idea that simulation changes should be stored separately from the original session.

---

# 19. Fault Injection

Once the simulator exists, deliberate faults become possible.

Examples:

```text
delay response
drop response
corrupt checksum
truncate frame
duplicate frame
disconnect
send invalid value
send unexpected frame
```

The original session should remain immutable.

Fault behavior should be represented as a simulation overlay or scenario.

---

# 20. Session + Scenario Instead of Editable Recording

This suggested two related file types:

```text
.kpsession
```

for original recordings

and later possibly:

```text
.kpsim
```

for simulation scenarios.

Conceptually:

```text
Original:
pump-startup.kpsession

Scenario:
pump-overtemperature.kpsim
```

The scenario could reference the original session and override:

- response parameters
- timing
- fault injection
- matching rules

This preserves the integrity of the original capture.

---

# 21. Why the Session Format Must Be Designed Early

Replay and simulation impose stronger requirements than simple save/load.

If Komport initially saved only:

```text
terminal text
```

then later replay would require redesign.

Likewise, storing only:

```text
raw byte stream
```

would lose:

- TX/RX direction
- timing
- transport state
- serial settings

Therefore the session format should be designed around replay and simulation from the beginning, even if those features are implemented later.

The architecture document defines this in detail.

---

# 22. Why Network Capability Matters at This Point

The project also has planned network capability.

This introduced an architectural ordering question:

Should network support be implemented before session/replay?

The conclusion was:

No.

But the local architecture must be network-ready from the beginning.

The key mistake to avoid would be:

```text
UI -> QSerialPort
```

because later network support would require replacing assumptions throughout the UI.

Instead:

```text
UI
 |
 v
Session / Communication Layer
 |
 v
Transport Interface
 |
 +-- Serial
 +-- TCP
 +-- Remote Agent
 +-- Replay
```

This means session recording, decoding, replay and simulation can all operate independently of the physical transport.

---

# 23. Transport Neutrality Became a Core Requirement

The transport should only be responsible for moving bytes and reporting transport events.

Possible transports:

```text
SerialTransport
TcpTransport
RemoteKomportTransport
ReplayTransport
PtyTransport
```

The rest of Komport should operate on the same SessionEvent stream.

This is what makes the following possible without separate implementations:

```text
live serial recording
TCP recording
remote serial recording
offline replay
active replay
remote replay
simulation
```

---

# 24. Remote Komport Agent Concept

Two forms of networking were distinguished.

## Serial-over-TCP

Komport connects directly to an Ethernet/serial gateway.

This is simply another transport.

## Remote Komport Agent

A small Komport component runs on the machine that physically owns the serial port.

Architecture:

```text
Komport UI
    |
 network
    |
Komport Agent
    |
 serial
    |
 device
```

The agent can preserve richer information than a raw TCP gateway:

- receive chunk boundaries
- serial errors
- line state
- serial settings
- transport events
- accurate local timestamps

This is especially useful for recording and later analysis.

---

# 25. Remote Timestamping

Remote operation revealed another important timing rule.

Bad design:

```text
device
  ->
network
  ->
Komport UI
  ->
timestamp
```

This would record network latency as if it were serial timing.

Preferred:

```text
device
  ->
remote serial agent
  ->
timestamp
  ->
network
  ->
Komport UI
```

Therefore timing belongs to the event source.

This reinforces the transport-neutral SessionEvent architecture.

---

# 26. Decoder, Replay and Network Converge on the Same Event Model

By this stage, several initially separate ideas all required the same abstraction:

## Protocol decoder needs

```text
bytes
direction
timing
```

## Recorder needs

```text
bytes
direction
timing
metadata
```

## Replay needs

```text
events
timing
direction
```

## Simulation needs

```text
events
request/response relationships
decoder information
```

## Remote networking needs

```text
serializable transport events
timestamps
binary-safe payloads
```

The common denominator is:

```text
SessionEvent
```

This is why the SessionEvent stream became the central architectural concept.

---

# 27. Resulting Design Direction

The evolution can be summarized as:

```text
Translation Table
        |
        v
Named Byte Sequences
        |
        v
Structured Protocol Decoder
        |
        +----------> Modbus
        |
        +----------> Custom RS-485 protocols
        |
        +----------> 6502/6510 disassembler
        |
        +----------> PETSCII/BASIC decoders
        |
        v
Store Raw Session
        |
        +----------> Hex Export
        |
        +----------> Decoded Export
        |
        v
Passive Replay
        |
        v
Active TX Replay
        |
        v
Recorded Device Simulation
        |
        v
Protocol-Aware Simulation
        |
        +----------> Editable Parameters
        |
        +----------> Fault Injection
        |
        v
Transport-Neutral Architecture
        |
        +----------> Local Serial
        |
        +----------> TCP
        |
        +----------> Remote Komport Agent
```

---

# 28. Product Direction

This path changes the nature of Komport.

The project can remain a serial terminal, but its broader identity becomes closer to:

> Serial Terminal + Protocol Analyzer + Recorder + Replay Tool + Device Simulator

The useful property is that these are not independent features bolted onto the terminal.

They all build on the same raw event stream.

---

# 29. Why This Is a Strong Open-Source Direction

Many serial tools provide some combination of:

- terminal display
- hex mode
- logging
- send files
- macros

A unified architecture that additionally provides:

- lossless sessions
- offline re-decoding
- timing-aware replay
- exact device simulation
- protocol-aware simulation
- custom decoders
- remote capture

would make Komport useful in significantly more scenarios.

Examples:

- embedded development
- industrial maintenance
- reverse engineering
- regression testing
- legacy equipment
- education
- retro computing
- field diagnostics
- remote support

---

# 30. Important Non-Goals

The concept should remain controlled.

Komport should not become:

- a full IDE
- a full PLC engineering suite
- a complete logic analyzer replacement
- a general-purpose CPU debugger
- a SCADA system
- a CAM system

The common scope remains:

```text
communication
capture
interpretation
reproduction
simulation
```

---

# 31. Recommended Historical Note for the Project Documentation

A concise explanation for future contributors could be:

> Komport's decoder and replay architecture evolved from the existing translation-table feature. The first idea was to translate binary RS-232/RS-485 messages into symbolic commands and parameters. This naturally led to structured protocol decoders such as Modbus, and later to other byte-oriented decoders including PETSCII, tokenized BASIC and CPU machine-code disassembly. Once communication could be interpreted after capture, saving complete byte-exact TX/RX sessions became more valuable than saving rendered terminal text. From there, offline replay, active replay and recorded-device simulation became natural extensions. The same requirements for raw bytes, direction, timing and metadata also aligned with the planned remote/network architecture, leading to a transport-neutral SessionEvent model as the central abstraction.

---

# 32. Relationship to the Architecture Document

This document describes the design history.

The companion document:

```text
komport-session-replay-simulation-architecture.md
```

contains the technical proposal, including:

- SessionEvent structure
- session file format
- replay modes
- simulation modes
- export formats
- decoder architecture
- network architecture
- remote timestamping
- implementation order
- testing strategy
- safety considerations

Both documents should be kept together.

A possible documentation layout is:

```text
docs/
├── decoder-session-concept-evolution.md
└── session-replay-simulation-architecture.md
```

The first answers:

> Why are we building it this way?

The second answers:

> How should it be built?

---

# 33. Core Conclusion

The decisive insight was not the replay button itself.

It was the realization that several desirable features all require the same foundation:

```text
byte-exact
timestamped
direction-aware
transport-neutral
session events
```

Once that foundation exists:

- translation becomes decoding,
- logging becomes session recording,
- loading becomes offline analysis,
- playback becomes replay,
- replay becomes simulation,
- local serial becomes just one transport among several.

That is the architectural path that led to the current Komport design proposal.
