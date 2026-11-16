Verdict: **3a must not be adopted as cut, and it should not be committed as it stands.**

The central adoption condition fails. The core contains partial §5.11 re-entrancy semantics, despite lacking the required identities/snapshot/authorization helper.

| Finding | Severity | Disposition | Minimal fix |
|---|---|---|---|
| `onDeliveryDue()` conditionally suppresses a callback based on live state/session, and continues after `eventDelivered`. A receiver can call `stop()` during the final delivery: `report()` returns `finished == true` while `Paused`, then the callback commits `Finished` and emits `replayFinished`. That is §5.11 rules 1/9 behavior hidden in 3a. A non-final receiver-side stop also leaves another callback armed, which the state check suppresses. | Critical | Blocking | Move all receiver-aware continuation/completion behavior to 3b. In 3a, rely on the scheduler’s cancellation contract under the valid-context precondition; remove the live-state callback guard and the paused-at-end completion/report behavior. |
| The disclosure that the delivery state check is only scheduler defence-in-depth is not supported. It also affects direct signal re-entry: `play()` emits `stateChanged(Playing)`, a receiver can stop/close, then `play()` arms a callback whose outcome is decided by `onDeliveryDue()`’s state/session check. | Critical | Blocking | Same as above; do not retain this partial authorization behavior in the core. |
| The comment claims a “schedule token” and “replay id” invalidate callbacks, but neither exists in 3a. | Medium | Same-pass | Remove/rewrite the comment before 3a; those are explicitly 3b mechanisms. |
| The test comment says `stop()` “emits nothing,” contrary to §5.5’s required `stateChanged(Playing → Paused)`, and the test does not assert that transition. | Medium | Same-pass | Correct the comment and assert the emitted state sequence, while retaining the assertion that it emits no `eventDelivered`/`replayFinished`. |
| The evidence does not pin all stated refusal/timing-state semantics. In particular, no test establishes that a declared `setTiming()` succeeds in both `Idle` and `Finished`; nor do the tests exhaustively establish every refusal’s non-empty reason and unchanged state (e.g. `play()` in `Finished`, null `start()` unchanged). | Medium | Same-pass | Add a compact state/data-driven test covering declared timing acceptance in all allowed states and all refusal paths’ reason plus unchanged observable state. |

References: [sessionreplayplayer.cpp](/home/max/Development/misc/komport-qt6/komport/sessionreplayplayer.cpp:180), [sessionreplayplayer.cpp](/home/max/Development/misc/komport-qt6/komport/sessionreplayplayer.cpp:358), [sessionreplayplayer.cpp](/home/max/Development/misc/komport-qt6/komport/sessionreplayplayer.cpp:379), [tst_sessionreplayplayer.cpp](/home/max/Development/misc/komport-qt6/tests/tst_sessionreplayplayer.cpp:669), [tst_sessionreplayplayer.cpp](/home/max/Development/misc/komport-qt6/tests/tst_sessionreplayplayer.cpp:838).

On the second disclosure: refusing `play()` while already `Playing` is correct. The state table does not accept it, and a value-returned refusal preserves the no-state-change rule.

Other static conclusions:

- Under the stated precondition, the state machine, timing arithmetic, scaling/clamping, inclusive step behavior, completion report, close/release behavior, and QtCore-only seam design otherwise track §§5.5/5.6.
- The shipped set is correctly accounted as 25 planned core tests plus three justified additions: direct next-event stepping, overflow clamping, and proof that the clock seam is unused.
- Deferring `noSeekOperationExists` and the offline no-write audit is consistent with §11 steps 7 and 9.
- The friend-only injection path matches the existing recorder pattern; no production test API was widened. The contract-check/CMake additions are structurally consistent.
- The `Paused`-at-end behavior is precisely an impermissible dependency on §5.11 in 3a.

I could not run the build or test suite in this read-only sandbox. Thus the reported `-Wall -Wextra` clean build, `ctest` 19/19, and 30 QTest entries remain owner-reported evidence rather than independently verified results.