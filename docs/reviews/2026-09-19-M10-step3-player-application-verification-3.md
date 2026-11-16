## Verdict

**No — do not commit slice 2 yet.** The two prior blocking defects are fixed, but three new re-entrancy defects remain.

1. **HIGH — `play()` must not return a refusal after it has entered `Playing`.**  
   The new post-signal guard prevents the old callback from being armed after `close()`, `start()`, or `stop()`—that part is correct. But [play()](/home/max/Development/misc/komport-qt6/komport/sessionreplayplayer.cpp:221) now returns `ok == false` after having changed state and emitted `stateChanged(Playing)`. This violates §5.5’s rule that a refusal changes nothing; its four specified refusal conditions were already passed.

   This is demonstrably wrong for `stop(); play()` in the `stateChanged(Playing)` receiver: the nested `play()` legitimately leaves a new callback armed, while the outer call returns a refusal. The receiver, not the outer call, armed it, but the returned refusal is still false.

   The correct completion of the accepted spec is **success**, not a new refusal: once the preconditions pass, `play()` succeeded, even if a synchronous receiver subsequently superseded it. A new “superseded” refusal would require an amendment to §5.5.

2. **HIGH — synchronous steps do not fully defend against re-entry.**  
   [performStep()](/home/max/Development/misc/komport-qt6/komport/sessionreplayplayer.cpp:507) checks only generation and session identity. An `eventDelivered` receiver can call `play()` or another step without changing either.

   For example, `stepNextRx()` emits an initial non-RX event; its receiver calls `play()`, which arms automatic delivery. The outer loop then continues synchronously through the target despite the player now being `Playing`, leaving automatic and step advancement active in the same operation. A nested step similarly changes `mPosition` without invalidating the outer operation.

   It also returns a false `matched == true` after a receiver closes/restarts during a nonmatching prefix event: the target was found during the initial search but was never delivered. The result must describe what that call actually delivered. Re-check state and expected position after each emission, stop on any supersession/position movement, and derive `matched` from the actual delivered target.

3. **HIGH — `stop()` returns a report after a re-entrant state signal has changed the player.**  
   [stop()](/home/max/Development/misc/komport-qt6/komport/sessionreplayplayer.cpp:240) emits `stateChanged(Paused)` and only then calls `report()`. A receiver can `close()` or `start(other)` during that signal, so a stop from position 1 can return position/count zero or facts about the replacement session. That contradicts §5.5’s preserved-position/count stop report.

   Capture the stop report before emitting `stateChanged(Paused)` and return that snapshot. There is also an untested boundary case: stopping from the final `eventDelivered` slot leaves `Paused` at end-of-stream, with no completion report; that conflicts with the table’s definition of `Paused` as having events remaining. The spec needs an explicit priority rule for stop-at-final-delivery versus automatic completion.

## Prior-item disposition

| Item | Disposition |
|---|---|
| `play()` re-arming after `stateChanged(Playing)` | Safety fix resolved: generation/state/session guard prevents the old call from arming after close/restart/stop. The new refusal policy is rejected as new HIGH finding 1. |
| No-op `stop()` suppressing completion | Resolved. Only `Playing` has a pending automatic delivery in the valid state machine; `Idle`/`Ready`/`Paused`/`Finished` no-op stops preserve generation. A restart from `Finished` still changes generation and suppresses the stale completion. |
| Test-accounting shortfall | Arithmetic resolved: independent comparison finds 27 planned, 35 shipped, precisely the two deferred tests absent, and 10 extras. The executed target reports 37 QTest entries. However, the self-review still says “26 test methods” and “before the independent review” at [lines 3 and 14](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-19-M10-step3-player-implementation-selfreview.md:3); this remains LOW same-pass record debt. |

Other tracing found `start()` and `close()` have no unsafe post-signal work; `setTiming()` and accessors emit nothing; a `stateChanged(Playing)` receiver’s direct step is correctly refused; and `replayFinished` has no post-emission assumptions.

I ran the current `tst_sessionreplayplayer` binary directly: **37 passed, 0 failed**. The existing build cache uses `-Wall -Wextra`, and its prior `LastTest.log` records 19/19 tests, but I did not rebuild or rerun full `ctest` in the read-only sandbox, so warning-free compilation and the full-suite result are not independently verified.