**Verdict: No — do not adopt or commit step 3b as it stands.**

**Disposition**

The added `play() then stop()` case exists and does execute an acting `stop()` in `Playing`. However, it does not isolate rule 8’s claimed reason that the `stop()` token bump refuses the outer step: the outer step began in `Ready`, so the preceding `play()` already changes the live state to `Playing` and independently refuses the `Ready` entry snapshot. The subsequent `stop()` changes it to `Paused`.

A valid pin would begin the outer step in `Paused`, then call `play(); stop();`: state returns to the entry state, leaving the schedule-token bump as the reason the outer continuation is refused.

The five non-interrupting rule-8 operations are properly exercised from both `Ready` and `Paused`.

**Acceptance-table rows still overstated**

- `aNoOpStopFromAFinishedReceiverStillReportsTheCompletion` claims the `stop()` return is the Finished no-op report, but the test discards that return value.
- `aStopFromTheTransientPausedIsANoOpAndTheCompletionStillHappens` claims nothing is armed, but never checks `scheduler.armed()`. It also claims the exact `"unknown timing policy"` reason but checks only that a reason is nonempty; `start(nullptr)` is not checked for a reason at all.
- The new interruption row’s causal claim—that the acting stop refuses the snapshot through its token bump—is not established for the reason above.

**New findings**

- The design note and self-review still say “eleven plus six” / 17 contract tests and 46 methods / 48 QTest entries, while the current suite has the added seventh test and the claimed 47 methods / 49 entries. See [design note](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-20-M10-step3b-design-note.md:14) and [self-review](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-20-M10-step3b-implementation-selfreview.md:23).
- The interruption test’s header still says `stop` interrupts generally, although its own no-op-stop row deliberately continues. [test](/home/max/Development/misc/komport-qt6/tests/tst_sessionreplayplayer.cpp:1662)

**Could not verify**

- The existing focused player binary exited successfully.
- I could not independently run the warning-enabled rebuild or full CTest: CTest cannot create its log file in this read-only environment.
- `git diff --check` reports an extra blank line at EOF in the specification.