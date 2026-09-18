## Round-4 verification

| Round-3 item | Verdict | Evidence |
|---|---|---|
| Port error during apply was lost | Closed | `slotPortError()` records the error during the apply window instead of emitting a runtime observation: [komportserial.cpp:525](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:525). The result consumes it, appends its message, and demotes `full` to `partial`/`failed`: [komportserial.cpp:765](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:765). The emitted error retains `kind: "apply"`, a stable code, and result message: [komportserial.cpp:845](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:845). The seam test now requires the successful-but-erroring apply to produce `apply_partial`: [tst_transport.cpp:834](/home/max/Development/misc/komport-qt6/tests/tst_transport.cpp:834). |
| Transaction window was unsafe under direct-signal reentrancy | Closed | The prior window value is saved and restored around the hardware seam: [komportserial.cpp:721](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:721). A nested apply therefore restores `true` for its outer apply. The test triggers an error before and after the nested receiver, and rejects any runtime-kind error: [tst_transport.cpp:887](/home/max/Development/misc/komport-qt6/tests/tst_transport.cpp:887). |
| Activation-zero test omitted a real close | Closed | The test opens a real PTY, calls `close()`, then exercises RX, flush, and error slots while preserving legacy character/error notifications but requiring no transport observations: [tst_transport.cpp:850](/home/max/Development/misc/komport-qt6/tests/tst_transport.cpp:850). The reset itself is performed in `close()`: [komportserial.cpp:213](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:213). |

No new section-13 finding.

The new error state does not leak across completed hardware transactions in the synchronous, exception-free path: it is consumed and cleared unconditionally after the seam returns ([komportserial.cpp:726](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:726)). There is no exception-free early return after the window is set; restoration occurs immediately after the sole seam call.

Folding the port error into the apply outcome does not hide the session-record information required by SPEC-M8 §9. The resulting `transportError` has `kind`, `code`, and the propagated human-readable `message` ([komportserial.cpp:774](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:774), [komportserial.cpp:845](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:845)); this matches the requirement at [SPEC-M8 §9](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M8-session-transport-foundation.md:327).

Final verdict: **this M8 step-2/step-3 slice may be committed as one commit.**

I could not verify the claimed warning-free build, `ctest` result, test-method counts, or real-device/Qt signal timing in this read-only environment. I verified the source-level behavior and the PTY/seam test assertions only.