# M10 implementation self-review, slice 2: `SessionReplayPlayer` (SPEC-M10 §11 step 3)

Status: self-review of an implementation slice. It was written before the first review round
and is updated after every round; the fix rounds are recorded below, in order, and the
independent records are the authority for what each round found. Date 2026-09-19. Baseline: `v2/m10-session-reader-replay` at `18a3943` (slice 1
committed and pushed).

## What the slice contains

| File | Change |
| --- | --- |
| `komport/sessionreplayseams.h` | new: `SessionReplayScheduler`; reuses M9's `SessionMonotonicClock` |
| `komport/sessionreplayplayer.h` | new: `SessionReplayTiming`, `SessionReplayState`, the four result values, `SessionReplayPlayer` |
| `komport/sessionreplayplayer.cpp` | new: the player, its delay arithmetic and the production `QTimer`/clock defaults |
| `tests/tst_sessionreplayplayer.cpp` | new: the player's test methods (37 at this revision) |
| `tests/CMakeLists.txt`, `CMakeLists.txt`, `tests/session_contract_compile.cpp` | wiring: the new target, the unit in `komport_core` and in the widget-free contract check |

Not in this slice: the reply-free rendering entry points (§11 step 4), the document and UI
wiring (steps 5-8). The player is complete on its own.

## Contract compliance, decision by decision

| SPEC-M10 | Implementation |
| --- | --- |
| §5.5 the seven operations, five states, the transition list, the state table | `start/play/stop/stepNextEvent/stepNextRx/stepNextTx/close`, `Idle/Ready/Playing/Paused/Finished`, `setState()` emits on a real change only |
| §5.5 every refusal is a value with a reason | `SessionReplayStart`, `SessionReplayStep`, `SessionReplayTimingChange`, and `SessionReplayReport` for the stop; no refusal is silent |
| §5.5 `play()` refused in `Idle`, for zero events, at the end and in `Step` | all four refusals, each with its own reason |
| §5.5/§5.6 `Step` never coexists with automatic advancement | `play()` in `Step` refuses; `setTiming(Step)` while `Playing` refuses with "pause the replay before selecting step mode" and leaves state, policy and the armed delivery untouched |
| §5.6 the delay table | `delayNsFor()`: first event 0; `Original`/`Scale1` the stored gap; `Immediate` 0; ×10/×4/×2 with checked multiplication and clamping; ÷2/÷5/÷10 truncating; `Step` never scheduled |
| §5.6 one delivery per callback, always through the seam | `scheduleNextDelivery()` on every path, including `Immediate`; a full replay never delivers synchronously |
| §5.6 a change of policy applies to the next scheduled delivery | the pending callback is never re-computed (`changingTimingAppliesToTheNextEventOnly`) |
| §5.6 stored timing is never rewritten | the player only reads `SessionFile::events`; the session is `shared_ptr<const SessionFile>` |
| §5.6 steps | inclusive prefix, the match delivered last, `advanced`/`matched`/`reachedEnd` reported, no match advances to the end and leaves `Finished`, steps refused in `Idle`/`Playing`/`Finished`, synchronous and never moving backwards |
| §5.5 `close()` from every state, cannot refuse | cancels the pending delivery, releases the session, returns to `Idle` |
| §5.5 `eventDelivered` by `const` reference, one stored event per delivery | `deliverAtCurrentPosition()` emits the stored value; `deliveredEventsAreTheStoredEvents` compares every field |
| §5.5 threading, no new thread, no blocking | no thread, no queued connection, no loop over the session outside a step (which is user-driven and bounded by the session length) |

## Test mapping (§7 player table → shipped test)

§7's player table plans 27 tests. 25 are shipped here; the two the plan itself defers are
`noSeekOperationExists` (a static inspection of the surface, step 9) and
`offlineUnitsNameNoWriteEntryPointOrTransport` (the source/dependency audit, step 7). The
slice adds **twelve** tests the table does not name, which §8's clause about unlisted tests
covers - `thePlayerNeverConsultsWallClockTime` (the evidence for the clock seam) plus the
eleven the three fix rounds added for the re-entrancy contract:

| Added test | Why |
| --- | --- |
| `stepNextEventDeliversExactlyTheNextEvent` | positive `stepNextEvent()` coverage |
| `scalingUpClampsInsteadOfWrapping` | the checked-multiplication clamp |
| `stopFromWithinADeliverySlotIsHonoured`, `closeFromWithinADeliverySlotIsHonoured` | a receiver acting from a delivery slot |
| `delayedReceiversSeeTheEventAfterAFirstSlotClosesThePlayer` | the emitted event outlives the session reference |
| `everyStepOperationSurvivesAReceiverThatActsOnThePlayer` | three step operations × close / restart / play / nested step |
| `aCompletionReportIsImmutableAcrossAReceiverRestart` | the completion report is captured, never recomputed |
| `everyPlaySupersessionLeavesNothingArmed` | close / restart / stop / stop+play from the state signal |
| `aNoOpStopFromAFinishedReceiverStillReportsTheCompletion` | a no-op stop cannot swallow the completion |
| `aStopReportSurvivesAReceiverThatActsOnTheStateSignal` | the stop report is captured before its state change |
| `aStopAtTheLastDeliveryStillCompletesTheReplay` | the amended completion-priority rule |
| `aRestartDuringTheFinalDeliveryOwesNoCompletionToTheReplacement` | the replay-identity rule |
| `aStepResultIsASnapshotThatAReceiverCannotRewrite` | the step result is a snapshot |

Shipped: **39 test methods** (25 planned + 14 extras; QTest reports 41 pass entries, including
`initTestCase`/`cleanupTestCase`). A scripted comparison of §7's table against the shipped
method list produced these numbers (review: slice-2 application verification 2, 3 and 4,
same-pass items - two earlier enumerations in this section were short by one and by two).

## Fix round after the independent slice review

`docs/reviews/2026-09-19-M10-step3-player-implementation-review.md` blocked the slice with
two blocking findings and three same-pass items. Disposition (the review is unchanged; this
is the fix record):

| Review item | Disposition |
| --- | --- |
| 1 HIGH blocking - a receiver of `eventDelivered` may call `close()`/`stop()`, after which the delivery dereferenced the released session or re-armed past a stop | fixed: a **generation token**. `start()`, `stop()` and `close()` bump `mGeneration`; every scheduled callback carries the generation it was armed with, and `onDeliveryDue()` re-checks generation, state and session **after** the emission before touching anything or arming the next delivery. Two new tests drive the re-entrancy directly (`stopFromWithinADeliverySlotIsHonoured`, `closeFromWithinADeliverySlotIsHonoured`) |
| 2 HIGH blocking - the production scheduler capped a delay at `INT_MAX` ms, so a long delay fired orders of magnitude early | fixed: the delay is now waited out in consecutive `INT_MAX`-ms chunks (`startChunk()`/`onChunkElapsed()`), so the specified delay is honoured; the remainder is carried, never shortened |
| 3 MEDIUM - the multiplication cases never overflow, so the clamp is unproven | fixed: `scalingUpClampsInsteadOfWrapping` uses a stored gap of `qint64` maximum and asserts the scheduler receives exactly that (and the scaled-down case stays finite) |
| 4 MEDIUM - `stepNextEvent()` had only refusal coverage | fixed: `stepNextEventDeliversExactlyTheNextEvent` asserts one delivered event, `advanced == 1`, `matched`, the position advance and repeated stepping |
| 5 LOW - the production clock used wall-clock `QDateTime` while implementing a monotonic contract | fixed: `ElapsedMonotonicClock` with `QElapsedTimer`, M9's pattern |

### Second fix round (after the application verification)

Note on the trail: the second application round could not run on its first attempt (the
reviewer CLI answered `You've hit your usage limit ... try again at 1:50 PM` at 10:52 and
wrote no record). The re-run succeeded the same day at 23:53 and produced
`docs/reviews/2026-09-19-M10-step3-player-application-verification-2.md`, so both rounds are
on file. The failed attempt is recorded here because the trail has to show the difference
between "no record because the round failed" and "no record because nothing was found".

### Fourth fix round (after the third application round)

That round resolved both of the second round's items, **rejected** the refusal I had invented
for the superseded `play()` (rightly: the preconditions were passed and the state did change,
so a refusal would be untrue), and raised two further blocking findings plus one documentation
item. Disposition:

| Review item | Disposition |
| --- | --- |
| 1 HIGH blocking - `play()` must not return a refusal once it has entered `Playing` | fixed: the superseded case returns `ok == true` and simply arms nothing (a nested `play()` of the receiver keeps the schedule it legitimately armed). The test `everyPlaySupersessionLeavesNothingArmed` now asserts success plus the resulting state and schedule for close / restart / stop / stop+play |
| 2 HIGH blocking - a synchronous step defended only against generation and session, so a receiver calling `play()` or a step (neither changes the generation) let the outer step run on; and `matched` could be claimed for a target that was never delivered | fixed: the step snapshots generation, session, **state** and the position it expects next, stops at the first delivery after which any of them moved, reports the deliveries it actually made, and claims `matched` only when the target itself went out. `everyStepOperationSurvivesAReceiverThatActsOnThePlayer` now runs three step operations × close / restart / play / nested step |
| 3 HIGH blocking - `stop()` computed its report **after** emitting `stateChanged(Paused)`, so a receiver could make the report describe a different replay; and stopping inside the final delivery left `Paused` at the end of the stream, which contradicts the state table | fixed in two parts: the report is captured before the state change is emitted (test `aStopReportSurvivesAReceiverThatActsOnTheStateSignal`), and SPEC-M10 §5.5 now states the priority rule - the completion of a replay that delivered its last event outranks a `stop()` that arrives while that event is being delivered (test `aStopAtTheLastDeliveryStillCompletesTheReplay`) |
| LOW same-pass - the self-review's counts and its opening "before the independent review" line were stale | fixed: the mapping section carries the scripted comparison (25 planned + 12 extras = 37) with a table naming every extra, and the status line now says that the document is updated after every round |

One more amendment went in with this round: §5.5's **general re-entrancy rule**, the general
form of the four cases the three rounds found one by one. Three fix rounds each found another
instance of the same contract gap, so the specification now states the rule once - no operation
may assume the player is unchanged after an emission, and a superseded operation stops safely
and reports what it actually did - instead of leaving the next corner to the next review.

### Fifth fix round (after the fourth application round) - the re-cut, not another patch

That round resolved the three earlier blocking items, raised three new ones, and recommended
**re-cutting the slice around an explicit per-operation identity/snapshot model instead of
another local guard**. I followed that recommendation (it is the reviewer's advice, implemented
on my judgement, not an owner decision):

- **`mGeneration` is gone**; the player now carries two explicit identities. `mReplayId` is the
  identity of the *replay* - bumped by `start()` and `close()`, never by `stop()`, which
  preserves the position and therefore the replay. `mScheduleToken` is the identity of the
  *pending schedule* - bumped by everything that cancels one (`start()`, `stop()`, `close()`).
  Every scheduled callback carries both; the completion is decided by "the last event of *this*
  replay" (`replayId`), which a session pointer cannot express, and the guards in `play()`,
  `onDeliveryDue()` and `performStep()` all work on that pair plus state and position instead of
  the single counter they used to share.
- Three amendments went into SPEC-M10 with this round: (a) the completion rule now names the
  replay identity, so a restart during the final delivery - even with the same session - owes
  nothing to the old callback; (b) §5.6 states that a step's result is a snapshot of the replay
  the call advanced ("`reachedEnd` means this call took its replay to the end, not that the
  player is at the end now") and that "leaves the player `Finished`" holds unless a receiver
  supersedes it; (c) §5.5's `stop()` wording no longer says "emits nothing" - the transition is
  announced through `stateChanged`, and what `stop()` never emits is an event delivery or a
  report of its own.
- Two tests encode the new rules: `aRestartDuringTheFinalDeliveryOwesNoCompletionToTheReplacement`
  (the reviewer's chained scenario: the old callback must not report for a replacement that a
  step advanced, and a step never reports a completion) and
  `aStepResultIsASnapshotThatAReceiverCannotRewrite`.

**Cycle count.** This is the fourth fix cycle of this slice. If the next application round
finds further defects of the same re-entrancy family, the process's rule applies: pause and put
a milestone re-cut to the owner instead of starting a fifth patch cycle. The general re-entrancy
rule in §5.5 and the identity model are the two structural answers that were missing, so the
expectation is that the family is now closed rather than that another corner exists.

### Sixth round: the family is not closed, so the slice stops here

`docs/reviews/2026-09-20-M10-step3-player-application-verification-6.md` resolved the two items
of the fifth round (the snapshot-based step result and the contradictory `stop()` sentence),
found **one more HIGH of the same family** - a final synchronous step calls `setState(Finished)`
without checking that its replay still owns the live player, so a receiver that closes mid-step
is overwritten to `Finished` - and stated its cycle judgement plainly: **"this should be
re-cut/split for owner review, not patched locally again."**

The process rule ("count your cycles": after five review-fix cycles with unresolved critical
findings, pause and propose re-cutting the milestone or sharpening the spec instead of starting
another text cycle) therefore applies, and I stopped. The slice is **not committed**, the
seventh patch cycle was not started, and the decision is documented in
`docs/reviews/2026-09-20-M10-step3-recut-proposal.md` for the owner: split the slice into the
re-entrancy-free state machine and the re-entrancy contract (recommended), finish it in one
slice against the reviewer's advice, or ship it with the re-entrancy corner as a documented
limitation. The snapshot re-cut of this round stays in the tree as the work it is - verified for
both of its findings, incomplete only at the permission layer.

### Third fix round (after the second application round)

That round resolved both of the second round's items and raised two **new** blocking findings
of the same family. Disposition:

| Review item | Disposition |
| --- | --- |
| 1 HIGH blocking - `play()` emitted `stateChanged(Playing)` and *then* armed a delivery unconditionally, so a receiver that closed, restarted or stopped the player during that signal still ended up with a callback armed (and a restart is specified to re-arm nothing) | fixed: `play()` snapshots the generation before the state change and re-checks generation, state and session after it; when a receiver superseded the start, it arms nothing and returns `ok == false` with "the replay was superseded while it was starting" instead of reporting a success that no longer exists. New test `everyPlaySupersessionLeavesNothingArmed` covers close, restart with the same session, restart with a different session and stop |
| 2 HIGH blocking - `stop()` bumped the generation in every state, so an accepted no-op `stop()` from a `stateChanged(Finished)` receiver suppressed the completion report that transition owes | fixed: only a `Playing` stop invalidates (bump + cancel + `Paused`); a stop in `Idle`/`Ready`/`Paused`/`Finished` is a pure no-op report again. New test `aNoOpStopFromAFinishedReceiverStillReportsTheCompletion` |
| same-pass - the self-review's test accounting was short by one (and by two before that) | fixed: the mapping section now carries the scripted comparison of §7's table against the shipped method list (25 planned + 10 extras = 35) and names every extra |

The new refusal reason of finding 1 is an addition to §5.5's list of `play()` refusals, which
does not name this case (it is unreachable unless a receiver of `stateChanged` acts inside the
call). It is disclosed as choice 8 below for the reviewer to confirm or reject.

The application verification (`docs/reviews/2026-09-19-M10-step3-player-application-verification.md`)
confirmed the two earlier blocking fixes and raised two **new** blocking findings of the same
family - re-entrancy from a signal receiver. Disposition:

| Review item | Disposition |
| --- | --- |
| 1 HIGH blocking - `eventDelivered` could hand a *dangling* event to receivers that run after one that closed/restarted the player, and `performStep()` kept delivering after a receiver reset the position (null dereference) | fixed twice over: `deliverAtCurrentPosition()` now holds a local `SessionFilePtr` across the whole emission, so the event and its session outlive every receiver; `performStep()` works against a snapshot (generation + session) and stops at the first delivery after which a receiver changed either, reporting the events it actually delivered. Three new tests: `delayedReceiversSeeTheEventAfterAFirstSlotClosesThePlayer`, `everyStepOperationSurvivesAReceiverThatActsOnThePlayer` (all three step operations × close/restart), `aCompletionSupersededByARestartIsNotReported` |
| 2 HIGH blocking - `setState(Finished)` emits synchronously, so a `stateChanged` receiver could restart the replay and the old callback then emitted `replayFinished` computed from the *new* state | fixed: the completion report is captured **before** the state change is emitted, and `replayFinished` is suppressed when a receiver superseded the completion (generation or state changed). The order stays state change first, then the report, and `replayFinishedReportsDeliveredRemainingAndTiming` now pins that order through the fixture's signal log |

## Disclosures (choices a reviewer should check)

1. **`play()` while `Playing` refuses** with "the replay is already running". SPEC-M10 §5.5's
   state table lists `stop()`, `start()`, `close()` and `setTiming()` for `Playing` and does
   not define `play()` there. Accepting it would re-arm the pending delivery with a fresh
   delay and stretch the replay, which contradicts "no refusal changes the state"; the
   refusal keeps every rule literal. A test found this gap. If the reviewer prefers a no-op
   success instead, that is a one-line change.
2. **The clock seam is M9's existing type, and §5.10 was corrected accordingly.** The
   accepted §5.10 block sketched a second `SessionMonotonicClock` with a `const` accessor;
   M9's accepted `sessionrecorderseams.h` already declares that name with a non-const
   accessor, so two declarations could not coexist in one library. The specification now
   states the reuse (correction of 2026-09-19 in §5.10) and the player never consults the
   clock at all - it exists so that a test can prove that (`thePlayerNeverConsultsWallClockTime`).
3. **A step that reaches the end emits `stateChanged(Finished)` but no `replayFinished`.**
   §5.5 gives `replayFinished` to "a `Playing` replay that delivers its last event" and
   §5.6 says a step reports through its result value; the step's own `reachedEnd` and the
   state change carry the information. Recorded so a reviewer can disagree explicitly.
4. **`stateChanged` is emitted only when the state actually changes**, so `close()` in `Idle`
   is silent rather than emitting `stateChanged(Idle)`. §5.5 says `close()` "emits nothing
   but the state change"; nothing is claimed for a no-op close.
5. **The production timer rounds a positive delay up** to the next whole millisecond (a
   nanosecond timeline against a millisecond timer) and waits out a delay longer than one
   `QTimer` interval in consecutive chunks rather than shortening it (after review finding 2:
   an earlier version capped the interval at `INT_MAX` ms, which would have fired a very long
   delay far too early). Zero stays zero, so `Immediate` still turns the event loop. ADR-005
   makes real-world precision best-effort; the seam keeps the arithmetic exact and testable.
6. **A hand-built session with a non-monotonic timeline** would produce delay 0 for the
   offending pair instead of a negative delay; the loader refuses such a stream in
   production, so this is defensive only.
7. **`eventCount()` is 0 while `Idle`** (no session attached); after `start()` with a
   zero-event session it is 0 as well, which is the session's real size.
8. **A `play()` superseded by a receiver of `stateChanged(Playing)` reports success and arms
   nothing.** I first returned a refusal here; the third application round rejected that
   (finding 1) and the code now returns `ok == true`: the four refusal conditions of §5.5 were
   passed and the state did change to `Playing`, so a refusal would have been untrue and would
   have contradicted "a refusal changes nothing". What the call must not do is arm a delivery
   the receiver just cancelled or re-arm over the delivery a nested `play()` of that receiver
   armed - neither happens. The state the caller observes is whatever the receiver left, which
   is the general re-entrancy rule of §5.5.

## Evidence

- Warning-free build with `-Wall -Wextra`; `ctest` **19/19 targets** (18 + the new player
  target); `tst_sessionreplayplayer` **39 test methods**, 41 QTest pass entries.
- The widget-free contract check now compiles `sessionreplayplayer.cpp` against `Qt6::Core`
  alone (ADR-009), so a widget or transport type entering the player breaks the build.

## Corrections to my own work during this slice

- Three test failures in the first run were mine, not the player's: a fixture whose step
  match *was* the last event (so `reachedEnd` was legitimately true and the fixture now
  carries a fourth event), the `play()`-while-`Playing` gap (item 1 above, now an explicit
  refusal), and a clock test that stepped after a finished replay without restarting first.
- The `mOwnedScheduler`/`mOwnedClock` members were missing from the header on first write;
  the single production path creates both defaults in one constructor, so an injected seam
  and the default behaviour differ in nothing but the seam object.
