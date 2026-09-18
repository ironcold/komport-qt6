## Round-2 verdict: do not commit

One new HIGH activation-filter defect remains, and the accepted spec still requires amendment for the serial-descriptor contradiction.

| Round-1 item | Verdict | Evidence |
|---|---|---|
| 1. Failed-open identities | **Partially closed** | All failed IDs are retained and failed IDs are rejected by `opened`: [sessioncontroller.h:150](/home/max/Development/misc/komport-qt6/komport/sessioncontroller.h:150), [sessioncontroller.cpp:115](/home/max/Development/misc/komport-qt6/komport/sessioncontroller.cpp:115), [sessioncontroller.cpp:182](/home/max/Development/misc/komport-qt6/komport/sessioncontroller.cpp:182). Direct duplicate/failed-then-open cases are covered: [tst_sessioncontroller.cpp:624](/home/max/Development/misc/komport-qt6/tests/tst_sessioncontroller.cpp:624). However, a failed error does not advance the opened-ID watermark; see new HIGH below. |
| 2. `eventObserved` API name | **Closed** | The only public event signal is [sessioncontroller.h:87](/home/max/Development/misc/komport-qt6/komport/sessioncontroller.h:87), and uses are updated, e.g. [tst_sessiondocument.cpp:81](/home/max/Development/misc/komport-qt6/tests/tst_sessiondocument.cpp:81). `sessionEvent` remains only as historical text in the required round-1 review record, not in implementation/tests. |
| 3. ADR-005 diagnostic bypassed FIFO | **Closed** | The diagnostic is a complete queued observation with an already-mapped time: [sessioncontroller.cpp:238](/home/max/Development/misc/komport-qt6/komport/sessioncontroller.cpp:238), [sessioncontroller.cpp:253](/home/max/Development/misc/komport-qt6/komport/sessioncontroller.cpp:253). The test verifies triggering event → one error diagnostic → later event and non-decreasing time: [tst_sessioncontroller.cpp:458](/home/max/Development/misc/komport-qt6/tests/tst_sessioncontroller.cpp:458). |
| 4. Undocumented public diagnostic API | **Partially closed** | The unapproved signal/counters/accessor are gone; the only public signal is `eventObserved` and drops use `qWarning()`: [sessioncontroller.h:84](/home/max/Development/misc/komport-qt6/komport/sessioncontroller.h:84), [sessioncontroller.cpp:197](/home/max/Development/misc/komport-qt6/komport/sessioncontroller.cpp:197). The accepted spec still does not define the diagnostic channel, so C4 must be incorporated if this is the intended contract. |
| 5. Missing serial descriptor | **Not closed** | The accepted spec still requires a serial descriptor: [SPEC-M8 §6.2](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M8-session-transport-foundation.md:231). `KomportDoc` creates only the controller bound to `mSerial`: [komportdoc.cpp:39](/home/max/Development/misc/komport-qt6/komport/komportdoc.cpp:39). No descriptor type or configuration exists. |
| 6. Identity/anchor test gaps | **Partially closed** | Added tests cover retained failed IDs, failed-open anchor, late close, and repeated open: [tst_sessioncontroller.cpp:624](/home/max/Development/misc/komport-qt6/tests/tst_sessioncontroller.cpp:624), [tst_sessioncontroller.cpp:668](/home/max/Development/misc/komport-qt6/tests/tst_sessioncontroller.cpp:668), [tst_sessioncontroller.cpp:689](/home/max/Development/misc/komport-qt6/tests/tst_sessioncontroller.cpp:689). They do not cover a later failed ID delivered before an older delayed `opened`, which exposes the remaining HIGH. |
| 7. Controller operation-table coverage | **Partially closed** | The new test verifies pass-through of a representative full object, partial mapping, failed-without-change, compatibility-only mapping, and sequences: [tst_sessioncontroller.cpp:718](/home/max/Development/misc/komport-qt6/tests/tst_sessioncontroller.cpp:718). It does not cover every §6.2 row as required by [SPEC-M8 §13](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M8-session-transport-foundation.md:396): its “full” metadata omits several required requested/effective hardware fields ([tst_sessioncontroller.cpp:729](/home/max/Development/misc/komport-qt6/tests/tst_sessioncontroller.cpp:729)); it has no `failed`-with-changed-effective-state case, nor controller-level endpoint-change/reopen-plus-result coverage. |
| 8. Document lifetime test | **Closed** | The controller is explicitly reset before by-value serial destruction: [komportdoc.cpp:51](/home/max/Development/misc/komport-qt6/komport/komportdoc.cpp:51). The real-PTY document test verifies open/config/RX bytes/close, and destruction with an open port: [tst_sessiondocument.cpp:65](/home/max/Development/misc/komport-qt6/tests/tst_sessiondocument.cpp:65), [tst_sessiondocument.cpp:119](/home/max/Development/misc/komport-qt6/tests/tst_sessiondocument.cpp:119). Target and `libutil` linkage are present: [tests/CMakeLists.txt:34](/home/max/Development/misc/komport-qt6/tests/CMakeLists.txt:34). |
| 9. QtCore-only proof | **Closed** | The Core-only object target now compiles the implementation too: [tests/CMakeLists.txt:50](/home/max/Development/misc/komport-qt6/tests/CMakeLists.txt:50). `SessionController` itself includes only Core types: [sessioncontroller.h:32](/home/max/Development/misc/komport-qt6/komport/sessioncontroller.h:32). |

## New findings

| Category | Finding | Minimal fix |
|---|---|---|
| **HIGH** | Activation admission still trusts delivery order after a failed open. The controller only compares `opened` IDs to `mLastOpenedActivationId` ([sessioncontroller.cpp:119](/home/max/Development/misc/komport-qt6/komport/sessioncontroller.cpp:119)); accepting `transportError(2)` while idle does not update that value ([sessioncontroller.cpp:182](/home/max/Development/misc/komport-qt6/komport/sessioncontroller.cpp:182)). A delayed `opened(1)` is then accepted, although ADR-003 requires activation IDs to strictly increase and says delivery order is not trustworthy. `opened(0)` is also accepted, contradicting the “starts at 1” contract. | Track a non-zero high-water mark for admitted activation attempts, including accepted failed-open errors; reject `opened(0)` and stale `opened`/failed-error IDs. Add tests for `error(2) → delayed opened(1)` and `opened(0)`, each producing one warning and no event/state transition. |
| **TEST GAP** | The warning-counter test counts every Qt message and does not verify `QtWarningMsg`, activation ID, or rejection reason: [tst_sessioncontroller.cpp:185](/home/max/Development/misc/komport-qt6/tests/tst_sessioncontroller.cpp:185). Thus it would not protect C4 if `qWarning()` regressed to another message type or omitted required contents. | Filter for `QtWarningMsg` and assert the text contains the activation ID and reason. |

## Proposed clarifications

- **C4: approve as written.** It precisely documents the existing non-public diagnostic behavior and prevents recreating an unapproved signal/counter/accessor surface. Add the matching stronger test above.

- **C5: approve with this wording.**

  > `KomportDoc` configures source ID 1. M8 defines no runtime source-descriptor type. Source descriptors are session-header data defined by ADR-006, with their multi-source semantics defined by ADR-008. The local serial descriptor is therefore deferred; M8 fixes one local source with source ID 1, using the single process-monotonic clock domain required by ADR-003 and ADR-005.

This avoids implying that M8 has a runtime descriptor or clock-domain identifier to configure.

## Minimal must-fix list before commit

1. Fix the HIGH activation high-water-mark/zero-ID admission defect and add its regression tests.
2. Complete the required controller operation-table coverage, including full requested/effective metadata and failed-with-change mapping.
3. Apply and review one normative SPEC-M8 amendment containing approved C4 and C5.
4. Strengthen the diagnostic test to enforce C4’s warning contract.

## Not verified

I did not run the claimed warning-clean build or `ctest`, nor execute the PTY tests, because this review environment is read-only. I verified their target wiring and test logic by inspection only.