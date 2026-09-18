## Verdict

M8 is **not yet ready for formal completion** under the accepted §13 test strategy: round-1 item 3 is only partially closed. The production fixes are sound; two focused test assertions remain.

| Round-1 item | Verdict | Evidence |
|---|---|---|
| 1. Synchronous failed-open notification | **Closed** | `open()` directly emits `settingsFailed()` before returning on failure, after the single open-error observation: [komportserial.cpp](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:185). `slotPortError()` suppresses the later error for that reported failed attempt: [komportserial.cpp](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:527). The new test checks both counts at return: [tst_transport.cpp](/home/max/Development/misc/komport-qt6/tests/tst_transport.cpp:930). |
| 2. Failed-with-changed-effective-state mapping | **Closed** | A port error during application is captured as part of the transaction: [komportserial.cpp](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:779), forces `failed`: [komportserial.cpp](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:797), and emissions follow the required config-then-error order: [komportserial.cpp](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:858). The test creates a changed effective state and asserts `failed`: [tst_transport.cpp](/home/max/Development/misc/komport-qt6/tests/tst_transport.cpp:852). |
| 3. Test gaps | **Partially closed** | Failed-without-change metadata is now asserted: [tst_transport.cpp](/home/max/Development/misc/komport-qt6/tests/tst_transport.cpp:476). Setter coverage for device/RX queue/flush rate is added: [tst_transport.cpp](/home/max/Development/misc/komport-qt6/tests/tst_transport.cpp:954). TX/RX non-empty checks exist in both PTY tests: [tst_transport.cpp](/home/max/Development/misc/komport-qt6/tests/tst_transport.cpp:805), [tst_sessiondocument.cpp](/home/max/Development/misc/komport-qt6/tests/tst_sessiondocument.cpp:115). Residual gaps below. |
| 4. Build/test evidence | **Closed as recorded evidence; not independently verifiable** | The self-review records commands, 0-warning build, 13/13 CTest, target list, and 54 methods: [self-review §6](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-18-M8-implementation-selfreview.md:149). I could statically confirm 13 registrations and 54 declared M8 test methods, but could not run them. |
| 5. Qt 6.3 | **Closed as an open criterion** | The self-review correctly leaves it unverified: [self-review §2](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-18-M8-implementation-selfreview.md:28). The declared floor is 6.3: [CMakeLists.txt](/home/max/Development/misc/komport-qt6/CMakeLists.txt:32). No Qt 6.3 build evidence exists. |

## Remaining must-fix

**TEST GAP — §13’s Start Bits UI/persistence condition is still not tested.** The revised transport test proves metadata and `requestedConfiguration()` storage, but it does not exercise profile persistence or dialog presentation. The accepted spec explicitly requires unchanged UI/configuration behavior: [SPEC-M8](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M8-session-transport-foundation.md:415). Current code does retain the profile key and dialog wiring: [komport.cpp](/home/max/Development/misc/komport-qt6/komport/komport.cpp:552), [komport.cpp](/home/max/Development/misc/komport-qt6/komport/komport.cpp:1210), but that is inspection, not the required test.

Minimal fix: add one focused profile/UI-path test showing `StartBits=2` is loaded, passed into the configuration request, and saved unchanged.

**TEST GAP — failed-with-change ordering is implemented but not asserted by its dedicated test.** The code emits configuration before apply error, but [the test](/home/max/Development/misc/komport-qt6/tests/tst_transport.cpp:852) uses separate spies and never records cross-signal order. Minimal fix: add the same order collector used for the ordinary partial case and assert `configurationChanged,transportError`.

## New findings

**DOCUMENTATION — cosmetic.** Two comments describe the now-obsolete asynchronous failed-open route:

- [komport.h](/home/max/Development/misc/komport-qt6/komport/komport.h:268)
- [tst_profileerror.cpp](/home/max/Development/misc/komport-qt6/tests/tst_profileerror.cpp:64)

Minimal fix: say failed `open()` emits `settingsFailed()` directly and synchronously; `errorOccurred()` is only suppressed as the corresponding duplicate path.

## Disputed readings

1. The historical claim is correct: pre-M8 `open()` did not emit `settingsFailed()`; the only source was `slotPortError()`. My round-1 finding should not be read as claiming that an explicit pre-M8 source-level guarantee was lost. The accepted documents nevertheless require a synchronous result now: [accepted amendment](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-18-M8-foundation-amendments-v3-consolidated.md:246), [SPEC-M8](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M8-session-transport-foundation.md:286). The current direct emission is therefore the correct fix. The application does depend on it, because it checks the pending-error state immediately after its configuration/open sequence: [komport.cpp](/home/max/Development/misc/komport-qt6/komport/komport.cpp:788).

2. The classification refinement is consistent with the accepted contract. `Partial` means the port accepted only part of the requested settings, while `Failed` means the application failed: [transportconfiguration.h](/home/max/Development/misc/komport-qt6/komport/transportconfiguration.h:86). A port error during an apply is correctly classified as `failed`, even if read-back shows changes; §6.2 then explicitly maps that row like `partial`: [SPEC-M8](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M8-session-transport-foundation.md:175).

## Completion and §14

Do **not** declare M8 complete until the two §13 test gaps are closed or the repository owner explicitly waives them.

After that decision and after committing the current step-6/7 slice:

- Leave the first §14 checkbox open pending a genuine Qt 6.3 build.
- The remaining §14 checkboxes may be ticked, relying on the recorded §6 build/test evidence where execution cannot be independently repeated.

I did not run configure, build, CTest, a Qt 6.3 build, a live-device test, or a platform-delayed `QSerialPort::errorOccurred()` test in this read-only environment. The failed-with-change test exercises production `KomportSerial` transaction code, but injects the port-error callback through its test subclass rather than provoking a physical/QSerialPort settings failure.