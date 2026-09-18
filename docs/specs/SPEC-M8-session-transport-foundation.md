# Implementation Spec: M8 Session and Transport Foundation

Status: Accepted (independent review rounds 1–5 and the application verification
closed on 2026-09-18; review record in `docs/reviews/`)

Date: 2026-09-17

## 1. Objective

Create the transport-neutral, byte-exact live-event foundation required by
recording, loading, passive replay, decoders and later network transports.
This slice makes the current local serial path produce ordered `SessionEvent`
objects without changing visible terminal behavior or transmitting any new
bytes.

## 2. Scope

Included:

- `SessionEvent`, source-ID, direction and event-type value types from
  ADR-002/004/005/008.
- `ITransport` v1 from ADR-003, including its activation identity.
- Migration of `KomportSerial` into the local `ITransport` implementation
  while preserving its existing character-oriented public API as a temporary
  compatibility adapter.
- One configuration entry point that produces exactly one configuration
  transaction per user action, with read-back effective values (see §3's
  authorised exception and §6.2).
- A `SessionController` owned by `KomportDoc`, which converts current live
  serial observations to ordered events and emits `eventObserved`.
- Unit and PTY tests proving chunk, direction, sequence, timestamp ordering
  and all-byte preservation.

## 3. Non-goals

- No `.kpsession` reader/writer, load/save UI or replacement of the current
  text logger.
- No replay, simulation, TCP, remote agent, decoder framework or UI timeline.
- No multi-source controller, clock synchronization, offset/drift estimation,
  time-alignment persistence or alignment UI.
- No analyzer/agent executable, no shared-library extraction, no CMake target
  split into production or shared-library targets, and no migration of existing
  terminal UI classes. A dedicated test-only compile/object target that contains
  only the new public session and transport headers and links only `Qt6::Core` is
  explicitly not such a split: §14 requires it to prove that the public contracts
  have no widget dependency. It is not installed, not shipped, and changes no
  existing target's link interface.
- No change to the meaning or set of serial settings: no new setting, no
  different hardware encoding of an existing setting, and no change to how the
  settings dialog, profiles or `QSettings` present them. Charset translation,
  macros and file transfer semantics are unchanged. Two narrowly authorised
  exceptions belong to M8 because the session contract cannot be satisfied
  without them: (a) the duplicate application of an unchanged setting
  combination performed by `open()` is removed, so that one configuration
  request yields one result; (b) a single configuration entry point is added and
  both application call sites use it, so that one user action is one
  configuration transaction. Both are confined to serial-configuration
  orchestration inside the terminal; neither changes which settings exist or
  which values they can take.
- No removal of legacy `receivedChar`/`sentChar` signals in M8.

## 4. Architecture References

- `docs/komport-session-replay-simulation-architecture.md`, sections 5–8,
  20–23, 39 and 42.
- `docs/komport-engineering-governance-spec-review-workflow.md`, sections
  7–10 and 16.
- ADR-002 through ADR-009.

## 5. Current State

`KomportSerial` owns `QSerialPort`, reads into `mRxBuffer` and later emits
individual `receivedChar(char)` values. `putStr()` sends a batch but emits
individual `sentChar(char)` values. `KomportView`, emulation, Hex monitor and
the text logger consume those character signals directly. Thus current
terminal behavior is correct for its purpose but is not a session source:
observed chunks, TX batch boundaries, monotonic timing and event order are not
represented.

Configuration is currently applied through several paths: `open()` applies the
current settings and then emits `settingsChanged()`, whose self-connection calls
`applyPortSettings()` a second time; `setFraming()` and `setFlowControl()` call
`applyPortSettings()` directly; `setBaudRate()` applies indirectly through
`settingsChanged()`. The two application call sites stage individual setters —
`KomportApp::applyConnectionSettings()` closes, stages six setters and opens
unconditionally (`komport.cpp:625-650`), `KomportApp::slotShowPreferences()`
stages setters on the current port and opens only when it is closed
(`komport.cpp:1250-1276`) — so one user action can currently cause several
hardware applications and transient failures of intermediate combinations.

## 6. Proposed Design

### 6.1 New components

- `sessionevent.h`: `SessionEvent`, `SessionDirection`, `SessionEventType`.
  Its public value types use QtCore only and no QWidget, `KomportApp` or other
  executable-specific type.
- `itransport.h`: `ITransport` and its binary chunk notifications, including
  the activation identity of ADR-003.
- `transportconfiguration.h` (or an equivalent type declared with
  `sessionevent.h`/`itransport.h`): the complete requested configuration as a
  QtCore-only value type — hardware settings (endpoint, baud rate, data bits,
  stop bits, parity, flow control), local buffering settings (RX queue, flush
  rate) and the stored compatibility field `startBits`.
- `sessioncontroller.h/.cpp`: binds one `ITransport` to one live session;
  assigns `sequence` and physical `sourceId` 1, applies the ADR-005 mapping
  (session-origin-relative, one reference per clock domain) and emits
  `SessionEvent`. It introduces no runtime clock-alignment object.

### 6.2 Existing components

`KomportSerial` implements `ITransport`. All byte-transmitting entry points
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
`write()` accepted, with no electrical-delivery claim.

Existing `receivedChar`/`sentChar` signals continue with their present behavior
and remain responsible for legacy terminal/Hex/logger display. They are not
used by `SessionController`.

**Entry point.** `KomportSerial` exposes one configuration entry point that takes
the complete requested configuration as a value: hardware settings (endpoint, baud
rate, data bits, stop bits, parity, flow control), local buffering settings (RX
queue, flush rate), and the stored compatibility field `startBits`. The type is
declared next to `KomportSerial`, is QtCore-only, and contains no widget or
application type. The entry point stores all requested values and then performs
the operation of the table below. Both application call sites migrate to it:
`KomportApp::applyConnectionSettings()` (`komport.cpp:625-650`, which currently
closes, stages six setters and opens unconditionally) and
`KomportApp::slotShowPreferences()` (`komport.cpp:1250-1276`, which stages setters
on the current port and calls `open()` only when the port is closed).

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

`open()`'s documented behavior of closing an already-open port first is
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

The `open()` rows of the operation table always perform the open-and-configure
transaction, and that transaction applies the hardware settings even when the
requested values equal the last effective snapshot, because a freshly opened port
carries no settings. Its result therefore includes `"hardware"` in `changedGroups`,
even where a configure-level comparison would find no value difference. This is an
explicit exception for an open-and-configure transaction; the documented no-op row
applies only to `configure(request)` on a live port with an unchanged endpoint.

The value returned to the caller additionally carries `storedOnly`. It is true
only when `configure(request)` is called while the port is closed and the request
is stored without a transaction. It is a return-value flag, never event metadata,
and such an operation emits no observation. It is false for a live endpoint-change
request that fails to reopen; that path reports its open failure through the
single `Error(kind: "open")` observation and returns a failed result.

`startBits` is stored and reported for UI and configuration compatibility only: a
UART always transmits a single start bit and `QSerialPort` exposes no such
setting, so the field is never applied to the hardware (this matches the current
behavior and the comment in `komportserial.cpp`). A change of `startBits` alone
therefore produces `changedGroups: ["compatibility"]` with unchanged hardware and
buffering sections, and it is reported as a result rather than silently ignored,
so that a session record shows which configuration the user had selected.
`changedGroups` lists every group whose values actually changed in one
transaction.

**Legacy setters and signals.** The legacy setters keep their signatures and
behavior as compatibility adapters: each one stores its value and, when the port
is open, applies through the same transaction routine once, and then emits
`settingsChanged()` exactly when that setter emitted it before M8. The pre-M8
sources of `settingsChanged()` were `open()` (including the reopen
`setDeviceName()` performs on a live port when the device changes) and
`setBaudRate()`; `setFraming()`, `setFlowControl()`, `setRxQueue()` and
`setFlushRate()` never emitted a signal and still do not. The self-connection
`settingsChanged()` →
`slotSettingsChanged()` is removed as part of exception (a) of §3, because it
*is* the duplicate application: after this change `settingsChanged()` and
`settingsFailed()` are pure notifications for existing consumers and never cause a
hardware application. Consequences of removing the duplicate: one redundant
hardware application of an identical setting combination disappears, and at most
one duplicate `settingsFailed()` emission per open disappears. Whether re-applying
identical settings is an observable no-op is platform- and device-dependent; that
dependence is part of why the duplicate is removed, and the removal is covered by
the existing serial tests plus one new test asserting one transaction result per
open. Metadata contains no credentials.

`KomportDoc` owns the transport and `SessionController`; it exposes only the
controller's read-only event signal to new consumers. `KomportDoc` configures
source ID 1. M8 defines no runtime source-descriptor type. Source descriptors are
session-header data defined by ADR-006, with their multi-source semantics defined
by ADR-008. The local serial descriptor is therefore deferred; M8 fixes one local
source with source ID 1, using the single process-monotonic clock domain required
by ADR-003 and ADR-005. No Qt widget gains a `QSerialPort` dependency.
The transport owns no semantic source role: future passive sniffing can retain
generic Rx and attach its communication role to the source descriptor.
This ownership is an incremental adapter in the current terminal, not a claim
that `KomportDoc` is the future analyzer/agent base class (ADR-009).

One read-only addition to the controller belongs to M9 and is recorded here so
that the frozen surface stays complete: `SessionController` gains an accessor
that returns the clock domain's anchor reference pair (a `valid` flag plus
`sourceTimestampNs` and `sessionTimestampNs`), because a recorder started while
the session is already live must write the domain's true anchor rather than
derive one from the first event it happens to see (ADR-010, decision D8). It is a
read-only view of state the controller already keeps, it is QtCore-only, and it
does not extend the event surface this section fixes.

A second M9 addition belongs to the transport and is recorded here for the same
reason: `KomportSerial` gains the read-only accessor

```cpp
QJsonObject appliedConfigurationSnapshot() const;
```

which returns the applied configuration as the four sections `requested`,
`effective`, `localBuffering` and `compatibility` - the same sections, built by
the same helper, that the live configuration metadata carries, with no
per-transaction member (`applyStatus`, `changedGroups`, a message). A recording
header carries that snapshot (ADR-010, decision D7), and the alternative - an
application assembling the shape itself, or filtering the transaction metadata -
would create a second definition of the same object. The accessor neither
configures nor emits anything; while the port is open it reads the effective
values back, and with the port closed `effective` is the configuration that was
last in force, while `requested` may be a stored request that was never applied
(a recording is live-only, so a header is always written with the port open, and
that distinction exists for callers that read the snapshot at another time).

The controller is destroyed before the transport, and its destructor neither
emits events nor calls into the transport (ADR-003). Concrete layout: the
controller is owned through a
`std::unique_ptr` reset in `KomportDoc`'s destructor body, or declared as a
member after `mSerial` so that it is destroyed first. A QObject-child arrangement
that relies on `~QObject` ordering is not acceptable, because `KomportSerial` is a
by-value member of `KomportDoc` and would already be destroyed.

Modem-line events are part of the frozen `ITransport` surface but remain
unimplemented until a transport actually exposes them.

### 6.3 Data flow

```text
QSerialPort
  -> KomportSerial / ITransport (binary RX or successful TX chunk)
  -> SessionController
  -> SessionEvent
  -> future recorder / decoder / passive replay UI

KomportSerial compatibility signals
  -> existing terminal, hex monitor, text logger
```

The two branches observe the same wire data but have different purposes.
M8 does not replace terminal rendering with SessionEvent delivery.

## 7. State model

`SessionController` has `Idle` and `Live`. A controller session exists from
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
  `Live`, and creates no live activation — its attempt id is consumed all the same
  and is never reused (ADR-003). The legacy `settingsFailed()` signal
  keeps its present synchronous behavior for existing consumers.
- destruction: no events and no calls into the transport (§6.2).
- activation filter: the controller stores `currentActivationId` (set on
  `opened`, cleared on `closed`) and emits an event only for observations whose
  `activationId` equals it. An observation with any other activation id is
  dropped with exactly one diagnostic and produces no event. An `Error` whose
  activation id is unknown and was never preceded by an `opened` for that id is
  the failed-open case above; any further observation carrying the same id is
  suppressed. Arrival order is never trusted — only the id (ADR-003). The
  diagnostic of a rejected observation is exactly one Qt warning message emitted
  by the controller, naming the rejected activation id and the reason; it is not an
  event, and no signal, counter or accessor is added for it. Its purpose is to make
  a filter rejection visible without extending the frozen session surface: a
  consumer-facing channel for it would be new public API that this specification
  does not define.

## 8. Invariants

- `QByteArray` payloads are copied byte-for-byte, including NUL and every value
  `00..FF`; no rule in this specification may drop, reorder or rewrite a payload.
- one transport observation produces exactly one `Data` event; M8 never coalesces
  or splits it. (A configuration transaction may produce two observations, and
  therefore two events — §6.2 — but each observation still maps to exactly one
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
  M8 preserves that legacy behavior deliberately — changing it would alter
  visible terminal behavior and requires its own reviewed change — and the
  activation filter of §7 keeps the event path correct. No test may assert
  universal equality between the event stream and the character signals.

## 9. Error handling

- Partial/failed serial writes keep their current `false` return and warning. The
  accepted prefix is still reported as one `Data`/`Tx` event (§6.2); the refused
  remainder is not data and is reported through the `Error` observation of §6.2.
- A transport error becomes an `Error` non-data event while a session is live; the
  existing user-facing error path remains unchanged. Its metadata carries at least
  `kind` (`open`, `apply`, `write`, `runtime`), a stable machine-readable `code`, a
  human-readable `message`, and the relevant non-secret numeric context (for
  example `acceptedBytes`/`refusedBytes`); never credentials and never decoded
  payload data.
- An `open()` attempt that fails produces exactly one `Error(kind: "open")` while
  `Idle` (§7); a duplicate error for the same attempt is suppressed by the
  transport's own guard, never by comparing text.
- Empty successful reads are ignored; an empty data event is never emitted.
- A source observation that would violate non-decreasing session time within its
  clock domain is not dropped and its timestamp is not rewritten: it is emitted
  with its payload unchanged and the last emitted session time, and the controller
  records exactly one anomaly diagnostic (ADR-005).

## 10. Threading and concurrency

M8 is main-Qt-thread only. `KomportSerial`, `SessionController` and legacy UI
connections share the owning document's thread. Signals are direct within that
thread; no queue, lock, worker or cross-thread lifetime rule is introduced.

Delivery is non-reentrant and ordered. `SessionController` keeps a FIFO of
complete observations, assigns `sequence` at dequeue/emission time, and completes
the delivery of one event before processing the next observation — for every
event type, including close, error and configuration, not only data. An
observation produced while an event is being delivered (a consumer that writes
from its handler, as the existing emulation does for device-status replies) is
appended to the FIFO and emitted after the current delivery returns; emissions
never nest. With two subscribers, both must observe the same order: for a
consumer that writes one byte in response to an RX event, both subscribers see
`RX` then `TX`, and neither may see a nested `TX` inside its `RX` callback.

## 11. Persistence, replay and network impact

M8 introduces no persistence and no replay. Its events satisfy ADR-006's
future writer input, including source identity and raw source timing. One
active local transport is deliberate; it is not a format limitation. Remote
adapters will supply the same observation contract with source-side timestamps.
Multi-source capture and all cross-domain alignment behavior are explicitly
deferred to the ADR-008 backlog; M8 has neither synchronization protocol nor
mapping estimation/persistence. Passive replay later feeds `SessionEvent` into
the same downstream consumer interface, but is not a transport or writer in this
milestone.

The physical build remains one existing executable and object library. M8
creates only logical seams: its new public session/transport contracts must not
depend on widgets or terminal application internals. A later accepted M13 spec
decides whether and how those seams are extracted into shared library targets.

## 12. Safety

M8 neither opens a transport automatically nor introduces a write API beyond
the existing user-driven serial calls. It cannot replay or simulate traffic.
ADR-007 remains binding for later milestones.

## 13. Testing strategy

Unit tests: origin capture on the first accepted event per clock domain and
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
- one test per operation-table row of §6.2, including the read-back values in the
  metadata and `changedGroups`;
- a `startBits`-only change produces exactly one result with
  `changedGroups: ["compatibility"]`, unchanged hardware and buffering sections,
  no hardware application, and unchanged UI/config behavior of that field;
- the observable mapping of §6.2 for `full`, `partial`, `failed`-with-change and
  `failed`-without-change, in the stated order;
- FIFO order and two-subscriber non-reentrancy (RX then TX for both subscribers);
- activation filtering: an observation carrying an old activation id, delivered
  after a new `opened`, is dropped with exactly one diagnostic and produces no
  event (this test deliberately models a non-conforming or queued-late delivery —
  see the emission-versus-delivery rule in ADR-003); a second test models
  conforming emission order and asserts one event per observation in order;
- a failed `open()` attempt whose error is reported twice by a faulty double
  yields exactly one `Error(kind: "open")`.

`KomportSerial` tests: the accepted-prefix behavior and the one-transaction-per-
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

Regression tests:

- all existing `tst_serial`, emulation, charset and UI tests still pass;
- Hex monitor and text logger retain their current character-signal behavior.

M8 ships no production consumer; the recorder arrives in M9. `tests/` therefore
contains a test-only event collector plus the deterministic double above (never
linked into the shipped executable, no CLI flag, no UI, no persistence). Event
behavior is verified primarily against the double; comparison with the legacy
character signals may only be used in a fixture with no RX overflow and a forced
flush, because the two paths are intentionally divergent. No production code
depends on the collector.

## 14. Acceptance criteria

- [ ] Core types and the transport contract compile against the locally installed
  Qt 6 and are documented against the declared floor of Qt 6.3; "compiles with Qt
  6.3" counts as an acceptance criterion only once a Qt 6.3 build is actually run
  (CI job or release gate — the repository currently has no CI workflow).
- [x] A dedicated compile/object target containing only the public session and
  transport headers and linking `Qt6::Core` (not the existing test targets, which
  link `komport_core` and therefore Widgets) builds, proving the new public
  contracts have no widget dependency.
- [x] Current serial I/O exposes binary chunk observations without changing
  legacy public calls/signals.
- [x] A document-owned controller emits ordered, byte-exact live events.
- [x] Every M8 event carries source ID 1, including a failed-open
  `Error(kind: "open")`; zero is reserved by ADR-002 and unused in M8, and every
  event preserves its source-local timing separately from its derived session
  time (ADR-005).
- [x] New public session/transport value contracts have no QWidget or
  executable-specific type dependency.
- [x] Open, close, error and effective serial configuration are observable as
  non-data events. Compatibility expectations for the authorised orchestration
  change of §3: the settings vocabulary, their persistence and the settings
  dialog's presentation are unchanged; one user action from the settings dialog
  or a profile load produces exactly one configuration transaction and its
  result(s); the synchronous legacy `settingsFailed()` notification on a failed
  open is preserved.
- [x] NUL/all-byte PTY tests pass.
- [x] Existing full `ctest` suite passes.
- [x] No file format, replay UI, decoder or active transmission is introduced.

Abnahme (2026-09-18): the ticked criteria were verified by the implementation
self-review under `docs/reviews/2026-09-18-M8-implementation-selfreview.md`,
including the recorded build and test evidence, and released by the independent
final review in `docs/reviews/2026-09-18-M8-final-implementation-review*.md`
(round 3: "M8 may be declared complete"). The first criterion stays **open**: no
genuine Qt 6.3 build has been run — the repository has no CI workflow and the
local toolchain is Qt 6.11.1.

## 15. Risks

| Risk | Mitigation |
| --- | --- |
| Raw chunk notification subtly changes terminal behavior | It is additive and emitted before the existing buffer/character path; existing tests remain required. |
| `QSerialPort::write()` buffering is mistaken for a physical wire guarantee | Event semantics are "accepted complete write by transport API", not electrical delivery; documented in code/API and in ADR-003. |
| Two event paths drift | New recorder/decoder code is forbidden to consume legacy character signals; the test-only collector compares them only in a no-overflow, fully flushed fixture. |
| Full UI migration expands scope | M8 explicitly preserves compatibility adapters; migration is a later spec. |
| Removing the duplicate settings application changes hidden coupling | The self-connection is documented as the source of the duplicate; a new test asserts one transaction result per operation, and the legacy notification signals keep their behavior. |
| A configuration entry point changes how the UI stages settings | Only the two known call sites migrate; the operation table is normative and tested per row. |

## 16. Decision required

None. The decisions needed for M8 are proposed in ADR-002 through ADR-009.
They must be reviewed and accepted before implementation begins.

## 17. Implementation plan

1. Review and accept ADR-002 through ADR-009 and this spec.
2. Add core value/interface types (including the configuration request type and
   the activation identity) and CMake entries, plus the test-only
   `Qt6::Core` compile target of §14.
3. Implement additive raw-chunk observations and the single configuration
   transaction in `KomportSerial`, with the injectable seams of §13.
4. Implement document-owned `SessionController` with the ADR-005 mapping, the
   activation filter and FIFO delivery.
5. Migrate the two application call sites to the configuration entry point.
6. Add focused unit/controller/PTY tests and run the complete test suite.
7. Perform self-review and an independent review against this specification.
