# Amendment Package v2.2 — corrections after review round 3

Date: 2026-09-18
Applies to: `docs/reviews/2026-09-18-M8-foundation-amendments-v2.md` (v2) and
`docs/reviews/2026-09-18-M8-foundation-amendments-v2.1.md` (v2.1).
Supersedes: v2.1 items N2's mapping formula, N3 (N3-2/N3-3/N3-5/N3-6 wording),
N6's correlation contract, plus v2's B2-3 and the residual texts listed below.
Source: `docs/reviews/2026-09-18-M8-foundation-independent-review-round3.md`
(2 BLOCKER, 3 HIGH, 1 TEST GAP, 1 DOCUMENTATION).
Gate status unchanged: no ADR/SPEC text changed, no status changed, no code.

Round 3 confirmed N7, N8 and N9 (as an instruction) as resolved, and N1, N3, N4,
N5, N6, N10 as partially resolved. This document closes those partial
resolutions. Everything not named here stays as in v2/v2.1.

## P1 (BLOCKER, round-3 item 1) — one unambiguous timestamp formula

Round 3 is right: v2.1 used `sessionTimestampNs` for two different things — the
reference field (definitionally zero in v1) and a runtime "last emitted" value.
Replace the mapping definition of N2 and the invariant of N5 with:

**Reference (ADR-006 header, per clock domain).**

```text
reference.sourceTimestampNs       i64 - raw source-clock value of the anchor event
reference.sessionTimestampNs      i64 - session time of that same anchor event; exactly 0 in v1
```

**Controller state (ADR-005).** Per clock domain the controller keeps
`lastEmittedSessionTimestampNs`. It is initialized to
`reference.sessionTimestampNs` (i.e. 0) in the moment the domain's reference is
set — when the controller accepts the first event whose source belongs to that
domain, which is that domain's anchor event — and it is updated to the emitted
`timestampNs` after every emitted event of that domain.

**Mapping (ADR-005), for every event of a source in that domain.**

```text
rawSessionTimestampNs    = sourceTimestampNs - reference.sourceTimestampNs
timestampNs              = max(lastEmittedSessionTimestampNs, rawSessionTimestampNs)
lastEmittedSessionTimestampNs = timestampNs        (after emission)
```

Consequences, which the tests assert: the anchor event has
`rawSessionTimestampNs == 0` and therefore `timestampNs == 0`;
`timestampNs` is non-negative and non-decreasing per domain; `sourceTimestampNs`
is stored raw and never rewritten. A null/zero-length payload is unaffected by
this rule; the payload is never modified or dropped by it.

**Stale wording to replace (round-3 item 1, second half).** SPEC-M8 §13's unit
test line "source-time preservation, timestamp normalization and non-decreasing
aligned-time clamp" becomes "source-time preservation, the exact mapping of P1
including `timestampNs == 0` for the anchor event, and non-decreasing session
time with exactly one anomaly diagnostic per violation". The word "clamp" is
removed everywhere: SPEC §9's bullet becomes the P1 anomaly rule of N5, and no
other place describes clamping. The private term for the runtime value is
`lastEmittedSessionTimestampNs` only; `reference.sessionTimestampNs` is never
used as a synonym for it.

## P2 (BLOCKER, round-3 item 2) — one configuration operation table

Round 3 found three internal contradictions in N3 (per-call vs. deferred result;
baud through `settingsChanged()` vs. "legacy signals must not drive hardware";
open-ordering applied to a live reconfiguration). Replace N3-2, N3-3, N3-5 and
N3-6 with the following, which is the complete contract.

**P2-1 — Resolution of the hidden coupling.** The self-connection
`settingsChanged() → slotSettingsChanged()` is removed as part of D3(a). After
this, `settingsChanged()` and `settingsFailed()` are pure notification signals
for existing consumers and never cause a hardware application. Every legacy
setter applies through the shared transaction routine exactly once while the
port is open, then emits its compatibility signal. This is what makes "one
result per logical change" and "legacy signals never drive hardware" compatible.

**P2-2 — Operation table.** One row per operation, one result per row, always
emitted by the transaction routine (never by a setter, never by a signal):

| Operation | Precondition | Activation events | Hardware applied | Configuration result |
| --- | --- | --- | --- | --- |
| `configure(request)` | port open | none (endpoint unchanged) | once, all groups in the request | one result at the current position in the stream |
| `configure(request)` | endpoint changed while open | `TransportClosed` (old activation), `TransportOpened` (new activation) | once, after the reopen | one result immediately after `TransportOpened` |
| `configure(request)` | port closed | none | none (values stored only) | none; the next `open()` produces it (next row) |
| `open()` | port closed, values stored | `TransportOpened` | once, open-and-configure | one result immediately after `TransportOpened` |
| `open()` | port already open | `TransportClosed` (old activation), `TransportOpened` (new activation) | once, open-and-configure after the reopen | one result immediately after `TransportOpened` |
| legacy setter (each of the six) | port open | as its effect requires (`setDeviceName()` may close/reopen) | once, through the transaction routine | one result |
| legacy setter (each of the six) | port closed | none | none (value stored only) | none |
| `close()` | port open | `TransportClosed` | none | none |

`open()`'s documented behaviour of closing an already-open port first stays
unchanged: in that case it is a real activation boundary, and it therefore
produces the closing event, the opening event and exactly one configuration
result. M8 authorises no change to this, only the removal of the duplicate
application (P2-1). The entry point of P2-4 replaces both the staged setters and
the conditional `if (!serial->isOpen()) serial->open()` at the two application
call sites, so no redundant reopen happens per user action.

**P2-3 — Result content and ordering.** A result's metadata contains the
requested hardware settings, the read-back effective hardware settings, the local
buffering settings and the groups whose values actually changed (for example
`changedGroups: ["hardware"]`). `applyStatus` is `full`, `partial` or `failed`.
If the effective hardware settings changed, `TransportConfigChanged` is emitted
even for `partial`, followed by an `Error` when the application was partial or
failed; a `failed` application that changed nothing emits only the `Error`. A
change that affects only the local buffering settings (RX queue, flush rate)
still produces one result with `changedGroups: ["localBuffering"]` and an
unchanged hardware section — this is deliberate, because those values explain
byte loss in the legacy display path (BS-6) and belong in the session record.
`TransportOpened`/`TransportClosed` are never withheld from the session stream to
make a result fit: open-and-configure always emits `TransportOpened` first, a
live reconfiguration never emits activation events at all.

**P2-4 — One entry point, complete request.** N3-2's entry point is kept, with
one clarification: it stores the complete requested configuration and performs
the table's operation. "One call, one result" applies to calls that reach the
hardware; a call on a closed port stores values and produces no result, and the
accompanying `open()` produces exactly one. Both application call sites
(`komport.cpp:625-650`, `:1250-1276`) use the entry point in M8, so one user
action produces exactly one result. The legacy setters remain for compatibility
and appear in the table as separate rows.

**P2-5 — Scope and acceptance-criteria wording.** SPEC-M8 §3's exception stays as
N3-1 wrote it. SPEC-M8 §14's criterion "Open, close, error and effective serial
configuration are observable as non-data events without changing their existing
UI behavior" is replaced by:

"Open, close, error and effective serial configuration are observable as
non-data events. Compatibility expectations for the authorised orchestration
change of §3: the settings vocabulary, their persistence and the settings
dialog's presentation are unchanged; one user action from the settings dialog or
a profile load produces exactly one configuration result for the final requested
configuration; the synchronous legacy `settingsFailed()` notification on a failed
open is preserved (see C5)."

**P2-6 — Tests (replaces N3-6's test list).** SPEC-M8 §13 gains one test per
operation-table row, using the injectable configuration-result seam: one result
for a live reconfiguration with `changedGroups: ["hardware"]`; one result with
`changedGroups: ["localBuffering"]` and an untouched hardware section for an
RX-queue-only change; one result after `TransportOpened` for open-and-configure
and none for a redundant `open()` on an open port; no result from the six legacy
setters while closed and exactly one each while open; `partial`/`failed`
read-back outcomes producing `TransportConfigChanged` plus `Error` in that order;
and the existing serial suite passing unchanged.

## P3 (HIGH, round-3 item 3) — the residual replay statement in ADR-003

Replace ADR-003's consequence bullet "Replay and future TCP/remote transports can
feed the same controller." with:

"Future TCP and remote transports can feed the same controller as live
transports, because they are live byte-movement transports. Passive replay is not
a transport: it distributes stored, immutable `SessionEvent` values through the
replay player interface (ADR-007), never through the controller's live
observation path."

## P4 (HIGH, round-3 item 4) — activation identity on the transport surface

Round 3 is right that N6's contract cannot be enforced or tested without an
identifier: after a new `opened`, an untagged late `bytesReceived` is
indistinguishable from a valid one. Replace N6's third bullet and extend
ADR-003's interface accordingly:

**P4-1 — `ITransport` signals carry an activation identity.**

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

**P4-2 — Conformance rules for a transport.** `activationId` starts at 1 for a
transport instance and strictly increases on every `open()` **attempt** (a failed
attempt consumes an id and never produces an `opened` for it); every observation
of an activation carries that activation's id; an attempt's failure is reported
exactly once, by the transport, using an internal in-progress/result guard rather
than text matching; and `closed()` is the last signal a transport emits for an
activation.

**P4-3 — Controller rules.** The controller stores `currentActivationId` (set on
`opened`, cleared on `closed`). It emits an event only for observations whose
`activationId` equals `currentActivationId`. An observation with any other
activation id while `Idle` is dropped with exactly one diagnostic and produces no
event. An `Error` whose activation id is unknown and which was never preceded by
an `opened` for that id is classified as the failed-open error: the controller
emits exactly one `Error(kind: "open")` for that id, does not enter `Live`, and
suppresses any further observation carrying the same id. The legacy
`settingsFailed()` path is untouched (C5).

**P4-4 — Synchronous open requirement.** `ITransport` v1 still requires `open()`
to report its own result synchronously, as the local serial transport does and as
the profile-error regression depends on. A transport whose open completes
asynchronously must define its own correlation and is not part of v1 — the
activation id makes that a documented extension point rather than a gap.

**P4-5 — Tests.** With the transport double: a data observation carrying an old
activation id, delivered after a new `opened`, is dropped with one diagnostic and
produces no event; a failed attempt's error followed by the same id's
asynchronous error yields exactly one `Error(kind: "open")`; a valid observation
of the current activation produces exactly one event.

## P5 (HIGH, round-3 item 5) — B2-3 must not restate the wrong widths

Replace B2-3 entirely with: "Lossless encoding applies to the values that are
64-bit and appear in JSON — `sequence`, both timestamps and any future 64-bit
quantity such as an alignment mapping's reference point or offset — which are
written as canonical decimal strings. 32-bit quantities keep their specified
widths and stay JSON numbers, which represent them exactly: `sourceId` is
`quint32` in the event model and `u32` in the record, `eventType` is `u16`,
`direction` and `flags` are `u8`, and every length field is `u32` (see N1). A
JSON number must never be assumed to carry a full nanosecond value; a reader must
never silently truncate."

## P6 (HIGH, round-3 item 6) — capture metadata vs. alignment mapping

Round 3 is right that v2.1 made the per-domain origin reference mandatory in the
file header while claiming ADR-005/ADR-008's ban on persisted mappings was
unchanged. Both statements are correct once the terms are separated:

- **Mandatory in v1 (capture metadata):** the per-clock-domain origin reference
  pair of P1, plus the clock domain's identity. It describes *how this file's
  own timestamps were produced* and is written by the capture side. It is not an
  alignment mapping and it maps no domain onto another.
- **Deferred (explicitly not M8/M9):** any mapping *between* clock domains
  (offset, scale, uncertainty), any synchronization exchange or clock probe, any
  runtime `TimeMapping` component, and any alignment UI.

ADR-005's sentence "M8 and M9 do not implement a clock synchronization exchange,
offset/drift estimation, a `TimeMapping` runtime component, persisted alignment
mappings, or an alignment UI" becomes: "M8 and M9 implement neither a clock
synchronization exchange, offset/drift estimation, a `TimeMapping` runtime
component, nor an alignment UI, and they persist no mapping between clock
domains. The per-domain origin reference that ADR-006 requires is capture
metadata describing this file's own timing, not a cross-domain alignment
mapping." ADR-008's equivalent statement gets the same distinction, and its
"Every event preserves `sourceTimestampNs`" sentence stays as is.

## P7 (TEST GAP, round-3 item 7)

Covered by P2-6 (one test per operation-table row) and P4-5 (delayed
old-activation signal, duplicate open-failure error). No further test contract is
needed before acceptance.

## P8 (DOCUMENTATION, round-3 item 8) — the seven notes must be applied

N9 lists seven locations that need a "Superseded by ADR-00x" note. They exist
only as instructions so far. Applying them is a precondition of Acceptance, not
part of this package's text: the seven notes are applied in the same step that
applies v2 + v2.1 + v2.2 to the ADRs, SPEC-M8 and the two architecture documents,
together with the status change to `Accepted`. Round 4 is therefore asked to
confirm the amendment *text*; the application step then follows the governance
rule (documentation updated where required, §14).

## Status after applying v2 + v2.1 + v2.2

| Round-3 acceptance condition | Answer |
| --- | --- |
| 1. Timestamp state/formula + stale SPEC test wording | P1 |
| 2. One complete configuration operation/result table + §14 amendment | P2-1 … P2-6 |
| 3. Activation/attempt correlation | P4-1 … P4-5 |
| 4. Residual ADR-003 replay statement | P3 |
| 5. B2-3 widths + capture metadata vs. deferred alignment | P5, P6 |
| 6. Apply N9's seven documentation notes | P8 (application step) |
| plus TEST GAP closure (round-3 item 7) | P2-6, P4-5 |

Round 4 should check in particular: P1 against P6 (is the origin reference now
consistently *both* mandatory capture metadata and outside the ban?), P2 against
the real call sites and against §3/§14, P4 against C5 and
`tests/tst_profileerror.cpp`, and whether the activation id on seven signals
remains compatible with "one observation → one event".
