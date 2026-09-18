# Amendment Package v2.1 — corrections to v2 after review round 2

Date: 2026-09-18
Applies to: `docs/reviews/2026-09-18-M8-foundation-amendments-v2.md`
Supersedes: the v2 passages named in each N-item below (B6-2, B5-1, B6-1,
BS-3 with SPEC §3's non-goal, B3-5, BS-12 with SPEC §9's clamp bullet, BS-1's
guard bullet, B2-2, one sentence of BS-2, BD-1's coverage).
Source: `docs/reviews/2026-09-18-M8-foundation-independent-review-round2.md`
(3 BLOCKER, 4 HIGH, plus one MEDIUM, one DOCUMENTATION and one TEST GAP item).
Gate status unchanged: no status changed, no implementation, ADRs and SPEC-M8
text still untouched.

Round 2's verdict was "not accepted as v2 stands" with a fixed list of causes.
This document is the fix cycle answer to that list, item by item. Everything not
named here is unchanged v2 text.

## N1 (BLOCKER, round-2 item 1) — B6-2 claimed the wrong field widths

Replace the whole B6-2 text with:

"Binary widths in the record are unchanged: `sequence` is `u64`;
`sourceTimestampNs` and `timestampNs` are `i64`; `eventType` is `u16`;
`direction` and `flags` are `u8`; `sourceId` is `u32`; and every length field
(`recordLength`, header byte length, `metadataLength`, `payloadLength`) is `u32`.
The v1 record prefix is 44 bytes. Changing one of these widths is a
format-version change, not an amendment.

The lossless-encoding rule applies only to values that are 64-bit and appear
**in JSON**: `sequence`, the two timestamps, and any future 64-bit quantity such
as an alignment mapping's reference point or offset. Those are written as
canonical decimal strings. 32-bit quantities — `sourceId`, all lengths, event
type, direction, flags — remain JSON numbers, which represent them exactly. A
reader must accept the documented encoding and must never silently truncate a
value. A JSON number must not be assumed to carry a full nanosecond value."

## N2 (BLOCKER, round-2 item 2 / round-1 X1, F1) — one coherent clock-domain model

Replaces the mapping paragraph of B5-1 and the schema sentence of B6-1. Both
documents must use the same model:

**Source descriptors (ADR-006 header, `sources[]`).** Each entry carries a stable
`sourceId` (non-zero, `u32` values in the binary record; written as a JSON number
in the header) **and** a `clockDomainId` string that references an entry in
`clockDomains[]`. A source without a `clockDomainId` is not v1-conformant.

**Clock domains (ADR-006 header, `clockDomains[]`).** Each entry carries:

```text
id                      opaque string, unique within the file
kind                    "process-monotonic" (local) or "agent-monotonic" (future remote)
reference               the pair that defines the session origin for this domain
  sourceTimestampNs     i64 (decimal string in JSON) - raw source-clock value of the anchor event
  sessionTimestampNs    i64, exactly 0 in v1 - the session time of that same event
wallClockCorrelation    optional, informational
  wallClock             ISO-8601 UTC string of the session origin, best effort
  precisionNs           i64 uncertainty of that correlation (decimal string)
```

**Origin rule (ADR-005).** The session origin of a clock domain is the raw source
timestamp of the **first event the controller accepts whose source belongs to
that domain**, and that event carries `sessionTimestampNs = 0`. Once set for a
domain, the reference never moves for the rest of the session: a later
activation, a reopen or a second source in the same domain does not re-anchor it.
For M8 (one source, one process clock) this is: the first accepted event anchors
the domain and has session time zero.

A failed-open `Error` event is a legitimate anchor event: it is a real
observation of that clock domain, its timing is meaningful, and treating it as
the anchor removes the ambiguity of "which event is first" without special cases.

**Mapping (ADR-005).** For every later event of a source in that domain:

```text
timestampNs = max( sessionTimestampNs, sourceTimestampNs - reference.sourceTimestampNs )
```

with `sourceTimestampNs` always retained raw and never rewritten. The `max()`
term is the explicitly documented non-decreasing normalization of N5, not a
correction of stored data. Sources sharing one `clockDomainId` are ordered
directly on that domain's timeline; sources in different domains need a later,
separately specified versioned mapping (offset, scale, uncertainty) and may not
be ordered directly. M8 has exactly one domain.

The rest of ADR-005 (no synchronization exchange, no runtime `TimeMapping`, no
persisted alignment mapping, no alignment UI in M8/M9; stored timing is evidence,
replay timing is policy) is unchanged.

## N3 (BLOCKER, round-2 item 3 + TEST GAP item 10 / round-1 X5, F6, D3a) — configuration transactions

Round 2's objection is that BS-3 promises one transaction result per logical
change while M8's scope forbids changing serial-settings behaviour, and while the
real call sites stage several setters per user action. The code confirms it:
`KomportApp::slotShowPreferences()` (`komport.cpp:1250-1276`) calls
`setDeviceName()` (implicit close/reopen when the name changed), `setFraming()`,
`setFlowControl()` and `setBaudRate()` on an open port — four hardware applies for
one OK click, with a documented false-alarm risk for the intermediate
combinations — while `KomportApp::applyConnectionSettings()`
(`komport.cpp:625-650`) stages the same setters on a closed port and then opens.

**N3-1 — Scope and non-goals (SPEC-M8 §3).** Replace "No change to serial
settings, charset translation, macros or file transfer semantics." with:

"No change to the meaning or set of serial settings: no new setting, no different
hardware encoding of an existing setting, no change to how the settings dialog,
profiles or `QSettings` present them. Charset translation, macros and file
transfer semantics are unchanged. Two narrowly authorised exceptions are part of
M8 because the session contract cannot be satisfied without them: (a) the
duplicate application of an unchanged setting combination performed by `open()`
is removed, so that one configuration request yields one result; (b) a single
configuration entry point is added and both existing application call sites use
it, so that one user action is one configuration transaction. Both are confined
to serial configuration orchestration inside the terminal; neither changes which
settings exist or what values they can take."

**N3-2 — One entry point (SPEC-M8 §6.2).** Add:

"`KomportSerial` exposes one configuration entry point that takes the complete
requested configuration as a value: hardware settings (endpoint, baud rate, data
bits, stop bits, parity, flow control) and local buffering settings (RX queue,
flush rate). It (1) stores all requested values, (2) if the port is open and the
endpoint changed, performs exactly one close/reopen activation, (3) performs
exactly one hardware application while the port is open, and (4) emits exactly
one configuration result. The type is declared next to `KomportSerial`,
QtCore-only, and contains no widget or application type. Both existing call sites
— `KomportApp::applyConnectionSettings()` and `KomportApp::slotShowPreferences()`
— use this entry point in M8 instead of staging individual setters.

The legacy setters (`setDeviceName()`, `setFraming()`, `setFlowControl()`,
`setBaudRate()`, `setRxQueue()`, `setFlushRate()`) keep their current signatures
and behaviour as compatibility adapters: each one that actually reaches the
hardware produces its own configuration result. They are documented as legacy,
not as the way to change a configuration, and the app must not use them for that
purpose after M8."

**N3-3 — Transaction rule (replaces the first paragraph of BS-3).**

"A configuration transaction is exactly one call to that entry point, and it
produces exactly one configuration result, after all requested settings have been
attempted. Its metadata contains the requested hardware settings, the read-back
effective hardware settings (endpoint, baud rate, data bits, parity, stop bits,
flow control as accepted by `QSerialPort`), the local buffering settings, and
`applyStatus` ∈ {`full`, `partial`, `failed`}. If the effective hardware settings
changed, `TransportConfigChanged` is emitted even for `partial`, followed by an
`Error` event where applicable; a `failed` change that altered nothing emits only
the `Error`. No duplicate result is emitted for one call. A transaction is only
possible while the port is open: while it is closed, the entry point stores the
requested values and the following successful open produces the single
open-and-configure transaction."

**N3-4 — Consequence statement (replaces BS-3's D3 paragraph).**

"Removing the duplicate application (D3 option 1) touches exactly one call:
`open()` no longer triggers the self-connected `settingsChanged()` →
`slotSettingsChanged()` → `applyPortSettings()` second application. Consequences:
one redundant hardware application of an identical setting combination is gone,
and at most one duplicate `settingsFailed()` emission per open disappears.
Whether re-applying identical settings is an observable no-op is platform- and
device-dependent; that dependence is part of why the duplicate is removed, and
the removal is therefore covered by the existing serial tests plus one new test
asserting a single configuration result per open. The legacy
`settingsChanged()`/`settingsFailed()` signals keep their present behaviour for
existing consumers and must never drive a hardware application on their own
after M8."

**N3-5 — Complete entry-point list (adds what round 2 found missing).**

"Configuration results are produced by: the single entry point (N3-2); each
legacy setter when it actually reaches an open port — `setBaudRate()` indirectly
through `settingsChanged()`, `setFraming()` and `setFlowControl()` directly;
`setDeviceName()` when its implicit reopen happens; and `open()` itself, whose
result follows its `TransportOpened`. Setters called while the port is closed,
and profile loading that stages values before opening, change requested values
only and produce no result."

**N3-6 — Test seam (round-2 TEST GAP item 10).** Add to SPEC-M8 §13:

"The configuration path exposes a testable result (requested values, read-back
effective values, `applyStatus`) and a small injectable seam — an override point
or test subclass — so that `full`, `partial` and `failed` applications are
reproducible without hardware. Tests prove: one result per entry-point call; one
result for an open that also configures; no result from setters executed while
closed; correct ordering (`TransportOpened`, then the configuration result, then
an `Error` if the application was partial or failed); and read-back values rather
than requested ones in the metadata. The existing serial suite must still pass
unchanged."

## N4 (HIGH, round-2 item 4 / F7, D2) — replay wording contradiction

Replace the whole B3-5 with:

"`SessionController` is the sole adapter from **live transport observations** to
`SessionEvent`: it assigns sequence numbers, associates the controller-owned
source descriptor/ID, maps source time to session time (ADR-005) and emits events
to recorder, views and decoders. Passive replay is not a live transport and does
not use `ITransport`: it is a separately specified read-only event-source/player
interface that delivers immutable stored `SessionEvent` values — including their
stored sequence numbers, source identity, event types, annotations and times —
to views and decoders. `ITransport` describes live byte movement and cannot
reconstruct stored events. The replay interface's exact shape is specified in
M10 and must not be foreclosed here."

B7-1 stays as written in v2 (it already states that passive replay uses neither
`ITransport` nor the live observation path); with B3-5 replaced, the two agree.

## N5 (HIGH, round-2 item 5) — timestamp semantics: exact mapping vs. clamping

Replace BS-12's invariant and SPEC-M8 §9's clamp bullet with one definition
(also referenced by N2):

"Every event retains `sourceTimestampNs` exactly as observed and never rewrites
it. `timestampNs` is the session time of that event,
`max(sessionTimestampNs, sourceTimestampNs − reference.sourceTimestampNs)` for the
event's clock domain (N2), so it is non-negative, zero for the anchor event, and
non-decreasing across the session. If the raw `max()` term would be lower than
the last emitted session time — which contradicts 'one monotonic clock per
domain' in ADR-003 — the event is still emitted with its payload unchanged and
carries the last emitted session time, and the controller records exactly one
anomaly diagnostic (`Error`, `kind: "runtime"`,
`code: "non_monotonic_observation"`, with the raw and normalized values). Byte
evidence is never dropped and raw time is never rewritten; monotonic session
order is an explicitly documented normalization on top of the exact mapping, not
a replacement for it. Clamping is not described anywhere else and is not applied
to `sourceTimestampNs`."

Tests to add to SPEC-M8 §13: "a scripted out-of-order observation produces the
payload unchanged, a session timestamp equal to the previous one, and exactly one
anomaly diagnostic; an in-order sequence produces no diagnostic."

## N6 (HIGH, round-2 item 6 / F9, F10) — open/error correlation without inventing a token

Replace BS-1's failed-open bullet and its activation-generation sentence with:

"- failed open: the transport reports the failure itself, because it knows its
  own attempt. An `ITransport` must report the result of an `open()` attempt
  exactly once: it emits its error observation for that attempt and must not emit
  a second observation for the same failure. The local serial transport
  suppresses the port's own asynchronous error for an attempt whose result it has
  already reported, by an internal in-progress/result guard, never by matching
  human-readable text, while the legacy `settingsFailed()` signal keeps its
  present synchronous behaviour (see C5). The controller emits exactly one
  `Error` with `kind: "open"` for it, does not emit `TransportOpened`, does not
  enter `Live` and does not consume an activation.
- `ITransport` v1 requires `open()` to report its own result synchronously, as
  the local serial transport does. A transport whose open completes
  asynchronously must define its own correlation mechanism and is not part of
  `ITransport` v1.
- an `ITransport` must order its own emissions so that `closed()` is the last
  signal it emits for an activation; a transport that cannot guarantee that is
  not v1-conformant. The controller needs no activation token: it drops every
  observation that arrives while it is not `Live` and records one diagnostic. For
  M8 that also means the delayed legacy characters caused by the retained RX
  buffer (BS-6) are not transport observations at all — if the emulation
  reacts to them and writes, the resulting bytes are genuine observations of the
  current activation."

Tests: "a scripted open failure followed by an asynchronous error for the same
attempt yields exactly one `Error(kind: "open")`; an observation delivered after
`closed()` is dropped with one diagnostic and produces no event; a late
observation from a previous activation cannot appear in a later activation's
stream."

## N7 (HIGH, round-2 item 7 / X3) — validation is structural plus stateful

Replace the enforcement paragraph of B2-2 with:

"Validation has two parts, and the producer must satisfy both before an event
becomes part of the stream:

- Structural validation is event-local and stateless: type/direction
  consistency, the empty/non-empty payload rule, non-zero source ID, non-negative
  timestamps, JSON-encodable metadata. It is implemented once as a free function
  and used by the loader, replay and tests.
- Stream validation is stateful and belongs to the producing controller:
  `sequence` starts at one and strictly increases within the session, session
  time is non-decreasing, and each event's source resolves to its clock domain
  and uses that domain's single reference (N2). `SessionController` is the
  factory for M8's live events and the only place where an event enters the
  stream, so both parts are enforced on emission and are testable through the
  deterministic transport double.

`SessionEvent` itself stays a plain aggregate; a stricter type-level encoding
(private members with accessors) is not required by M8."

## N8 (MEDIUM, round-2 item 8) — chunk boundary wording

Replace "no layer may recombine or re-split stored chunks" in BS-2 with:

"The bytes stored in an event are never rewritten, and an emitted or persisted
`SessionEvent` is immutable. Decoders and views may derive reassembled byte
streams and protocol frames for their own display; such derived framing is a
view of the stored events, must never be written back, and must never replace
what the stored events contain."

## N9 (DOCUMENTATION, round-2 item 9 / F14) — documentation drift, complete list

BD-1 covers only part of the drift. The complete set of places to mark as
superseded (verified by search, not by memory):

- `docs/komport-session-replay-simulation-architecture.md` §5
  (`QVariantMap metadata;`, line 226) → superseded by ADR-002 (`QJsonObject`;
  lossless 64-bit encoding per ADR-006/N1).
- same document §7.2 (suggested magic `KOMPORTSESSION`) → superseded by ADR-006
  (`KPSN 0x1A CR LF NUL`).
- `docs/komport-multiport-sniffer-time-alignment.md` line 121 and line 1117
  (`QVariantMap metadata;`) → superseded by ADR-002.
- same document line 128 (`Source 0:`) and line 323
  (`Source 0 semanticDirection = controller_to_device`) → superseded by ADR-002 /
  ADR-008: physical sources use non-zero ids, zero is reserved, and a semantic
  role is descriptor metadata, never encoded in direction (the concept of the
  example survives; only the numbering changes).
- `docs/komport-multi-executable-product-architecture.md` line 570
  (`QVariantMap metadata;`) → superseded by ADR-002. (This one was missing from
  v2 entirely and is added here.)

Each note is one line: "Superseded by ADR-00x: <what replaces it>." No document is
rewritten or translated.

## N10 — round-2 item 10

Covered by N3-6 (configuration result seam and its tests).

## Status after applying v2 + v2.1

Round 2's acceptance conditions are answered as follows:

| Round-2 condition | Answer |
| --- | --- |
| 1. B6-2's incompatible type/length statements | N1 |
| 2. Source-to-clock-domain/session-origin schema | N2 |
| 3. D3's configuration transaction scope and complete operation contract | N3-1 … N3-6 |
| 4. D2's replay wording contradiction | N4 |
| 5. Timestamp violation semantics | N5 |
| 6. Open/error/activation correlation and stateful event validation | N6, N7 |
| plus MEDIUM chunk wording | N8 |
| plus DOCUMENTATION drift | N9 |
| plus TEST GAP configuration seam | N3-6 |

The blocking items of round 2 are therefore addressed by text, not deferred.
D1 and D2's boundary remain decided as recommended by both rounds; D3(a) is
inside M8 as authorised by N3-1, D3(b) stays a documented legacy behaviour with a
dedicated later change. Round 3 should confirm that no new contradiction was
introduced by N1 … N9, in particular: N2 against N5 (mapping and normalization),
N3 against SPEC-M8 §3 (scope) and §14 (acceptance criteria), N6 against C5 and
the existing `tests/tst_profileerror.cpp` expectation, and N7 against B2-2's
"plain aggregate" statement.
