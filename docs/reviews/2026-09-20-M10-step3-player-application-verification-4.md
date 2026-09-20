## Verdict

**No — do not commit slice 2 as it stands.**

Two HIGH re-entrancy defects remain, plus a HIGH normative contradiction. This is the third consecutive fix cycle finding the same family; I recommend re-cutting the replay-player re-entrancy slice around an explicit per-replay-operation identity/snapshot model before another patch, rather than adding another local guard.

## New blocking findings

1. **HIGH — a restart of the same `SessionFilePtr` during the final delivery can spuriously emit `replayFinished`.**

   [`onDeliveryDue()`](/home/max/Development/misc/komport-qt6/komport/sessionreplayplayer.cpp:406) identifies the old replay only by session pointer and end position. A receiver of the final `eventDelivered` can `start(theSameSession)`, and a chained `stateChanged(Ready)` receiver can step that replacement to its end. The outer callback then sees the same pointer at end and emits `replayFinished`—although §5.5 expressly says a `start()` during delivery replaces the old replay and owes no old completion. The replacement was completed by a step, which must not emit `replayFinished`.

   The existing completion test only exercises mutation from `stateChanged(Finished)`, where the amendment deliberately preserves the captured report; it does not cover this before-completion, same-pointer chain.

2. **HIGH — a final synchronous step can return an outcome contradicted by the player state a receiver leaves behind.**

   [`performStep()`](/home/max/Development/misc/komport-qt6/komport/sessionreplayplayer.cpp:557) sets `reachedEnd == true`, then emits `stateChanged(Finished)`. A receiver can `close()` or `start()` during that signal. The returned result says the position is at end “after the call” ([header](/home/max/Development/misc/komport-qt6/komport/sessionreplayplayer.h:69)), while the actual position is now zero or a replacement session’s position; §5.6 also says the step leaves the player `Finished`.

   This is precisely the case the new general rule must define: either step results are immutable snapshots of the replay they advanced, or they describe the final player state. The spec currently says both.

3. **HIGH — §5.5 contradicts the implementation and its own state-transition model for `stop()`.**

   §5.5 says `stop()` “emits nothing” ([spec](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:507)), including in the new priority text, but `stop()` transitions `Playing → Paused` through `setState()` and therefore emits `stateChanged(Paused)`. The new test explicitly relies on that signal. The state table and code are coherent; the normative “emits nothing” wording is not. It must be amended to distinguish `stateChanged` from `eventDelivered`/`replayFinished`, including the last-delivery priority case.

## Prior-item disposition

| Prior item | Disposition |
|---|---|
| `play()` returned refusal after entering `Playing` | **Resolved.** It now returns success after the four preconditions pass, while the post-signal guard avoids arming over a receiver’s close/restart/stop or nested `play()`. |
| Steps ran past a receiver takeover / falsely claimed match | **Resolved for the stated paths.** Generation, session, state, and expected position are checked per delivery; `advanced` and `matched` are derived from actual outer deliveries. The final-step result issue above is a separate remaining boundary. |
| `stop()` report captured after `stateChanged`, and stop-at-final left `Paused` | **Resolved for the direct cases.** The stop report is captured before `stateChanged(Paused)`; a last-delivery `stop()` reaches `Finished` and emits the captured completion report. The same-session replacement chain above still violates the amended completion rule. |
| Counts and stale status line | **Resolved.** Static count matches 27 planned player rows, 25 implemented planned tests, two deferred tests, and 12 named extras: 37 methods. The self-review status is updated. |

The historical self-review text still says completion was “suppressed when a receiver superseded” it ([self-review](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-19-M10-step3-player-implementation-selfreview.md:129)); that is now superseded by the amended capture rule and should be labelled as historical to avoid misleading future reviewers. **LOW.**

The delay arithmetic, one-callback delivery, refusal values, normal step semantics, clock non-use, and deferred-test accounting otherwise match the accepted spec on source inspection.

I did not modify files or rerun builds/tests. The existing build cache records `-Wall -Wextra`, and its latest `ctest` log records 19/19 with `tst_sessionreplayplayer` at 39 QTest pass entries, but warning-free compilation and runtime results were not independently re-executed in this read-only review.