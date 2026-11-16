## Verdict

**No — do not commit slice 2 yet.** The original long-delay and arithmetic fixes landed, but event-delivery re-entrancy remains unsafe.

## Prior-item disposition

1. **HIGH blocking, partially resolved.**  
   `onDeliveryDue()` now rejects stale automatic callbacks before and after `eventDelivered` ([sessionreplayplayer.cpp](/home/max/Development/misc/komport-qt6/komport/sessionreplayplayer.cpp:360)). `stop()` followed by `play()`, restart from a delivery slot, and a delayed stale callback after close/restart cannot deliver or re-arm the old replay. The new stop/close tests would catch restoration of the original post-emission dereference/re-arm bug.  
   However, see new HIGH findings below.

2. **HIGH blocking, resolved.**  
   [sessionreplayplayer.cpp:79](/home/max/Development/misc/komport-qt6/komport/sessionreplayplayer.cpp:79) waits in `INT_MAX`-ms chunks. Exact chunks, one-nanosecond remainders, and `qint64`-maximum delays retain the remainder; `INT_MAX * 1,000,000` is safely within `qint64`. `cancelPending()` clears timer, callback, remainder, and chunk state.

3. **MEDIUM same-pass, resolved.**  
   [tst_sessionreplayplayer.cpp:928](/home/max/Development/misc/komport-qt6/tests/tst_sessionreplayplayer.cpp:928) passes `qint64::max()` through `Scale0_1` and asserts exactly `qint64::max()`, distinguishing a clamp from a wrapped result.

4. **MEDIUM same-pass, resolved.**  
   [tst_sessionreplayplayer.cpp:903](/home/max/Development/misc/komport-qt6/tests/tst_sessionreplayplayer.cpp:903) positively covers `stepNextEvent()`: one event, match, position/count advance, no timer, then a second step.

5. **LOW same-pass, resolved.**  
   [sessionreplayplayer.cpp:121](/home/max/Development/misc/komport-qt6/komport/sessionreplayplayer.cpp:121) uses `QElapsedTimer`, not wall-clock time.

## New findings

1. **HIGH, blocking — `eventDelivered` still exposes a dangling event, and steps remain re-entrancy-unsafe.**  
   [sessionreplayplayer.cpp:386](/home/max/Development/misc/komport-qt6/komport/sessionreplayplayer.cpp:386), [sessionreplayplayer.cpp:474](/home/max/Development/misc/komport-qt6/komport/sessionreplayplayer.cpp:474).

   `deliverAtCurrentPosition()` emits a reference owned only by `mSession`. A first direct receiver may call `close()` or `start(newSession)`, releasing the old session while later direct receivers are still invoked with that reference. The current close test copies the event before closing, but has no subsequent receiver that reads it.

   More critically, `performStep()` continues after each synchronous emission without a generation/state/session check. For `stepNextEvent()`, a delivery-slot `close()` resets `mPosition` to zero; the loop immediately calls `deliverAtCurrentPosition()` again with a null `mSession`.

   Minimal fix: retain a local `SessionFilePtr` across every emission; capture/check an operation generation around every synchronous step delivery and stop the superseded operation safely if re-entered. Add ordered-slot tests: first slot closes/restarts, second reads the event; also cover close/start/stop+play from all step operations.

2. **HIGH, blocking — final `stateChanged(Finished)` can invalidate completion before `replayFinished()`.**  
   [sessionreplayplayer.cpp:378](/home/max/Development/misc/komport-qt6/komport/sessionreplayplayer.cpp:378).

   `setState(Finished)` emits synchronously. A `stateChanged` receiver can call `start(newSession)`; the old callback then emits `replayFinished(report())` for the new Ready replay (`delivered == 0`, remaining non-zero, `finished == false`). Thus an old callback can still produce an invalid/stale finish report.

   Minimal fix: make completion re-entrancy-safe—either invalidate and suppress the old completion after `setState()`, or capture a correct immutable completion report before notifying re-entrant receivers, with explicitly tested signal ordering/semantics.

3. **LOW, same-pass — self-review test accounting is inconsistent.**  
   [2026-09-19-M10-step3-player-implementation-selfreview.md:14](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-19-M10-step3-player-implementation-selfreview.md:14) says 26 methods, while line 46 correctly says 30. Lines 42–45 list four unplanned tests, but the current 30 methods require five beyond the 25 non-deferred planned tests; `stepNextEventDeliversExactlyTheNextEvent` is omitted from that accounting. The “before independent review” status is also stale for a document containing a fix-round record.  
   Minimal fix: correct the status and reconciled test mapping/counts.

Disclosure 5 is now accurate, and the accepted §5.10 correction is consistent with the code.

## Verification limits

I did not execute a build or tests in this read-only sandbox. I verified that the existing `LastTest.log` corresponds to binaries newer than the reviewed sources and records 19/19 targets and 32 QTest entries; the build cache contains `-Wall -Wextra`. That is supporting evidence, not an independent rerun.