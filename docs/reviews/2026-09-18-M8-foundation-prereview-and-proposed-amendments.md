# Pre-Review and Proposed Amendments: ADR-002 … ADR-009 and SPEC-M8

Date: 2026-09-18
Author role: implementation agent (spec author) — Claude-side pre-review
Gate status: **not accepted.** ADR-002 … ADR-009 are `Proposed`, SPEC-M8 is
`Draft — implementation blocked pending ADR review`. No status was changed by
this document, and no production code exists for M8.

## 0. Purpose and how to use this document

This is the spec-author's own review of the M8 foundation package against the
review checklist in `docs/komport-engineering-governance-spec-review-workflow.md`
section 8, plus proposed textual amendments for every finding. It is **not** the
independent review required by section 12 (Step 8 – Independent Codex Review).

The intended use is:

1. Codex reviews the package: ADR-002 … ADR-009, SPEC-M8, and the proposed
   amendments in this document.
2. Codex either approves the amendments, rejects them, or replaces them with
   better ones, and reports its own findings.
3. Only then are the amendments applied to the ADRs/spec, the documents are
   marked `Accepted`, and M8 implementation begins (SPEC-M8 section 17 step 2).

Findings F1–F6 are considered blocking before acceptance: they fix contracts
that section 20 (interface freeze) would otherwise lock in. F7–F16 should be
resolved in the same textual pass but do not individually block M8.

Nothing in this document proposes implementation, file format, UI, replay,
network, or decoder work beyond M8's declared scope.

## 1. Reviewed material and method

Reviewed documents:

- `docs/architecture-decisions/ADR-002-session-event-v1.md` … `ADR-009-*`
- `docs/specs/SPEC-M8-session-transport-foundation.md`
- `docs/komport-engineering-governance-spec-review-workflow.md` (sections 3, 4,
  7–10, 12–14, 16, 20, 23, 26, 27)
- referenced sections of
  `docs/komport-session-replay-simulation-architecture.md` (§5–§8, §20–§23,
  §29, §39, §42) and
  `docs/komport-multiport-sniffer-time-alignment.md` (§3, §11, §19, §23)

Reviewed code (to validate the spec's "Current State" and proposed design):

- `komport/komportserial.h`, `komport/komportserial.cpp`
- `komport/komportdoc.h`
- all callers of `putChar()` / `putStr()` / `receivedChar` / `sentChar`:
  `komport/komport.cpp:89-91,1372-1386`, `komport/komportemulation.cpp:655,705-736,1271-1292,1584`,
  `komport/komporttransfer.cpp:56,96,122`, `komport/komportview.cpp:92`
- `tests/tst_serial.cpp`, `tests/CMakeLists.txt`, `.github/` (CI presence)

Checklist verdict (section 8), one line each:

| Checklist question | Verdict |
| --- | --- |
| Does this fit the architecture? | Yes, except the clock/session definitions (F1, F3). |
| Does it create a new side path? | Yes, deliberately (legacy character path + event path). Acceptable, but the "new code must not consume legacy signals" rule needs an explicit invariant and a test (F11, F16). |
| Does it duplicate an existing abstraction? | No. The configuration observation does overlap unclearly with the existing `settingsChanged`/`settingsFailed` path (F6). |
| Does it close future extension paths? | Partly: replay seam (F7), TX entry points (F2), partial-write API (F4). |
| Does it preserve byte-exact data? | Intent yes; rules for partial writes (F4), byte-wise TX (F5) and the intended display/recorder divergence (F11) are missing. |
| Does it preserve timing/direction semantics? | Direction is consistent (ADR-004 vs section 16.7). Timing has a real contradiction (F1). |
| Does it remain transport-neutral? | Yes by inspection (QtCore-only types); no mechanical proof (F13b). |
| Does it remain decoder-neutral? | Not touched by M8; nothing foreclosed. |
| Are failure modes defined? | Partly (F4, F6, F9, F10). |
| Are tests sufficient? | Reasonable skeleton; F5 and F12 gaps, F13 makes one acceptance criterion unverifiable. |

Reference integrity was checked mechanically: every ADR/doc section reference
resolves (`session-replay-simulation-architecture.md` §5–§8, §20–§23, §29, §39,
§42 and `engineering-governance` §16.x exist), and the record type widths in
ADR-006 match ADR-002 (`u16` event type, `u8` direction, `u64` sequence, `u32`
source id, `i64` timestamps). No ADR contradicts §16.1–§16.10 except through the
timing wording in F1.

## 2. F1 — High — Clock origin and clock domain are contradictory and unanchored

**Evidence.** `ADR-003` says "Timestamps are monotonic and relative to a single
transport activation", while `SPEC-M8` §6.1/§8 says the session timestamp is
"derived from the same local common monotonic clock" and the controller
"preserves the monotonic source timestamp". Both readings cannot hold at once:
either the transport reports activation-relative time and the controller applies
a non-identity offset (contradicting the spec's "same clock domain"), or the
timestamps are already process-relative and ADR-003's wording is wrong.

Neither ADR-005 nor ADR-006 defines the zero point of that clock. The source
architecture document offers only `"capture": {"clock": "monotonic",
"timestampResolution": "ns"}` (`komport-session-replay-simulation-architecture.md`
§7.3). A persisted timestamp without an origin anchor cannot be converted to
absolute time later, and ADR-008's future alignment mapping ("a source/session
reference point, offset, scale and uncertainty") has no defined reference point
to anchor against.

**Proposed amendment A1.1 — ADR-003, replace the timestamp paragraph.**

Current: "Timestamps are monotonic and relative to a single transport
activation. The source that observes bytes creates them: local `SerialTransport`
on its Qt thread, a future remote adapter at the remote agent. `ITransport`
neither stores sessions nor decodes bytes."

Proposed: "Timestamps are non-negative monotonic nanoseconds in the observing
source's clock domain. A source must use one clock instance whose zero point
does not move for the lifetime of the process or agent — never a per-activation
zero: the local serial transport uses the process-wide monotonic clock defined
in ADR-005, a future remote adapter uses its own agent-wide monotonic clock.
Because the clock domain, not the activation, defines the zero point,
observations taken before and after a transport activation on the same source
remain comparable, and a later alignment mapping has a stable reference point.
The source that observes bytes creates the timestamps: local `SerialTransport`
on its Qt thread, a future remote adapter at the remote agent. `ITransport`
neither stores sessions nor decodes bytes."

**Proposed amendment A1.2 — ADR-005, add a decision paragraph.**

Add after the paragraph that begins "M8 contains exactly one active local
transport source.":

"For M8's single local source, `sourceTimestampNs` and `timestampNs` are the
same value of one process-wide monotonic clock; the source-to-session mapping is
the documented identity mapping, not a per-activation offset. The clock's zero
point is the start of its owning process/agent, which means a persisted session
can only be interpreted if the writer records the wall-clock instant that
corresponds to that zero point (ADR-006). Any writer of a persisted session must
record this anchor per clock domain; a timestamp that cannot be related to
absolute time must not be written as if it could. This obligation is a
requirement on the format, implemented when the format is implemented (M9), not
a clock-synchronisation feature."

**Proposed amendment A1.3 — ADR-006, extend the header requirement.**

Current: "The JSON header contains format/version, wall-clock creation time,
application version, an array of source descriptors/configuration snapshots,
capture clock domains, optional decoder hints and notes."

Proposed: "The JSON header contains format/version, wall-clock creation time,
application version, an array of source descriptors/configuration snapshots,
capture clock domains with an explicit origin anchor per domain, optional
decoder hints and notes. Each clock-domain entry records the clock's
identity/source (for example boot- or process-relative monotonic clock) and the
wall-clock instant corresponding to that clock's zero point, so that every
persisted `sourceTimestampNs` can be converted to an absolute time and can serve
as the reference point of a later, separately specified alignment mapping
(ADR-008). A clock domain without such an anchor is not v1-conformant."

**Proposed amendment A1.4 — SPEC-M8, wording and tests.**

- §6.1: replace "derives the session timestamp in the same local clock domain"
  with "derives the session timestamp from the same process-wide monotonic clock
  (ADR-005), so for M8's single source `timestampNs` equals the preserved
  `sourceTimestampNs` (identity mapping)".
- §7: state model, "establish the source-time to session-time mapping" becomes
  "establish the identity source-time to session-time mapping (ADR-005)".
- §8 invariants: replace "has a session timestamp derived from the same local
  common monotonic clock" with "has a session timestamp derived from the same
  process-wide monotonic clock, which for M8 is the identity mapping
  (`timestampNs == sourceTimestampNs`)".
- §13 unit tests: replace "source-time preservation, timestamp normalization"
  with "source-time preservation, identity mapping of M8
  (`timestampNs == sourceTimestampNs`), and non-decreasing behaviour of the
  process clock across repeated observations".

## 3. F2 — High — The TX observation must cover every write entry point

**Evidence.** `SPEC-M8` §6.2 and §13 describe the TX observation in terms of a
successful `QSerialPort::write()`. In the current code the byte-transmitting
entry points are:

- `KomportSerial::putChar(char)` (`komportserial.cpp:172`), used by
  `KomportEmulation` for escape sequences (`komportemulation.cpp:705-714,736`),
  device-status/attribute replies (`:1271,1277,1292`), key input (`:1584`) and
  by `KomportTransfer::upload()` for every uploaded byte (`komporttransfer.cpp:56`),
- `KomportSerial::putStr(const char*)` (`:184`), used for macro text and line
  endings (`komport.cpp:1381,1386`) and emulation replies
  (`komportemulation.cpp:720,1277,1292`),
- `KomportSerial::putStr(const char*, qsizetype)` (`:190`),
- the new `ITransport::writeBytes()`.

If the observation lives only in the new method, the entire UI, macro,
emulation and transfer TX traffic — the traffic a recording exists to capture —
never becomes an event. A related trap: `KomportEmulation` sends one logical
key press as several calls (`putChar(ESC)` then `putStr("[1~")`); the spec must
state that this produces several events and that this is intended, so that no
later change "fixes" it by coalescing, which would violate chunk preservation
(architecture document §6).

**Proposed amendment A2.1 — SPEC-M8 §6.2, replace the TX paragraph.**

"`KomportSerial` emits exactly one TX observation per accepted write call, for
every byte-transmitting entry point it exposes: the legacy `putChar(char)`, both
`putStr()` overloads and the new `ITransport::writeBytes()`. All of them are
implemented on one internal write helper; that helper — not each public method —
creates the observation. UI keystrokes, emulation replies, macro output, line
endings, file-transfer bytes and future `writeBytes()` callers therefore reach
the session stream through the same rule.

One write call produces exactly one observation, even when the call covers a
whole escape sequence, and separate calls are never coalesced into one event.
A single logical action can therefore appear as several TX events in the
recording: `KomportEmulation` sends cursor/insert/delete keys as
`putChar(ESC)` followed by one or more `putStr()` calls, and each of those calls
is one chunk on the wire. This is intended and matches the chunk-preservation
rule of the architecture document; recorder, replay and decoder code must not
recombine them."

**Proposed amendment A2.2 — SPEC-M8 §13 tests, add one TX test per entry point.**

- "each TX entry point (`putChar`, null-terminated `putStr`, length-aware
  `putStr`, `writeBytes`) produces exactly one byte-identical TX event per
  accepted write; a multi-call key sequence such as `ESC` + `[1~` produces two
  ordered TX events, not one;"

**Proposed amendment A2.3 — ADR-003, make the observation obligation explicit.**

Add after "`KomportSerial` is migrated incrementally into the local serial
implementation and may keep its legacy character methods/signals during the
transition.":

"Every byte-transmitting entry point of an `ITransport` implementation —
including any legacy compatibility method it keeps — must produce the same TX
observation. A compatibility method that writes bytes without producing an
observation would create exactly the bypass this ADR forbids."

## 4. F3 — High — Sequence reset per transport activation contradicts "one session"

**Evidence.** `SPEC-M8` §7 resets the sequence to one whenever the transport is
opened. In the current code, a normal user action can close and reopen the
transport at runtime: `KomportSerial::open()` calls `close()` first
(`komportserial.cpp:64-73`), and `setDeviceName()` closes and reopens the port
implicitly when the device name changes (`:48-55`). ADR-006's header has one
`created` value, one source array and no activation/generation field, so a
`.kpsession` containing two sequence generations would be undefined, while
ADR-002 requires the sequence to be "strictly increasing within one session".
Over a longer session (port off/on cycles, profile switching), M9's file would
be unreadable by its own contract.

**Proposed amendment A3.1 — SPEC-M8 §7, replace the state model.**

"`SessionController` has `Idle`, `Live`, `Closed`.

- construction: `Idle`.
- transport opened successfully: emit exactly one `TransportOpened` non-data
  event and enter (or re-enter) `Live`. `sequence` continues to increase
  monotonically for the whole lifetime of the controller and is never reset by a
  transport activation.
- observations in `Live`: emit ordered events.
- transport closed: emit exactly one `TransportClosed` non-data event and return
  to `Idle`. The sequence does not restart; a later successful open continues it.
- destruction: no further events. `Closed` denotes the end of the session, not
  the end of one activation.

A failed open produces an `Error` observation and does not enter `Live`; it does
not count as an activation.

Rationale: activations repeat within a single application session (reconnect,
device change, profile switch), so a per-activation reset would collide with
ADR-002 and would make an append-only `.kpsession` ambiguous. Should a
per-activation reset ever be wanted, ADR-002 and ADR-006 must first introduce a
session-generation field and the reader must accept repeated sequence ranges;
that is deliberately not part of M8."

**Proposed amendment A3.2 — ADR-002, one clarifying sentence.**

Current: "`sequence` is strictly increasing within one session and begins at
one."

Proposed: "`sequence` is strictly increasing within one session and begins at
one. A session may contain several transport activations (open, close, reopen of
the same source); `TransportOpened` and `TransportClosed` events mark them, and
`sequence` continues across them."

**Proposed amendment A3.3 — SPEC-M8 §13 tests.**

Replace "sequence reset on a new live session and strict increment thereafter"
with "sequence begins at one for a new controller, strictly increments across
transport activations, and is not reset by close/reopen".

## 5. F4 — High — A partial write loses accepted bytes from the event stream

**Evidence.** `SPEC-M8` §6.2/§9 require that a partial or failed write emits no
successful TX data event. But `QIODevice::write()` returning fewer bytes than
requested means those bytes were accepted and queued by the transport; and the
current legacy path already reports them: `komportserial.cpp:206-208` emits
`sentChar()` for exactly the accepted prefix. The two observation paths would
then disagree about the same wire data — the hex monitor showing bytes that the
recording does not contain. `ITransport::writeBytes()` returning `bool` also
cannot express how many bytes were accepted.

**Proposed amendment A4.1 — ADR-003, change the write signature and document it.**

Current: `virtual bool writeBytes(const QByteArray &bytes) = 0;`

Proposed:

```cpp
    // Sends bytes. Returns the number of bytes the transport accepted
    // (0..bytes.size()); a negative value means the transport refused the
    // write entirely. Callers must not treat a short return as success.
    virtual qint64 writeBytes(const QByteArray &bytes) = 0;
```

And add to the signal list description: "`bytesWritten` always carries exactly
the bytes the transport accepted — for a partially accepted write that is the
accepted prefix, never the requested length."

`KomportSerial` keeps its `bool` compatibility methods (`putChar`, both
`putStr` overloads) and returns `true` only when all requested bytes were
accepted, so existing callers keep their current behaviour.

**Proposed amendment A4.2 — SPEC-M8 §6.2, replace the TX success rule.**

Current: "It emits exactly one TX observation only after a complete successful
`QSerialPort::write()` call; partial/failed writes remain failures and emit no
successful TX data event."

Proposed: "It emits exactly one TX observation for the bytes a write call
actually accepted: a fully accepted write produces one event covering all
requested bytes; a partially accepted write produces one event covering the
accepted prefix, and the call still returns failure with the existing warning;
a write that accepts nothing produces no data event at all. An event payload
therefore always equals bytes the transport accepted and can never contain bytes
it refused — and the stream never silently omits bytes that did reach the
transport, which is also what the legacy `sentChar()` path already reports."

**Proposed amendment A4.3 — SPEC-M8 §8 and §9.**

§8, add: "A `Tx` data event covers exactly the bytes the transport accepted; a
refused remainder appears in no event."

§9, replace "Partial/failed serial writes keep current `false` return and
warning; no success TX event is emitted." with "Partial/failed serial writes
keep their current `false` return and warning. The accepted prefix is still
reported as one `Tx` data event (see §6.2); the refused remainder is not
reported as data and should be visible as an `Error` non-data event when it is
non-zero."

**Proposed amendment A4.4 — SPEC-M8 §13 tests.**

Replace "failed/partial writes produce no successful TX event" with "a partial
write produces exactly one TX event for the accepted prefix and no event for the
refused remainder; a write that accepts nothing produces no data event".

## 6. F5 — Medium/High — Byte-wise TX makes one event out of every byte

**Evidence.** `KomportTransfer::upload()` sends the file one byte per
`putChar()` call (`komporttransfer.cpp:47-75`, with `processEvents()` and a
250 ms pause after `\n`). Under "one observation = one event", a 1 MiB transfer
becomes ~1 M events; each event carries a heap-allocated `QByteArray` and a
`QJsonObject` member, and ADR-006's record prefix adds ~30 bytes per event. The
resulting memory footprint (~10^2 bytes per event) and, from M9 on, file size
(~30–40x the payload) are a deliberate consequence that should be recorded
before a recorder exists.

**Proposed amendment A5.1 — SPEC-M8 §11 (or §16), add an accepted-consequence
paragraph.**

"Accepted for M8: `KomportTransfer::upload()` writes one byte per call, so a
file upload produces one `Tx` event per byte, and `processEvents()`/`msleep()`
pacing (including the 250 ms line pause) is preserved in the event timestamps.
M8 does not change the transfer path: the event stream reports what the
transport observed and the pacing is protocol-relevant. M9 must therefore
specify a streaming and size policy for session files — an upload legitimately
produces 10^5–10^6 events, and neither a memory-resident event vector nor a
naive per-event record may turn that into a surprise. Batching the upload onto
the length-aware write helper changes transfer pacing and requires its own spec;
it is explicitly not part of M8."

**Proposed amendment A5.2 — ADR-002/ADR-006, empty metadata.**

ADR-002, add: "`metadata` is empty for `Data` events in M8; writers must encode
empty metadata as a zero-length field (ADR-006 `metadataLength`), and readers
must treat it as an empty object."

**Proposed amendment A5.3 — SPEC-M8 §13, add a volume test.**

"- a volume/pathology test: a byte-wise producer (64 KiB written through
`putChar()`, as `KomportTransfer::upload()` does) yields exactly one TX event per
accepted write with correct payload bytes; the test asserts the event count
explicitly, so the per-byte chunking cost is documented before M9 persists it."

## 7. F6 — Medium/High — "Successfully applied settings" is not derivable today

**Evidence.** `SPEC-M8` §6.2 wants a configuration observation after a
successfully applied settings change, with the effective serial settings. The
current code cannot report that reliably:

- `open()` emits `settingsChanged()` unconditionally, including when opening
  failed (`komportserial.cpp:67-72`);
- `slotSettingsChanged()` applies settings only when the port is open, and
  `applyPortSettings()` (`:115-169`) returns `void`, signalling failure only by
  emitting `settingsFailed()`;
- `setFraming()` (`:289`) and `setFlowControl()` (`:299`) call
  `applyPortSettings()` directly and therefore never emit `settingsChanged()`;
- the device path in metadata would come from the requested name
  (`mDeviceName`), not from what the port actually opened.

**Proposed amendment A6.1 — SPEC-M8 §6.2, replace the configuration paragraph.**

"While live, `KomportSerial` emits exactly one configuration observation after a
settings change that was applied to the open port without error. The
observation is derived from the values `QSerialPort` actually accepted (read
back from the port: port name, baud rate, data bits, parity, stop bits, flow
control) and never from requested values, so a partially applied or failed
change can never be reported as an effective configuration. M8 routes every
settings-application path through one helper that reports success or failure —
`open()`, `slotSettingsChanged()`, `setFraming()` and `setFlowControl()` — so that
profile changes, flow-control changes and direct serial calls are observable
through the same rule. The existing `settingsChanged()`/`settingsFailed()`
signals keep their present behaviour for existing consumers and are not the
source of this observation, because `settingsChanged()` is also emitted on a
failed open and is not emitted at all by `setFraming()`/`setFlowControl()`.
Metadata captures only the effective serial settings and the endpoint; it never
contains credentials, and an unsuccessful change emits no configuration
observation."

**Proposed amendment A6.2 — SPEC-M8 §13 tests.**

"- a settings change applied through each path (`open()`, `slotSettingsChanged()`,
`setFraming()`, `setFlowControl()`) produces exactly one configuration event
carrying read-back values;
- a settings change that fails to apply produces no configuration event;
- a failed open produces no configuration event."

**Proposed amendment A6.3 — SPEC-M8 §14 acceptance criteria.**

Replace "Open, close, error and effective serial configuration are observable as
non-data events without changing their existing UI behavior." with "Open, close,
error and effective serial configuration are observable as non-data events,
including configuration applied through `setFraming()`/`setFlowControl()`, with
the metadata carrying values read back from the port; existing UI behavior is
unchanged."

## 8. F7–F16 — Remaining findings and proposed amendments

F7–F16 are not individually blocking, but they are cheap textual fixes and
should be resolved in the same pass so the frozen contracts do not need
reshaping later.

**F7 (Medium) — Passive replay vs. `ITransport`.** ADR-003 declares
`SessionController` the sole adapter from transport signals while ADR-007 has
passive replay emitting through the controller "with no `ITransport` output
target"; it is undefined whether a replay reader is a read-only `ITransport` or
needs a second input seam. Amendment A7.1 (ADR-003, after "No new feature may
bypass `ITransport` or `SessionController`."):

"A future passive replay reader is expected to implement `ITransport` in
read-only form: `open()` opens the session file, `writeBytes()` refuses every
write (fails closed to zero/negative and emits no `bytesWritten`), and
`bytesReceived` delivers the stored chunks with their stored source timestamps.
'No `ITransport` output target' (ADR-007) then means 'no writable peer', not 'no
`ITransport`'. If M10's own spec decides against a read-only transport,
`SessionController` needs an explicitly specified second, event-injection input
seam; that decision belongs to M10, but the option must not be foreclosed here."

**F8 (Medium) — Delivery is not defined as non-reentrant.** SPEC-M8 §10 covers
threads only; a consumer that writes from its event handler can nest
observations. The pattern already exists in this codebase on the legacy path:
`KomportEmulation` writes escape-sequence replies from inside the
character-signal call chain (`komportemulation.cpp:1271-1292`, reached via
`komportemulation.cpp:655` with the serial port as its write target).
Amendment A8.1 (SPEC-M8 §10, add):

"Delivery is non-reentrant. `SessionController` completes the delivery of one
event before processing the next observation; an observation produced while an
event is being delivered is queued in observation order and emitted after the
current delivery returns. Emissions therefore never nest, and timeline order
remains the order in which the transport observed bytes. A consumer must not
assume that a write it performs inside an event handler has produced its TX
event before that handler returns."

Test (add to §13): "a consumer that writes one byte from inside its event
handler sees no nested emission and yields exactly two ordered events (RX then
TX)."

**F9 (Medium) — Error events are undifferentiated and open-failure ordering is
timing-dependent.** The error path is one `settingsFailed(QString)` for apply
failures and runtime port errors, plus the asynchronous `errorOccurred` slot,
while the spec requires structured non-secret metadata and "a failed open does
not enter `Live`". Amendments:

- SPEC-M8 §6.2 (A9.1): "open failures are emitted deterministically from the
  `open()` return path using the port's reported error, not only from the
  asynchronous `errorOccurred` slot, so that the state transition is
  platform-independent; if `QSerialPort` also delivers the same failure
  asynchronously, no second event is emitted for it."
- SPEC-M8 §9 (A9.2): "An `Error` event's metadata carries at least `kind` with
  one of `open`, `apply`, `runtime`, and a human-readable `message`; no
  credentials and no decoded payload data. A refused TX remainder (see §6.2) is
  reported through `kind: runtime` with the byte count."

**F10 (Medium) — Behaviour outside `Live` is undefined.** Amendment A10.1
(SPEC-M8 §7, after the state list): "Observations that arrive while the
controller is not `Live` — after `TransportClosed`, or from a transport that
never opened — are dropped and logged as a transport/controller anomaly; no
event is emitted for them, and `TransportClosed` remains the last event of its
activation. Bytes still buffered in the legacy RX buffer when the port closes
are not a session event: they were observed when they were read."

**F11 (Medium) — The intended recorder/display divergence is undocumented.**
`komportserial.cpp:240-244` trims the RX buffer to `setRxQueue()`, the legacy
path flushes at `setFlushRate()` cadence and buffered bytes are dropped on
close, while the event stream would be loss-free. Amendment A11.1 (SPEC-M8 §8,
add):

"The event stream is loss-free with respect to transport observations, while the
legacy character path remains a lossy display adapter: `KomportSerial` still
trims its RX buffer to `setRxQueue()`, still emits `receivedChar()` at
`setFlushRate()` cadence, and still drops buffered bytes when the port closes.
This divergence is intended and load-bearing: the event stream is wire truth,
the character path is what the user sees. Neither `RxQueue` nor `FlushRate` may
influence which events are produced."

Tests (§13, add): "an RX test with `setRxQueue(1)` and `setFlushRate()` at both
extremes produces the same event stream as with default values, proving events
are not derived from flushed characters; the existing `receivedChar`/`sentChar`
counts are unchanged by the new path (hex monitor and text logger regression)."

**F12 (Medium) — PTY chunk assertions would be flaky.** SPEC-M8 §13's "one
byte-identical RX event per observed read chunk" cannot be asserted
deterministically over a PTY, and `tests/tst_serial.cpp` deliberately avoids
such claims today. Amendment A12.1 (replace that PTY bullet):

"- RX data with embedded NUL and values spanning `00..FF` produces byte-identical
  RX events: the concatenation of all RX payloads equals the sent byte stream,
  each event corresponds to exactly one non-empty transport read, an empty read
  produces no event, and no event is empty. Tests must not assert a fixed number
  or split of chunks (PTY and kernel buffering make chunking timing-dependent)
  and must not depend on the legacy 250 ms flush timer."

**F13 (Low/Medium) — Two acceptance criteria are unverifiable as written.**
Amendment A13.1 (SPEC-M8 §14):

- Replace "Core types and transport contract compile with Qt 6.3." with "Core
  types and transport contract compile against the project's declared Qt 6
  minimum (`CMakeLists.txt`: Qt 6.3) and against the locally installed Qt 6
  version; the minimum is a documented floor, not a CI-verified claim — the
  repository has no CI workflow (`.github/` contains templates only)."
- Add: "A test target that includes only the new session/transport headers and
  links `Qt6::Core` (not `Qt6::Widgets`) compiles, proving the new public
  contracts have no widget dependency."

**F14 (Low) — Documentation drift in the architecture templates.** The
superseded suggestions should be marked so no implementer follows them.
Amendment A14.1:

- `docs/komport-session-replay-simulation-architecture.md` §5: mark the
  `QVariantMap metadata` member as superseded by ADR-002's `QJsonObject`.
- Same file §7.2: mark the suggested magic `KOMPORTSESSION` as superseded by
  ADR-006's `KPSN 0x1A CR LF NUL`.
- `docs/komport-multiport-sniffer-time-alignment.md` §3: mark the example
  "Source 0" numbering as superseded by ADR-002 (physical sources use non-zero
  ids; zero is reserved for future session-wide non-data events).

**F15 (Low) — Precision in SPEC-M8 §8.** Amendment A15.1: replace "Every event
has source ID 1" with "Every M8 event carries source ID 1 (ADR-002 reserves
zero for future session-wide non-data events; it is unused in M8)".

**F16 (Medium) — M8 ships no consumer, so the contract is only exercised by
tests.** Amendment A16.1 (SPEC-M8 §13, add):

"M8 ships no production consumer of the event stream; the recorder arrives in
M9. To keep the contract under test in M8 rather than first exercised in M9,
`tests/` contains a test-only consumer (not part of the shipped executable, no
CLI flag, no UI, no persistence) that records every event and cross-checks the
event payloads against the legacy character signals. No production code may
depend on it."

## 9. Requested outcome of the independent review

The independent review should state, for each of F1–F16:

1. is the finding valid as stated, and is the evidence correct;
2. is the proposed amendment the right resolution, or is there a better one;
3. what additional findings the reviewer sees that this document missed —
   especially contradictions between ADR-002 … ADR-009 themselves, foreclosed
   future extension paths, or byte/timing guarantees that cannot hold;
4. whether the amended package can be accepted as the frozen M8 contract, or
   which items must change first.

Explicitly not in scope for this review: M9 and beyond, `.kpsession` reader or
writer implementation, replay, decoders, network, and any UI change.
