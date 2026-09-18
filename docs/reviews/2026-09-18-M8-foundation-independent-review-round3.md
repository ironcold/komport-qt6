# Round 3 independent architecture review — M8

**Verdict: not accepted.** Applying v2 + v2.1 would resolve substantial parts of round 2, but leaves two BLOCKER-level contracts internally undefined, plus several HIGH findings. No files were modified.

## 1. N1–N10 assessment

| Item | Status | Evidence |
|---|---|---|
| N1 | **Partially resolved** | N1 correctly restores ADR-006’s actual binary widths and 44-byte prefix ([v2.1:21-34](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-18-M8-foundation-amendments-v2.1.md:21); [ADR-006:29-41](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-006-kpsession-file-format-v1.md:29)). But unsuperseded B2-3 still says source IDs and byte counts “are 64-bit” ([v2:178-186](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-18-M8-foundation-amendments-v2.md:178)), contradicting N1’s `quint32 sourceId` and 32-bit lengths. |
| N2 | **Partially resolved** | It fixes the missing source-to-domain association and supplies concrete header fields ([v2.1:41-69](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-18-M8-foundation-amendments-v2.1.md:41)). However its mapping formula is not executable: `sessionTimestampNs` is both a reference field defined as zero and, apparently, intended to mean the last emitted timestamp ([v2.1:52-53](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-18-M8-foundation-amendments-v2.1.md:52), [v2.1:71-79](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-18-M8-foundation-amendments-v2.1.md:71)). |
| N3 | **Partially resolved** | It explicitly authorizes the necessary scope exception and identifies both problematic UI call sites ([v2.1:90-126](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-18-M8-foundation-amendments-v2.1.md:90)). But the operation/result contract conflicts internally; see cross-check and remaining findings below. |
| N4 | **Partially resolved** | The replacement B3-5 and retained B7-1 now agree that passive replay is neither `ITransport` nor the live controller path ([v2.1:191-203](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-18-M8-foundation-amendments-v2.1.md:191)). However ADR-003’s unchanged consequence still says replay can “feed the same controller” ([ADR-003:55-60](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-003-transport-abstraction-v1.md:55)). |
| N5 | **Partially resolved** | It correctly chooses preservation of raw source time plus derived-time normalization and an anomaly event ([v2.1:210-227](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-18-M8-foundation-amendments-v2.1.md:210)). It inherits N2’s undefined `sessionTimestampNs` operand. It also falsely says clamping is described nowhere else while SPEC §13 still requires a “non-decreasing aligned-time clamp” ([SPEC-M8:185-190](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M8-session-transport-foundation.md:185)). |
| N6 | **Partially resolved** | It preserves the synchronous legacy error path and adds useful v1 conformance constraints ([v2.1:233-260](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-18-M8-foundation-amendments-v2.1.md:233)). But a controller cannot distinguish an old-activation observation delivered after a new activation begins without an activation identifier. The required “cannot appear in a later activation” test is therefore not testable at the controller boundary. |
| N7 | **Resolved** | The structural/stateful split is coherent: a stateless free validator covers event-local rules, while the controller enforces sequence, ordering, and source/domain stream rules before emission ([v2.1:266-282](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-18-M8-foundation-amendments-v2.1.md:266)). |
| N8 | **Resolved** | It correctly preserves stored event boundaries while permitting decoder/view-derived reassembly that is not written back ([v2.1:286-292](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-18-M8-foundation-amendments-v2.1.md:286)). |
| N9 | **Resolved as an amendment instruction** | The list is complete for the identified drift: repository search confirms the two session-architecture hits, four multiport hits, and one multi-executable `QVariantMap` hit. The seven supersession notes still need to be physically applied before acceptance, since v2.1 expressly leaves source documents untouched ([v2.1:10](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-18-M8-foundation-amendments-v2.1.md:10)). |
| N10 | **Partially resolved** | N3-6 adds a useful configuration-result seam and test cases ([v2.1:175-185](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-18-M8-foundation-amendments-v2.1.md:175)). It cannot fully close the test gap until N3 defines what operation produces which result, and N6 makes cross-activation isolation objectively testable. |

## 2. Required cross-checks

### N2 versus N5

**No: they do not yet form one consistent, implementable rule.**

Both use:

```text
max(sessionTimestampNs, sourceTimestampNs - reference.sourceTimestampNs)
```

But `reference.sessionTimestampNs` is defined to be exactly zero, while N5 describes the first operand as “the last emitted session time.” Those are different values. Define:

```text
rawSessionTimestampNs = sourceTimestampNs - reference.sourceTimestampNs
timestampNs = max(lastEmittedSessionTimestampNs, rawSessionTimestampNs)
```

with `lastEmittedSessionTimestampNs` initialized to the reference session timestamp (zero) and updated after every emitted event. Also replace SPEC §13’s remaining “clamp” wording with this normalization rule.

### N3 versus SPEC-M8 §3 and §14

**The exception is directionally right but incomplete, and §14 is inconsistent with it.**

N3-1 explicitly permits removing duplicate application and replacing staged UI setters with one complete configuration request. This is a behaviour change to orchestration, not merely a documentation clarification. Yet SPEC §14 still requires effective configuration observability “without changing their existing UI behavior” ([SPEC-M8:217-218](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M8-session-transport-foundation.md:217)).

The real paths demonstrate why the exception must be explicit:

- Profile loading deliberately closes, stages six setters, then opens ([komport.cpp:625-647](/home/max/Development/misc/komport-qt6/komport/komport.cpp:625)).
- Preferences currently can close/reopen via `setDeviceName()`, then apply framing, flow control, and baud separately ([komport.cpp:1250-1276](/home/max/Development/misc/komport-qt6/komport/komport.cpp:1250)).

N3 must amend §14 to permit this specific transaction-orchestration change and state observable compatibility expectations: unchanged settings vocabulary/persistence/dialog presentation, one result for the final requested configuration, and preserved synchronous legacy error notification.

### N6 versus C5 and `tst_profileerror`

**It preserves the existing test’s dependency.**

C5 correctly identifies that the test depends on direct, synchronous:

```text
errorOccurred → slotPortError → settingsFailed
```

during failed `open()` ([v2:136-140](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-18-M8-foundation-amendments-v2.md:136)). The current direct connections and legacy emission are present in the serial implementation ([komportserial.cpp:36-39](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:36), [komportserial.cpp:247-252](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:247)); the test asserts the status message after a failed profile open ([tst_profileerror.cpp:64-97](/home/max/Development/misc/komport-qt6/tests/tst_profileerror.cpp:64)).

N6 expressly limits duplicate suppression to the new event path and retains the synchronous legacy `settingsFailed()` behaviour. Therefore it does not break this regression dependency.

### N7 versus B2-2’s “plain aggregate”

**Coherent and testable.**

B2-2 already declined private members/accessors ([v2:170-176](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-18-M8-foundation-amendments-v2.md:170)). N7 keeps `SessionEvent` a plain aggregate but requires structural validation plus controller-owned stream validation before emission. A deterministic transport double can test both emission-time enforcement and stream ordering. The aggregate cannot technically prevent a caller from mutating its own copy, but that is not required for the stated producer/consumer contract.

## 3. Remaining and newly introduced findings

| Priority | Category | Finding and minimal fix |
|---|---|---|
| 1 | **BLOCKER** | **Timestamp formula is ambiguous.** N2/N5 use `sessionTimestampNs` for two incompatible things. **Fix:** introduce `lastEmittedSessionTimestampNs` explicitly, define initialization/update order, and update SPEC §13’s old clamp wording. |
| 2 | **BLOCKER** | **Configuration transaction contract remains contradictory.** N3-2 says every entry-point call emits one result, while N3-3 says a closed-port call only stores values and a later `open()` produces the transaction. N3-5 says baud application occurs indirectly through `settingsChanged()`, while N3-4 says legacy signals must never drive hardware. N3-6’s `TransportOpened → result → Error` ordering cannot apply to a reconfiguration of an already-open same-endpoint port. **Fix:** add one operation table covering `configure(request)`, `open()`, device-change reconfiguration, and each legacy adapter; state event/result ownership, whether a local-buffer-only change emits a result event, and the ordering for open-and-configure versus live reconfiguration. Make legacy setters delegate directly to the transaction implementation, then emit compatibility signals; signals must not trigger hardware. Amend §14 accordingly. |
| 3 | **HIGH** | **Replay contradiction remains in ADR-003 consequences.** N4 changes the decision paragraph but not the line that says replay feeds the controller. **Fix:** change ADR-003’s consequence to name only future TCP/remote live transports; state replay distributes stored events through the M10 player interface. |
| 4 | **HIGH** | **N6 still cannot prove late-activation isolation.** After a new successful `opened`, an untagged old `bytesReceived` signal is indistinguishable from a valid current one. **Fix:** carry an activation/attempt ID on lifecycle, data, configuration, and error observations and have the controller reject mismatches. This also makes the requested test meaningful. |
| 5 | **HIGH** | **The 64-bit correction leaves B2-3 contradictory.** B2-3 must not continue to characterize source IDs and byte counts as universally 64-bit. **Fix:** supersede/replace B2-3 with N1’s actual-width rule. |
| 6 | **HIGH** | **Persisted-origin wording conflicts with “no persisted alignment mapping.”** N2 makes a per-domain source/session reference mandatory in the file header, then says the ADR’s ban on persisted alignment mapping remains unchanged ([v2.1:84-86](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-18-M8-foundation-amendments-v2.1.md:84)). **Fix:** distinguish mandatory v1 per-domain origin references from deferred cross-domain alignment mappings, and amend ADR-005/008 consistently. This is a data-contract clarification, not M9 implementation work. |
| 7 | **TEST GAP** | **N3-6/N6 do not fully specify objective lifecycle tests.** A fake can produce status values, but not prove ambiguous transaction ownership or reject an untagged old-activation observation. **Fix:** resolve findings 2 and 4, then require tests for each operation-table row and a deliberately delayed old-generation signal. |
| 8 | **DOCUMENTATION** | **N9’s required notes are not yet applied.** **Fix:** add the seven specified “Superseded by ADR-00x” notes to the named source locations. |

No M9+ implementation work is improperly added by N4, N8, or N9. N2’s header schema is a frozen future file-format contract, which is appropriate at ADR level, but its persistence terminology must be corrected as above. M10’s replay interface remains properly deferred.

## 4. Acceptance decision

**No. ADR-002 through ADR-009 and SPEC-M8 must not be marked Accepted after v2 + v2.1 as written.**

Acceptance requires, in order:

1. Resolve the timestamp state/formula and update the stale SPEC test wording.
2. Replace N3’s conflicting prose with one complete configuration operation/result table; amend SPEC §14 for the authorized orchestration exception.
3. Add activation/attempt correlation to make the lifecycle contract enforceable.
4. Remove the residual ADR-003 replay statement.
5. Correct B2-3 and distinguish v1 origin persistence from deferred cross-domain alignment mapping.
6. Apply N9’s seven documentation notes.

Documented follow-ups that remain outside M8:

- Exact passive replay/player API design in M10.
- M9 streaming writer and persistence implementation.
- Multi-source synchronization, cross-domain mapping estimation, and uncertainty UI.
- A separately reviewed decision on clearing the retained legacy RX buffer.
- Actual Qt 6.3 CI/release-gate execution.

## 5. Verification limits

- No M8 implementation exists yet: no `SessionEvent`, `ITransport`, or `SessionController` files are present in production sources.
- I did not build or run tests; this environment is read-only and building would create artifacts.
- I could verify the current synchronous error-path dependency from source and test text, but not Qt’s runtime signal timing on this host.
- I could not verify real serial-device partial-write behaviour, physical wire delivery, PTY RX chunk boundaries, or Qt 6.3 compatibility.