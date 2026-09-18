# ADR-011: Session reader and passive replay (`.kpsession` reader/player) v1

Status: Proposed

Date: 2026-09-18

## Context

M9 delivered the `.kpsession` v1 writer: `SessionRecorder` consumes
`SessionController::eventObserved`, appends one record per accepted event, writes and
flushes the complete header at start, and stops as damaged on a write, flush or
encoding failure (ADR-010, SPEC-M9). The format itself was frozen earlier by ADR-006,
including its byte layout, its limits (header at most 16 MiB, record body at most
64 MiB, all lengths `u32`), its normative v1 writer profile (member names, value
shapes, the fixed local profile values) and its **reader rules**: invalid magic,
version, header encoding or header refuse the file; a structurally inconsistent
record refuses the file; a truncated final record is ignored and reported as
recovered. Verified in the shipped code: `komport/sessionrecordcodec.h` carries the
44-byte prefix, `kMaxHeaderBytes` and `kMaxRecordBodyBytes`; `komport/sessionrecorder.h`
carries the recorder states; `tests/sessionrecordreader.h` is M9's *test-side*
structural parser, which SPEC-M9 decision D3 explicitly constrains: it links no
production codec code, is not public API and **is not M10's reader**.

Nothing consumes the format yet. M10 is the first and only consumer: it reads a
recorded file back and replays it passively, in the same view, without hardware.

Three accepted documents bind this milestone:

- **ADR-007** fixes the replay safety model: "Loading a `.kpsession` creates an
  offline session only. Passive replay distributes stored, immutable `SessionEvent`
  values to views and decoders through the read-only replay-player interface
  specified in M10; it is neither a live transport (ADR-003) nor the live observation
  path of `SessionController`. It has no `ITransport` output target and cannot create
  one, and it is safe to start without a confirmation." This ADR decides that
  interface, so it must satisfy ADR-007 rather than reinterpret it. Active TX replay
  and simulation are explicitly later milestones and are not part of it.
- **ADR-002** fixes the event model and its two-part validation contract: structural,
  event-local validation (`isValidSessionEvent()`) and stateful stream validation
  (`sequence` starts at one and strictly increases; session time is non-decreasing
  within a clock domain), which "belongs to the producing controller". A loaded file is a
  stream that a controller produced earlier, so M10 has to decide what a loader does with
  a stored stream that violates it - and, because recording may begin mid-session
  (ADR-010 §4), a file's first record may legitimately carry a sequence above one, which
  D3 makes explicit.
- **ADR-005**, **ADR-004** and **ADR-008** fix, respectively, that stored timing is
  *evidence* while "replay timing is an explicitly chosen policy and must not
  overwrite it", that TX/RX keep one global meaning in replay too, and that a session
  is source-aware while cross-source alignment is a later, separately specified
  concern. **ADR-009** requires the new contracts to be QtCore-only, carrying no
  widget or application type, with the library extraction still deferred.

From the architecture document, section 9.1 defines passive replay (offline analysis,
"no bytes leave Komport", the timing options and step mode), section 23 places passive
replay before active replay and expects the reader to act as an event source, section 42
defines the MVP boundary (load a session, show it in the current text/hex views, show
RX/TX direction, navigate events; passive original-speed, passive immediate and step
replay) and section 10 keeps active TX replay - with its required confirmation and its
visible target - out of it.

The project owner also took four scope decisions that this ADR records rather than
negotiates: (a) the M10 timing ladder is original timing, immediate, 0.1x, 0.25x, 0.5x,
1x, 2x, 5x, 10x and step mode with next event / next RX / next TX, while "next decoded
frame" belongs to M11; (b) replay happens in the same view in an explicit offline
state whose indicator reads `Offline Session — Passive Replay`, and loading is only
permitted while neither a live session nor a recording is active; (c) M10 is
forward-only - start/restart, play/stop, event/RX/TX step, no arbitrary seek, though
internal indexing may be prepared for a later seek; (d) a file whose `sources[]` holds
more than one source is refused cleanly with `multi-source sessions not supported in
M10`, with no partial rendering and no implicit source selection, because multi-source
remains M14.

What is *not* decided by those documents, and belongs to M10's design: what a loader
is in this architecture (a component, a transport, or the controller), how much of the
header it interprets, what it does with a file that violates the stream contract, what
the replay-player interface looks like, how the timing ladder and the step operations
are defined precisely, where the offline session lives and who owns it, and what makes
"no code path can transmit" checkable rather than asserted.

## Decision

Each decision is numbered so that SPEC-M10, its tests and its reviewers can cite it;
the register at the end lists all of them in one line each.

### 1. Loading creates an offline session through a reader, never through a transport (D1)

`SessionReader` is a QtCore-only component that opens a `.kpsession` file **read-only**,
validates it against ADR-006, and returns either a refusal or one immutable offline
session (`SessionFile`: the typed header facts plus the ordered stored events). It is
not an `ITransport` implementation, it is not registered with `SessionController`, it
holds no transport and it never opens, configures, closes or writes one. The offline
session is a value that views and the replay player consume; it has no output side.

*Rejected:* a `ReplayTransport : ITransport` that "reads" the file (the architecture's
component sketch mentions one). Replay would then sit inside the live observation path,
`SessionController` would treat stored events as live observations, and the future
active-output milestone of architecture section 25 would have a transport-shaped socket
to plug into - exactly the accidental enablement ADR-007 exists to prevent.

*Rejected:* the portal/controller feeding the file into the existing view as if it were
live traffic. ADR-007 names the live observation path of `SessionController` explicitly
as *not* the replay path, and a file has no activation, no transport and no
transmission to observe.

*Rejected:* extending the M9 test-side parser (`tests/sessionrecordreader.h`) into the
production reader. SPEC-M9 D3 forbids it: the test reader's independence is what makes
it able to catch a writer defect, and inverting that would make one implementation both
producer's mirror and consumer.

### 2. The reader validates the format itself and interprets the header exactly once (D2)

The reader enforces ADR-006's rules with its **own constants and its own profile
check**; it does not call `SessionRecordCodec` and does not share its code. Where the
two must agree (the magic, version 1, header encoding 1, the 16 MiB and 64 MiB limits,
the record prefix layout) a test pins them equal, so drift is caught while a change made
for the writer's benefit cannot silently relax the reader.

The header is validated against ADR-006's normative v1 writer profile - root members,
`format` `komport-session`, `version` the JSON number 1, `created` an ISO-8601 UTC
string, `application` the `{name, version}` pair, the source descriptor with its
`sourceId`/`clockDomainId`/`name`/`transport`/`configuration`, the exhaustive nested
`configuration` schema, the clock domain entry with its reference pair as canonical
decimal strings and `sessionTimestampNs` exactly `"0"`, and the source's
`clockDomainId` resolving to an existing domain - and **interpreted into typed facts**;
the raw header JSON is not exposed to any consumer. The header has exactly one
interpretation in the program, and no view can invent a second one.

The reader accepts exactly one source descriptor and exactly one clock domain. A file
with more than one source is refused with the verbatim message

```text
multi-source sessions not supported in M10
```

and a file with more than one clock domain with the message of the same form
`multi-clock-domain sessions not supported in M10`. Both are refusals of the whole
file: no partial rendering, no implicit selection of a source, no rendering of one
source as if it were the session. The file is identified by its magic and version, not
by its name or extension.

*Alternatives:* accepting a header that merely parses (rejected: the offline session
would then carry unvalidated facts, and "the profile is part of the format" - a writer
that deviates is as broken as one that writes bad lengths); exposing the raw header
JSON for views to interpret (rejected: two dialects of the same header, the failure
mode ADR-010 D7 already refused for the configuration snapshot); supporting the first
source of a multi-source file implicitly (rejected by the owner's scope decision, and
implicit selection would present one channel as the session); requiring the
`.kpsession` extension (rejected: the format is bytes, and a renamed file is still
readable).

### 3. Record-level validation, refusal and the recovery rule (D3)

Every declared length is validated against ADR-006's limits *before* the bytes it
describes are read or any buffer is sized from it. A structurally inconsistent
complete record - `40 + metadataLength + payloadLength != recordLength`, a body above
64 MiB, non-zero `flags`, a metadata blob that is not a JSON object - refuses the whole
file, which is ADR-006's rule.

Beyond ADR-006's byte structure, the reader refuses a file whose records do not form a
valid ADR-002 stream, because a loaded session is presented to views as an ordered event
stream and must satisfy what a live stream satisfies:

- event-local validity of every record: a declared `eventType` and `direction`, `Data`
  with a non-empty payload and direction `Tx` or `Rx`, every non-data event with an
  empty payload and direction `None`, a non-zero `sourceId`, non-negative timestamps
  (the same rules `isValidSessionEvent()` encodes);
- source resolution: every record's `sourceId` equals the file's single source
  descriptor's `sourceId`, and one clock domain covers the whole file;
- the stream properties ADR-002 assigns to the producer: `sequence` is non-zero and
  strictly increases from record to record, and `timestampNs` is non-decreasing within
  the domain. A file is **not** required to start at `sequence` 1 or at session time 0:
  recording may begin mid-session (ADR-010 §4 writes the domain's true anchor into the
  header while the first recorded event already sits at a later session time and a later
  sequence number), so the record's first sequence is evidence, not a defect. What must
  hold is that the records of one file are a strictly increasing, non-decreasing-time
  sub-sequence of what the controller emitted.

A violation is a refusal naming the record index and the property; the file is not
partially presented, and the offline session is not created.

A file that ends inside a record - fewer than 44 bytes remaining at a record boundary,
or a `recordLength` that runs past the end of the file - is **not** an error: ADR-006's
recovery rule applies. The complete prefix is recovered as the offline session, the
truncated record is ignored, never delivered and never treated as a valid event, and
the load result reports that a truncated final record was recovered. A truncation
inside the start block or the header, by contrast, refuses the file, because ADR-006
fails loading on an invalid header rather than recovering it.

*Alternatives:* refusing on stream violations at all (rejected: this is less strict than
ADR-006 requires in spirit and would let a defective file be presented as a valid,
ordered, presumably authoritative event stream); treating stream violations as
diagnostics and loading anyway (rejected for M10: it needs a diagnostics channel and a
policy for "which defect still renders", both of which are later design work - and a
forensic format should refuse rather than guess); recovering a prefix across a
*malformed complete* record (rejected: that is the difference between "the file was cut
short mid-write" and "these bytes are inconsistent", and ADR-006 draws exactly that
line).

### 4. Loading is all-or-nothing per file; a recovered prefix is the only partial result (D4)

A refused file yields no offline session, no events, no rendering and no partial
display; the reason is reported to the user. The only partial outcome the milestone
has is ADR-006's recovery of a truncated final record, which is a *complete* session
whose last, unfinished record was dropped and reported. The two outcomes stay
distinguishable everywhere: in the load result, in the status report and in the tests.

*Alternatives:* showing the complete prefix of a refused file with a warning (rejected:
the user would be looking at evidence that the format, and therefore the loader, says
is inconsistent; silently extending "recovered" to cover malformed bytes would erase
the only signal the format gives about file integrity).

### 5. The loaded session is immutable and delivered verbatim (D5)

Payload bytes, metadata JSON, `sequence`, `sourceId` and both timestamps are delivered
exactly as stored. The reader never translates (no charset translation), trims,
coalesces or splits payloads, never rewrites `sourceTimestampNs`, and never recomputes
`timestampNs` - stored timing is evidence (ADR-005), and the reference pair is retained
as evidence, not used to re-derive what is already stored. The player hands out `const`
references to stored values, never replaces them with derived data (ADR-002's
immutability contract), and never modifies the stored session or the file it came from.

### 6. The replay player is a read-only QtCore-only interface (D6)

`SessionReplayPlayer` is the interface ADR-007 refers to. It is QtCore-only, has no
widget, application or transport type in its interface, takes the loaded session as a
`std::shared_ptr<const SessionFile>` (so the offline session exists once in memory and
cannot be mutated through the player) and exposes exactly these operations and states:

```cpp
enum class SessionReplayTiming {
  Original, Immediate,
  Scale0_1, Scale0_25, Scale0_5, Scale1, Scale2, Scale5, Scale10,
  Step
};
enum class SessionReplayState { Idle, Ready, Playing, Paused, Finished };

class SessionReplayPlayer : public QObject {
  Q_OBJECT
public:
  SessionReplayStart  start(const SessionFilePtr &session);  // attach; also restart
  SessionReplayStart  play();                                // begin/resume automatic replay
  SessionReplayReport stop();                                // stop, keep the position
  SessionReplayStep   stepNextEvent();                       // one step, forward only
  SessionReplayStep   stepNextRx();
  SessionReplayStep   stepNextTx();
  void                close();                               // detach, back to Idle
  SessionReplayState  state() const;
  SessionReplayTiming timing() const;
  void                setTiming(SessionReplayTiming timing);
  quint64             position() const;      // index of the next event to deliver
  quint64             deliveredCount() const;
  quint64             eventCount() const;
signals:
  void eventDelivered(const SessionEvent &event);
  void stateChanged(SessionReplayState state);
  void replayFinished(const SessionReplayReport &report);
};
```

`eventDelivered` is the only delivery path: one stored event per emission, in stored
order, `const`, identical to the value the loader read. That signal is the seam a
decoder framework attaches to in M11 and a second view attaches to later; M10 connects
exactly one consumer (the application's offline display adapter). Every operation
reports a refusal as a value carrying a reason (as `SessionRecorder::start()` does),
so no refusal is silent. `stop()` returns a report (delivered count, remaining count,
position, timing, whether the replay finished); `replayFinished` is emitted when a
*playing* replay delivers its last event, mirroring ADR-010's rule that an end without an
explicit user action still has to reach the application.

The player owns no file, no transport and no timer of its own choosing: automatic
advancement goes through an injectable scheduler seam and delays through an injectable
monotonic-clock seam (defaults `QTimer` and the production clock), exactly as M9's
recorder does, so replay is deterministic in tests. Automatic advancement is **always**
scheduled - even at `Immediate`, where the delay is zero - so delivery yields to the
event loop between events, the user interface stays responsive, and `stop()` can take
effect at a delivery boundary instead of after a whole synchronous replay. No operation
blocks, loops over the session or delivers more than one scheduled event per callback
(step operations deliver the stepped prefix synchronously and involve no timer).

*Alternatives:* a free function or a static API (rejected: no state, no reports, no
signal seam for views/decoders); a signal-only interface without operations (rejected:
no refusals, no position, and the user needs start/play/stop/step); delivering directly
from the loader iterated by the caller (rejected: the timing policy, the position and
the refusal semantics belong to one owner, and a second consumer would re-implement
them); an immediate replay as a synchronous loop (rejected: it blocks the UI and makes
cancellation unobservable).

### 7. Timing ladder, scale arithmetic and the Step policy (D7)

The timing policy is chosen per session and may be changed at any time; it applies to
the next scheduled delivery and **never** rewrites stored timing (ADR-005). Delays are
computed on the session timeline (`timestampNs`), in nanoseconds:

| Timing | Delay before the next event |
| --- | --- |
| `Original` | the stored gap `timestampNs[i] - timestampNs[i-1]`, verbatim (scale 1) |
| `Immediate` | 0 (scheduled, so the event loop turns) |
| `Scale0_1` | stored gap × 10 |
| `Scale0_25` | stored gap × 4 |
| `Scale0_5` | stored gap × 2 |
| `Scale1` | stored gap × 1 |
| `Scale2` | stored gap ÷ 2 |
| `Scale5` | stored gap ÷ 5 |
| `Scale10` | stored gap ÷ 10 |
| `Step` | nothing is scheduled; only a step operation advances the player |

Normative details: the first event after a start or restart has no predecessor and is
delivered with delay 0; scaling is integer arithmetic in nanoseconds with truncating
division, so a sub-nanosecond remainder is dropped and a scaled delay is never negative;
a non-positive stored gap (two events at the same session time) is delivered with delay 0
in stored order, never reordered; multiplications are checked and clamp to the largest
representable delay rather than wrapping, because a wrap would turn a long gap into an
instant one. `Original` and `Scale1` are arithmetically identical by design - the ladder
carries both because section 9.1's option list does - and a test pins that identity.
`Step` is a ladder entry, not a mode that silently coexists with automatic replay:
`play()` while `Step` is selected is refused with a reason ("step mode advances only on
a step command"), and no timer is armed.

*Alternatives:* defining `Original` as "the stored timestamps as absolute delays"
(rejected: absolute session times would make a replay started mid-file wait for the
session's whole history, and the recorded gaps - not the session origin - are what the
user sees as timing); `Immediate` as an unscheduled tight loop (rejected above);
rejecting sub-nanosecond rounding by rounding half-up (rejected: truncation is simpler to
state, to test and to compare between platforms, and a nanosecond is far below any
scheduling precision ADR-005 promises); making `Step` a separate flag beside a still
active scale (rejected: two independent controls that contradict each other).

### 8. Step semantics: the inclusive prefix (D8)

A step operation advances the position to the next matching event **inclusively** - the
matching event is the last one delivered by that call - and delivers every event in
between in stored order as well. `stepNextEvent()` matches the next event unconditionally,
`stepNextRx()` and `stepNextTx()` match the next `Data` event with direction `Rx`/`Tx`
(ADR-004: direction is `Tx`/`Rx` if and only if the event is `Data`). Non-data events and
the other direction are delivered, not skipped, so a view's rendered state always
corresponds to a complete prefix of the stored stream; the step result reports how many
events it advanced and whether it reached the end.

Step operations are accepted in `Ready` and `Paused` and refused while `Playing` (the
reason says to stop first) and in `Finished`/`Idle`. A step that finds no match before
the end advances to the end, delivers the rest of the stream, reports that no match was
found and leaves the player `Finished`. The position is index-based and forward-only: no
step ever moves it backwards.

*Alternatives:* delivering only the matching event (rejected: the delivered stream would
have holes, so a terminal view would silently lose the skipped characters and a later
decoder would see a stream that never existed); refusing a step when no match exists
(rejected: the natural forward operation must still make progress, and the result can
report that the match was not found); allowing steps during playback (rejected: two
advancement mechanisms acting on one position at the same time is the classic source of
double-delivery and lost events).

### 9. Forward-only in M10: no seek (D9)

The milestone's operations move forward only: `start()`/restart, `play()`, `stop()`,
the three step operations. There is no seek operation, no position setter, no range
selection and no timeline slider in M10; the only way back is a restart from the first
event. Internally the session is stored so that a future seek is additive - the events
are in one ordered, randomly accessible sequence and the position is a plain index - but
no API, no signal and no user interface for it exists in this milestone.

*Alternatives:* preparing a hidden `seek()` for later use (rejected: an unreviewed,
untested API exists in production the moment it is written, and M10 has no view that
could exercise it); building a range-selection UI now (rejected: architecture section 43
puts range selection in the second milestone, and M10's owner decision is explicit).

### 10. One materialised session per file, a stateless reader, no retained file handle (D10)

`load()` reads the whole file once, then closes it: the offline session holds no file
handle, so nothing can observe a later change to the file, and the evidence is a
snapshot. The reader keeps no state between loads (no "last file", no cached header), so
a load cannot be affected by a previous one. The cost is stated rather than hidden: the
session is held in memory, proportional to the number of records and their bytes, and a
`.kpsession` file is larger than its payload volume because M9 writes one record per
accepted write (SPEC-M9 §9 and ADR-010's consequence on byte-wise uploads). A
bounded-memory cursor that re-reads the file during replay is a rejected alternative
*for M10* and the named follow-up when the analyzer needs sessions larger than memory;
it would tie a replay to a live file handle and add a mid-replay I/O failure surface,
which a snapshot does not have.

### 11. The offline state is explicit, exclusive and reversible (D11)

A loaded session puts the application into one explicit offline state:

- the state is entered by loading a file and left by closing the offline session
  (`KomportDoc::closeOfflineSession()`), which destroys the player and releases the
  session;
- the permanent indicator is a status-bar label (object name
  `offlineReplayStatusLabel`) whose source text is exactly
  `Offline Session — Passive Replay`, visible exactly while the offline state is active;
- loading is permitted **only** while the session is not live (`SessionController` is
  `Idle`), no recording is running (`SessionRecorder::State::Stopped`), and no offline
  session is loaded. Otherwise the load is refused **explicitly**, with the reason
  reported through the existing translated status mechanism, and nothing is closed,
  opened, configured or changed: a refusal never terminates a live session to make room
  for a file, and it never discards the loaded session by replacing it implicitly;
- every control that can change the connection (the profile combo, Save/Delete Profile,
  Settings) and every control that can transmit (terminal key input, the macro bar,
  `Upload`, `Download`, `Upload recent file`) is disabled while the offline state is
  active, so the user-facing path cannot reach the transport either.

Closing the offline session restores the previous live controls and hides the indicator;
it does not open a port and does not resume anything.

*Alternatives:* allowing a live session and a loaded file side by side (rejected: the
same view would render two streams, the advisory "loading is only permitted while
neither a live session nor a recording is active" is the safe and simple rule, and mixing
them is precisely how an accidental output path would appear); auto-closing a live
session to load a file (rejected: a load must never be able to terminate live traffic);
implicitly replacing an already loaded session (rejected: it would discard the current
position and state without a decision - one session at a time, replaced explicitly);
making the offline state a per-view flag instead of an application state (rejected: the
transmission-capable controls are shared app state, and a per-view flag could leave the
macro bar or an upload action able to write).

### 12. Recorded transport settings are informational and are never applied (D12)

The loaded configuration snapshot is displayed (read-only, in the offline summary and
available from the loaded session facts) and is never applied to any port: the offline
state does not configure a transport, does not open one, and never re-applies recorded
settings "conveniently" before or after replay. ADR-007's rule that recorded transport
settings are informational holds in M10 even though M10 has no active target at all, so
that the future milestone cannot inherit a habit of applying them.

*Alternatives:* offering "reconnect with the recorded settings" after a replay
(rejected: that is the first half of active replay, with a hardware side effect, and
needs the confirmation and the visible target of architecture section 10); hiding the
recorded settings entirely (rejected: section 42 asks the loaded session to show what
was recorded, and a device endpoint is not a credential - ADR-006).

### 13. Ownership: the document owns the reader, the session and the player (D13)

`KomportDoc` owns the `SessionReader`, the loaded session and the `SessionReplayPlayer`
beside the controller and the recorder, and exposes document operations that make the
offline state testable without a window, exactly as `closeSession()` does for M9:

```cpp
SessionLoadOutcome  loadOfflineSession(const QString &path);  // preconditions of D11, then load
void                closeOfflineSession();                    // destroy player + session
bool                hasOfflineSession() const;
const SessionFilePtr &offlineSession() const;
SessionReplayPlayer *getSessionReplayPlayer();
```

The destruction order stays `controller -> recorder -> replay player -> reader/session ->
transport`, so no offline object outlives the document, and the existing rule that
`~KomportSerial` emits nothing is untouched. The reader is stateless (D10) but is owned
for its injected seams, as the recorder is. Live and offline are mutually exclusive by
D11, so no ownership question about "both at once" arises.

### 14. Safety: no code path in the offline path can transmit (D14)

This decision is the M10 answer to ADR-007, and the argument is mechanical rather than
a promise:

1. **No transport in the offline objects.** `SessionReader` and `SessionReplayPlayer`
   have no `ITransport` member, no `ITransport*` parameter in any operation and no
   `writeBytes()`/`putChar()`/`putStr()` call, direct or deferred. Their only outputs are
   a byte source it *reads* and a signal carrying `const SessionEvent &`.
2. **Replay is not in the live path.** The offline session is not an `ITransport`, is
   never handed to `SessionController`, and the controller is not a consumer of the
   file; the two paths share only the value types of ADR-002, never a runtime object.
3. **The user-facing path is closed too.** The offline state (D11) disables terminal key
   input, the macro bar, the upload/download actions and every connection control, so no
   user gesture during a replay can reach a transport write.
4. **Nothing can be enabled accidentally.** There is no confirmation, no checkbox and no
   persisted state that could arm an output: per ADR-007 a passive replay is safe to
   start without confirmation, and M10 has no target to select.
5. **The boundary is compiled, not just reviewed.** The new units join the widget-free
   contract check (`tests/session_contract_compile.cpp`, `komport_session_contract_check`,
   built against `Qt6::Core` alone), so a widget, application or transport type entering
   `sessionreader.h` or `sessionreplayplayer.h` breaks the build (ADR-009).

Active TX replay and simulation remain unimplemented and un-enabled; they require their
own ADR/decision, an explicitly selected open `ITransport`, a visible target and a
confirmation per start.

## Consequences

- M10 delivers the first productive consumer of `.kpsession`, and the format's reader
  rules are enforced by the reader the format was designed for - not by the writer's
  mirror and not by M9's test parser.
- Passive replay is testable without hardware and without a window: a byte source seam,
  a scheduler seam and a clock seam make loading, timing, stepping and refusals
  deterministic, exactly as M9's recorder tests are.
- A rejected file is a first-class outcome with a reason, distinguishable from a
  recovered one; a damaged recording produced under ADR-010's crash bounds loads as a
  session whose truncated final record was recovered and reported.
- The milestone keeps its boundaries: no decoder, no active output, no simulation, no
  multi-source, no seek, no format change and no second format.
- The offline path holds a session in memory (D10). That is a stated cost, proportional
  to the file, with the bounded-memory follow-up named for the analyzer rather than
  pretended away.
- The stream checks of D3 mean a file that a future, laxer writer might produce could be
  refused; that is deliberate, because presenting an inconsistent stream as an ordered
  session is the failure mode this milestone cannot afford.
- The reader duplicates a small number of format facts (magic, version, encoding, the
  two limits, the profile shape) that the writer also knows. The duplication buys
  independence; a test pins the constants equal, and the profile rules are asserted from
  both sides (writer tests, reader tests).

## Decisions register

| Id | Decision, in one line |
| --- | --- |
| D1 | Loading creates an offline session through a QtCore-only reader; never a transport, never the controller. |
| D2 | The reader enforces ADR-006 itself, interprets the header once into typed facts, and refuses `sources[] > 1` with `multi-source sessions not supported in M10` (and `clockDomains[] > 1` likewise). |
| D3 | Record structure, event-local validity, source resolution and the ADR-002 stream properties are validated; a malformed complete record refuses the file; a truncated final record recovers the complete prefix and is reported. |
| D4 | Loading is all-or-nothing; a recovered truncated final record is the only partial result. |
| D5 | Loaded events are immutable and delivered verbatim; no re-timestamping, no translation, no derived replacement. |
| D6 | `SessionReplayPlayer` is the read-only, QtCore-only interface of ADR-007, with named operations, five states, one delivery signal and value-returned refusals; automatic advancement is always scheduled through a seam. |
| D7 | The timing ladder is Original, Immediate, 0.1x, 0.25x, 0.5x, 1x, 2x, 5x, 10x and Step, with integer-nanosecond scale arithmetic on the stored session-time gaps; `Step` refuses `play()`. |
| D8 | A step delivers the inclusive prefix up to and including the match; steps are refused while playing. |
| D9 | Forward-only: no seek API, no position setter, no range selection; restart is the only way back, while the internal store stays seek-ready. |
| D10 | One materialised session per load, read once and closed; the reader is stateless between loads; memory cost stated, bounded-memory cursor deferred. |
| D11 | An explicit, exclusive, reversible offline state with the indicator `Offline Session — Passive Replay`; loading refused while live, while recording or while a session is loaded; every transmit entry point disabled. |
| D12 | Recorded settings are informational and never applied to any port. |
| D13 | `KomportDoc` owns reader, session and player, with `loadOfflineSession()`/`closeOfflineSession()` as testable document operations. |
| D14 | No code path in the offline path can transmit: no transport in the objects, no live path, closed user-facing path, nothing to arm, compile-level boundary check. |

No decision remains open in this ADR. SPEC-M10 implements D1-D14; where SPEC-M10 and
this ADR would differ, this ADR is authoritative and the spec must be amended first.

## References

- ADR-007 (replay safety model - the binding requirement this ADR satisfies, explicitly
  by D1, D6, D11, D12 and D14), ADR-006 (`.kpsession` v1 file format, its limits and its
  reader rules), ADR-002 (session event v1 and its two-part validation contract),
  ADR-004 (direction semantics), ADR-005 (timing as evidence, replay timing as policy),
  ADR-008 (source identity, multi-source deferred), ADR-009 (QtCore-only contracts),
  ADR-010 (live session recording: the writer whose output this reader must accept)
- `docs/specs/SPEC-M10-session-reader-passive-replay.md` (the implementation spec of
  this milestone, citing D1-D14)
- `docs/specs/SPEC-M9-live-session-recording.md`, sections 3, 5.3, 5.4, 5.9, 7, 9
  (the writer's decisions, the test-side reader of decision D3 and the acceptance
  criteria this milestone builds on)
- `docs/komport-session-replay-simulation-architecture.md`, sections 3.4 (Replay),
  9 and 9.1 (Replay Modes, Passive Replay), 10 (Active TX Replay - explicitly not
  M10), 23 (Phase 4), 25 (the replay-source split active output will need), 39.2/39.3
  (replay and timing test expectations) and 42/43 (the MVP boundary and the second
  milestone)
- `docs/komport-engineering-governance-spec-review-workflow.md` (spec-first workflow,
  section 16.6 "Replay is safe by default")
- Repository evidence read for this ADR: `komport/sessionrecordcodec.h`,
  `komport/sessionrecorder.h`, `komport/sessioncontroller.h`, `komport/komportdoc.h`,
  `komport/komport.h`, `komport/komportserial.h`, `komport/komporthexview.h`,
  `komport/komportview.h`, `tests/sessionrecordreader.h`, `tests/CMakeLists.txt`
