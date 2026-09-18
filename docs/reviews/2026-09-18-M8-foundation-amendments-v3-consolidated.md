# M8 Foundation — Consolidated Amendment Package (v3, final text)

Date: 2026-09-18
Status: **under review — round 5 done, the three round-5 findings folded in.**
No ADR, SPEC or architecture document has been modified, no status changed, no
implementation written.

Revision 2 (2026-09-18, after round 5). The three round-5 findings are folded into
this document **in place** instead of into yet another delta document, because the
layering itself produced round 4's contradictions: (a) A3-9 now exempts the single
failed-open `transportError` from the "no observation without `opened()`" rule;
(b) new S-11 amends the retained §3 non-goal so that the test-only `Qt6::Core`
compile target required by S-10 is explicitly allowed; (c) S-3's request, result
metadata and observable mapping now include the stored compatibility field
`startBits` with its own `changedGroups` entry, and S-9 tests it. Round-5 review:
`docs/reviews/2026-09-18-M8-foundation-independent-review-round5.md`.

This document is the **consolidated, self-contained final text** of every
amendment needed to make ADR-002 … ADR-009 and SPEC-M8 accept-ready. It replaces
the layered delta documents as the working text:

- `2026-09-18-M8-foundation-prereview-and-proposed-amendments.md` (round 0, v1)
- `2026-09-18-M8-foundation-independent-review.md` (round 1)
- `2026-09-18-M8-foundation-amendments-v2.md`
- `2026-09-18-M8-foundation-independent-review-round2.md` (round 2)
- `2026-09-18-M8-foundation-amendments-v2.1.md`
- `2026-09-18-M8-foundation-independent-review-round3.md` (round 3)
- `2026-09-18-M8-foundation-amendments-v2.2.md`
- `2026-09-18-M8-foundation-independent-review-round4.md` (round 4)

Those eight documents stay as the process record and are **not** the source of
truth for the wording any more. The reason for consolidating is the round-4
finding: the layered deltas themselves produced contradictions between an earlier
superseded passage and a later correction (a stale test bullet, an inconsistent
operation row, three different owners named for the replay interface). A single
final text removes that failure mode.

How to apply this document: each item names the target document and the passage to
replace, and contains the replacement text. Part D maps every finding of rounds
0–4 to the item that answers it. Part E is the acceptance checklist for round 5.

Nothing here implements M9+ work. Where an obligation can only be implemented in
M9 (the session writer), the text says so and keeps it at contract level.

## Part A — ADR amendments

### A2 — ADR-002 (`SessionEvent` v1)

**A2-1 (replace the sequence sentence).** "`sequence` is strictly increasing
within one session and begins at one." becomes:

"`sequence` is strictly increasing within one session and begins at one. A
session may contain several transport activations (open, close and reopen of the
same source); `TransportOpened` and `TransportClosed` events mark them, and
`sequence` continues across them. An activation never resets `sequence`."

**A2-2 (replace the immutability paragraph).** Replace "The event is immutable
after emission. Views and decoders may derive data but must not modify the event
or use derived data as its replacement." with:

"The event is immutable after emission: a consumer must not modify an event and
must not replace stored events with derived data. This is a contract with two
parts, and the producer satisfies both before an event becomes part of the
stream:

- Structural validation is event-local and stateless — `type` is a declared
  enumerator; `direction` is exactly `Tx` or `Rx` if and only if `type == Data`
  and `None` otherwise; `Data` has a non-empty `payload` while every non-data
  event has an empty one; `sourceId` is non-zero for physical/event sources (zero
  is reserved for future session-wide non-data events and unused in M8); both
  timestamps are non-negative; `metadata` is JSON-encodable or empty. It is
  implemented once as a free function and used by the loader, replay and tests.
- Stream validation is stateful and belongs to the producing controller —
  `sequence` starts at one and strictly increases within the session; session
  time is non-decreasing **within each clock domain** (ADR-005; different clock
  domains are ordered only by `sequence` and arrival order until a versioned
  cross-domain mapping exists, ADR-008); every event's source resolves to its
  clock domain and uses that domain's single reference.

`SessionController` is the factory for M8's live events and the only place where
an event enters the stream, so both parts are enforced on emission and are
verifiable through the deterministic transport double. `SessionEvent` stays a
plain aggregate; a stricter type-level encoding (private members with accessors)
is not required by M8."

**A2-3 (replace the metadata-encoding paragraph).** Replace the paragraph about
values that "carry nanosecond counts, sequence numbers, source ids or byte
counts" with:

"Lossless encoding applies to the values that are 64-bit and appear in JSON:
`sequence`, both timestamps, and any future 64-bit quantity such as an alignment
mapping's reference point or offset. Those are written as canonical decimal
strings, because JSON numbers are IEEE-754 doubles and lose integer precision
beyond 2^53 (about 104 days of nanoseconds). 32-bit quantities keep their
specified widths and remain JSON numbers, which represent them exactly: `sourceId`
is `quint32` in the event model and `u32` in the record, `eventType` is `u16`,
`direction` and `flags` are `u8`, and every length field is `u32`. A JSON number
must never be assumed to carry a full nanosecond value, and a reader must never
silently truncate."

**A2-4 (add).** "Metadata is empty for `Data` events in M8; an empty metadata
object is encoded as a zero length (ADR-006), not as `null`."

### A3 — ADR-003 (`ITransport` and the controller boundary)

**A3-1 (replace the timestamp paragraph).** Replace "Timestamps are monotonic and
relative to a single transport activation. … `ITransport` neither stores sessions
nor decodes bytes." with:

"Timestamps are non-negative monotonic nanoseconds in the observing source's
clock domain. A source uses one monotonic clock for the lifetime of its clock
domain (process or agent) and never a per-activation zero; a clock domain is
identified by a stable identifier (ADR-006). The source that observes bytes
creates the timestamps: local serial uses the process-wide clock defined in
ADR-005, a future remote adapter uses its own agent-wide clock, and observations
within one clock domain remain directly comparable across activations.
`ITransport` neither stores sessions nor decodes bytes and does not assign
session time — that is the controller's mapping (ADR-005)."

**A3-2 (replace the compatibility/bypass sentences and add the chunk rule).**
Replace "`KomportSerial` is migrated incrementally into the local serial
implementation and may keep its legacy character methods/signals during the
transition. No new feature may bypass `ITransport` or `SessionController`." with:

"Every byte-transmitting entry point of an `ITransport` implementation — the
legacy `putChar()` and both `putStr()` overloads as well as `writeBytes()` — is
implemented on one internal write primitive, and that primitive alone creates a
TX observation. A compatibility method that writes bytes without producing an
observation would be exactly the bypass this ADR forbids.

`KomportSerial` is migrated incrementally into the local serial implementation
and may keep its legacy character methods and signals during the transition. M8
legacy callers may continue to use them directly, provided every such call routes
through the single observed write primitive. M8 adds no new UI-side transport
API, and no new consumer may derive session data from character signals; those
signals remain display adapters (ADR-002).

One accepted API write produces exactly one transport-observation chunk. This
records the application/API acceptance boundary, not physical wire framing:
`QSerialPort::write()` acceptance is not electrical delivery. A single logical
action may therefore produce several events (the emulation sends a cursor or
insert key as `putChar(ESC)` followed by one or more `putStr()` calls). The bytes
stored in an event are never rewritten, and an emitted or persisted `SessionEvent`
is immutable. Decoders and views may derive reassembled byte streams and protocol
frames for their own display; such derived framing is a view of the stored
events, must never be written back, and must never replace what the stored events
contain."

**A3-3 (replace the write signature and document the signals).** Replace
`virtual bool writeBytes(const QByteArray &bytes) = 0;` with:

```cpp
    // Sends bytes and returns the number of bytes the transport accepted
    // (0..bytes.size()); a negative value means the write was refused.
    virtual qint64 writeBytes(const QByteArray &bytes) = 0;
```

and add: "`bytesReceived`/`bytesWritten` report bytes observed/accepted by the
transport API; for a partially accepted write, `bytesWritten` carries exactly the
accepted prefix, and 'accepted' never means 'physically delivered'.
`KomportSerial`'s `bool` compatibility methods keep their signatures and return
`true` only when all requested bytes were accepted."

**A3-4 (add).** "No new feature may bypass `ITransport` or `SessionController`."

**A3-5 (replace the controller/replay sentences).** Replace "`SessionController`
is the sole adapter from transport signals to `SessionEvent`: …" up to and
including "M8 has one active transport source; this is an implementation
boundary, not an implicit constraint of `SessionEvent` or `.kpsession` v1
(ADR-008)." with:

"`SessionController` is the sole adapter from **live transport observations** to
`SessionEvent`: it assigns sequence numbers, associates the controller-owned
source descriptor/ID, maps source time to session time (ADR-005) and emits events
to recorder, views and decoders. `ITransport` deliberately has no semantic source
role: a passive sniffer source may emit only generic `Rx` while its
controller-owned descriptor says `controller_to_device`.

Passive replay is not a live transport and does not use `ITransport`: it
distributes stored, immutable `SessionEvent` values — including their stored
sequence numbers, source identity, event types, annotations and times — through a
read-only replay-player interface whose exact shape is specified in M10. This ADR
and ADR-007 state only the constraints that interface must respect (live/replay
separation and safety); neither ADR defines the interface.

M8 has one active transport source; that is an implementation boundary, not an
implicit constraint of `SessionEvent` or `.kpsession` v1 (ADR-008)."

**A3-6 (replace the first consequence).** Replace "Replay and future TCP/remote
transports can feed the same controller." with:

"Future TCP and remote transports can feed the same controller as live
transports, because they are live byte-movement transports. Passive replay is not
a transport (A3-5)."

**A3-7 (add).** "Observation is byte-exact at the application
transport-observation boundary: RX is exactly the bytes `readAll()` returned, TX
exactly the bytes `write()` accepted. It is not an electrical or logic-analyzer
record: no `waitForBytesWritten()` acknowledgement is implied, and bytes still
buffered below the application layer when a port closes are never observed.
(ADR-008 already states that bit-level electrical order is outside Komport's
observation model.)"

**A3-8 (add the lifetime invariant).** "A controller must not outlive its
transport, must never be destroyed after it, and must not emit events or call
into the transport from its destructor. Where a transport is a by-value member of
its owner (as `KomportSerial` is in `KomportDoc`), the controller's destruction
order relative to that member must be explicit; relying on `~QObject` child
destruction is not acceptable, because Qt destroys children after the owner's own
members."

**A3-9 (replace the signal list and add the activation contract).** Replace the
signal block with:

```cpp
signals:
    void bytesReceived(quint64 activationId, const QByteArray &bytes, qint64 sourceTimestampNs);
    void bytesWritten(quint64 activationId, const QByteArray &bytes, qint64 sourceTimestampNs);
    void opened(quint64 activationId, qint64 sourceTimestampNs, const QJsonObject &metadata);
    void closed(quint64 activationId, qint64 sourceTimestampNs, const QJsonObject &metadata);
    void configurationChanged(quint64 activationId, qint64 sourceTimestampNs, const QJsonObject &metadata);
    void lineStateChanged(quint64 activationId, qint64 sourceTimestampNs, const QJsonObject &metadata);
    void transportError(quint64 activationId, qint64 sourceTimestampNs, const QJsonObject &metadata);
```

and add:

"**Activation identity.** `activationId` starts at 1 for a transport instance and
strictly increases on every `open()` attempt; a failed attempt consumes an id and
never produces an `opened` for it. Every observation of an activation carries
that activation's id, and an attempt's failure is reported exactly once, by the
transport, using an internal in-progress/result guard rather than any text
comparison.

**Emission order versus delivery.** A conforming transport emits `closed()` as
the last signal it emits for an activation and never emits a **non-error**
observation carrying an activation id for which no `opened()` was emitted. The
single `transportError` of a failed `open()` attempt is the only observation that
may carry such an id; the controller consumes it as the failed-open error (S-5).
Signal emission order and signal
delivery order are different things, however: a consumer must not rely on
arrival order, because a connection may be queued, an event loop may delay
delivery, or a transport may be faulty. The controller therefore filters every
observation by `activationId` (SPEC-M8) instead of trusting order.

**Synchronous open result.** `ITransport` v1 requires `open()` to report its own
result synchronously, as the local serial transport does and as the profile-error
regression depends on. A transport whose open completes asynchronously must
define its own correlation and is not part of v1."

### A5 — ADR-005 (source timing and session time)

**A5-1 (replace the mapping paragraphs).** Replace the paragraph beginning
"`SessionEvent::timestampNs` is an aligned, non-negative duration…" together with
the following M8 paragraph with:

"`SessionEvent::timestampNs` is the non-negative duration on the session timeline,
derived from source time by one explicit mapping. Per clock domain the controller
keeps a reference and a last emitted value:

```text
reference.sourceTimestampNs        i64  raw source-clock value of the anchor event
reference.sessionTimestampNs       i64  session time of the same anchor event; exactly 0 in v1
lastEmittedSessionTimestampNs      runtime state; initialized to reference.sessionTimestampNs
                                        when the reference is set, updated after every emission

rawSessionTimestampNs = sourceTimestampNs - reference.sourceTimestampNs
timestampNs           = max(lastEmittedSessionTimestampNs, rawSessionTimestampNs)
lastEmittedSessionTimestampNs = timestampNs        (after emission)
```

The anchor event of a clock domain is the first event the controller accepts whose
source belongs to that domain; its reference is set from that event, so it has
`rawSessionTimestampNs == 0` and `timestampNs == 0`. A failed-open `Error` event
is a legitimate anchor event: it is a real observation of that domain, its timing
is meaningful, and treating it as the anchor removes the ambiguity of 'which event
is first' without special cases. Once set for a domain, the reference never moves
for the rest of the session: a later activation, a reopen or a second source in
the same domain does not re-anchor it. `sourceTimestampNs` is retained raw for the
whole life of the session and is never rewritten.

The `max()` term is the explicitly documented non-decreasing normalization of
session time within a clock domain; it is not a correction of stored data. If the
raw term would be lower than the last emitted session time — which contradicts
'one monotonic clock per domain' — the event is still emitted with its payload
unchanged, carries the last emitted session time, and the controller records
exactly one anomaly diagnostic (`Error`, `kind: "runtime"`,
`code: "non_monotonic_observation"`). Byte evidence is never dropped and raw time
is never rewritten.

M8 contains exactly one active local transport source and one clock domain, so its
mapping has a single reference and needs no synchronization. A later multi-source
session may add sources only with a persisted descriptor: sources sharing a
`clockDomainId` are ordered directly on that domain's timeline, while sources in
different domains may not be ordered directly and need a later, separately
specified versioned mapping (reference point, offset, scale, uncertainty).
Multi-host merging is not part of M8 and must visibly report uncertainty when no
trustworthy alignment exists."

**A5-2 (replace the deferred-work sentence).** Replace "M8 and M9 do not
implement a clock synchronization exchange, offset/drift estimation, a
`TimeMapping` runtime component, persisted alignment mappings, or an alignment
UI." with:

"M8 and M9 implement neither a clock synchronization exchange, offset/drift
estimation, a `TimeMapping` runtime component, nor an alignment UI, and they
persist no mapping **between** clock domains. The per-domain origin reference that
ADR-006 requires is capture metadata describing how this file's own timestamps
were produced; it is not a cross-domain alignment mapping."

### A6 — ADR-006 (`.kpsession` v1)

**A6-1 (replace the header description and add the schema).** Replace "The JSON
header contains format/version, wall-clock creation time, application version, an
array of source descriptors/configuration snapshots, capture clock domains,
optional decoder hints and notes." with:

"The JSON header contains format/version, wall-clock creation time, application
version, an array of source descriptors, an array of capture clock domains,
optional decoder hints and notes. Its two related parts are normative:

```text
sources[]                          one entry per capture source
  sourceId                         32-bit, non-zero, unique in the file
  clockDomainId                    string, references clockDomains[].id (mandatory)
  name, transport, configuration   human-readable name and the applied configuration snapshot

clockDomains[]                     one entry per clock domain used by this file
  id                               opaque string, unique in the file
  kind                             "process-monotonic" (local) or "agent-monotonic" (future remote)
  reference.sourceTimestampNs      i64, decimal string - raw source-clock value of the anchor event
  reference.sessionTimestampNs     i64, decimal string - session time of that event, exactly 0 in v1
  wallClockCorrelation             optional, informational
    wallClock                      ISO-8601 UTC string of the anchor event, best effort
    precisionNs                    i64, decimal string - uncertainty of that correlation
```

A source without a `clockDomainId` is not v1-conformant, and a clock domain
without identifier and reference pair is not v1-conformant: a timestamp that
cannot be related to a reproducible session timeline must not be written as if it
could. The wall-clock correlation is informational — the zero point of a monotonic
clock cannot in general be converted to an exact wall-clock instant, so no
exactness is claimed or required, and `sourceTimestampNs` remains authoritative."

**A6-2 (add).** "Binary widths are fixed: `sequence` is `u64`,
`sourceTimestampNs` and `timestampNs` are `i64`, `eventType` is `u16`,
`direction` and `flags` are `u8`, `sourceId` is `u32`, and `recordLength`, the
header byte length, `metadataLength` and `payloadLength` are `u32`. The v1 record
prefix is therefore 44 bytes. Changing one of these widths is a format-version
change, not an amendment. In JSON, only the values that are actually 64-bit
(`sequence`, the timestamps, and any future 64-bit quantity such as an alignment
mapping's reference point or offset) are written as canonical decimal strings;
32-bit quantities stay JSON numbers, which represent them exactly."

**A6-3 (replace the metadata sentence).** "`metadataLength == 0` is the encoding
of an empty metadata object (`{}`) and is normal, not an error; readers must treat
it as empty metadata."

### A7 — ADR-007 (replay safety)

**A7-1 (replace the passive-replay paragraph).** Replace "Passive replay emits
events through `SessionController` to views and decoders, and has no `ITransport`
output target." with:

"Passive replay distributes stored, immutable `SessionEvent` values to views and
decoders through the read-only replay-player interface specified in M10; it is
neither a live transport (ADR-003) nor the live observation path of
`SessionController`. It has no `ITransport` output target and cannot create one,
and it is safe to start without a confirmation."

### A8 — ADR-008

**A8-1 (add after the mapping sentence).** "Because a mapping's reference points,
offsets, scales and uncertainties are nanosecond-scale integer values, they are
encoded losslessly (ADR-006): never as JSON numbers."

**A8-2 (replace the deferred-work sentence).** Replace the sentence listing what
"is introduced by this ADR, M8 or M9" so that it reads: "No synchronization
algorithm, clock probe, offset/drift estimator, `TimeMapping` runtime object,
cross-domain mapping or alignment UI is introduced by this ADR, M8 or M9. Those
are separate future work, started only when the multi-host use case is actually
needed. The per-domain origin reference of ADR-006 is mandatory capture metadata
for this file's own timeline and is not such a mapping."

## Part B — SPEC-M8 amendments

**S-1 (§3, replace the settings non-goal).** Replace "No change to serial
settings, charset translation, macros or file transfer semantics." with:

"No change to the meaning or set of serial settings: no new setting, no different
hardware encoding of an existing setting, and no change to how the settings
dialog, profiles or `QSettings` present them. Charset translation, macros and
file transfer semantics are unchanged. Two narrowly authorised exceptions belong
to M8 because the session contract cannot be satisfied without them: (a) the
duplicate application of an unchanged setting combination performed by `open()`
is removed, so that one configuration request yields one result; (b) a single
configuration entry point is added and both application call sites use it, so
that one user action is one configuration transaction. Both are confined to
serial-configuration orchestration inside the terminal; neither changes which
settings exist or which values they can take."

**S-2 (§6.2, replace the TX/RX paragraphs).** Replace the paragraph describing the
`ITransport` implementation and the TX observation with:

"`KomportSerial` implements `ITransport`. All byte-transmitting entry points
(`putChar()`, both `putStr()` overloads, `writeBytes()`) are implemented on one
internal write primitive, and that primitive alone creates TX observations, so UI
keystrokes, emulation replies, macro output, line endings, transfer bytes and
future `writeBytes()` callers all reach the session stream through one rule.

TX: a fully accepted write produces exactly one `Data`/`Tx` event covering all
requested bytes; a partially accepted write produces exactly one `Data`/`Tx`
event covering the accepted prefix, keeps the existing failure return and warning,
and additionally produces exactly one `Error` observation with `kind: "write"`
carrying `acceptedBytes` and `refusedBytes`; a write that accepts nothing produces
no data event and exactly one `Error` observation.

RX: exactly one observation for every successful non-empty `readAll()` result,
emitted before the existing buffering/character path; an empty read produces
nothing.

Byte-exactness holds at the application transport-observation boundary only
(ADR-003): RX is exactly the bytes `readAll()` returned, TX exactly the bytes
`write()` accepted, with no electrical-delivery claim."

**S-3 (§6.2, replace the configuration paragraphs) — normative operation table.**
Replace the configuration paragraph and the duplicate-application paragraph with:

"**Entry point.** `KomportSerial` exposes one configuration entry point that takes
the complete requested configuration as a value: hardware settings (endpoint, baud
rate, data bits, stop bits, parity, flow control), local buffering settings (RX
queue, flush rate), and the stored compatibility field `startBits`. The type is
declared next to `KomportSerial`, is QtCore-only,
and contains no widget or application type. The entry point stores all requested
values and then performs the operation of the table below. Both application call
sites migrate to it: `KomportApp::applyConnectionSettings()`
(`komport.cpp:625-650`, which currently closes, stages six setters and opens
unconditionally) and `KomportApp::slotShowPreferences()`
(`komport.cpp:1250-1276`, which stages setters on the current port and calls
`open()` only when the port is closed).

**Operation table (normative).**

| Operation | Precondition | Activation events | Hardware applied | Configuration transaction |
| --- | --- | --- | --- | --- |
| `configure(request)` | port open, endpoint unchanged | none | iff at least one hardware field's effective value changes | one transaction |
| `configure(request)` | port open, endpoint changed | `TransportClosed` (old activation), then `TransportOpened` (new activation) | once, after the reopen | one transaction, its result immediately after `TransportOpened` |
| `configure(request)` | port closed | none | none (values stored only) | none; the next `open()` performs it |
| `open()` | port closed | `TransportOpened` | once (open-and-configure) | one transaction, its result immediately after `TransportOpened` |
| `open()` | port already open | `TransportClosed` (old activation), then `TransportOpened` (new activation) | once, after the reopen | one transaction, its result immediately after `TransportOpened` |
| legacy setter (each of the six) | port open | as its effect requires (`setDeviceName()` may close/reopen) | iff at least one hardware field's effective value changes | one transaction |
| legacy setter (each of the six) | port closed | none | none (value stored only) | none |
| `close()` | port open | `TransportClosed` | none | none |

`open()`'s documented behaviour of closing an already-open port first is
unchanged; in that case it is a real activation boundary and produces the closing
event, the opening event and one transaction result.

**Observable mapping (normative).** One transaction produces one or two transport
observations, and each observation becomes exactly one event:

| Transaction outcome | Observations | Events |
| --- | --- | --- |
| requested values equal the current effective values, port open, endpoint unchanged | none (documented no-op: no hardware application is attempted) | none |
| `full`, hardware changed | one `configurationChanged` | one `TransportConfigChanged` |
| `full`, only local buffering changed | one `configurationChanged` (`changedGroups: ["localBuffering"]`, hardware section unchanged) | one `TransportConfigChanged` |
| `full`, only the compatibility field `startBits` changed | one `configurationChanged` (`changedGroups: ["compatibility"]`, hardware and buffering sections unchanged) | one `TransportConfigChanged` |
| `partial` | one `configurationChanged`, then one `transportError` with `kind: "apply"` | `TransportConfigChanged` then `Error`, in that order |
| `failed` with a changed effective state | as `partial` | as `partial` |
| `failed` with no change | one `transportError` with `kind: "apply"` | one `Error` |

A transaction never yields more than two observations and never fewer than one
unless it is the documented no-op. A result's metadata contains the requested
hardware settings, the read-back effective hardware settings (endpoint, baud
rate, data bits, parity, stop bits, flow control as accepted by `QSerialPort`),
the local buffering settings, the stored compatibility field `startBits`,
`changedGroups` and `applyStatus` ∈ {`full`, `partial`, `failed`}.

`startBits` is stored and reported for UI and configuration compatibility only: a
UART always transmits a single start bit and `QSerialPort` exposes no such
setting, so the field is never applied to the hardware (this matches the current
behaviour and the comment in `komportserial.cpp`). A change of `startBits` alone
therefore produces `changedGroups: ["compatibility"]` with unchanged hardware and
buffering sections, and it is reported as a result rather than silently ignored,
so that a session record shows which configuration the user had selected.
`changedGroups` lists every group whose values actually changed in one
transaction.

**Legacy setters and signals.** The legacy setters keep their signatures and
behaviour as compatibility adapters: each one stores its value and, when the port
is open, applies through the same transaction routine once, then emits its
compatibility signal. The self-connection `settingsChanged()` →
`slotSettingsChanged()` is removed as part of exception (a) of §3, because it *is*
the duplicate application: after this change `settingsChanged()` and
`settingsFailed()` are pure notifications for existing consumers and never cause a
hardware application. Consequences of removing the duplicate: one redundant
hardware application of an identical setting combination disappears, and at most
one duplicate `settingsFailed()` emission per open disappears. Whether re-applying
identical settings is an observable no-op is platform- and device-dependent; that
dependence is part of why the duplicate is removed, and the removal is covered by
the existing serial tests plus one new test asserting one transaction result per
open. Metadata contains no credentials."

**S-4 (§6.2 and §10, add the ownership rule).** "`KomportDoc` owns the transport
and the controller. The controller is destroyed before the transport, and its
destructor neither emits events nor calls into the transport (ADR-003). Concrete
layout: the controller is owned through a `std::unique_ptr` reset in
`KomportDoc`'s destructor body, or declared as a member after `mSerial` so that it
is destroyed first. A QObject-child arrangement that relies on `~QObject`
ordering is not acceptable, because `KomportSerial` is a by-value member of
`KomportDoc` and would already be destroyed."

**S-5 (§7, replace the state model).**

"`SessionController` has `Idle` and `Live`. A controller session exists from
construction and ends with the controller: the session boundary is
controller/document lifetime, not `File → New` (`KomportDoc::newDocument()` does
not replace the serial object) and not a transport activation.

- construction: `Idle`.
- transport opened successfully: emit exactly one `TransportOpened` and enter
  `Live`. `sequence` continues to increase monotonically for the whole session and
  is never reset by an activation.
- observations in `Live`: emit ordered events, subject to the activation filter
  below.
- transport closed: emit exactly one `TransportClosed` and return to `Idle`. This
  is an activation boundary, not the end of the session; M8 has no terminal
  `Closed` state, because repeated closing and reopening is normal.
- failed open: the transport reports the failure itself, because it knows its own
  attempt, exactly once (ADR-003). The controller emits exactly one `Error` with
  `kind: "open"`, does not emit `TransportOpened` for that attempt, does not enter
  `Live`, and does not consume an activation. The legacy `settingsFailed()` signal
  keeps its present synchronous behaviour for existing consumers.
- destruction: no events and no calls into the transport (S-4).
- activation filter: the controller stores `currentActivationId` (set on
  `opened`, cleared on `closed`) and emits an event only for observations whose
  `activationId` equals it. An observation with any other activation id is
  dropped with exactly one diagnostic and produces no event. An `Error` whose
  activation id is unknown and was never preceded by an `opened` for that id is
  the failed-open case above; any further observation carrying the same id is
  suppressed. Arrival order is never trusted — only the id (ADR-003)."

**S-6 (§8, replace the invariants list).**

"Invariants:

- `QByteArray` payloads are copied byte-for-byte, including NUL and every value
  `00..FF`; no rule in this specification may drop, reorder or rewrite a payload.
- one transport observation produces exactly one `Data` event; M8 never coalesces
  or splits it. (A configuration transaction may produce two observations, and
  therefore two events — S-3 — but each observation still maps to exactly one
  event.)
- `sequence` strictly increases; equal session times are ordered by `sequence`.
- every event carries source ID 1 (including a failed-open `Error(kind: "open")`);
  zero is reserved by ADR-002 and unused in M8.
- every event preserves `sourceTimestampNs` unchanged; `timestampNs` follows the
  mapping of ADR-005, is non-negative, is `0` for its clock domain's anchor event,
  and is non-decreasing within that domain. Global stream order is `sequence` and
  arrival order, never a comparison of timestamps across clock domains.
- RX and TX use ADR-004 semantics.
- no live event is created from terminal-decoded text or charset-converted data.
- M8 does not write through `SessionController`; it cannot create a new
  hardware-output path.
- the event stream preserves every non-empty `readAll()` result observed by the
  transport before legacy buffering and is independent of `RxQueue` and
  `FlushRate`. The legacy character path stays a lossy, delayed display adapter:
  it trims its RX buffer to `setRxQueue()` and delivers through the
  `setFlushRate()` timer. That buffer is not cleared by `close()` and its timer
  keeps running, so bytes received before a close can still be rendered after a
  reopen, and an emulation reply to them can appear as TX in a later activation.
  M8 preserves that legacy behaviour deliberately — changing it would alter
  visible terminal behaviour and requires its own reviewed change — and the
  activation filter of S-5 keeps the event path correct. No test may assert
  universal equality between the event stream and the character signals."

**S-7 (§9, replace the error handling list).**

"- Partial/failed serial writes keep their current `false` return and warning. The
  accepted prefix is still reported as one `Data`/`Tx` event (S-2); the refused
  remainder is not data and is reported through the `Error` observation of S-2.
- A transport error becomes an `Error` non-data event while a session is live; the
  existing user-facing error path remains unchanged. Its metadata carries at least
  `kind` (`open`, `apply`, `write`, `runtime`), a stable machine-readable `code`, a
  human-readable `message`, and the relevant non-secret numeric context (for
  example `acceptedBytes`/`refusedBytes`); never credentials and never decoded
  payload data.
- An `open()` attempt that fails produces exactly one `Error(kind: "open")` while
  `Idle` (S-5); a duplicate error for the same attempt is suppressed by the
  transport's own guard, never by comparing text.
- Empty successful reads are ignored; an empty data event is never emitted.
- A source observation that would violate non-decreasing session time within its
  clock domain is not dropped and its timestamp is not rewritten: it is emitted
  with its payload unchanged and the last emitted session time, and the controller
  records exactly one anomaly diagnostic (ADR-005). This replaces any formulation
  of 'clamping'."

**S-8 (§10, add delivery rules).** "Delivery is non-reentrant and ordered.
`SessionController` keeps a FIFO of complete observations, assigns `sequence` at
dequeue/emission time, and completes the delivery of one event before processing
the next observation — for every event type, including close, error and
configuration, not only data. An observation produced while an event is being
delivered (a consumer that writes from its handler, as the existing emulation does
for device-status replies) is appended to the FIFO and emitted after the current
delivery returns; emissions never nest. With two subscribers, both must observe
the same order: for a consumer that writes one byte in response to an RX event,
both subscribers see `RX` then `TX`, and neither may see a nested `TX` inside its
`RX` callback. M8 is main-Qt-thread only; signals are direct within the owning
thread and no queue, lock or worker is introduced."

**S-9 (§13, replace the test strategy).**

"Unit tests: origin capture on the first accepted event per clock domain and
`timestampNs == 0` for that anchor; non-negativity and non-decreasing session time
per domain with exactly one anomaly diagnostic per violation; source-time
preservation; identity of the mapping where the anchor is the first event;
structural validation of every event; source ID 1 on every event including
`Error(kind: "open")`.

Controller tests, deterministic, using a test `ITransport` double that emits
scripted observations (including empty reads, partial accepts, configuration
observations and scripted errors):

- one observation → one event; sequence continuity across activations without
  reset; the documented no-op producing nothing;
- one test per operation-table row of S-3, including the read-back values in the
  metadata and `changedGroups`;
- a `startBits`-only change produces exactly one result with
  `changedGroups: ["compatibility"]`, unchanged hardware and buffering sections,
  no hardware application, and unchanged UI/config behaviour of that field;
- the observable mapping of S-3 for `full`, `partial`, `failed`-with-change and
  `failed`-without-change, in the stated order;
- FIFO order and two-subscriber non-reentrancy (RX then TX for both subscribers);
- activation filtering: an observation carrying an old activation id, delivered
  after a new `opened`, is dropped with exactly one diagnostic and produces no
  event (this test deliberately models a non-conforming or queued-late delivery —
  see the emission-versus-delivery rule in ADR-003); a second test models
  conforming emission order and asserts one event per observation in order;
- a failed `open()` attempt whose error is reported twice by a faulty double
  yields exactly one `Error(kind: "open")`.

`KomportSerial` tests: the accepted-prefix behaviour and the one-transaction-per-
operation rule need an injectable seam (a small protected/override point or a test
subclass) for the write result and the configuration result, because a genuine
partial hardware write is not reproducible in this environment (see the comment
in `tests/tst_serial.cpp`) and PTYs do not deterministically select chunk
boundaries. The existing serial suite must pass unchanged, including the
synchronous failed-open path that `tests/tst_profileerror.cpp` depends on.

PTY tests (end-to-end binary preservation, no chunk-count claims): RX with
embedded NUL and values spanning `00..FF` — the concatenation of all RX payloads
equals the sent byte stream, no event is empty, order is deterministic; TX via the
length-aware path is byte-identical; tests must not assert a fixed number or split
of chunks and must not depend on the legacy flush timer.

Volume/pathology: 64 KiB written byte-wise through `putChar()`, as
`KomportTransfer::upload()` does, yields exactly one TX event per accepted write
with correct payload bytes; the test asserts event count, byte count and ordering
and must not require an in-memory event vector in the consumer. For sizing: the
v1 record prefix is 44 bytes per event (ADR-006), so a byte-wise upload is a
deliberate cost that M9's streaming policy must handle.

M8 ships no production consumer; the recorder arrives in M9. `tests/` therefore
contains a test-only event collector plus the deterministic double above (never
linked into the shipped executable, no CLI flag, no UI, no persistence). Event
behaviour is verified primarily against the double; comparison with the legacy
character signals may only be used in a fixture with no RX overflow and a forced
flush, because the two paths are intentionally divergent. No production code
depends on the collector."

**S-10 (§14, replace the affected acceptance criteria).**

- Replace "Core types and transport contract compile with Qt 6.3." with: "Core
  types and the transport contract compile against the locally installed Qt 6 and
  are documented against the declared floor of Qt 6.3; 'compiles with Qt 6.3'
  counts as an acceptance criterion only once a Qt 6.3 build is actually run (CI
  job or release gate — the repository currently has no CI workflow)."
- Add: "A dedicated compile/object target containing only the public session and
  transport headers and linking `Qt6::Core` (not the existing test targets, which
  link `komport_core` and therefore Widgets) builds, proving the new public
  contracts have no widget dependency."
- Replace "Open, close, error and effective serial configuration are observable as
  non-data events without changing their existing UI behavior." with: "Open, close,
  error and effective serial configuration are observable as non-data events.
  Compatibility expectations for the authorised orchestration change of §3: the
  settings vocabulary, their persistence and the settings dialog's presentation are
  unchanged; one user action from the settings dialog or a profile load produces
  exactly one configuration transaction and its result(s); the synchronous legacy
  `settingsFailed()` notification on a failed open is preserved."
- Replace "Every M8 event carries source ID 1 …" wording with the S-6 invariant
  wording, and keep the remaining criteria (NUL/all-byte PTY tests, full `ctest`
  suite, no file format / replay UI / decoder / active transmission).

**S-11 (§3, amend the CMake/target non-goal).** Replace the non-goal "No
analyzer/agent executable, shared-library extraction, CMake target split or
migration of existing terminal UI classes." with:

"No analyzer/agent executable, no shared-library extraction, no CMake target split
into production or shared-library targets, and no migration of existing terminal
UI classes. A dedicated test-only compile/object target that contains only the new
public session and transport headers and links only `Qt6::Core` is explicitly not
such a split: §14 requires it to prove that the public contracts have no widget
dependency. It is not installed, not shipped, and changes no existing target's
link interface."

## Part C — Documentation notes (apply in the same step)

Add one line, "Superseded by ADR-00x: <what replaces it>", at all seven
locations. Three documents are affected:

1. `docs/komport-session-replay-simulation-architecture.md` line 226
   (`QVariantMap metadata;`) → superseded by ADR-002 (`QJsonObject`; lossless
   64-bit encoding per ADR-006).
2. same document, §7.2 (suggested magic `KOMPORTSESSION`) → superseded by ADR-006
   (`KPSN 0x1A CR LF NUL`).
3. `docs/komport-multiport-sniffer-time-alignment.md` line 121
   (`QVariantMap metadata;`) → superseded by ADR-002.
4. same document line 128 (`Source 0:`) → superseded by ADR-002/ADR-008: physical
   sources use non-zero ids, zero is reserved, and a semantic role is descriptor
   metadata rather than a direction.
5. same document line 323 (`Source 0 semanticDirection = controller_to_device`) →
   same as (4).
6. same document line 1117 (`QVariantMap metadata;`) → superseded by ADR-002.
7. `docs/komport-multi-executable-product-architecture.md` line 570
   (`QVariantMap metadata;`) → superseded by ADR-002.

No document is rewritten or translated; the conceptual examples stay, only their
numbering or type is marked as superseded.

## Part D — Traceability

| Source finding | Answered by |
| --- | --- |
| round 0 F1 clock origin/domain | A3-1, A5-1, A6-1, S-5, S-6 |
| round 0 F2 all TX entry points | A3-2, A3-3, S-2 |
| round 0 F3 sequence across activations | A2-1, S-5 |
| round 0 F4 partial writes | A3-3, S-2, S-7 |
| round 0 F5 byte-wise TX volume | S-9 (volume test) |
| round 0 F6 effective configuration | S-3 |
| round 0 F7 replay boundary | A3-5, A3-6, A7-1 |
| round 0 F8 non-reentrancy | S-8 |
| round 0 F9 error distinction/open failure | A3-9, S-5, S-7 |
| round 0 F10 outside `Live` | S-5 |
| round 0 F11 display/recorder divergence | S-6, S-9 |
| round 0 F12 PTY chunk assertions | S-9 |
| round 0 F13 unverifiable criteria | S-10, S-11 |
| round 0 F14 documentation drift | Part C |
| round 0 F15 source-ID wording | S-6, S-9 |
| round 0 F16 no production consumer | S-9 |
| round 1 X1 clock-domain schema | A3-1, A5-1, A6-1 |
| round 1 X2 JSON integer precision | A2-3, A6-2, A8-1 |
| round 1 X3 event value contract | A2-2 |
| round 1 X4 ownership/destruction order | A3-8, S-4 |
| round 1 X5 duplicate apply / open-configure | S-1, S-3 |
| round 1 X6 capture-point limitation | A3-7, S-2, S-6 |
| round 1 X7 legacy RX buffer across activations | S-5, S-6 |
| round 1 X8 deterministic test seams | S-9 |
| round 1 X9 scoped compatibility exception | A3-2, A3-4 |
| round 2 (B-items) and round 3/4 (N/P-items) | folded into the items above; specifically: timestamp formula → A5-1; operation table → S-3; activation id → A3-9, S-5; B2-3 widths → A2-3; capture vs. cross-domain → A5-2, A8-2; replay authority → A3-5, A7-1; three documentation documents → Part C |

## Part E — Acceptance checklist for round 5

Round 5 should confirm:

1. No item contradicts another item or a retained ADR/SPEC passage. Specifically:
   A5-1's per-domain time rule against A2-2's stream validation; S-3's operation
   table against S-3's observable mapping, against §3 and §14, and against
   `komport.cpp:625-650` / `:1250-1276`; A3-9's emission-versus-delivery rule
   against S-9's two activation-filter tests; Part C's three documents against the
   seven listed locations.
2. The deleted contradictions of round 4 are actually gone: no "none for a
   redundant open", no unconditional "hardware applied" for local-only changes,
   no claim that both application call sites use a conditional `open()`, no
   unfiltered global non-decreasing time rule, no single owner named for the
   replay interface other than M10.
3. Whether, with all items applied to ADR-002 … ADR-009, SPEC-M8 and the three
   documentation files, the M8 foundation can be marked `Accepted`.
4. Revision 2 of this document: the three round-5 findings (failed-open exception
   in A3-9, S-11's test-only compile target, `startBits` in S-3/S-9) are closed,
   and §3 no longer contradicts §14's required target or the settings vocabulary.

Known follow-ups outside M8, unchanged: M9 streaming writer and persistence; M10
replay-player interface; cross-domain synchronization and uncertainty UI; a
separately reviewed change of the retained legacy RX buffer; actual Qt 6.3
CI/release-gate execution.
