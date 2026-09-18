## Round-5 verification

| Round-4 item | Verdict | Evidence |
|---|---|---|
| 1. Zero-ID / foreign-TX diagnostic tests | **Partially closed** | The foreign `TX(2)` check is sound: prior to it no warning with activation 2 and the foreign-live reason can exist, so [the assertion](tests/tst_sessioncontroller.cpp:804) proves that exact drop. `rx(0)` remains insufficiently reason-specific: its count delta proves one warning was added, but [the following `hasWarning()`](tests/tst_sessioncontroller.cpp:781) searches the whole log and can match the preceding `error(0)` warning. `hasWarning()` has no post-index scoping. [tests/tst_sessioncontroller.cpp:220](tests/tst_sessioncontroller.cpp:220) |
| 2. Failed with changed effective state | **Closed** | The full fixture’s requested/effective baud is consistently `9600`; the failed result requests `38400` and reads back `19200`. [tests/tst_sessioncontroller.cpp:823](tests/tst_sessioncontroller.cpp:823), [tests/tst_sessioncontroller.cpp:903](tests/tst_sessioncontroller.cpp:903). The test explicitly compares the two effective baud rates and verifies config-result then apply-error order, representing SPEC §6.2’s failed-with-change mapping. [tests/tst_sessioncontroller.cpp:911](tests/tst_sessioncontroller.cpp:911), [SPEC-M8](docs/specs/SPEC-M8-session-transport-foundation.md:176) |
| 3. Failed-open wording | **Partially closed** | The controller comment is corrected and matches the accepted §7 rule. [sessioncontroller.cpp](komport/sessioncontroller.cpp:179), [SPEC-M8](docs/specs/SPEC-M8-session-transport-foundation.md:286). But [tests/tst_sessioncontroller.cpp](tests/tst_sessioncontroller.cpp:437) still says a failed open “consumes no activation.” The literal claim that no other comment/document repeats the ambiguity is also not true for several historical review documents, including [application verification](docs/reviews/2026-09-18-M8-foundation-application-verification.md:42) and the superseded amendment records. |

## Findings

- **TEST GAP:** `rx(0)` does not prove that its own diagnostic contains the required ID/reason. Minimal fix: inspect the warning newly added at `warningsBeforeZeroRead` (or add an indexed helper) and assert both `activation 0 -` and `the observation carries no activation id`.

- **DOCUMENTATION:** The failed-open test comment contradicts ADR-003 and accepted SPEC §7. Minimal fix: state that no live activation is created while the attempt ID is consumed. If the “no doc in tree” requirement is literal, also clearly mark/reword the obsolete wording in review/archive records; otherwise limit that claim to current normative and source documentation.

No additional defects found. No cosmetic/style remarks.

## Final verdict

**Do not commit yet.** Minimal must-fix list:

1. Make the `rx(0)` ID/reason assertion specific to its newly emitted warning.
2. Correct the stale test comment; resolve the stated archival-document scope if the “entire tree” condition is intended literally.

I did not run the build, `ctest`, or inspect runtime skip status. Static inspection confirms 18 controller test slots and 2 document test slots; runtime results remain unverified.