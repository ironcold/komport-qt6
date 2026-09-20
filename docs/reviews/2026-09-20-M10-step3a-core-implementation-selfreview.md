# M10 implementation self-review, step 3a: the replay core (`SessionReplayPlayer`)

Status: self-review of an implementation slice, written before its independent review. Date
2026-09-20. Slice 3a of `docs/specs/SPEC-M10-session-reader-passive-replay.md` §11, per the owner's
cut of 2026-09-20 ("A"): the replay **core**, specified by §5.5 and §5.6 under the
valid-mutation-context precondition of §5.11, with **no re-entrancy or mutation-authorization
logic of its own**.

## What the slice contains

| File | Change |
| --- | --- |
| `komport/sessionreplayplayer.h` | new: the five states, the seven operations, the result and report value types, the delivery signals, the injection constructor and the §5.10 seams |
| `komport/sessionreplayplayer.cpp` | new: the state machine, the timing arithmetic, the steps, the refusals, the completion path, the production scheduler (chunked single-shot `QTimer`) and the production clock (`QElapsedTimer`) |
| `komport/sessionreplayseams.h` | new: `SessionReplayScheduler` (§5.10); the clock reuses M9's existing `SessionMonotonicClock` |
| `tests/tst_sessionreplayplayer.cpp` | new: **29 test methods** (31 QTest entries), the core set of §7's player table |
| `CMakeLists.txt`, `tests/CMakeLists.txt` | the new source and the new test target |
| `tests/session_contract_compile.cpp` | the QtCore-only contract check for the new header (ADR-009) |

## The adoption condition: no hidden re-entrancy semantics

The owner requires 3a to be adopted only when it is demonstrably free of re-entrancy semantics.
The evidence:

1. **No identity, snapshot or guard state exists in the class.** A text search of both source
   files for the concepts §5.11 introduces returns nothing: no replay id, no schedule token, no
   operation snapshot, no authorization helper, no per-transition guard. The private section holds
   the session, the state, the timing policy, the position, the delivered count, the report/step
   helpers and the two seams - nothing that exists to defend against a receiver.
2. **The operations do rely on the precondition** - and that is the point of the cut. `play()`
   emits `stateChanged(Playing)` and then arms, `stop()` emits `stateChanged(Paused)` and returns
   that state's report, the delivery callback emits `eventDelivered` and then completes or arms, and
   the steps emit once per delivered event and continue; each of these reads live state after its
   own emission. That is unsupported behaviour under §5.11's precondition, not safe behaviour: the
   core is correct *because* no receiver calls back in, and 3b is what makes the other case defined
   (review: step-3a application verification, correction of an earlier wording here).
3. **The tests contain no callback affordance.** 3a's suite has no fixture hook that calls a player
   operation from a receiver, no `stop()`/`close()`/`start()` inside an emission, and no ordering
   beyond recording the emissions. The 3b tests that existed in the unsplit slice were removed
   with the layer they exercise; against 3a's code one of them segfaults on the dangling session
   reference, which is exactly the class §5.11 exists for. The unsplit implementation and its full
   suite are preserved outside the repository (`/tmp/m10-slice2-unsplit/`) for 3b.

## Disclosures (choices a reviewer should check)

1. ~~The delivery callback still checks the state.~~ **Withdrawn in the fix round:** the first
   3a review showed that the check was not defence-in-depth but hidden mutation-authorization
   logic (a receiver's `stop()` during the final delivery decided the completion, and a
   receiver's stop during a normal delivery decided whether the next delivery was armed). The
   guard is removed. The core now relies on the scheduling seam's contract, which the fix round
   made explicit in §5.10: **a scheduler implementation must not invoke a callback that
   `cancelPending()` removed** (amendment of 2026-09-20 in the same fix round). Without that
   guarantee the core could not be guard-free, so it is a precondition of this slice and not an
   assumption.
2. **`play()` while `Playing` refuses** with "the replay is already running". §5.5's state table
   lists `stop()`, `start()`, `close()` and `setTiming()` for `Playing` and not `play()`, so the
   operation is not accepted there; refusing with a reason keeps "no refusal is silent and no
   refusal changes the state" literal, where accepting it would re-arm the pending delivery with a
   fresh delay and stretch the replay. This is my reading of a gap in §5.5's table (the same choice
   was made in the unsplit slice); the reviewer should say if a no-op success is preferable.
3. **`report().finished`** is true when the replay reached the end, so a `stop()` executed while
   the replay is at the end would report `finished` true; 3a cannot reach that combination, because
   the completion path runs before any further operation can be called under a valid mutation
   context.
4. **Delays**: a positive nanosecond delay is rounded **up** to the next millisecond and a wait
   longer than one `QTimer` interval is chained (the timer cannot hold more than ~24.8 days per
   interval); zero stays immediate, i.e. still one delivery per callback. Scaling up clamps at
   `qint64` maximum instead of wrapping.
5. **A step's result** is computed from the values of the call itself (`advanced`, `matched`,
   `reachedEnd`); under a valid mutation context nothing can move them, which is why 3a needs no
   snapshot for this and §5.11's rule 3 is 3b's business.

## Evidence

- Warning-free build with `-Wall -Wextra`; `ctest` **19/19 targets**; `tst_sessionreplayplayer`
  **29 test methods**, 31 QTest pass entries; the widget-free contract check compiles the new
  header against `Qt6::Core`.
- §7's player table plans 27 tests. The 25 that hold under a valid mutation context are shipped
  here; the two the plan defers are `noSeekOperationExists` (a static inspection of the surface,
  §11 step 9) and `offlineUnitsNameNoWriteEntryPointOrTransport` (the no-write proof, §11 step 7).
  Three tests beyond the plan are core ones: `stepNextEventDeliversExactlyTheNextEvent`,
  `scalingUpClampsInsteadOfWrapping` and `thePlayerNeverConsultsWallClockTime` (the last one pins
  that the clock seam is never consulted, i.e. that the player has no second timing domain).
- The 3b test set of §5.11 is **not** part of this slice and will be added with it.

## Fix round after the 3a review

`docs/reviews/2026-09-20-M10-step3a-core-implementation-review.md` refused adoption ("3a must not
be adopted as cut") with two critical findings and three same-pass items. Disposition:

| Finding | Fix |
| --- | --- |
| Critical - `onDeliveryDue()` suppressed a callback on live state/session and continued after an emission; a receiver's `stop()` in the final delivery made the callback commit `Finished` with a `finished` report while `Paused` (rule 1/9 behaviour in the core) | The guard is **removed**: under the precondition and §5.10's cancellation guarantee the callback always belongs to a replay that is still playing. The completion path is now unconditional |
| Critical - the "scheduler defence-in-depth" disclosure did not hold (`play()` arming after its own state signal decided its outcome through that check) | same fix; the disclosure is withdrawn above |
| Same-pass - a comment claimed a schedule token and a replay id that 3a does not have | the comment in `start()` was rewritten to rely on §5.10's cancellation guarantee |
| Same-pass - the stop test's comment said "emits nothing" and did not assert the transition | the comment now names the two signals that are excluded, and the test asserts `stateChanged(Paused)` through the emission log |
| Same-pass - the evidence did not pin every refusal and every timing acceptance | new test `theTimingMatrixAndEveryRefusalLeaveThePlayerUnchanged`: every refusal carries a reason and leaves state/position/delivered/timing/event count unchanged (Idle: `play`, all three steps, null `start`; Finished: `play`, steps; Paused: nothing refused), every declared ladder value is accepted and readable in Idle, Ready, Paused and Finished, an unknown policy is refused, and a declared timing change emits no delivery and no completion |

Evidence after the fix round: warning-free build, `ctest` **19/19**,
`tst_sessionreplayplayer` **29 test methods** (31 QTest pass entries).
