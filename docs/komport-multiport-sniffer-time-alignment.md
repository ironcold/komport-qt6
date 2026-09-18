# Komport Qt 6 – Multi-Port Sniffer, Interleaved Analysis and Time Alignment

## Purpose

This document records a future Komport feature idea and its architectural implications.

The original use case was a serially controlled measurement device where communication between a controller and the device could be passively sniffed by observing both directions separately.

The goal is to support:

- two or more simultaneously observed communication channels
- synchronized analysis of both directions
- interleaved and split views
- hex/text/decoded representations
- byte-exact session recording
- later replay and simulation
- local and remote capture
- robust timestamp handling across multiple sources

This is a future TODO, but some architectural consequences should be considered early so the core design does not block it later.

---

# 1. Original Use Case

Example:

```text
Controller  ---------------------->  Measurement Device
            <----------------------
```

The idea is to passively observe both directions using two independent receive channels.

Conceptually:

```text
Controller TX  --------+--------> Device RX
                       |
                       +--------> Sniffer Port A RX

Device TX      --------+--------> Controller RX
                       |
                       +--------> Sniffer Port B RX
```

Important:

The sniffing connections must be electrically passive.

Do not connect multiple active RS-232 TX drivers together.

The exact electrical implementation depends on the interface and hardware, but from Komport's perspective the result is:

```text
Source A = Controller -> Device
Source B = Device -> Controller
```

Both are receive-only observation sources.

---

# 2. Why This Fits the Existing Session Architecture

The already proposed Komport architecture is based on byte-exact timestamped session events.

This feature therefore should not require a special-purpose sniffing subsystem.

Instead it becomes a natural extension from:

```text
one transport source
```

to:

```text
multiple transport sources
```

The central event model remains the same.

Conceptually:

```text
Source A ----\
              \
Source B ------> Session Event Stream ---> Recorder
              /                         ---> Decoder
Source C ----/                          ---> UI
```

---

# 3. Source Identity Must Be Separate From Direction

This is an important architectural requirement.

A session event should not assume:

```text
Source A == TX
Source B == RX
```

because source identity and direction are different concepts.

Recommended conceptual fields:

```cpp
struct SessionEvent {
    quint64 sequence;
    qint64 sourceTimestampNs;

    SessionSourceId source;
    SessionDirection direction;
    SessionEventType type;

    QByteArray payload;
    QVariantMap metadata;
};
```

Possible source metadata:

```text
Source 0:
    name: Controller -> Gauge
    transport: serial
    endpoint: /dev/ttyUSB0

Source 1:
    name: Gauge -> Controller
    transport: serial
    endpoint: /dev/ttyUSB1
```

Later:

```text
Source 2:
    TCP diagnostic channel

Source 3:
    secondary RS-485 bus
```

Therefore:

> `SessionEvent v1` and the session format should support multiple independently configured sources.

Even if the first implementation only uses one source, source identity should not be implicitly encoded through direction.

---

# 4. UI Mode: Dual-Port / Multi-Port Analyzer

A future UI mode could be named:

```text
Sniffer View
Dual Port Analyzer
Multi-Source Analyzer
```

The UI should offer several representations of the same session data.

---

# 5. Interleaved View

This is likely the most useful view for protocol reverse engineering.

Example:

```text
+0.000000  A  Controller -> Device   02 41 10 00 17 03
+0.018412  B  Device -> Controller   02 81 10 34 27 03
+0.045103  A  Controller -> Device   02 42 01 03
+0.051922  B  Device -> Controller   02 82 00 03
```

Advantages:

- request/response rhythm is immediately visible
- protocol timing is visible
- both directions remain in one chronological stream
- decoder output can be inserted directly below the corresponding event/frame

Color may additionally identify the source.

Example:

```text
Source A -> style/color A
Source B -> style/color B
```

Color must not be the only distinction; source labels should remain visible for accessibility and export.

---

# 6. Split View

Alternative representation:

```text
Controller -> Device             Device -> Controller
────────────────────             ────────────────────

02 41 10 00 17 03

                                 02 81 10 34 27 03

02 42 01 03

                                 02 82 00 03
```

Both panes should share one logical timeline.

Useful behavior:

- synchronized scrolling
- selecting an event on one side highlights the corresponding time position on the other
- optional horizontal timeline ruler
- optional request/response links

The split view is only a presentation.

It must not own independent receive buffers.

---

# 7. Timeline / Semantic View

Once protocol decoders exist, the same recording could be rendered as:

```text
0.000 ms   Controller ─── SetMeasurementMode ───> Device
18.412 ms  Controller <── Status: Ready ───────── Device
45.103 ms  Controller ─── ReadHeight ───────────> Device
51.922 ms  Controller <── Height = 127.43 mm ──── Device
```

This turns the multi-port capture into a protocol-analysis tool rather than merely two terminals shown next to each other.

---

# 8. Hex, Text and Decoder Output Are Views

The underlying session events remain byte-exact.

Possible views:

```text
Interleaved Hex
Split Hex

Interleaved Text
Split Text

PETSCII
ASCII
UTF-8

Protocol Decoder
Timeline
Statistics
```

The architecture should therefore be:

```text
Raw Session Events
        |
        +----> Hex View
        |
        +----> Text View
        |
        +----> Protocol Decoder
        |
        +----> Timeline View
```

not:

```text
Port A Terminal Buffer
Port B Terminal Buffer
```

The latter would make synchronized replay and later decoding unnecessarily difficult.

---

# 9. Session Recording

A multi-source session should preserve at least:

```text
source
source-local timestamp
payload
event type
direction
transport metadata
```

Example:

```text
+0.000000000  source=0  RX  02 41 10 00 17 03
+0.018412381  source=1  RX  02 81 10 34 27 03
```

In a passive sniffer configuration, both physical ports may technically be `RX`.

The semantic communication direction can be represented separately in source metadata:

```text
Source 0 semanticDirection = controller_to_device
Source 1 semanticDirection = device_to_controller
```

This avoids abusing the generic TX/RX meaning.

---

# 10. Local Dual-Port Timestamping

If both serial ports are captured by the same Komport process, timing is straightforward.

Use one common monotonic clock.

Example:

```text
Application monotonic clock

Port A readyRead:
    103842137 ns

Port B readyRead:
    103921804 ns
```

Because both sources use the same clock, their events can be placed directly on one timeline.

This gives a reliable ordering at the granularity visible to the host OS/application.

Important limitation:

This is the order and time at which Komport observed the data.

It is not necessarily the exact electrical bit-level order on the physical wire.

For normal serial protocol analysis this is generally sufficient.

For sub-byte or highly timing-critical analysis, dedicated logic-analyzer hardware remains more appropriate.

---

# 11. Relative Time Instead of Absolute Time

For analysis, the most useful display is often relative to the first observed event.

Example:

```text
First event:
    t = 0

Later events:
    +18.412 ms
    +45.103 ms
    +51.922 ms
```

This has several advantages:

- easier human interpretation
- independent of wall clock
- immune to wall-clock adjustments
- ideal for replay
- ideal for response-time measurement

Therefore every source should use a monotonic clock.

The session should additionally contain a human-readable absolute session start timestamp.

Example:

```text
sessionStartedWallClock:
    2026-09-18T11:05:42.313+02:00

events:
    timestampRelativeNs
```

---

# 12. Does Relative Time Solve Network Timing?

Not completely.

This distinction is important.

Consider two remote agents:

```text
Agent A                     Agent B
Port A                      Port B

local t=0                   local t=0
local +10 ms                local +10 ms
```

Relative timing within each source is excellent.

Network jitter no longer affects the *intervals inside each source*, because timestamps are generated before transport over the network.

However, there is still one unknown:

```text
Where is Agent A's t=0 relative to Agent B's t=0?
```

This is the cross-source offset.

Therefore the user's idea is essentially correct:

> Once the starting relationship between the clocks is known, relative timestamps can preserve the rest of the event timing much better than timestamping events after network transport.

But there are two additional effects to consider:

1. clock offset
2. clock drift

---

# 13. Clock Offset

Suppose:

```text
Agent A monotonic t=0
```

corresponds physically to:

```text
Agent B monotonic t=4.2 ms
```

Then events need an alignment offset.

Conceptually:

```text
sessionTimeA = localTimeA + offsetA
sessionTimeB = localTimeB + offsetB
```

If the offset is known, both event streams can be merged.

---

# 14. Clock Drift

Two independent computer clocks do not run at exactly the same speed.

Over a short recording this may be negligible.

Over longer recordings:

```text
Agent A:
1000.000000 ms

Agent B:
1000.000037 ms
```

Small differences accumulate.

Therefore a better model is:

```text
sessionTime = offset + scale * sourceTime
```

where:

```text
offset = clock relationship at reference point
scale  = clock-rate correction / drift correction
```

In many ordinary use cases:

```text
scale ~= 1.0
```

but the architecture should not assume exact equality forever.

---

# 15. Estimating Remote Clock Relationship

A remote-agent protocol can estimate offset using synchronization exchanges.

Conceptually:

```text
UI/Coordinator             Remote Agent

t1 -------- ping -------->
             <-------- pong -------- t2/t3
t4
```

Similar to NTP-style clock estimation, round-trip time can be used to estimate:

- clock offset
- network delay
- uncertainty

Repeated samples improve the estimate.

The network delay need not be perfectly symmetric, so the result should be treated as an estimate.

---

# 16. Jitter Filtering

Multiple synchronization samples can be taken.

Example:

```text
Sample 1 RTT:  2.1 ms
Sample 2 RTT: 17.4 ms
Sample 3 RTT:  2.0 ms
Sample 4 RTT:  2.3 ms
Sample 5 RTT:  8.7 ms
```

The high-latency samples are likely affected by scheduling/network jitter.

Possible strategy:

- take several measurements
- prefer lowest-RTT samples
- calculate robust offset estimate
- continuously refine
- estimate uncertainty

This is much better than timestamping serial data after it arrives at the central UI.

---

# 17. Time Alignment Model

A useful abstraction could be:

```text
SourceTimestamp
    |
    v
TimeAlignment
    |
    v
SessionTimestamp
```

Conceptually:

```cpp
struct TimeMapping {
    qint64 sourceEpochNs;
    qint64 sessionEpochNs;

    double scale;
    qint64 uncertaintyNs;
};
```

Then:

```text
sessionTime =
    sessionEpoch
    + (sourceTime - sourceEpoch) * scale
```

This allows:

- local sources
- same-agent sources
- different remote agents
- future synchronization improvements

without changing the raw source timestamps.

---

# 18. Preserve Both Raw and Aligned Time

Do not overwrite original source timestamps.

Store:

```text
source-local timestamp
```

as authoritative observation time.

Derived/aligned time may be calculated from synchronization metadata.

Conceptually:

```text
SessionEvent:
    sourceTimestampNs
    sourceId

TimeMapping:
    sourceId
    reference points
    offset
    scale
    uncertainty
```

The displayed session time becomes:

```text
alignedTimestamp = map(sourceTimestamp)
```

This is preferable to permanently rewriting events after synchronization.

Future algorithms may produce better alignment.

---

# 19. Session Time vs Source Time

Komport should conceptually distinguish:

## Source Time

Time measured by the machine that physically captured the event.

This is the trustworthy timing inside that source.

## Session Time

A common timeline onto which multiple sources are mapped.

For one process:

```text
source time == session time
```

For two ports on one remote agent:

```text
both already share the same source clock
```

For two independent agents:

```text
source time -> synchronization mapping -> session time
```

---

# 20. Best Capture Topology

If precise cross-channel ordering matters, prefer:

```text
one capture host / one remote agent
    |
    +-- Port A
    +-- Port B
```

rather than:

```text
Agent A -> Port A
Agent B -> Port B
```

The first arrangement gives both channels a naturally shared monotonic clock.

No cross-host synchronization is required.

This should be documented as the preferred topology for dual-port sniffing.

---

# 21. Multi-Agent Capture Is Still Useful

Independent agents should still be supported.

Examples:

- devices physically far apart
- distributed systems
- ship/industrial installations
- two buses in different cabinets
- remote diagnostics

In this case Komport should report timing confidence rather than pretending perfect synchronization.

Example UI:

```text
Aligned event time:
+124.381 ms

Timing uncertainty:
±0.8 ms
```

or:

```text
Clock alignment quality:
Excellent / Good / Approximate / Unsynchronized
```

This makes the tool honest about what can actually be inferred.

---

# 22. Relative Time Can Greatly Reduce Network Influence

The important conclusion is:

Network jitter does not need to contaminate captured serial timing.

If each agent timestamps events locally using a monotonic clock:

```text
serial event occurs
    |
    v
local timestamp
    |
    v
network transport
```

then network latency only affects when the event becomes visible centrally.

It does not alter the stored local event time.

Cross-source ordering then depends on the quality of the clock alignment model.

This is substantially better than:

```text
serial event
    |
    v
network
    |
    v
central timestamp
```

---

# 23. Potential Synchronization Metadata in Session Files

A future `.kpsession` file could contain:

```json
{
  "sources": [
    {
      "id": 0,
      "name": "Controller -> Gauge",
      "clock": {
        "type": "monotonic",
        "clockDomain": "agent-01"
      }
    },
    {
      "id": 1,
      "name": "Gauge -> Controller",
      "clock": {
        "type": "monotonic",
        "clockDomain": "agent-01"
      }
    }
  ]
}
```

If both sources use the same clock domain:

```text
no alignment required
```

For independent agents:

```json
{
  "clockMappings": [
    {
      "sourceId": 2,
      "referenceSourceTimestampNs": 7342234000,
      "referenceSessionTimestampNs": 118002000,
      "scale": 0.999999987,
      "uncertaintyNs": 420000
    }
  ]
}
```

Exact schema should be decided later.

The architectural point is that the format should be able to represent it.

---

# 24. Session Start Alignment

The user's proposed idea of aligning all channels to the first event is useful for display.

Example:

```text
earliest aligned event = session t=0
```

Then all views show:

```text
0.000 ms
5.123 ms
18.402 ms
...
```

However:

> The first event should not itself be used as the only clock synchronization mechanism for independent hosts unless there is a known physical reason that the first events are simultaneous.

Otherwise there is no way to know whether:

```text
Agent A first event
```

and:

```text
Agent B first event
```

occurred at the same physical time.

Therefore:

```text
clock synchronization first
then normalize display to first aligned event
```

is the robust approach.

---

# 25. Optional Protocol-Assisted Alignment

An interesting future technique is to use the protocol itself to improve alignment.

Suppose both capture sources observe causally related events:

```text
Controller sends request
Device responds
```

If enough protocol structure is known, correlations may help validate or refine time alignment.

Similarly, repeated patterns may reveal:

- impossible event ordering
- expected minimum response delay
- duplicated observations

However, protocol-assisted alignment should be considered an advanced analysis feature.

It must not silently rewrite raw timestamps.

---

# 26. Replay Implications

Multi-source timestamps are also valuable for replay.

Passive replay can reproduce:

```text
A event
18 ms later B event
27 ms later A event
```

A future two-endpoint replay could reproduce separate sources independently.

Example:

```text
Replay source A -> endpoint A
Replay source B -> endpoint B
```

The common session timeline already contains their relative ordering.

---

# 27. Simulation Implications

The same captured communication can become a simulator.

Workflow:

```text
Dual-Port Sniffer
        |
        v
Multi-Source Session
        |
        v
Interleaved Analysis
        |
        v
Protocol Decoder
        |
        v
Request/Response Understanding
        |
        v
Device Simulator
```

For the original height-measurement-device scenario:

```text
Step 1:
sniff controller and device

Step 2:
record both directions

Step 3:
decode command structure

Step 4:
identify request/response pairs

Step 5:
disconnect physical device

Step 6:
Komport emulates the device
```

Or the opposite:

```text
Komport emulates the controller
```

This demonstrates how sniffing, sessions, decoders, replay and simulation reinforce each other.

---

# 28. Decoder Correlation

With multiple sources, a decoder may consume events from more than one source.

Example:

```text
Source A:
Request

Source B:
Response
```

A protocol decoder could correlate them into:

```text
Transaction #42

Request:
    ReadHeight

Response:
    Height = 127.43 mm

Round-trip:
    6.819 ms
```

This enables statistics such as:

- response time
- timeout frequency
- command frequency
- error response rate

---

# 29. Future Transaction View

A future UI could show:

```text
#   Time       Request             Response               Δt
----------------------------------------------------------------
1   0.000 ms   GetStatus           Ready                 18.4 ms
2  45.103 ms   ReadHeight          127.43 mm              6.8 ms
3  82.004 ms   ReadTemperature     22.4 C                 7.1 ms
```

This is derived from session data and decoder output.

It should not replace the raw interleaved view.

---

# 30. Suggested Event/Source Architecture

Conceptually:

```text
              +-------------------+
Port A ------>| Capture Source A  |
              +---------+---------+
                        |
                        | source timestamp
                        v
                  SessionEvent

              +-------------------+
Port B ------>| Capture Source B  |
              +---------+---------+
                        |
                        | source timestamp
                        v
                  SessionEvent

                        |
                        v
                Time Alignment Layer
                        |
                        v
                 Session Timeline
                        |
          +-------------+-------------+
          |             |             |
          v             v             v
     Interleaved     Split View     Decoder
       View
```

Remote sources simply move the capture source to another machine.

---

# 31. Recommended Future Architecture Requirement

Codex should consider the following requirement before freezing `SessionEvent v1` and the session format:

> A Komport session may contain events from multiple independently configured capture sources. Each event must preserve source identity and source-local monotonic timing. Sources may share a clock domain or belong to independent clock domains. The session architecture must allow independent clock domains to be mapped onto a common session timeline without modifying the original source timestamps.

This requirement does not force immediate implementation of distributed clock synchronization.

It only prevents the initial event/session model from making it impossible later.

---

# 32. Recommended Source Model

Possible conceptual model:

```cpp
using SessionSourceId = quint32;

struct SessionSource {
    SessionSourceId id;

    QString name;
    QString clockDomain;

    TransportDescription transport;

    QVariantMap metadata;
};
```

Examples:

```text
id: 0
name: Controller -> Gauge
clockDomain: local-process-1

id: 1
name: Gauge -> Controller
clockDomain: local-process-1
```

Both sources share a clock.

Remote example:

```text
id: 2
name: Remote Bus A
clockDomain: remote-agent-a

id: 3
name: Remote Bus B
clockDomain: remote-agent-b
```

These require alignment.

---

# 33. Recommended Timing Model

Conceptually, retain:

```text
event.sourceTimestampNs
event.sourceId
```

and maintain:

```text
source.clockDomain
```

Then derive:

```text
event.sessionTimestampNs
```

through a mapping layer.

For same-clock sources:

```text
session timestamp = source timestamp - common epoch
```

For remote independent clocks:

```text
session timestamp =
    offset
    + scale * source timestamp
```

with an associated uncertainty.

---

# 34. Architectural Priorities

This future feature does not need to be implemented now.

However, the following should be considered now:

1. `SessionEvent` supports source identity.
2. Session files support multiple source definitions.
3. Source and direction remain separate concepts.
4. Event timestamps are monotonic and source-local.
5. Session timeline is conceptually derived.
6. Clock domain information is representable.
7. Raw timestamps are never destroyed by alignment.
8. Decoder APIs are capable of seeing source information.
9. Recorder does not assume exactly one transport.
10. UI models are not built around one terminal buffer.

---

# 35. Things That Can Wait

The following should remain future TODOs:

- actual dual-port sniffer UI
- synchronized split view
- source colors
- timeline view
- remote multi-agent synchronization
- NTP-style offset estimation
- drift estimation
- timing uncertainty UI
- transaction correlation
- protocol-assisted alignment
- two-endpoint replay
- automatic simulator generation

Only the architectural hooks need to remain open.

---

# 36. Suggested Roadmap Position

Possible order:

```text
Foundation
    SessionEvent
    source identity
    clock-domain-capable timestamp model

Single-source recording
    ↓
Replay
    ↓
Decoders
    ↓
Network transport / remote agent
    ↓
Multi-source recording
    ↓
Dual-port sniffer UI
    ↓
Clock alignment between remote agents
    ↓
Transaction correlation
    ↓
Simulation generation
```

This keeps implementation incremental.

---

# 37. Core Design Rule

The most important rule for this feature is:

> Do not synchronize display buffers. Synchronize timestamped source events.

That makes:

- hex
- text
- decoder output
- replay
- remote capture
- multi-port sniffing

different views or consumers of the same data model.

---

# 38. Summary

The dual-port sniffer idea extends Komport naturally.

The original scenario:

```text
serial controller <-> serial device
```

can be captured as two passive receive sources.

Those sources can then be:

- shown side by side
- interleaved chronologically
- rendered in different colors/styles
- displayed as hex or text
- decoded semantically
- stored in one session
- replayed
- converted into a device simulation

Relative monotonic timestamps are the correct basis.

For local multi-port capture, one shared monotonic clock provides direct ordering.

For multiple remote agents, local relative timestamps eliminate network jitter from the recorded serial timing, but a clock-offset/drift relationship is still needed to create one common cross-host timeline.

The robust model is therefore:

```text
source-local monotonic time
        +
clock-domain mapping
        =
aligned session time
```

with the original source timestamps permanently preserved.

This allows Komport to grow into a multi-channel serial protocol analyzer without compromising the byte-exact, transport-neutral architecture already planned.
