# Komport Qt 6 – Multi-Executable Product Architecture

## Purpose

This document defines a product and repository architecture in which Komport remains a small, focused serial terminal while advanced analysis, recording, replay, simulation and remote functionality live in separate executables that share common libraries.

The goal is to avoid turning the main terminal application into an oversized all-in-one tool while still allowing the project to grow substantially.

The proposed product split is:

```text
komport
komport-analyzer
komport-agent
```

All executables share common libraries.

The libraries are the architectural foundation.

The executables are clients of those libraries.

---

# 1. Core Product Principle

The main Komport application should remain:

> a small, fast and focused serial terminal

Advanced functionality should not force the terminal UI to become a protocol-analysis workbench.

The new capabilities discussed for the project include:

- session recording
- byte-exact capture
- protocol decoders
- Modbus analysis
- PETSCII / retro decoders
- machine-code disassembly
- multi-port sniffing
- synchronized timelines
- replay
- device simulation
- fault injection
- remote serial access
- multi-source time alignment

These features are valuable, but they do not all belong in the main terminal application.

Therefore the preferred direction is multiple executables built on one shared architecture.

---

# 2. Proposed Executables

## 2.1 `komport`

Purpose:

```text
small serial terminal
```

Primary responsibilities:

- open serial ports
- terminal I/O
- profiles
- character encodings
- hex mode
- translation tables
- macros
- file transfer
- basic networking where appropriate
- lightweight logging

The terminal should remain simple to start and simple to understand.

It should not require the analyzer UI or simulation framework.

---

## 2.2 `komport-analyzer`

Purpose:

```text
protocol analysis, recording, replay and simulation
```

Primary responsibilities:

- session capture
- session loading
- multi-source analysis
- synchronized interleaved views
- split source views
- timeline views
- protocol decoding
- binary / retro decoding
- Modbus analysis
- replay
- active TX replay
- device simulation
- request/response correlation
- export
- statistics
- time alignment
- future fault injection

The analyzer may grow significantly without compromising the usability of the terminal.

---

## 2.3 `komport-agent`

Purpose:

```text
headless or lightweight remote communication endpoint
```

Primary responsibilities:

- expose serial ports remotely
- own local physical transports
- timestamp events close to the physical interface
- stream byte-exact events
- receive controlled TX requests
- preserve serial transport metadata
- report line state and errors
- support remote capture
- support remote replay/simulation targets

Typical deployment:

```text
Raspberry Pi
industrial PC
remote workstation
lab machine
embedded service host
```

The agent should preferably work without a full desktop environment.

A small optional status UI may exist later, but the core service should be headless-capable.

---

# 3. Shared Library Architecture

A possible library split:

```text
komport-core
komport-transport
komport-session
komport-decoder
komport-replay
komport-simulation
komport-network
```

Possible repository structure:

```text
src/
├── libs/
│   ├── core/
│   ├── transport/
│   ├── session/
│   ├── decoder/
│   ├── replay/
│   ├── simulation/
│   └── network/
│
├── apps/
│   ├── komport/
│   ├── komport-analyzer/
│   └── komport-agent/
│
└── tests/
```

The exact physical split can evolve.

The important architectural rule is dependency direction.

---

# 4. Dependency Rule

The central rule is:

> Executables may depend on shared libraries. Shared libraries must not depend on executable-specific UI code.

Good:

```text
komport
    |
    v
shared libraries

komport-analyzer
    |
    v
shared libraries

komport-agent
    |
    v
shared libraries
```

Bad:

```text
komport-analyzer
    |
    v
komport executable internals
```

Bad:

```text
shared session library
    |
    v
MainWindow
```

The terminal executable must not become the hidden base application of the analyzer.

The shared libraries are the base.

---

# 5. Suggested Dependency Layers

A useful dependency direction could be:

```text
             komport
                |
                v

             komport-analyzer
                |
                v

             komport-agent
                |
                v

          application-facing APIs
                |
                v

      +-------------------------+
      | komport-network         |
      +-------------------------+
                |
                v
      +-------------------------+
      | komport-simulation      |
      +-------------------------+
                |
                v
      +-------------------------+
      | komport-replay          |
      +-------------------------+
                |
                v
      +-------------------------+
      | komport-decoder         |
      +-------------------------+
                |
                v
      +-------------------------+
      | komport-session         |
      +-------------------------+
                |
                v
      +-------------------------+
      | komport-transport       |
      +-------------------------+
                |
                v
      +-------------------------+
      | komport-core            |
      +-------------------------+
```

This is conceptual.

Some dependencies may be siblings rather than strict layers.

The important point is that UI-specific application code remains at the top.

---

# 6. `komport-core`

The core should contain only broadly reusable concepts.

Possible contents:

- common IDs
- common error types
- byte-safe utility types
- SessionEvent base definitions
- SessionSource
- timestamp / clock-domain types
- serialization-neutral data structures
- shared configuration primitives

The core should not depend on:

- Widgets
- application windows
- terminal widgets
- analyzer widgets

Ideally it should have minimal Qt dependencies beyond QtCore where practical.

---

# 7. `komport-transport`

Responsibilities:

- serial transport abstraction
- TCP transport abstraction
- PTY test transport
- future remote transport
- transport state
- port configuration
- flow control
- modem line state
- transport errors

Possible types:

```text
ITransport
SerialTransport
TcpTransport
PtyTransport
RemoteTransport
```

This is likely where existing reusable serial code from Komport should be moved or wrapped.

---

# 8. `komport-session`

Responsibilities:

- SessionEvent stream
- source identity
- source-local timestamps
- clock domains
- session recording
- session loading
- session file format
- event indexing
- metadata
- annotations/bookmarks
- time-alignment metadata

The session layer must remain independent of terminal rendering.

---

# 9. `komport-decoder`

Responsibilities:

- decoder interface
- text decoders
- translation-table decoder
- protocol decoders
- Modbus RTU
- Modbus ASCII
- custom protocol definitions
- PETSCII
- tokenized BASIC
- machine-code disassemblers

Possible decoder categories:

```text
TextDecoder
ProtocolDecoder
BinaryDecoder
FormatDecoder
```

The decoder layer consumes session/event data.

It does not own transports.

---

# 10. `komport-replay`

Responsibilities:

- passive replay
- immediate replay
- original timing replay
- scaled timing
- step replay
- active TX replay
- range replay
- replay scheduling

Replay must write through the normal transport abstraction when active.

It must never bypass shared transport rules.

---

# 11. `komport-simulation`

Responsibilities:

- exact request/response simulation
- sequential simulation
- decoder-aware matching
- parameterized responses
- future fault injection
- simulation scenario overlays

Source sessions remain immutable.

Simulation rules should be separate from original recordings.

---

# 12. `komport-network`

Responsibilities may include:

- remote-agent protocol
- remote transport serialization
- binary-safe event transport
- remote port enumeration
- remote configuration
- event timestamp transport
- remote capture control
- connection management

The network layer should transport shared data structures.

It should not define a second event model.

---

# 13. Terminal Application Scope

The terminal application should intentionally stay conservative.

Recommended feature scope:

```text
Connect / Disconnect
Serial profiles
Terminal text
Hex display
Character translation
Macros
File send/receive
Basic capture/logging
Basic TCP/network endpoint support
```

Possible advanced features may still be exposed if they remain simple.

Example:

```text
Record Session
Open in Komport Analyzer
```

This could provide a bridge between the two applications without embedding the analyzer itself.

---

# 14. Analyzer Application Scope

The analyzer can be more complex.

Possible UI sections:

```text
Capture
Sessions
Timeline
Sources
Hex View
Decoded View
Transactions
Replay
Simulation
Export
Statistics
```

The analyzer is allowed to become an engineering workbench.

That complexity should not leak into the terminal.

---

# 15. Agent Application Scope

The agent should be operationally small.

Conceptually:

```text
komport-agent --listen <address>
```

Responsibilities:

```text
enumerate local transports
open local port
timestamp RX locally
stream SessionEvents
accept TX
report errors
close port
```

Potential service deployment:

```text
systemd unit
Docker container where hardware access permits
embedded appliance
```

Security/authentication can be added deliberately later.

---

# 16. Shared SessionEvent Model

All executables should use the same conceptual SessionEvent model.

Example:

```cpp
struct SessionEvent {
    quint64 sequence;
    SessionSourceId sourceId;
    qint64 sourceTimestampNs;
    SessionEventType type;
    SessionDirection direction;
    QByteArray payload;
    QVariantMap metadata;
};
```

This supports:

```text
komport:
live terminal

komport-analyzer:
capture / decode / replay

komport-agent:
remote source
```

without separate data semantics.

---

# 17. Multi-Source Support

The analyzer requires multiple sources.

The terminal may initially only expose one active source.

This is acceptable.

The architecture should support:

```text
one-source UI
```

on top of:

```text
multi-source-capable core
```

The terminal must not force the shared libraries into a single-source design.

---

# 18. Clock and Time Support

The analyzer may eventually require:

- same-host multi-port timing
- remote-agent timing
- PTP-assisted alignment
- estimated offset/drift
- uncertainty metadata

The terminal does not need to expose this complexity.

This is another strong reason to separate applications.

The shared session/time model supports it.

The analyzer consumes it.

The terminal can ignore most of it.

---

# 19. PTP and Remote Capture

Future remote analyzer setups may use PTP.

Possible deployment:

```text
PTP-synchronized Agent A
PTP-synchronized Agent B
        |
        v
Komport Analyzer
```

The shared time model should be capable of representing:

```text
clockDomain
syncMethod
syncQuality
uncertainty
```

The analyzer may display or use these fields.

The terminal does not need any PTP UI.

---

# 20. Optional Integration Between Applications

The applications may cooperate.

Example workflow:

```text
komport
    |
    | Record Session
    v
session.kpsession
    |
    | Open in Analyzer
    v
komport-analyzer
```

Possible UI action:

```text
Analyze Current Session...
```

This can launch:

```text
komport-analyzer <session-file>
```

Such integration keeps the terminal simple while making advanced functionality easily discoverable.

---

# 21. Shared Profiles

Serial/device profiles may be useful in both applications.

Possible approach:

```text
shared profile format
```

Used by:

```text
komport
komport-analyzer
komport-agent
```

Care must be taken to separate:

```text
transport profile
```

from:

```text
UI preferences
```

The shared format should contain only reusable configuration.

---

# 22. Shared Decoder Definitions

Custom protocol/translation definitions should ideally be reusable.

Example:

```text
~/.config/komport/decoders/
```

or project-local definitions.

The terminal may only use lightweight translation tables.

The analyzer may use full structured protocol decoders.

Both can share the same lower-level decoder infrastructure where appropriate.

---

# 23. Packaging Strategy

Possible package split:

```text
komport
komport-analyzer
komport-agent
komport-common
```

Distribution packaging may choose whether libraries are internal packages or bundled.

The source architecture should not depend on packaging details.

Users who only want the terminal should not need to install analyzer-specific UI dependencies if avoidable.

---

# 24. Build Configuration

CMake options could eventually allow:

```text
BUILD_KOMPORT
BUILD_ANALYZER
BUILD_AGENT
BUILD_TESTS
```

Example:

```cmake
option(BUILD_KOMPORT "Build Komport terminal" ON)
option(BUILD_ANALYZER "Build Komport Analyzer" ON)
option(BUILD_AGENT "Build Komport remote agent" ON)
```

Embedded/headless builds may disable analyzer/terminal GUI components.

---

# 25. Testing Benefits

Separate executables improve testability.

Shared library tests can cover:

```text
SessionEvent
transport
session serialization
decoder
replay
simulation
network protocol
```

Application tests then focus on:

```text
terminal UI behavior
analyzer UI behavior
agent lifecycle
```

This reduces the need to test core behavior through GUI workflows.

---

# 26. Development Benefits

This split supports parallel work well.

Example:

```text
Developer / Agent A:
komport terminal UI

Developer / Agent B:
session/replay libraries

Developer / Agent C:
Modbus decoder

Developer / Agent D:
remote agent

Developer / Agent E:
analyzer timeline UI
```

As long as shared interfaces are frozen and reviewed, work can proceed independently.

---

# 27. Architecture Governance Implication

Before implementation, Codex should decide and document:

```text
Which functionality belongs in shared libraries?
Which functionality belongs in komport?
Which functionality belongs in komport-analyzer?
Which functionality belongs in komport-agent?
```

This should become an ADR.

Suggested ADR:

```text
ADR: Multi-Executable Product Architecture
```

Core decision:

> Komport Qt 6 is a shared communication platform with multiple executable front ends. The terminal, analyzer and remote agent are peers built on common libraries. No executable is the architectural base of another executable.

---

# 28. Recommended Dependency Invariant

A useful invariant:

```text
libs -> libs
apps -> libs
apps -X-> apps
libs -X-> apps
```

Meaning:

```text
shared library may depend on lower shared library
application may depend on shared libraries

application must not depend on another application's internals
shared library must not depend on application code
```

---

# 29. UI Code Placement

Terminal UI:

```text
apps/komport/ui/
```

Analyzer UI:

```text
apps/komport-analyzer/ui/
```

Agent-specific service/control UI:

```text
apps/komport-agent/
```

Reusable non-visual models should move into libraries.

Avoid sharing QWidget-derived components merely because two applications currently look similar.

Share behavior/data abstractions first.

Share visual widgets only when they are truly generic.

---

# 30. Migration From Current Komport Code

The current Komport Qt 6 codebase likely already contains reusable serial/profile/translation functionality.

Migration should be incremental.

Recommended:

```text
1. Identify reusable classes.
2. Extract or wrap them behind library APIs.
3. Keep current terminal working.
4. Add tests around extracted behavior.
5. Only then let analyzer/agent depend on those libraries.
```

Do not perform a large repository-wide rewrite solely to achieve an ideal directory structure.

Architecture should improve in controlled steps.

---

# 31. Avoid Premature Library Explosion

Multiple executables do not require dozens of shared objects immediately.

Initially the implementation may use fewer physical libraries.

Example:

```text
komport-core
komport-analysis
```

Later split when boundaries stabilize.

The important requirement is logical separation and dependency direction.

Physical library boundaries can follow real maintenance needs.

---

# 32. Possible Initial Practical Split

A conservative first step:

```text
libkomport-core
    serial transport
    profiles
    byte utilities
    SessionEvent primitives

komport
    existing terminal UI

komport-analyzer
    new analyzer UI
```

Later:

```text
libkomport-session
libkomport-decoder
libkomport-replay
libkomport-network
komport-agent
```

This avoids overengineering the first refactor.

---

# 33. Why This Product Split Is Valuable

Without separation, the main terminal could eventually contain:

```text
terminal
timeline
sniffer
decoder selector
replay
simulation
fault injection
remote nodes
clock alignment
transaction tables
```

This would damage the simplicity that makes a terminal useful.

With separate executables:

```text
komport
```

remains the everyday tool.

```text
komport-analyzer
```

becomes the advanced engineering tool.

```text
komport-agent
```

becomes infrastructure.

Each can optimize for its own use case.

---

# 34. User Experience

Typical user A:

```text
"I just need a serial terminal."

-> starts komport
```

Typical user B:

```text
"I need to reverse-engineer a Modbus-like protocol."

-> starts komport-analyzer
```

Typical user C:

```text
"The device is in another room/building."

-> runs komport-agent remotely
-> connects with komport or analyzer
```

The project serves all three without forcing them into the same UI.

---

# 35. Suggested Product Positioning

Possible descriptions:

## Komport

```text
Modern Qt 6 serial terminal
```

## Komport Analyzer

```text
Serial protocol analyzer, recorder, replay and device simulation workbench
```

## Komport Agent

```text
Remote serial capture and transport service
```

Together:

```text
Komport Qt 6 Communication Toolkit
```

The exact branding can be decided later.

---

# 36. Future Features and Ownership

Potential ownership:

| Feature | Komport | Analyzer | Agent | Shared Library |
|---|---:|---:|---:|---:|
| Serial terminal | Yes | Optional | No | Transport |
| Profiles | Yes | Yes | Yes | Yes |
| Translation table | Yes | Yes | No | Decoder |
| Session recording | Basic | Full | Capture | Session |
| Hex analysis | Basic | Full | No | Shared model |
| Modbus decode | Optional | Yes | No | Decoder |
| Multi-port sniffing | No | Yes | Capture | Session/Transport |
| Replay | No/basic | Yes | Target | Replay |
| Simulation | No | Yes | Target | Simulation |
| Remote serial | Client | Client | Server | Network |
| PTP/time alignment | No UI | Yes | Metadata/source | Session/Network |
| Fault injection | No | Yes | Execution target | Simulation |

This table is conceptual.

---

# 37. Recommended Codex Task

Before implementation of the analyzer, Codex should:

1. Review the current Komport codebase.
2. Identify reusable serial/profile/translation components.
3. Define dependency boundaries.
4. Propose the smallest practical shared-library extraction.
5. Create an ADR for the multi-executable architecture.
6. Ensure `SessionEvent`, `ITransport` and decoder contracts are application-neutral.
7. Define which functionality remains exclusively in `komport`.
8. Define which functionality starts exclusively in `komport-analyzer`.
9. Keep `komport-agent` architecturally possible without requiring immediate implementation.
10. Avoid a large-bang refactor.

No production implementation should begin until these boundaries are reviewed.

---

# 38. Central Architectural Rule

The most important rule is:

> Komport, Komport Analyzer and Komport Agent are separate products built on the same libraries. None of the executables is the implementation foundation of another.

In short:

```text
shared libraries = platform

komport          = terminal frontend
komport-analyzer = analysis frontend
komport-agent    = remote/headless frontend
```

This preserves the simplicity of the existing terminal while allowing the broader Komport ecosystem to grow without artificial limits.
