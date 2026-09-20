## Verdict

**Not clean — do not adopt or commit step 3a as it stands.** The core fixes are sound, but the closing evidence/documentation condition is still not met.

## Disposition

| Item | Result |
|---|---|
| Guard removal / no hidden authorization policy | Resolved. `onDeliveryDue()` has no state, identity, or session guard. |
| Re-entrancy precondition correction | Resolved. The self-review now correctly says the core relies on §5.11’s precondition. |
| Test inventory | Resolved: 29 methods / 31 QTest entries. |
| Seam cancellation documentation | Resolved in the seam header, including the executing-callback limit. |
| Complete timing/refusal matrix | **Not resolved.** Its header claims more than the matrix executes. |

## New findings

1. **The matrix does not cover every refusal it claims to cover.** It omits the implementation’s distinct `play()`-while-`Playing` refusal and the zero-event-session refusals (`play()` plus all three steps). Those paths exist in [sessionreplayplayer.cpp](/home/max/Development/misc/komport-qt6/komport/sessionreplayplayer.cpp:199) and [sessionreplayplayer.cpp](/home/max/Development/misc/komport-qt6/komport/sessionreplayplayer.cpp:211), but are absent from the matrix at [tst_sessionreplayplayer.cpp](/home/max/Development/misc/komport-qt6/tests/tst_sessionreplayplayer.cpp:1078).

2. **Two matrix-comment claims are not executed by that matrix.** In `Playing`, the `Step` branch proves only that the policy is *not* `Step`; it does not compare it with the pre-call policy, despite claiming the policy remains in effect. And it never fires the successor delivery, so it does not demonstrate that each accepted policy “applies to the next delivery.” [test](/home/max/Development/misc/komport-qt6/tests/tst_sessionreplayplayer.cpp:1051)  
   The older test proves next-delivery application for `Scale2`, not every accepted value. [test](/home/max/Development/misc/komport-qt6/tests/tst_sessionreplayplayer.cpp:473)

3. **§5.10 and the seam disagree by omission.** The seam correctly says cancellation does not affect an already executing callback, but §5.10 does not state that limit and instead says broadly that no callback armed before cancellation can run. Make the spec use the seam’s precise wording. [spec](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:876), [seam](/home/max/Development/misc/komport-qt6/komport/sessionreplayseams.h:60)

4. **The self-review has stale evidence text.** It names a nonexistent test, `refusalsAndTimingAcceptanceAreCompleteAndSilentInEffect`, rather than the actual matrix test. [self-review](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-20-M10-step3a-core-implementation-selfreview.md:97)

## Verification

I ran the current replay-player executable directly with `QT_QPA_PLATFORM=offscreen`: **31 passed, 0 failed**. The executable is newer than the reviewed source.

I could not independently run `ctest 19/19` or a warning-enabled rebuild: this read-only environment prevents CTest from writing its mandatory log files and prevents builds.