# Amendment Package v2: ADR-002 … ADR-009 and SPEC-M8

Date: 2026-09-18
Supersedes: the amendment texts A1.1 … A16.1 of
`docs/reviews/2026-09-18-M8-foundation-prereview-and-proposed-amendments.md`
(that document stays as the round-1 record; it is not amended in place).
Incorporates: `docs/reviews/2026-09-18-M8-foundation-independent-review.md`
(independent review round 1, verdict "not accepted").
Gate status: **not accepted yet.** ADR-002 … ADR-009 remain `Proposed`,
SPEC-M8 remains `Draft — implementation blocked pending ADR review`. This
document changes no status and contains no implementation.

## 0. How to review this package (round 2)

Round 2 reviews the ADRs, SPEC-M8 and this document. Its job is to decide
whether the amendment set below turns the package into a contract that can be
frozen, and whether it introduced new problems.

Structure: section 1 lists the three `Decision Required` items that must be
resolved by the owner/reviewer rather than chosen silently during
implementation. Section 2 records where round 1 corrected the round-1 pre-review
itself. Section 3 contains the amendments, grouped by target document and
referenced by ID (`B2-n` = ADR-002, `B3-n` = ADR-003, `B5-n` = ADR-005,
`B6-n` = ADR-006, `B7-n` = ADR-007, `B8-n` = ADR-008, `BS-n` = SPEC-M8,
`BD-n` = architecture documents). Section 4 is the traceability matrix for every
finding of rounds 0 and 1 (F1–F16, X1–X9).

Amendment texts are written as replacement text for the named section. Where an
amendment introduces an obligation that only M9 can implement, it says so
explicitly and does not pull M9 work into M8.

## 1. Decision Required

These three items are contract decisions with more than one defensible answer.
They are deliberately not resolved inside an amendment.

### D1 — Which session-time model becomes normative?

Context: ADR-003 currently says timestamps are "relative to a single transport
activation", while ADR-002/SPEC-M8 assume a common clock; nothing defines a
clock-domain identity or the session timeline's zero point. Round 1 rejected the
round-0 proposal (numeric identity plus a mandatory exact wall-clock anchor of
the monotonic zero) as both wrong and unnecessary.

Options:

1. **Session-origin-relative (recommended).** One monotonic clock per clock
   domain (process/agent), session time defined as
   `sourceTimestampNs − sessionOriginSourceTimestampNs`, origin recorded on the
   first event the controller accepts. Wall-clock creation time stays header
   metadata, with an optional wall-clock correlation of stated precision.
   Matches the architecture documents (`komport-multiport-sniffer-time-alignment.md`
   §10, §11, §33: relative time instead of absolute time) and gives every
   session a timeline starting at zero.
2. **Absolute process-monotonic session time.** Simpler (identity mapping), but
   persists a process-relative value that is meaningless without knowing the
   process, and makes two sessions harder to compare.
3. **Boot-relative OS monotonic clock** (e.g. `CLOCK_MONOTONIC`). Comparable
   across processes on one host and approximately convertible to wall clock, but
   platform-specific and reintroduces a conversion claim that cannot be exact.

Recommended: option 1. Its consequences are written out in B3-1, B5-1, B6-1 and
BS-12.

### D2 — Where does the replay event source sit relative to `ITransport`?

Context: ADR-003 declares `SessionController` the sole adapter from transport
signals; ADR-007 has passive replay delivering stored events through the
controller with no output target. Round 1 rejected the round-0 proposal (replay
as a read-only `ITransport`) because `ITransport` carries byte chunks and
transport metadata only and cannot preserve stored sequence numbers, source
identity, event types, annotations or aligned times.

Options:

1. **Separate read-only event-source/player interface (recommended).**
   ADR-003 is scoped to live observations; replay gets its own interface
   specified in M10, at one point in time (no `ITransport` involved).
2. **Read-only `ITransport` with exact-preservation semantics** defined by a
   later ADR (event identity carried through transport signals).
3. **Second event-injection input on `SessionController`** (a non-transport
   feed), specified in M10.

Recommended: option 1. Its wording is B3-5 and B7-1. Whatever is chosen must be
recorded before ADR-003/ADR-007 are accepted together, because ADR-003's "sole
adapter" sentence is frozen in M8.

### D3 — How far does M8 reach into two pre-existing serial behaviours?

Both are pre-existing defects/quirk interactions that M8 cannot ignore, but that
M8 does not have to fix:

- (a) `KomportSerial::open()` calls `applyPortSettings()` and then emits
  `settingsChanged()`, whose self-connection (`komportserial.cpp:36-39`,
  `:107-112`) applies the settings a **second** time.
- (b) `close()` does not clear the legacy RX buffer (`:75-80`) while its flush
  timer keeps running (`:36-40`), so bytes received before a close can be
  rendered after a reopen, and an emulation reply to them can appear as TX in a
  later activation.

Options:

1. **Recommended split.** (a) is fixed inside M8: the duplicate application is
   removed, because "exactly one configuration transaction" (BS-3) is otherwise
   unsatisfiable. Expected consequences: at most one fewer duplicate
   `settingsFailed()` emission per open, and no other behaviour change, since a
   second application of identical settings is a no-op. (b) is preserved as-is
   and documented, because clearing the buffer changes visible terminal
   behaviour; M8 only adds the activation-generation guard (BS-1, BS-6) so the
   event path cannot mis-attribute those bytes.
2. Fix both in M8 (touches visible terminal behaviour, needs its own review of
   the changed behaviour).
3. Fix neither; keep (a) and accept that a configuration event cannot be
   guaranteed unique per change (weaken BS-3 to "one result per apply attempt").

Recommended: option 1.

## 2. Corrections to the round-1 pre-review document

Round 1 verified these against the code; they are corrections of the round-0
review, not amendments to the ADRs:

- **C1 — record prefix size.** ADR-006's record prefix is **44 bytes**
  (4+2+1+1+8+4+8+8+4+4), not "~30 bytes" as the round-0 F5 text said. The
  per-event overhead conclusion is unaffected, the figure is corrected in BS-8.
- **C2 — the legacy RX buffer is not dropped on close.** `close()`
  (`komportserial.cpp:75-80`) leaves `mRxBuffer` intact and does not stop the
  flush timer, so pre-close bytes are delivered after the close. The round-0
  claim "still drops buffered bytes when the port closes" was wrong; the correct
  description (and its consequence for activations) is in BS-6 and X7.
- **C3 — "wire truth" is too strong.** Neither direction may be described as
  electrically exact. Correct formulation in B3-6 and BS-2/BS-6.
- **C4 — the existing test targets cannot prove QtCore-only.** Every test links
  `komport_core`, which links `Qt6::Widgets` publicly (`tests/CMakeLists.txt`,
  top-level `CMakeLists.txt`), so a dedicated compile target is required (BS-10).
- **C5 — the existing open-error path is load-bearing.**
  `tests/tst_profileerror.cpp:64-97` depends on `errorOccurred →
  slotPortError → settingsFailed` firing synchronously from inside a failed
  `open()`; the duplicate suppression of BS-1 therefore applies to the event
  path only and never to the legacy signals.

## 3. Amendments

### 3.1 ADR-002 — `SessionEvent` v1

**B2-1 (F3) — sequence across activations.** After "`sequence` is strictly
increasing within one session and begins at one." add:

"A session may contain several transport activations (open, close and reopen of
the same source); `TransportOpened` and `TransportClosed` events mark them, and
`sequence` continues across them. An activation never resets `sequence`."

**B2-2 (X3) — value contract instead of a convention.** After the paragraph that
ends "The event is immutable after emission." add:

"Immutable means the following contract, which every producer must satisfy and
every test may check:

- `type` is one of the declared enumerators; `direction` is exactly `Tx` or `Rx`
  if and only if `type == Data`, and `None` for every non-data event.
- `Data` requires a non-empty `payload`; every non-data event requires an empty
  `payload`.
- `sequence` starts at one and strictly increases within the session;
  `sourceId` is non-zero for physical/event sources and zero is reserved for
  future session-wide non-data events (unused in M8); both timestamps are
  non-negative.
- `metadata` is empty for `Data` events in M8 and is JSON-encodable when
  present.

The contract is enforced where it can be observed: a single explicit validation
function (used by the loader, replay and tests) checks an event instead of
scattered assumptions, the controller is the only M8 producer, and consumers
receive events as const references or copies and must not modify them. 'Only
the controller produces and nobody mutates' is a testable property of the
session layer; a stricter type-level encoding (private members with accessors)
is not required by M8."

**B2-3 (X2) — lossless 64-bit values in metadata.** Add:

"Values that carry nanosecond counts, sequence numbers, source ids or byte
counts are 64-bit integers and cannot be represented exactly as JSON numbers
(IEEE-754 doubles lose integer precision beyond 2^53 ns, about 104 days). Such
values are therefore encoded as canonical decimal strings wherever they appear
in JSON, or in a dedicated binary, versioned record (ADR-006); a reader must not
assume a JSON number can carry a full nanosecond value. Per-event timestamps and
ids live in the binary record and are unaffected."

### 3.2 ADR-003 — `ITransport` and the controller boundary

**B3-1 (F1, X1, D1 option 1) — clock domains.** Replace "Timestamps are
monotonic and relative to a single transport activation. The source that
observes bytes creates them: local `SerialTransport` on its Qt thread, a future
remote adapter at the remote agent. `ITransport` neither stores sessions nor
decodes bytes." with:

"Timestamps are non-negative monotonic nanoseconds in the observing source's
clock domain. A source uses one monotonic clock for the lifetime of its clock
domain (process or agent) and never a per-activation zero; a clock domain is
identified by a stable identifier (ADR-006). The source that observes bytes
creates the timestamps: local serial uses the process-wide clock defined in
ADR-005, a future remote adapter uses its own agent-wide clock, and observations
within one clock domain remain directly comparable across activations.
`ITransport` neither stores sessions nor decodes bytes; it does not assign
session time — that is the controller's mapping (ADR-005)."

**B3-2 (F2) — one observed write primitive; chunking is an API boundary.**
Replace the paragraph beginning "No new feature may bypass…" context by adding
before it:

"Every byte-transmitting entry point of an `ITransport` implementation — the
legacy `putChar()` and both `putStr()` overloads as well as `writeBytes()` — is
implemented on one internal write primitive, and that primitive alone creates a
TX observation. A compatibility method that writes bytes without producing an
observation is exactly the bypass this ADR forbids."

and append after the "may keep its legacy character methods/signals" sentence:

"One accepted API write produces exactly one transport-observation chunk. This
records the application/API acceptance boundary, not physical wire framing:
`QSerialPort::write()` acceptance is not electrical delivery. A single logical
action may therefore produce several events (the emulation sends a cursor or
insert key as `putChar(ESC)` followed by one or more `putStr()` calls). Event
storage preserves those boundaries; decoders may assemble byte streams across
events and must not assume that one event equals one protocol frame, and no
layer may recombine or re-split stored chunks."

**B3-3 (F4) — accepted byte count.** Replace
`virtual bool writeBytes(const QByteArray &bytes) = 0;` with:

```cpp
    // Sends bytes and returns the number of bytes the transport accepted
    // (0..bytes.size()); a negative value means the write was refused.
    virtual qint64 writeBytes(const QByteArray &bytes) = 0;
```

and add to the signal description: "`bytesReceived`/`bytesWritten` report bytes
observed/accepted by the transport API; for a partially accepted write
`bytesWritten` carries exactly the accepted prefix, and 'accepted' never means
'physically delivered'. `KomportSerial`'s `bool` compatibility methods keep
their signatures and return `true` only when all requested bytes were accepted."

**B3-4 (X9) — scoped compatibility exception.** Replace "No new feature may
bypass `ITransport` or `SessionController`." with:

"No new feature may bypass `ITransport` or `SessionController`. M8 legacy callers
may continue to call the local transport's compatibility methods (`putChar()`,
`putStr()`) directly, provided every such call routes through the single observed
write primitive (B3-2). M8 adds no new UI-side transport API and no new consumer
may derive session data from character signals."

**B3-5 (F7, D2 option 1) — live observations vs. replay.** Replace
"`SessionController` is the sole adapter from transport signals to
`SessionEvent`" with:

"`SessionController` is the sole adapter from **live transport observations** to
`SessionEvent`: it assigns sequence numbers, associates the controller-owned
source descriptor/ID, maps source time to session time (ADR-005) and emits
events to recorder/UI/decoders. A future passive replay source does not use
`ITransport`: it delivers immutable stored `SessionEvent` values through a
separately specified read-only event-source/player interface, because
`ITransport` carries byte chunks and transport metadata only and cannot preserve
stored sequence numbers, source identity, event types, annotations or aligned
times. 'No `ITransport` output target' (ADR-007) therefore means 'no writable
peer', not 'no `ITransport`'. The exact form of the replay interface is decided
in M10's own spec and must not be foreclosed here."

**B3-6 (X6) — capture-point limitation.** Add:

"Observation is byte-exact at the application transport-observation boundary:
RX is exactly the bytes `readAll()` returned, TX exactly the bytes `write()`
accepted. It is not an electrical or logic-analyzer record: no
`waitForBytesWritten()` acknowledgement is implied, and bytes still buffered
below the application layer when a port closes are never observed. (ADR-008
already states that bit-level electrical order is outside Komport's observation
model.)"

**B3-7 (X4) — lifetime invariant.** Add:

"A controller must not outlive its transport, must never be destroyed after it,
and must not emit events or call into the transport from its destructor. Where a
transport is a by-value member of its owner (as `KomportSerial` is in
`KomportDoc`), the controller's destruction order relative to that member must be
explicit; relying on `~QObject` child destruction is not acceptable, because Qt
destroys children after the owner's own members."

### 3.3 ADR-005 — source timing and session time

**B5-1 (F1, X1, D1 option 1).** Replace the paragraph beginning
"`SessionEvent::timestampNs` is an aligned, non-negative duration…" and the
following M8 paragraph with:

"`SessionEvent::timestampNs` is the non-negative duration on the session
timeline, derived from source time by one explicit mapping: on the first event a
controller accepts, the controller records `sessionOriginSourceTimestampNs` for
that source, and thereafter
`timestampNs = sourceTimestampNs − sessionOriginSourceTimestampNs`. Session time
therefore starts at zero for each session and never decreases; equal timestamps
are ordered by `sequence`. `sourceTimestampNs` is retained unchanged for the
whole life of the session and is never rewritten by any mapping.

A session file records, per clock domain, a stable identifier, the
source/session reference pair that was used, and an optional wall-clock
correlation with its stated precision (ADR-006). The wall-clock correlation is
informational: the zero point of a monotonic clock cannot in general be
converted to an exact wall-clock instant, so no exact conversion is claimed or
required, and `sourceTimestampNs` remains authoritative.

M8 contains exactly one active local transport source, so its mapping has a
single origin and needs no synchronization. A later multi-source session may add
sources only with a persisted descriptor and a versioned mapping containing a
reference point, offset, scale and uncertainty; sources sharing a clock domain
may be ordered directly, cross-domain order relies on that persisted mapping.
Multi-host merging is not part of M8 and must visibly report uncertainty when no
trustworthy alignment exists."

Keep the rest of the ADR unchanged, including the statement that M9/M8 implement
no clock-synchronization exchange, runtime `TimeMapping` component, persisted
alignment mapping or alignment UI, and that stored timing is evidence while
replay timing is a policy.

### 3.4 ADR-006 — `.kpsession` v1

**B6-1 (F1, X1) — clock-domain schema.** Replace "capture clock domains,
optional decoder hints and notes" in the header description with:

"an array of capture clock domains, optional decoder hints and notes. Each
clock-domain entry carries a stable `clockDomainId` (opaque string, unique
within the file), the identity/source of that clock (for example process- or
boot-relative monotonic clock), the source/session reference pair that defines
the session origin for sources in that domain, and an optional wall-clock
correlation with its stated precision. A clock domain without identifier and
reference pair is not v1-conformant: a timestamp that cannot be related to a
reproducible session timeline must not be written as if it could."

**B6-2 (X2) — lossless integers.** Add:

"`sourceTimestampNs`, `timestampNs`, `sequence`, `sourceId` and all lengths are
64-bit integers. In the binary record they are stored as such (unaffected). In
JSON — the header and any metadata that carries nanosecond values, such as a
future alignment mapping — a 64-bit integer is encoded as a canonical decimal
string, because JSON numbers are IEEE-754 doubles and lose integer precision
beyond 2^53. A reader must accept that encoding and must never silently truncate
a value."

**B6-3 (F5) — empty metadata.** Replace the sentence about metadata encoding
(where present) with:

"`metadataLength == 0` is the encoding of an empty metadata object (`{}`) and is
normal, not an error; readers must treat it as empty metadata."

### 3.5 ADR-007 — replay safety

**B7-1 (F7, D2 option 1).** Replace "Passive replay emits events through
`SessionController` to views and decoders, and has no `ITransport` output target."
with:

"Passive replay delivers immutable stored `SessionEvent` values to views and
decoders through the read-only event-source/player interface specified for
replay (ADR-003), not through `ITransport` and not through
`SessionController`'s live observation path; it has no `ITransport` output
target, no write path and cannot create one. It is safe to start without a
confirmation."

### 3.6 ADR-008 — source identity and alignment

**B8-1 (X2) — mapping encoding.** After the sentence about the mapping being
"versioned metadata, never a destructive rewrite of source time", add:

"Because a mapping's reference points, offsets, scales and uncertainties are
nanosecond-scale integer values, they are encoded losslessly (ADR-006): never as
JSON numbers."

### 3.7 SPEC-M8 — session and transport foundation

**BS-1 (F3, F9, F10, X7) — replace the state model (§7).**

"`SessionController` has `Idle` and `Live`. A controller session exists from
construction and ends with the controller: session boundary is
controller/document lifetime, not `File → New` (`KomportDoc::newDocument()` does
not replace the serial object) and not a transport activation.

- construction: `Idle`.
- transport opened successfully: emit exactly one `TransportOpened` and enter
  `Live`. `sequence` continues to increase monotonically for the whole session
  and is never reset by an activation.
- observations in `Live`: emit ordered events.
- transport closed: emit exactly one `TransportClosed` and return to `Idle`.
  This is an activation boundary, not the end of the session; M8 has no terminal
  `Closed` state, because repeated closing and reopening is normal (`open()`
  closes a port first, and a device-name change closes and reopens implicitly).
- failed open while `Idle`: emit exactly one `Error` with `kind: "open"`; do not
  emit `TransportOpened`, do not enter `Live`, do not consume an activation.
  `open()` is authoritative for its own result; the asynchronous port error
  belonging to that same failed attempt is suppressed by an explicit
  in-progress/error-generation guard — never by matching human-readable text —
  while the legacy `settingsFailed()` signal keeps its present synchronous
  behaviour for existing consumers (see C5).
- destruction: no events and no calls into the transport (B3-7).
- observations arriving while not `Live` (after `TransportClosed`, or from a
  transport that never opened) are dropped and reported as a diagnostic; they
  never become session events. Because a late signal can belong to an earlier
  activation (the legacy RX buffer is not cleared on close, X7), transport-signal
  handling is scoped by an activation generation, so a queued late signal cannot
  be attributed to a later opening."

**BS-2 (F2, F4, X6) — observation boundary and chunking.** Replace the TX
paragraph of §6.2 and extend §8:

"`KomportSerial` implements `ITransport`. All byte-transmitting entry points
(`putChar()`, both `putStr()` overloads, `writeBytes()`) are implemented on one
internal write primitive and that primitive alone creates TX observations, so UI
keystrokes, emulation replies, macro output, line endings, transfer bytes and
future `writeBytes()` callers all reach the session stream through one rule.

TX: a fully accepted write produces exactly one `Data`/`Tx` event covering all
requested bytes; a partially accepted write produces exactly one `Data`/`Tx`
event covering the accepted prefix and keeps the existing failure return and
warning, plus one `Error` event with `kind: "write"` carrying `acceptedBytes`
and `refusedBytes`; a write that accepts nothing produces no data event.
`ITransport::writeBytes()` returns the accepted byte count. `bytesWritten` means
'accepted by the transport API', never 'physically delivered'.

RX: exactly one observation for every successful non-empty `readAll()` result,
emitted before the existing buffering/character path; an empty read produces
nothing.

Chunking: one chunk is one accepted write or one non-empty read — an
application-observation boundary, not wire framing. A single logical action may
span several events (for example `putChar(ESC)` followed by `putStr("[1~")`).
Decoders may assemble byte streams across events and must not assume one event
equals one protocol frame; no layer may recombine or re-split stored chunks.
Byte-exactness holds at this boundary only (B3-6): RX is exactly the bytes
`readAll()` returned, TX exactly the bytes `write()` accepted, with no
electrical-delivery claim."

**BS-3 (F6, X5, D3 option 1) — one configuration transaction.** Replace the
configuration paragraph of §6.2 with:

"While live, one logical configuration change produces exactly one configuration
result, after all requested settings have been attempted. `open()` performs
open-and-configure, and its configuration result follows `TransportOpened`; a
change applied through `slotSettingsChanged()`, `setFraming()` or
`setFlowControl()` produces one result of its own. Metadata contains the
requested settings, the read-back effective settings (port name, baud rate, data
bits, parity, stop bits, flow control as accepted by `QSerialPort`) and
`applyStatus` ∈ {`full`, `partial`, `failed`}. If the effective settings changed,
`TransportConfigChanged` is emitted even for `partial`, followed by an `Error`
event where applicable; a `failed` apply that changed nothing emits only the
`Error`. No duplicate result is emitted for one change.

The duplicate application caused today by `open()`'s unconditionally emitted
`settingsChanged()` and its self-connection is removed (D3 option 1): expected
consequences are at most one fewer duplicate `settingsFailed()` emission per
open and no other behaviour change, since applying identical settings twice is a
no-op. The legacy `settingsChanged()`/`settingsFailed()` signals keep their
present behaviour for existing consumers and must never drive a second hardware
application. Metadata contains no credentials."

**BS-4 (F8) — FIFO delivery, sequencing at emission.** Extend §10:

"Delivery is non-reentrant and ordered. `SessionController` keeps a FIFO of
complete observations, assigns `sequence` at dequeue/emission time, and completes
the delivery of one event before processing the next observation — for every
event type, including close, error and configuration, not only data. An
observation produced while an event is being delivered (a consumer that writes
from its handler, as the existing emulation does for device-status replies) is
appended to the FIFO and emitted after the current delivery returns; emissions
never nest. With two subscribers, both must observe the same order: for a
consumer that writes one byte in response to an RX event, both subscribers see
`RX` then `TX`, and neither may see a nested `TX` inside its `RX` callback."

**BS-5 (X4) — ownership and destruction order.** Extend §6.2 and §10:

"`KomportDoc` owns the transport and the controller. The controller is destroyed
before the transport and its destructor neither emits events nor calls into the
transport. Concrete layout: the controller is owned through a `std::unique_ptr`
reset in `KomportDoc`'s destructor body, or declared as a member after `mSerial`
so that it is destroyed first. A QObject-child arrangement that relies on
`~QObject` ordering is not acceptable, because `KomportSerial` is a by-value
member of `KomportDoc` and would already be destroyed."

**BS-6 (F11, X7, D3 option 1) — divergence invariant, revised.** Replace the
divergence paragraph in §8 with:

"The event stream preserves every non-empty `readAll()` result observed by the
transport before legacy buffering and is independent of `RxQueue` and
`FlushRate`; it is byte-exact at the application observation boundary (BS-2),
not an electrical record. The legacy character path stays a lossy, delayed
display adapter: it trims its RX buffer to `setRxQueue()` (dropping the oldest
bytes) and delivers through the `setFlushRate()` timer. That buffer is not
cleared by `close()` and its timer keeps running, so bytes received before a
close can still be rendered after a reopen, and an emulation reply to them can
appear as TX in a later activation. M8 preserves that legacy behaviour
deliberately — changing it would alter visible terminal behaviour and requires
its own reviewed change — and guards the event path against mis-attribution with
the activation-generation rule of BS-1. No test may assert universal equality
between the event stream and the character signals."

**BS-7 (F9) — error metadata.** Extend §9:

"An `Error` event's metadata carries at least `kind` (`open`, `apply`, `write`,
`runtime`), a stable machine-readable `code`, a human-readable `message`, and the
relevant non-secret numeric context (for example `acceptedBytes`/`refusedBytes`);
never credentials and never decoded payload data. A failed open emits exactly one
`Error(kind: "open")` while `Idle` (BS-1); an apply failure emits its `Error`
after the configuration result (BS-3). A refused TX remainder is reported through
`kind: "write"` with its byte counts."

**BS-8 (F5, F12, X8) — tests.** Replace the PTY section of §13 and add the
missing pieces:

"Controller tests (deterministic, no PTY): a test `ITransport` double emits
scripted observations — including empty reads, partial accepts, configuration
results and scripted errors — and proves: one observation → one event; sequence
continuity across activations without reset; FIFO order and two-subscriber
non-reentrancy (RX then TX for both subscribers); dropping and diagnostics for
observations outside `Live`; activation-generation isolation; read-back
configuration transactions with `full`/`partial`/`failed`; and source-ID 1 on
every event including `Error(kind: "open")`.

`KomportSerial` accepted-prefix behaviour needs an injectable write-result seam
(a small protected/override point or test subclass): a genuine partial hardware
write is not reproducible in this environment (see the comment in
`tests/tst_serial.cpp` about the absent serial device), and PTYs do not
deterministically select read or write chunk boundaries.

PTY tests (end-to-end binary preservation, no chunk-count claims): RX with
embedded NUL and values spanning `00..FF` — the concatenation of all RX payloads
equals the sent byte stream, no event is empty, ordering is deterministic; TX via
the length-aware path is byte-identical; the tests must not assert a fixed number
or split of chunks and must not depend on the legacy flush timer.

Volume/pathology: 64 KiB written byte-wise through `putChar()`, as
`KomportTransfer::upload()` does, yields exactly one TX event per accepted write
with correct payload bytes; the test asserts event count, byte count and ordering
explicitly and must not require an in-memory event vector in the consumer. For
sizing information, ADR-006's record prefix is 44 bytes per event (C1), so a
byte-wise upload is a deliberate cost that M9's streaming policy must handle."

**BS-9 (F16, X8) — test-only consumer.** Add to §13:

"M8 ships no production consumer; the recorder arrives in M9. `tests/` therefore
contains a test-only event collector plus the deterministic transport double of
BS-8 (never linked into the shipped executable, no CLI flag, no UI, no
persistence). Event behaviour is verified primarily against the double;
comparison with the legacy character signals may only be used in a fixture with
no RX overflow and a forced flush, because the two paths are intentionally
divergent (BS-6). No production code depends on the collector."

**BS-10 (F13) — acceptance criteria.** Replace "Core types and transport contract
compile with Qt 6.3." and add the widget-freedom criterion:

"Core types and the transport contract compile against the locally installed
Qt 6 and are documented against the declared floor of Qt 6.3; 'compiles with Qt
6.3' counts as an acceptance criterion only once a Qt 6.3 build is actually run
(CI job or release gate — the repository currently has no CI workflow).
A dedicated compile/object target that contains only the public session and
transport headers and links `Qt6::Core` (not the existing test targets, which
link `komport_core` and therefore Widgets) builds, proving the new public
contracts have no widget dependency."

**BS-11 (F15) — source IDs.** Replace "Every event has source ID 1" with:

"Every M8 event carries source ID 1, including a failed-open
`Error(kind: "open")`; zero is reserved by ADR-002 for future session-wide
non-data events and is unused in M8."

**BS-12 (D1 option 1) — session origin in invariants and tests.** Extend §8:

"Every event preserves `sourceTimestampNs` unchanged; `timestampNs` is derived
relative to the session origin recorded on the first event the controller
accepted (ADR-005), is non-negative, never decreases, and is zero for that first
event; equal timestamps are ordered by `sequence`."

and §13 unit tests: "origin capture on the first accepted event, non-negativity
and non-decreasing session time, and `timestampNs == 0` for the first event; a
close/reopen does not re-anchor the origin."

### 3.8 Architecture documents (BD)

**BD-1 (F14, broadened).** Add short "Superseded by …" notes at every affected
place, not only the first occurrence:

- `docs/komport-session-replay-simulation-architecture.md` §5 (the
  `QVariantMap metadata` member) → superseded by ADR-002 (`QJsonObject`;
  lossless 64-bit encoding per ADR-006).
- same document §7.2 (suggested magic `KOMPORTSESSION`) → superseded by ADR-006
  (`KPSN 0x1A CR LF NUL`).
- `docs/komport-multiport-sniffer-time-alignment.md` §3 (source numbering from
  "Source 0") and its later source-zero examples → superseded by ADR-002:
  physical sources use non-zero ids, zero is reserved.

## 4. Traceability matrix

| Finding (round) | Disposition | Amendments |
| --- | --- | --- |
| F1 clock origin/domain (r0) | corrected by r1; option 1 | D1, B3-1, B5-1, B6-1, BS-12 |
| F2 all TX entry points (r0) | accepted with revised wording | B3-2, BS-2 |
| F3 sequence/activation (r0) | accepted, state model revised | B2-1, BS-1 |
| F4 partial write (r0) | accepted with precision fixes | B3-3, BS-2, BS-7 |
| F5 byte-wise TX volume (r0) | accepted in substance; figure corrected (C1) | BS-8 |
| F6 effective configuration (r0) | replaced by transaction model | BS-3, D3 |
| F7 replay boundary (r0) | replaced; architecture question | B3-5, B7-1, D2 |
| F8 non-reentrancy (r0) | accepted, extended (FIFO, sequencing, all types) | BS-4 |
| F9 error distinction/open failure (r0) | accepted, extended (code, guard, C5) | BS-1, BS-7 |
| F10 outside `Live` (r0) | accepted, extended (activation generation) | BS-1 |
| F11 recorder/display divergence (r0) | corrected (C2, C3) | BS-6 |
| F12 PTY chunk assertions (r0) | accepted, test double required | BS-8 |
| F13 unverifiable criteria (r0) | accepted, corrected (C4) | BS-10 |
| F14 document drift (r0) | accepted, broadened | BD-1 |
| F15 source-ID wording (r0) | accepted, extended | BS-11 |
| F16 test-only consumer (r0) | revised (no universal cross-check) | BS-9 |
| X1 clock-domain schema (r1) | accepted | B3-1, B5-1, B6-1 |
| X2 JSON integer precision (r1) | accepted | B2-3, B6-2, B8-1 |
| X3 event value contract (r1) | accepted | B2-2 |
| X4 ownership/destruction order (r1) | accepted | B3-7, BS-5 |
| X5 duplicate apply/open-configure (r1) | accepted | BS-3, D3 |
| X6 capture-point limitation (r1) | accepted | B3-6, BS-2, BS-6 |
| X7 legacy RX buffer across activations (r1) | accepted, preserve + guard | BS-1, BS-6 |
| X8 deterministic test seams (r1) | accepted | BS-8, BS-9 |
| X9 scoped compatibility exception (r1) | accepted | B3-4 |

## 5. Open items for round 2

1. Resolve D1, D2, D3 (or accept the recommended options).
2. Confirm that splitting `SessionController` into a live-observation adapter
   (B3-5) does not weaken the M8 acceptance criteria or §16.3.
3. Confirm that BS-3's removal of the duplicate settings application is
   acceptable inside M8 rather than a separate reviewed change.
4. Confirm the strictness of B2-2 (runtime validation plus convention) against
   the alternative of a type-level immutable encoding.
5. Identify anything in this amendment set that pulls M9+ work into M8.
