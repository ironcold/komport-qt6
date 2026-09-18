# Komport Qt 6 – Engineering Governance, Spec-First Workflow and Multi-Agent Review Rules

## Purpose

This document defines the development workflow for Komport Qt 6.

It is intended as a binding project rule for work performed with multiple coding agents and reviewers, especially:

- **Codex** as architecture owner and independent reviewer
- **Claude** as implementation-spec author and implementation agent
- **Qwen** as implementation agent for clearly scoped work packages

The goal is to protect the architectural foundation of Komport while allowing multiple agents to work efficiently in parallel.

This document complements the project architecture documents, especially:

```text
komport-decoder-session-concept-evolution.md
komport-session-replay-simulation-architecture.md
```

The central principle is:

> No production code is written before a reviewed implementation specification exists for the task.

---

# 1. Why This Workflow Exists

Komport is evolving from a traditional serial terminal into a broader communication platform with:

- transport abstraction
- session recording
- byte-exact event capture
- protocol decoding
- replay
- device simulation
- network transports
- remote agents
- protocol-aware testing

These features share common architectural foundations.

A locally convenient implementation can easily create long-term problems if it:

- bypasses the transport abstraction
- couples UI code directly to `QSerialPort`
- stores rendered text instead of raw bytes
- makes decoder output authoritative
- duplicates replay or simulation logic
- introduces transport-specific assumptions into decoders
- creates a second network-specific event model
- silently changes timing or direction semantics

Therefore architectural consistency has priority over implementation speed.

---

# 2. Roles

## 2.1 Codex – Architecture Owner and Independent Reviewer

Codex is responsible for:

- maintaining architectural consistency
- refining architecture documents
- approving or rejecting architectural changes
- defining or reviewing ADRs
- reviewing implementation specifications
- reviewing completed implementation work
- identifying architecture drift
- checking whether future network/replay/simulation paths remain open
- validating acceptance criteria
- performing adversarial reviews where appropriate

Codex should not merely review code style.

Its primary responsibility is:

> Does this implementation preserve the intended architecture and future extensibility?

---

## 2.2 Claude – Spec Author and Implementation Agent

Claude should normally work in two distinct phases:

```text
Phase 1: Specification
Phase 2: Implementation
```

Claude must not begin production implementation before the specification is reviewed.

Claude is expected to:

- analyze the assigned task
- identify relevant architecture documents
- identify affected interfaces and components
- define invariants
- define data flow
- define error handling
- define tests
- define acceptance criteria
- identify architectural ambiguities
- explicitly request decisions where required
- implement only after specification approval
- perform a self-review against the approved specification

Claude must not silently resolve architecture questions in code.

---

## 2.3 Qwen – Scoped Implementation Agent

Qwen should primarily receive tasks that already have:

- an approved specification
- defined interfaces
- defined architecture boundaries
- acceptance criteria
- required tests

Qwen may suggest improvements, but should not independently redefine project architecture during implementation.

If implementation reveals an architectural problem, work should stop at the decision boundary and the issue should be escalated.

---

# 3. Fundamental Project Rule: Spec Before Code

The following rule is binding:

> No production code is written before a reviewed implementation specification exists for the task.

Exceptions should be rare and explicit.

Reasonable exceptions may include:

- disposable experiments
- diagnostic probes
- temporary test scripts
- minimal reproduction cases
- throwaway prototypes

Such code must be clearly marked as non-production and must not silently become part of the production architecture.

---

# 4. Required Workflow

Every meaningful implementation task should follow this sequence:

```text
1. Task definition
2. Architecture lookup
3. Implementation specification
4. Spec review
5. Decision resolution
6. Implementation
7. Agent self-review
8. Independent review
9. Fix cycle
10. Acceptance
```

---

# 5. Step 1 – Task Definition

The task must have a clear scope.

Bad:

```text
Implement replay.
```

Better:

```text
Implement passive replay of loaded session events through the existing SessionEvent pipeline, with original-speed and immediate timing modes. No active transmission.
```

The task should define what is explicitly out of scope.

---

# 6. Step 2 – Architecture Lookup

Before writing the spec, the implementation agent must identify the relevant architecture sources.

Example:

```text
Relevant documents:
- docs/session-replay-simulation-architecture.md
  - sections 5, 9, 23
- docs/decoder-session-concept-evolution.md
  - sections 7-13
- ADR-0003 SessionEvent direction semantics
```

The implementation spec should reference these sources.

The agent must not rely only on memory of previous discussions.

---

# 7. Step 3 – Implementation Specification

A technical implementation specification must be produced before coding.

The specification should be concise enough to review, but detailed enough that implementation does not require architectural invention.

At minimum it must define:

- objective
- scope
- non-goals
- architecture references
- affected components
- public interfaces
- data flow
- invariants
- state transitions where applicable
- error handling
- concurrency/threading assumptions
- persistence implications
- backward compatibility
- test strategy
- acceptance criteria
- unresolved decisions

---

# 8. Step 4 – Spec Review

The specification must be reviewed before implementation.

Review should answer:

```text
Does this fit the architecture?

Does it create a new side path?

Does it duplicate an existing abstraction?

Does it close future extension paths?

Does it preserve byte-exact data?

Does it preserve timing/direction semantics?

Does it remain transport-neutral where required?

Does it remain decoder-neutral where required?

Are failure modes defined?

Are tests sufficient?
```

Only after review should implementation begin.

---

# 9. Step 5 – Decision Resolution

If the specification contains unresolved architecture questions, they must be resolved before implementation.

The agent must use a clearly visible section:

```text
Decision Required
```

Example:

```text
Decision Required:

Should passive replay emit TransportOpened / TransportClosed events from the
recording, or should those remain metadata-only during offline replay?

This affects whether UI connection-state code can consume replay events unchanged.
```

The agent must not choose an answer silently inside implementation code.

---

# 10. Step 6 – Implementation

Implementation must follow the approved specification.

If implementation requires a significant deviation, the implementation should pause at that boundary.

The spec must be amended and reviewed before continuing.

Small implementation details that do not affect:

- public interfaces
- architecture
- behavior
- persisted formats
- concurrency
- safety

may be resolved locally.

---

# 11. Step 7 – Agent Self-Review

Before handing work to Codex, the implementation agent must perform a self-review.

The self-review should compare:

```text
implemented behavior
vs.
approved specification
vs.
architecture rules
```

The result should explicitly report:

- completed requirements
- deviations
- known limitations
- test coverage
- unresolved issues
- architecture concerns discovered during implementation

---

# 12. Step 8 – Independent Codex Review

Codex performs an independent review.

The review should not assume the implementation is correct simply because tests pass.

It should inspect:

- architecture boundaries
- hidden coupling
- error behavior
- byte preservation
- timing semantics
- API consistency
- future network implications
- replay/simulation implications
- ownership/lifetime
- state management
- persisted format stability
- regression risks

Codex should review against the specification, not against the implementation author's intent.

---

# 13. Step 9 – Fix Cycle

Findings should be categorized.

Recommended categories:

```text
BLOCKER
HIGH
MEDIUM
LOW
ARCHITECTURE QUESTION
DOCUMENTATION
TEST GAP
```

Fixes should follow the same rule:

If a fix changes architecture or behavior beyond the approved spec, update the spec first.

---

# 14. Step 10 – Acceptance

A task is accepted only when:

- implementation matches the approved specification
- required tests pass
- architecture review is complete
- no unresolved blocker/high finding remains
- documentation is updated where required
- persisted format or API changes are documented

---

# 15. Claude Implementation Specification Template

The following template should be used for Claude implementation specifications.

---

## Claude Spec Template

```markdown
# Implementation Spec: <Task Name>

## 1. Objective

Describe exactly what this task adds or changes.

## 2. Scope

Included:

- ...
- ...
- ...

## 3. Non-Goals

Explicitly not included:

- ...
- ...
- ...

## 4. Architecture References

Relevant project documents and sections:

- `<document>` – section `<x>`
- `<ADR>` – decision `<x>`

## 5. Current State

Describe the current implementation relevant to this task.

Include:

- existing classes
- existing interfaces
- current data flow
- known limitations

## 6. Proposed Design

Describe the implementation approach.

### 6.1 Components Affected

- `ClassA`
- `ClassB`
- `InterfaceC`

### 6.2 New Components

- `NewClass`
- `NewInterface`

### 6.3 Public Interface Changes

```cpp
// proposed API
```

State whether interfaces are:

- new
- modified
- deprecated
- unchanged

## 7. Data Flow

Describe the path of data.

Example:

```text
Transport
  -> SessionEngine
  -> SessionEvent
  -> Recorder
  -> Decoder
  -> UI
```

## 8. State Model

If stateful, define states and transitions.

Example:

```text
Idle
 -> Running
 -> Paused
 -> Completed

Running
 -> Failed
 -> Cancelled
```

## 9. Invariants

List conditions that must always remain true.

Example:

- Raw payload bytes are never modified.
- TX/RX direction has global session semantics.
- Replay never writes to a real transport unless active replay is explicitly enabled.
- Decoder output is derived and non-authoritative.

## 10. Error Handling

Define expected failures.

For each:

- detection
- propagation
- user-visible behavior
- logging
- recovery

## 11. Threading and Concurrency

State explicitly:

- owning thread
- signal/slot crossing
- synchronization
- cancellation behavior
- ordering guarantees

If not applicable:

```text
No new threading behavior is introduced.
```

## 12. Persistence / File Format Impact

State:

- no persistence impact

or define:

- file format changes
- version changes
- migration behavior
- backward compatibility

## 13. Network / Remote Impact

State whether this task:

- is transport-neutral
- assumes local serial
- affects remote timestamping
- changes serialization requirements
- changes event ordering

Explicitly explain why future network support remains possible.

## 14. Replay / Simulation Impact

State whether the change affects:

- passive replay
- active replay
- simulation
- event reproducibility
- timing

## 15. Decoder Impact

State whether:

- decoders see new event types
- decoder API changes
- protocol state changes
- offline decoding remains possible

## 16. Security / Safety Considerations

Especially for anything that may transmit bytes.

Include:

- physical side effects
- automatic transmission prevention
- confirmation requirements
- replay target visibility

## 17. Testing Strategy

### Unit Tests

- ...

### Integration Tests

- ...

### PTY / Transport Tests

- ...

### Regression Tests

- ...

### Negative Tests

- ...

## 18. Acceptance Criteria

The task is complete when:

- [ ] ...
- [ ] ...
- [ ] ...

Acceptance criteria must be observable and testable.

## 19. Documentation Changes

Required updates:

- ...
- ...

## 20. Compatibility

Describe compatibility with:

- existing session files
- existing profiles
- existing translation tables
- public APIs
- configuration files

## 21. Risks

List foreseeable risks and mitigations.

| Risk | Impact | Mitigation |
|------|--------|------------|
| ... | ... | ... |

## 22. Decision Required

List unresolved architecture questions.

If none:

```text
None.
```

## 23. Implementation Plan

Ordered implementation steps:

1. ...
2. ...
3. ...

No production implementation begins until this specification is approved.
```

---

# 16. Required Invariants for Komport

The following rules should be treated as project-wide invariants.

## 16.1 Raw Bytes Are Authoritative

Raw communication bytes must never be replaced by interpreted text.

```text
Raw -> Decode
```

not:

```text
Decode -> reconstruct raw
```

---

## 16.2 Session Events Are the Common Data Model

Live traffic, loaded sessions and replay should converge on the same conceptual event model.

Avoid separate models such as:

```text
LiveSerialEvent
ReplayEvent
NetworkEvent
```

unless there is a compelling architectural reason.

---

## 16.3 UI Does Not Own Transport Logic

Avoid:

```text
MainWindow -> QSerialPort
```

Prefer:

```text
UI -> controller/session layer -> ITransport
```

---

## 16.4 Decoders Are Transport-Neutral

A decoder must not depend directly on:

- `QSerialPort`
- TCP socket objects
- device path names
- COM port names

It operates on session/event data.

---

## 16.5 Network Is a Transport, Not a Second Architecture

Future network functionality must plug into the same event/session pipeline.

---

## 16.6 Replay Is Safe by Default

Loading or passively replaying a session must never transmit to real hardware.

Active transmission must be explicit.

---

## 16.7 Session Direction Semantics Are Stable

Direction must always have one global definition.

Recommended:

```text
TX = Komport sent bytes toward its transport peer
RX = Komport received bytes from its transport peer
```

This must remain true for:

- live serial
- TCP
- remote agent
- replay
- simulation

---

## 16.8 Timing Origin Is the Transport Source

For local serial:

```text
timestamp locally
```

For remote serial:

```text
timestamp at remote agent
```

Network latency must not become serial timing.

---

## 16.9 Persisted Formats Are Explicitly Versioned

Do not rely on opaque framework serialization as a long-term public file format.

Any persistent public format must define:

- magic
- version
- byte order where relevant
- compatibility behavior

---

## 16.10 Original Sessions Are Immutable

Simulation changes should not rewrite forensic source captures.

Use overlays/scenarios for:

- parameter changes
- fault injection
- timing changes
- response substitutions

---

# 17. Architecture Escalation Rule

Implementation agents must escalate rather than silently decide when a question affects any of the following:

- public API
- persistent file format
- SessionEvent semantics
- direction semantics
- timing semantics
- transport abstraction
- decoder abstraction
- network architecture
- replay safety
- simulation behavior
- threading model
- backward compatibility
- GPL/license boundary

Required wording:

```text
ARCHITECTURE DECISION REQUIRED
```

followed by:

```text
Question:
...

Options:
1. ...
2. ...

Trade-offs:
...

Recommendation:
...

Affected architecture:
...
```

Implementation stops at this decision boundary until the decision is resolved.

---

# 18. ADR Rule

Significant architecture choices should be captured as ADRs.

Suggested location:

```text
docs/adr/
```

Suggested naming:

```text
0001-session-event-model.md
0002-transport-abstraction.md
0003-direction-semantics.md
0004-session-file-format-v1.md
0005-replay-safety-model.md
```

ADR structure:

```markdown
# ADR-NNNN: Title

## Status

Proposed / Accepted / Superseded / Rejected

## Context

Why is a decision required?

## Decision

What is being decided?

## Consequences

Positive and negative consequences.

## Alternatives Considered

What else was considered?

## References

Architecture documents / issues / specs.
```

---

# 19. Parallel Work Rules

Parallel implementation is allowed only when work packages have stable boundaries.

Good parallelization:

```text
Agent A:
Session file reader/writer

Agent B:
Modbus decoder

Agent C:
UI timeline view
```

provided their interfaces are already defined.

Bad parallelization:

```text
Agent A defines SessionEvent
Agent B independently defines replay events
Agent C independently defines network events
```

Parallel agents must not invent overlapping abstractions.

---

# 20. Interface Freeze Before Parallel Work

Before multiple agents implement related subsystems, Codex should freeze at least the relevant interface contract.

Example:

```text
SessionEvent v1
ITransport v1
ProtocolDecoder v1
```

"Freeze" does not mean immutable forever.

It means:

> implementation agents may rely on the contract and must escalate changes rather than independently modifying it.

---

# 21. Review Checklist for Codex

Codex should use the following checklist during implementation review.

## Architecture

- [ ] Matches approved spec
- [ ] Uses intended abstraction
- [ ] Does not bypass SessionEvent
- [ ] Does not bypass ITransport
- [ ] Does not duplicate an existing architecture path
- [ ] Remains future-network-compatible

## Data Integrity

- [ ] NUL bytes preserved
- [ ] Arbitrary binary payload preserved
- [ ] Direction preserved
- [ ] Timing preserved
- [ ] Chunk ordering preserved

## Decoder

- [ ] Decoder does not mutate raw events
- [ ] Decoder does not depend on physical transport
- [ ] Decoder state resets deterministically
- [ ] Offline and live behavior are consistent

## Replay

- [ ] Passive replay cannot transmit
- [ ] Active replay is explicit
- [ ] Replay uses normal transport abstraction
- [ ] Cancellation is defined
- [ ] Timing policy is explicit

## Simulation

- [ ] Simulator does not alter source session
- [ ] Request matching is deterministic
- [ ] Mismatch behavior is defined
- [ ] Responses use normal output path

## Network

- [ ] Remote timing semantics remain valid
- [ ] No UI-level network special case is introduced
- [ ] Event serialization remains possible

## Persistence

- [ ] Format versioning respected
- [ ] Reader handles invalid/truncated input safely
- [ ] Backward compatibility considered

## Tests

- [ ] Positive tests
- [ ] Negative tests
- [ ] byte-range tests
- [ ] NUL regression
- [ ] PTY/integration tests where applicable
- [ ] cancellation/error tests

---

# 22. Claude Self-Review Template

After implementation Claude should provide:

```markdown
# Implementation Self-Review: <Task>

## Spec Compliance

- Requirement 1: implemented / not implemented
- Requirement 2: implemented / not implemented

## Deviations From Approved Spec

None.

or:

- ...

## Architecture Check

- Transport abstraction respected: yes/no
- SessionEvent semantics unchanged: yes/no
- Network path preserved: yes/no
- Replay/simulation implications: ...

## Tests Added

- ...
- ...

## Test Results

- ...

## Known Limitations

- ...

## New Risks Found During Implementation

- ...

## Decisions That Need Follow-Up

None / ...

## Ready for Independent Review

Yes / No
```

---

# 23. Definition of Done

A feature is not done merely because:

```text
it compiles
```

or:

```text
the UI works
```

Definition of Done:

```text
Spec approved
Implementation complete
Tests added
Tests passing
Self-review complete
Independent review complete
Architecture documentation updated
No unresolved high/blocker issue
```

---

# 24. Commit and PR Guidance

Where practical, commits should separate:

```text
architecture/spec
implementation
tests
documentation
```

Avoid giant mixed commits that make architectural review difficult.

A PR or review request should reference:

```text
Spec:
docs/specs/<spec>.md

Architecture:
docs/...

ADR:
docs/adr/...

Acceptance Criteria:
<list or section>
```

---

# 25. Suggested Repository Documentation Structure

Recommended:

```text
docs/
├── architecture/
│   ├── decoder-session-concept-evolution.md
│   └── session-replay-simulation-architecture.md
│
├── adr/
│   ├── 0001-session-event-model.md
│   ├── 0002-transport-abstraction.md
│   └── ...
│
├── specs/
│   ├── SPEC-TEMPLATE.md
│   └── ...
│
└── development/
    └── engineering-governance.md
```

This keeps:

```text
WHY
WHAT
DECISIONS
IMPLEMENTATION SPECS
PROCESS
```

separate but linked.

---

# 26. Recommended First Codex Task

After receiving this document and the architecture documents, Codex should first formalize the architectural foundation before delegating implementation.

Suggested task:

```text
Review the Komport architecture documents and engineering governance.

Before implementation begins:

1. Define the minimal stable SessionEvent v1 contract.
2. Define ITransport v1.
3. Define ProtocolDecoder v1.
4. Create ADRs for:
   - SessionEvent semantics
   - transport abstraction
   - TX/RX direction semantics
   - timestamp semantics
   - session file format strategy
   - replay safety model
5. Identify any architecture contradictions.
6. Produce the first implementation milestone breakdown.
7. Define which interfaces must be frozen before Claude/Qwen work in parallel.

Do not implement production code during this task.
```

---

# 27. Project Rule Summary

The working model is:

```text
Codex:
    architecture
    ADRs
    independent review

Claude:
    spec first
    implementation second
    self-review

Qwen:
    implementation from approved specs

All:
    escalate architecture ambiguity
```

And the central development rule remains:

> Design decisions belong in reviewed specs and ADRs, not hidden inside implementation code.

---

# 28. Final Principle

Komport should grow by adding components to a stable foundation, not by repeatedly reshaping the foundation around new features.

Therefore:

```text
lay foundations
freeze contracts where needed
specify work
review the spec
implement narrowly
review independently
extend deliberately
```

This process is intentionally slower at the beginning.

The expected result is faster and safer development once recording, replay, simulation, decoders and network support begin to evolve in parallel.
