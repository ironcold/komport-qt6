# ADR-010: Live session recording (`.kpsession` writer) v1

Status: Accepted

Date: 2026-09-18

## Context

M8 delivered the ordered, byte-exact live event stream (`SessionEvent` from
`SessionController::eventObserved`) and deliberately ships no consumer: the
events are currently emitted into a void. The next milestone (M9 in
`TODO.md`) records a live session to disk.

The container is already decided. ADR-006 froze the `.kpsession` v1 file format
(magic, version, JSON header, length-prefixed records with a fixed 44-byte
prefix, little-endian, append-only, binary-safe), fixed its normative v1 writer
profile (member names, value shapes and the local profile values) and its reader
rules (length validation; a malformed complete record fails loading while a
truncated final record is ignored and reported as recovered). ADR-002 fixed the
event model, ADR-004 the direction semantics, ADR-005 the timestamp semantics,
ADR-007 forbids any output path to real hardware while loading or exploring a
file, and ADR-009 keeps the new contracts QtCore-only with library extraction
deferred.

What is *not* decided yet, and belongs to the writer's design rather than to the
container: how a live stream is turned into a file without buffering the session
in memory, what a failed or short write means, what the size limits are, how
recording starts and stops, and what the recorder is allowed to add to the data.

Two properties of the accepted documents shape the design:

- The header's clock-domain entry carries `reference.sourceTimestampNs`, the raw
  source-clock value of the *anchor event* (ADR-005), and a clock domain without
  that pair is not v1-conformant. The anchor is the first event the controller
  accepted, which is normally *earlier* than the moment a user decides to record.
- The header's source descriptor carries an *applied* configuration snapshot.
  M8 distinguishes a stored request from an applied configuration: while the
  transport is closed, a configuration request is stored and no hardware
  transaction happens (SPEC-M8 §6.2, `storedOnly`).

## Decision

### 1. One recorder per document, fed only by the session event stream

`SessionRecorder` consumes exactly one source: the controller's read-only
`eventObserved` signal. It must never read the legacy character signals
(`receivedChar`, `sentChar`) or the legacy RX buffer; those remain display
adapters (AGENTS.md, ADR-002). `KomportDoc` owns the recorder next to the
controller and destroys it after the controller and before the transport.

That order is a lifetime guarantee, not a recording mechanism: `KomportSerial`'s
destructor deliberately emits no `closed` event (M8 rule, with a regression test
proving document destruction emits nothing). The terminal `TransportClosed` event
is recordable because the application closes the transport *before* the document
is torn down - `KomportApp::closeEvent()` calls `KomportSerial::close()` while
controller and recorder are alive - not because of the destruction order. The
application-close ordering is therefore part of this decision, not an
implementation detail: close the transport, then stop and finalise the recorder,
then continue destruction.

### 2. Append-only streaming; the session is never held in memory

Every accepted event becomes exactly one complete record, written when it
arrives. The recorder holds no event vector and no session-sized buffer; its
memory use is bounded by one record's payload. A byte-wise upload therefore
produces one record per accepted write - the deliberate cost SPEC-M8 §13 handed
to M9 - and the recorder's job is to keep memory flat, not to coalesce.

### 3. Write completeness, size limits and what a failure means

Each record is assembled in one buffer with checked arithmetic and written with
one write call. Before allocating or serialising anything, the recorder verifies
that the header stays within 16 MiB, that `metadataLength` and `payloadLength`
stay within `u32`, and that the record body (`metadataLength + payloadLength`) plus
the 40 bytes of prefix that follow `recordLength` stay within 64 MiB - the limits
ADR-006 requires a *reader* to enforce, applied by the writer so it cannot produce
a file a conforming reader must reject. An event that exceeds them is not written:
the recording stops as damaged and the report names its sequence number and sizes.

A short or failed write stops the recording and is reported; the recorder never
continues silently past a partial record. The report names the path, the number of
complete records, the bytes written and the error. A stopped-because-damaged
recording is not deleted: its complete prefix is valid data.

### 4. Recording starts live, and writes its header at once

Recording may only be **started while the session is live** (`SessionController`
in state `Live`). A start request in any other state is refused without creating a
file, with a message explaining that live recording begins once the transport is
open.

The live precondition removes every reason to write the header late. Two facts are
available at the moment of start: the clock domain's anchor already exists (the
live state means an activation was opened, so the controller's reference is valid)
and the applied configuration snapshot can be read from the transport - which is
honest only in this state, because in the closed state M8 stores a request without
applying it. The recorder therefore creates the file at start, writes the magic
and the complete header (ADR-006's normative v1 writer profile) in one call, and
**flushes it before the recording is declared `Live`**. The start flush costs one
call per recording and keeps the header out of the process buffer: an otherwise
idle recording that never receives an event still has its complete header outside
the writer's process buffer - "on disk" in this document means exactly that, not
durable media, because v1 has no `fsync` (§8) - and the flush policy of §8 needs no
special case for it. There is no state in which
recording is switched on but nothing has been decided, and no waiting for a first
event.

The clock-domain reference pair comes from the controller through the read-only
accessor of decision D8, never from the event the recorder happens to see first.
That is what makes a mid-session recording conformant: the recorded stream may
begin at session time 5 s while the header carries the domain's true anchor, whose
session time is exactly 0 (ADR-006).

A recording that is stopped without any event is a valid v1 file - a complete
header and no records - and is reported as "no session data recorded". Once the
complete header has been written successfully, no *invalid* empty `.kpsession` can
exist; a failed or short header write is the one case that leaves an unloadable
file, and §8 states it (decision D1).

Per-event configuration changes are not retro-written into the header; they stay
in the stream as `TransportConfigChanged` events.

### 5. Recording is an explicit user action, and it is inert

Recording starts and stops only on an explicit user action; it is never started
automatically and never restored from a previous UI or application state. The
recorder does not open or close the transport, does not change the session's
state, never writes to the transport, and has no output path other than the file -
ADR-007's safety model applies to it unchanged. The existing text logger's
`Record Session...` action and the logger behind it are untouched; the new
capability gets its own action (decision D6).

### 6. The recorder adds nothing to the data

Every field of a record comes from the `SessionEvent` itself: `sequence`,
`sourceId`, `sourceTimestampNs`, `timestampNs`, event type, direction, metadata
JSON verbatim, payload bytes verbatim, `flags` zero (ADR-006 requires it in v1).
The recorder adds no wall-clock per event, no UI state, no file offsets, no
decoded output and no credentials, and it rewrites nothing: byte-exactness at the
application observation boundary survives recording unchanged (AGENTS.md). The
header it writes is exactly ADR-006's v1 writer profile - no private members.

### 7. QtCore-only, no widgets

`SessionRecorder` and its byte-assembly helper depend on Qt Core only, so they can
join a shared library later without a rewrite (ADR-009); they include no widget or
application type. The menu action, the file dialog, the recording indicator and
the error reporting are app-level wiring and stay outside the recorder.

### 8. Durability and crash bounds (no fsync in v1)

A successful flush bounds only the writer's own buffer; without `fsync` M9 gives
no durable-storage bound at all. Stated honestly:

- A *process* crash loses what the process had not yet flushed (at most the writes
  of the last second, because the periodic flush of §8's last paragraph bounds
  retention) and can leave the final record truncated.
  The file that remains is a loadable one whose complete prefix is recovered.
- A host or power failure can additionally lose records that the process had
  already flushed, because flushing hands bytes to the operating system and not to
  durable storage. A recovered file may therefore omit a suffix of records and may
  end in one truncated final record - which ADR-006 defines as recoverable.
- A short or failed write of the header at start can leave an incomplete header,
  and ADR-006 fails loading on an invalid header rather than recovering it. The
  bounds of the two bullets above apply only once the complete header has been
  written *and flushed* - that is, after the start flush of §4, which is what puts
  the header on disk instead of in the process buffer.
- A flush failure is a damaged-state transition: the recording stops and is
  reported like a write failure.

The flush policy is a **periodic** flush on the session thread, not a check that
only happens when something else moves: as soon as a write leaves unflushed data
in a `Live` recording, a timer is armed for one second, and the flush happens no
later than one second after that first unflushed write, whether or not another
event ever arrives. A burst of writes flushes at most once per second (the timer
is not re-armed while it is pending). Stopping flushes once more, and a timer flush
failure is a damaged transition like any other flush failure. This bounds the
writer's own buffer; it is not a durability guarantee, and v1 makes none.

### 9. Finalisation

Stopping closes the file after a final flush and reports the outcome: path,
complete record count, bytes written, session duration, and any error. Stopping
the application follows the ordering of §1. A destructor performs no more than
that final flush and emits nothing.

## Consequences

- Recording is testable without a GUI: a file, byte-level assertions, a
  deterministic event source and a test seam for the file sink.
- A process crash loses the records the process had not yet flushed - bounded by
  the flush cadence - and can leave the final record truncated; a host or power
  failure can additionally lose records that were already flushed, and a failure
  during the initial header write can leave a file that does not load. All three
  are stated in §8 rather than hidden behind a "recovered" claim.
- The milestones keep their order: no reader, no replay, no decoder, no
  transmission, no second format.
- Recording a byte-wise upload scales in records, not in memory. The file for a
  64 KiB upload is at least 64 KiB payload plus 44 bytes per accepted write plus
  JSON metadata per record; that is accepted deliberately and is a property M10's
  loader must handle.

## Decisions resolved by the gate review (round 1)

- **D1 empty recording: closed by §4, not by the earlier options.** Option (c)
  ("no file until the first record") was chosen under the assumption that the
  header cannot be written before the first event, which was only true while a
  start in the idle state was allowed. With the live-only start of §4 the anchor
  and the applied configuration are both known at start, so the file gets its
  complete header immediately and a zero-record recording is a *valid* v1 file
  rather than an invalid one. The substance of (c) is kept in the qualified form
  of §4: once the complete header is written, no invalid empty `.kpsession` can
  exist, and the recorder no longer has a state in which it is switched on but has
  decided nothing.
- **D2 flush policy: a one-second periodic flush.** An armed one-second timer on
  the session thread, started by the first unflushed write of a live recording, so
  the process buffer is bounded even when the line goes idle; a burst flushes at
  most once per second, stopping flushes once more, and a timer flush failure is a
  damaged transition. The durability bounds of §8 are stated and tested through a
  scheduler and clock seam.
- **D3 test-side structural reader: yes.** A minimal, independent parser under
  `tests/` that validates magic, version, header, record structure and extracts
  payloads. It must not link production codec code, must not become public API and
  is not M10's reader.
- **D4 identity strings: fixed.** Clock-domain id `local-process-monotonic-v1`,
  `kind` `"process-monotonic"`, source name `local serial`, `transport` `serial`,
  endpoint written unredacted (a device path is not a credential).
- **D5 file placement: current directory, timestamped suggested name.** Use
  `QDir::currentPath()` as the dialog's directory as the existing dialogs do, and
  a suggested name carrying date and time; overwriting an existing file requires
  the normal save-dialog confirmation.
- **D6 UI surface: a separate action.** The text logger's `Record Session...`
  action stays exactly as it is; `.kpsession` recording gets its own checkable
  action `Record Live Session...`, whose checked state is the indicator, with
  start/stop/failure outcomes reported through the existing translated status
  mechanism.
- **D7 configuration snapshot: supplied by the application by value at start**,
  valid only because start requires the live state (§4).
- **D8 anchor reference: a read-only controller accessor.** `SessionController`
  gains

  ```cpp
  struct SessionClockDomainReference {
    bool valid = false;
    qint64 sourceTimestampNs = 0;
    qint64 sessionTimestampNs = 0; // exactly 0 when valid in v1
  };
  SessionClockDomainReference clockDomainReference() const;
  ```

  It is QtCore-only, read-only, returns `valid == false` before the first accepted
  event, and is recorded as an M9 addition in SPEC-M8 §6.2 so that the frozen M8
  surface stays complete.

No decision remains open in this ADR.

## References

- ADR-002 (session event v1), ADR-004 (direction semantics), ADR-005 (timestamp
  semantics), ADR-006 (`.kpsession` v1 file format, including its normative v1
  writer profile), ADR-007 (replay safety model), ADR-009 (multi-executable
  product architecture)
- `docs/specs/SPEC-M8-session-transport-foundation.md`, sections 6.2, 8, 9, 13
  and 17 (the event stream, its timing, the `storedOnly` distinction and the
  streaming hand-off)
- `docs/komport-session-replay-simulation-architecture.md`, sections 4, 6, 7 and
  8 - note that its section 7.5 ("Crash recovery") is the older architecture text;
  the normative recovery rule is in ADR-006's Decision section
- `docs/komport-engineering-governance-spec-review-workflow.md`
