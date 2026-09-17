# ADR-004: Stable TX/RX semantics

Status: Proposed
Date: 2026-09-17

## Decision

For every `SessionEvent`, regardless of local serial, TCP, remote transport,
replay or simulation:

```text
TX = Komport sent bytes towards its transport peer.
RX = Komport received bytes from its transport peer.
```

Direction never means "host" versus "device" and never changes when a
recording is replayed or when Komport simulates the recorded device. A
simulator maps its external input/output to this fixed viewpoint when it
selects recorded requests and responses.

`TransportOpened`, `TransportClosed`, configuration, error, annotation and
bookmark events have direction `None`.

## Consequences

- A recording retains one unambiguous meaning across all modes.
- A future full-duplex bridge must explicitly map both endpoints rather than
  redefining directions.
- UI labels may say TX/RX without inventing transport-specific terminology.

## References

- `docs/komport-session-replay-simulation-architecture.md`, sections 11–16
- `docs/komport-engineering-governance-spec-review-workflow.md`, section 16.7
