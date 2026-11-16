# M10 step 3b design note: `mayMutateReplay(snapshot, phase, expectedPosition)` at every call site

Written 2026-09-20, before any 3b code, as the spec-first input its gate needs. Reference material:
the unsplit slice's implementation and re-entrancy tests, preserved at `/tmp/m10-slice2-unsplit/`;
its six failed rounds are the reason for the cut and are committed with step 3a.

## What 3b is

`docs/specs/SPEC-M10-session-reader-passive-replay.md` §5.11, added to step 3a's core: the two
identities (`mReplayId`, `mScheduleToken`), the immutable operation snapshot, and **one** private
authorization helper `mayMutateReplay(snapshot, phase, expectedPosition)` through which every state
mutation that can happen after an emission goes (3b's review, finding 2: `Arm` and `Continue` derive
their position from the entry snapshot inside the helper; only `StepContinue` uses the caller's
`expectedPosition`). No ad-hoc guards, the five phases (`Arm`, `Continue`, `Complete`, `StepContinue`, `Finish`) of §5.11's table, and the eleven
plus seven named tests of that section.

## The call sites and the phase each uses (as implemented; the *superseded* proposals of findings 1-2 further below are kept only as the record of what was tried first)

| Call site | Snapshot taken | Phase | What the phase permits |
| --- | --- | --- | --- |
| `play()` - arming the first delivery | before `stateChanged(Playing)` | `Arm` (the helper derives the position) | same replay, `Playing`, position equal to the snapshot's |
| `onDeliveryDue()` - continuing after a delivery | at callback entry | `Continue` | same replay, `Playing`, position `snapshot.position + 1` |
| `onDeliveryDue()` - committing the completion of the last event | at callback entry | `Complete` | same replay, `Playing` or `Paused` (a `stop()` in the delivery), schedule token exempt |
| `performStep()` - delivering the next event of a multi-event step | the step's **entry** snapshot | `StepContinue` (the one caller-derived coordinate) | same replay, the step's starting state, position `startPosition + deliveredSoFar` |
| `performStep()` - entering `Finished` at the end | at step entry | `Finish` | same replay, the step's starting state, position at the captured end |
| `stop()` / `close()` - cancelling a pending schedule | n/a | none (rule 10) | a canceller bumps the schedule token and cancels *before* it changes state, and does not authorize its own cancellation: cancelling is not a mutation |
| `setTiming()` | none | none (rule 6) | it changes only the policy, and an arming continuation reads the live policy |

## The gap this note flags: `play()`'s first arming has no phase

§5.11's helper says "for an arming phase the position is exactly `snapshot.position + 1`", and its
`Continue` row is "arming the next delivery **after a delivery**". `play()` arms *before* any
delivery: it captures its snapshot with the position at the start of the replay, emits
`stateChanged(Playing)`, and then arms the delivery of the event at that very position. Under the
approved predicate, `Continue` would deny that arming (the position is `snapshot.position`, not
`+ 1`), so a replay started by `play()` would never arm - the contract as written cannot be
implemented without an invention, which is exactly what 3b is supposed to avoid.

**Proposed amendment** (one phase added to §5.11's table, dated, for the gate to review):

| Phase | Used by | Permitted live state (with the identities above) |
| --- | --- | --- |
| `Arm` | arming the first delivery of a replay, from `play()` | the same replay; `Playing`; position equal to `snapshot.position` (nothing has been delivered by this continuation) |

Alternative considered and rejected: letting `Continue` take the expected position from the caller.
It would make the predicate dependent on each caller's arithmetic and so re-open the "distributed
logic" the cut closes; a named phase keeps the rule visible in one table.

## Second finding: a continuation's snapshot is taken before *its own* emission

§5.11 says an operation captures its snapshot "before its first synchronous emission", and the
position rules are written as `snapshot.position + 1`. That is exactly right for a call that emits
once, and it hides a second gap for a call that emits *several* times: a multi-event step delivers
event *k*, and the continuation that follows that delivery must be authorized with the position the
step has reached, not with the position of the step's first event. Read literally, a step could only
ever deliver its first event (`StepContinue` requires `snapshot.position + 1`, which the first
iteration does not satisfy) - the same shape of gap as `play()`'s missing `Arm` phase.

> **Superseded (3b's review, finding 2):** the per-continuation snapshot proposed here made the
> predicate a tautology and is withdrawn; what stands is the entry snapshot plus the explicit
> expected position, with `Arm`/`Continue` re-derived by the helper itself.

**Proposed clarification (withdrawn - superseded by finding 3 and by 3b's review, finding 2; kept
only as the record of what was tried first):** *each
continuation takes its own snapshot immediately before the emission it guards, and the position
rules are relative to that snapshot; the operation's entry snapshot remains the one that fixes its
result and its reports.* Then the step's `k`-th delivery is authorized with a per-delivery snapshot
whose position is the delivery's own index, `Finish` still uses the step's entry snapshot for
`reachedEnd`, and `play()`'s `Arm` phase stays the single case with no delivery behind it.

Both findings are the same class: **where a continuation's coordinates come from**. They are
deliberately reported here instead of being resolved in code, because §5.11 is the milestone's only
place for this mechanism and an implementation that invents the answer would recreate the very
problem the cut removed.

## Test plan (from §5.11, with the two evidence items the gates demanded)

The eleven re-entrancy tests preserved in `/tmp/m10-slice2-unsplit/tst_sessionreplayplayer.cpp`
(`stopFromWithinADeliverySlotIsHonoured`, `closeFromWithinADeliverySlotIsHonoured`,
`delayedReceiversSeeTheEventAfterAFirstSlotClosesThePlayer`,
`everyStepOperationSurvivesAReceiverThatActsOnThePlayer`,
`aCompletionReportIsImmutableAcrossAReceiverRestart`,
`aRestartDuringTheFinalDeliveryOwesNoCompletionToTheReplacement`,
`everyPlaySupersessionLeavesNothingArmed`, `aNoOpStopFromAFinishedReceiverStillReportsTheCompletion`,
`aStopReportSurvivesAReceiverThatActsOnTheStateSignal`, `aStopAtTheLastDeliveryStillCompletesTheReplay`,
`aStepResultIsASnapshotThatAReceiverCannotRewrite`), plus the seven §5.11 names that did not exist at 3a:
`anInterruptedMultiEventStepStopsAtTheInterruption`,
`mayMutateReplayAuthorizesOnlyItsOwnReplayAndPhase`,
`timingChangedFromADeliverySlotAppliesToTheNextArming`,
`aStopFromTheTransientPausedIsANoOpAndTheCompletionStillHappens`,
`aReceiverOfTheTransientPausedThatStartsOrClosesEndsTheCompletion`,
`aFinalStepDoesNotOverwriteAReceiverThatClosesOrRestarts` and `aStepContinuesThroughEverythingThatDoesNotSupersedeIt`.

Against step 3a's core the preserved tests fail (one of them segfaults on the dangling session
reference), which is the expected behaviour of the slice that adds the contract - and the reason
they were removed from 3a's suite rather than weakened.

## Order of work

1. This note's amendment (the `Arm` phase) through 3b's spec gate, together with the code.
2. Restore the identities, the snapshot and the helper in the core, and route every call site in
   the table above through it; keep 3a's suite untouched and green.
3. Add the eleven plus seven tests; build warning-free, `ctest` 19/19.
4. Self-review plus 3b's own gates, then commit.

## Third finding (found while implementing, 2026-09-20): a per-continuation snapshot makes the identity check vacuous

Implementing the table above as written produced a crash, and it is the same family once more. A
multi-event step takes a snapshot per delivery (the amendment of this note's second finding) and asks
the helper before each one. But a snapshot taken *inside* the loop carries the **current** identities:
after a receiver's `close()` - session null, state `Idle`, position 0, both identities bumped - the
next iteration's snapshot compares equal to the live player in every respect, so `sameReplay()`,
`StepContinue`'s state equality and the position equality all hold, the step delivers again and
dereferences `snapshot.session->events` on a null session. The identity of a continuation must come
from the **entry** snapshot (the replay the operation started on), never from a snapshot taken after
the receiver acted; otherwise the predicate degenerates into a tautology exactly when it is needed.

> **Refined by 3b's review, finding 2:** the explicit `expectedPosition` stays only where progress
> is inherently caller-derived (`StepContinue`); for `Arm` and `Continue` the helper re-derives the
> fixed relation to the entry snapshot instead of trusting the caller. The rest of this finding
> stands.

**Proposed refinement** (one clause in §5.11's predicate, no new mechanism): the helper takes the
continuation's **expected position** as an explicit input - `mayMutateReplay(snapshot, phase,
expectedPosition)` - with `snapshot` always the operation's entry snapshot. Each continuation then
names its own expectation in one expression: `snapshot.position` for `Arm`, `snapshot.position + 1`
for `Continue`, `snapshot.state` plus the entry snapshot's identity and `startPosition + advanced`
for `StepContinue`, and no position for `Complete`/`Finish`. That keeps a single authorization and
one identity, and it makes the reviewer able to read each call site's expectation directly instead
of deriving it from a second snapshot.

Until that clause is settled, 3b's implementation is incomplete on purpose: the machinery (the two
identities, the snapshot, the helper and the call-site wiring) is written and builds warning-free,
the eleven reference tests are restored, and the step tests fail - the multi-event step path is the
one this finding is about. Nothing is committed; step 3a (`41460f8`) is the safe baseline, and the
reference implementation remains at `/tmp/m10-slice2-unsplit/`.
