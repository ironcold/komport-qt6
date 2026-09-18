# Independent M8 Foundation Review — Round 2

## Verdict

**Not accepted as v2 stands.** The package resolves much of round 1, but it introduces/reveals several contract contradictions that prevent ADR-002…ADR-009 and SPEC-M8 from being frozen.

A corrected v2 can be accepted once the blocking and HIGH items below are fixed, the three decisions are recorded, and the specified documentation amendments are applied.

## Coverage of prior findings

No F1–F16 or X1–X9 item was silently omitted from v2’s traceability matrix. Some are nevertheless only partially resolved.

| Finding | v2 amendment(s) | Assessment |
|---|---|---|
| F1 clock origin/domain | D1, B3-1, B5-1, B6-1, BS-12 | **Incomplete.** Session-relative time is the right correction, but B5-1 defines an origin “for that source” while B6-1 defines one reference pair per clock domain. It also never requires each source descriptor to identify its `clockDomainId`. |
| F2 all TX entry points | B3-2, BS-2 | **Correct in substance.** All current TX routes do go through `putChar()`/both `putStr()` overloads; the unified observed write primitive covers them. |
| F3 sequence across activations | B2-1, BS-1 | **Correct.** The current `open()` closes first and device changes can close/reopen, so continuing sequence numbers is necessary. |
| F4 partial writes | B3-3, BS-2, BS-7 | **Correct.** Accepted-prefix events and a byte-count return type repair the previous data omission. |
| F5 byte-wise TX volume | BS-8, B6-3 | **Correct.** It acknowledges one event per accepted `putChar()` write and requires an appropriate pathology test. |
| F6 effective configuration | BS-3, D3 | **Incomplete.** The transaction model is sound, but its scope and entry points remain ambiguous; it also conflicts with SPEC-M8’s unchanged non-goal of no serial-settings semantic change. |
| F7 replay boundary | B3-5, B7-1, D2 | **Wrong/internally inconsistent.** B3-5 says passive replay does not use `ITransport`, then says “no `ITransport` output target” does not mean no `ITransport`. B7-1 correctly says replay uses neither `ITransport` nor the live controller path. |
| F8 re-entrant event delivery | BS-4 | **Correct.** FIFO delivery and sequence allocation on dequeue address the nested RX→TX case. Existing emulation can write during `receivedChar()` handling. |
| F9 error/open failure | BS-1, BS-7 | **Incomplete.** The desired result is clear, but `ITransport` carries neither an operation token nor activation generation; the proposed “guard” is not a complete, testable correlation contract. |
| F10 observations outside `Live` | BS-1 | **Incomplete.** The requested activation-generation isolation is not represented in the `ITransport` signals. Also, the retained legacy RX buffer produces delayed legacy characters, not delayed transport observations. |
| F11 display/recorder divergence | BS-6 | **Correct.** It correctly records that `close()` leaves `mRxBuffer` and its timer intact, while event capture precedes queue trimming. |
| F12 PTY chunk assertions | BS-8 | **Correct.** Deterministic controller tests and PTY concatenation assertions are the appropriate split. |
| F13 unverifiable acceptance criteria | BS-10 | **Correct.** A QtCore-only compile target and an actual Qt 6.3 gate are required; current test targets link `komport_core`, which publicly links Widgets. |
| F14 architecture-document drift | BD-1 | **Incomplete.** It covers the session architecture’s `QVariantMap` and magic and source-zero examples, but misses `QVariantMap` examples in `komport-multiport-sniffer-time-alignment.md` (for example lines 121 and 1117). |
| F15 source-ID wording | BS-11 | **Correct.** Failed-open errors must also use source ID 1. |
| F16 no production consumer | BS-9 | **Correct.** A test-only collector is appropriate and does not require a recorder in M8. |
| X1 clock-domain schema | B3-1, B5-1, B6-1 | **Incomplete.** A clock-domain identifier exists, but no source-to-domain association or unambiguous domain-level mapping semantics exists. |
| X2 JSON 64-bit precision | B2-3, B6-2, B8-1 | **Wrong.** The lossless-string principle is correct, but B6-2 falsely says `sourceId` and “all lengths” are 64-bit. ADR-002 specifies `quint32 sourceId`; ADR-006 specifies `u32` header/record/metadata/payload lengths. |
| X3 event value contract | B2-2 | **Incomplete.** A single stateless validator cannot verify “starts at one” or strict per-session ordering. The amendment also does not require the M8 producer to invoke validation/factory logic. |
| X4 destruction ordering | B3-7, BS-5 | **Correct.** This correctly addresses `KomportDoc`’s by-value `mSerial` member. |
| X5 duplicate apply/open-configure | BS-3, D3 | **Incomplete.** See F6. The duplicate currently exists: `open()` applies settings, then emits the self-connected `settingsChanged()` signal. |
| X6 capture-point limitation | B3-6, BS-2, BS-6 | **Correct.** The wording correctly limits truth to `readAll()` output and accepted API writes, not electrical delivery. |
| X7 RX buffer across activations | BS-1, BS-6, D3 | **Correct for the chosen behavior.** Preserving the existing visible behavior is a defensible M8 decision. The rationale should not claim that it itself creates a late transport event. |
| X8 deterministic test seams | BS-8, BS-9 | **Correct.** |
| X9 scoped compatibility exception | B3-4 | **Correct.** It permits existing callers while prohibiting new event/data bypasses. |

## New or remaining problems

| Priority | Category | Finding and minimal fix |
|---|---|---|
| 1 | **BLOCKER** | **ADR-006 field-width contradiction.** B6-2 says `sourceId` and all lengths are 64-bit. ADR-002 defines `sourceId` as `quint32`; ADR-006 defines its lengths as `u32`. Applying B6-2 would also invalidate the stated 44-byte record prefix. **Fix:** say only the actual 64-bit fields (`sequence`, timestamps, and any future 64-bit JSON value) use canonical decimal strings in JSON; retain the specified binary widths. |
| 2 | **BLOCKER** | **Clock-domain mapping is not serializable or coherent.** B5-1 uses a per-source first-event origin; B6-1 uses a per-domain reference pair. No source descriptor is required to reference a `clockDomainId`. **Fix:** define one model: each source descriptor includes `clockDomainId`; define exact reference fields and types; for M8’s sole source define `sessionOriginSourceTimestampNs` and session origin zero. State how a shared-domain multi-source session uses one common reference. |
| 3 | **BLOCKER** | **Configuration transaction contract conflicts with M8 scope and lacks a complete operation boundary.** SPEC-M8 §3 still says serial-settings semantics do not change, while BS-3 removes duplicate application. Current code applies through `setBaudRate()` → `settingsChanged()` → `slotSettingsChanged()`, and directly through `setFraming()`/`setFlowControl()`; profile loading also stages multiple setters before `open()`. **Fix:** explicitly amend scope/non-goals to authorize the narrowly defined duplicate-application removal; define every configuration entry point and one transaction/result rule, including `setBaudRate()` and device-triggered reopen. |
| 4 | **HIGH** | **Replay boundary contradicts itself.** B3-5 and B7-1 disagree on whether passive replay uses `ITransport`. **Fix:** retain B7-1’s rule: passive replay is a future read-only event source, not `ITransport` and not the live-observation path. Delete/rewrite B3-5’s final “not ‘no ITransport’” sentence. |
| 5 | **HIGH** | **Timestamp violation behavior is contradictory.** Current SPEC-M8 §9 retains “clamp to the last emitted timestamp,” while B5-1/BS-12 require the exact formula `timestampNs = sourceTimestampNs - origin` and unchanged source time. **Fix:** either reject/drop an out-of-order source observation with an error/diagnostic, or explicitly define a distinct derived-time normalization rule. Do not claim both exact mapping and clamping. |
| 6 | **HIGH** | **Open/error and activation correlation is underspecified.** BS-1 requires an in-progress/error-generation guard, but ADR-003’s signals contain no operation/activation identifier and do not define controller ownership of open attempts. **Fix:** define an M8-local operation/activation token contract, or state that `SessionController` initiates and synchronously brackets `open()`; tests must prove duplicate suppression and late-signal isolation. |
| 7 | **HIGH** | **Event validation remains insufficient.** B2-2’s “single explicit validation function” cannot validate sequence position or ordering without session state, and it is not required on controller emission. **Fix:** separate structural validation from stateful stream validation; require the controller/factory to enforce both before emitting. |
| 8 | **MEDIUM** | **Chunk wording is overly absolute.** B3-2/BS-2 allow decoders to assemble streams but say no layer may recombine or re-split stored chunks. That can be read as prohibiting derived decoder framing. **Fix:** say persisted `SessionEvent` boundaries are immutable; decoders may derive reassembled/framed views without mutating or replacing stored events. |
| 9 | **DOCUMENTATION** | **BD-1 does not fully repair F14.** It misses `QVariantMap` examples in the multiport architecture document. **Fix:** mark every contradictory conceptual template/example as superseded, including its `QVariantMap` definitions. |
| 10 | **TEST GAP** | **The configuration and lifecycle tests are not yet objectively specified.** A scripted transport double can prove controller behavior, but it cannot prove the serial implementation emits one configuration result per logical operation unless the operation boundary is exposed. **Fix:** specify an injectable configuration-result seam or a serial transaction abstraction, then test full/partial/failed read-back outcomes and event ordering. |

## Decision Required

### D1 — Session-time model

**Recommended option 1 is right.** Session-origin-relative time matches the existing architecture’s relative timeline model and preserves raw source time for later alignment.

Its main risk is ambiguity if “first event” can be a failed-open error or if several sources share a clock domain. That is manageable only with the source-to-domain and mapping corrections above.

**It cannot be deferred.** `timestampNs` semantics are part of the frozen `SessionEvent`/file-format foundation. The detailed future synchronization algorithm can remain deferred.

### D2 — Replay event-source boundary

**Recommended option 1 is right.** Passive replay must be a separately specified, read-only event source/player. Reconstructing persisted events from `ITransport` byte signals would lose event identity, annotations, stored sequence, source identity, and stored timing.

Its risk is a second abstraction, but it is an appropriate distinction: `ITransport` describes live byte movement; replay distributes immutable, already-recorded events.

**The interface shape can be deferred to M10, but the boundary cannot.** ADR-003 and ADR-007 must agree now that passive replay is not a live transport adapter.

### D3 — Duplicate settings application and legacy RX buffer

**Recommended option 1 is right with narrower claims.**

- Removing duplicate hardware application is necessary if M8 promises one configuration transaction/result.
- Retaining the RX buffer across close/reopen is appropriate for M8 because clearing it changes visible terminal behavior.

Risks:

- Reapplying identical serial settings is not proven to be an observable no-op on every platform/device; v2 should not promise “no other behaviour change.”
- Removing one application can change legacy `settingsFailed()` count, as v2 acknowledges.
- Preserved pre-close bytes can still drive emulation after reopen; that must remain documented as legacy display behavior, not misattributed as a delayed transport observation.

**Part (a) cannot be deferred if M8 retains configuration events.** Part (b), changing buffer behavior, can and should be deferred to a dedicated visible-behavior review.

## Acceptance status

ADR-002…ADR-009 and SPEC-M8 **must not be marked Accepted after applying v2 unchanged**.

They may be accepted after a corrected v2 resolves, in this priority order:

1. B6-2’s incompatible type/length statements.
2. The source-to-clock-domain/session-origin schema.
3. D3’s configuration transaction scope and complete operation contract.
4. D2’s replay wording contradiction.
5. Timestamp violation semantics.
6. Open/error/activation correlation and stateful event validation.

Acceptable documented follow-ups, not M8 implementation work:

- Exact replay/player API design in M10, after fixing the live-vs-replay boundary now.
- M9 streaming writer implementation and performance strategy.
- Multi-source synchronization, alignment estimation, uncertainty UI, and mapping runtime.
- A future reviewed decision on clearing the legacy RX buffer.
- Qt 6.3 CI/release-gate execution before claiming tested Qt 6.3 compatibility.

## Verification limits

- No M8 `SessionEvent`, `ITransport`, or `SessionController` implementation exists yet, so runtime event behavior could not be verified.
- I did not build or run tests because this review environment is read-only and the request prohibits file modification; building would create artifacts.
- The host exposes Qt 6.11.1, not Qt 6.3, and `.github/` contains templates but no CI workflow; Qt 6.3 compatibility remains unverified.
- Real serial-device partial-write behavior and physical delivery cannot be verified here. The current PTY test proves length-aware TX byte preservation, not deterministic partial writes or RX chunk boundaries.
