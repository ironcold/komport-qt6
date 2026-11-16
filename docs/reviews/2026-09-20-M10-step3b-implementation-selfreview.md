# M10 implementation self-review, step 3b: re-entrancy and mutation authorization

Status: self-review of an implementation slice, before its independent review. Date 2026-09-20.
Slice 3b of `docs/specs/SPEC-M10-session-reader-passive-replay.md` §11: the contract of §5.11,
added to step 3a's guard-free core (`41460f8`). Design input: the step-3b design note, which
records the three findings the implementation work produced.

## What the slice contains

- `komport/sessionreplayplayer.{h,cpp}`: the two identities (`mScheduleToken` = the pending
  schedule, `mReplayId` = the replay), the `OperationSnapshot` (schedule token, replay id, the
  session - kept alive, so a stream cannot die beneath an emission - state, position, delivered
  count, timing), and **one** private authorization,
  `mayMutateReplay(snapshot, phase, expectedPosition)`, called at every mutation site that can
  follow an emission. No other guard exists in the class.
- The call sites and their phases: `play()` arming -> `Arm` with `snapshot.position`; the delivery
  callback continuing -> `Continue` with `snapshot.position + 1`; the completion commit ->
  `Complete` (no position; the only phase the schedule token cannot deny, rule 1); a multi-event
  step's *k*-th delivery -> `StepContinue` with `startPosition + deliveredSoFar` against the step's
  entry snapshot; the step reaching the end -> `Finish`.
- `deliverAtCurrentPosition(snapshot)` takes the snapshot, so the emitted reference cannot dangle
  when a receiver closes the player.
- `tests/tst_sessionreplayplayer.cpp`: the 18 tests of §5.11 - the eleven re-entrancy tests of the
  unsplit slice (they fail or crash against 3a's core, which is why they belong to this slice),
  and seven new ones: the interrupted multi-event step (five receiver actions), the direct predicate
  test through the §5.10 seam, the timing change from a delivery slot, the transient `Paused`
  matrix, the transient-`Paused` supersession, and the final step that must not overwrite a
  receiver's state.
- `SPEC-M10 §5.11`: the slice's own amendments, all dated and listed in the design note - the
  `Arm` phase, the entry-snapshot + explicit-`expectedPosition` clause, and the corrected
  `StepContinue` position rule.
- The friend-for-testability seam gains the identities, a snapshot builder and the predicate, so the
  authorization is testable directly without widening the production API.

## Evidence

Warning-free build with `-Wall -Wextra`; `ctest` **19/19**; `tst_sessionreplayplayer` **47 test
methods** (49 QTest pass entries: 29 core + 18 contract).

## Disclosures (choices a reviewer should check)

1. **A `stop()` during a synchronous step in `Ready`/`Paused` is a no-op** (§5.5 defines `stop()`
   as accepted-but-inactive outside `Playing`), so a step is interruptible by a *supersession*
   (`play()`, `close()`, `start()`, a nested step) but not by a no-op stop. The interrupted-step
   test pins exactly that, with the reason in the row. My first expectation here was wrong and the
   suite corrected me; if the owner wants a step to be interruptible by a stop, that is a new
   mechanism (a pending-stop flag) and a specification decision, not a fix in this slice.
2. **The phase names the state it requires; the position is the caller's own claim.** The predicate
   cannot detect a call site that names the wrong position, which is why every call site's
   expression is listed above and in §5.11's table. The direct predicate test asserts the parts the
   predicate *can* decide (identity, token with the `Complete` exemption, state, and the equality of
   the named and live positions).
3. **`Complete` exempts the schedule token but never the identity**: a replay that was released or
   replaced during its final delivery owes no completion (rule 9's conditional branch), and the
   tests pin both branches.
4. **§5.11's wording was corrected three times while this slice was implemented** - the missing
   `Arm` phase, the snapshot's origin, and `StepContinue`'s position rule - each time because the
   text as written could not be implemented without inventing a rule. The design note holds all
   three with their reasoning; the amendments are dated in the section itself.


## Fix round after 3b's review

`docs/reviews/2026-09-20-M10-step3b-implementation-review.md` refused adoption with three
blocking findings and one same-pass item. Disposition:

| Finding | Fix |
| --- | --- |
| Critical - §5.11 rule 8 says a receiver's `stop()` from an interim delivery ends the step, but the code (and my test) let a no-op `stop()` continue it | Normative, dated amendment to rule 8: a `stop()` ends the step **when it does something**, i.e. in `Playing`, where rule 10's schedule-token bump stops the step's entry snapshot from authorizing; a no-op `stop()` in `Ready`/`Paused` changes nothing and does not interrupt a synchronous step, which is interruptible by a supersession (`play()`, `close()`, `start()`, a nested step). The test keeps its row and now cites the reason |
| High - `expectedPosition` was caller-trusted for phases the table fixes, so `Arm` and `Continue` had identical predicates, and my direct test declared a pre-delivery `Continue` authorized | The helper derives `Arm` (`snapshot.position`) and `Continue` (`snapshot.position + 1`) itself; only `StepContinue` consults the caller's `expectedPosition`, which is documented as the one inherently caller-derived coordinate. §5.11's predicate and the two table rows were reworded to match, and the direct test now asserts both the derived relations (including that a caller argument cannot talk `Continue` into anything) and `StepContinue`'s positive/wrong-coordinate cases |
| High - the test for the final-stop sequence did not inspect the emission log, and the transient-`Paused` matrix had no timing assertion for the first stop | The final-delivery test now clears the log and pins the sequence exactly - `event:3`, `state:Paused`, `state:Finished`, `finished`, four entries and nothing else; the transient matrix asserts the stopped replay's captured timing as well as the completion report's |
| Medium - the two-argument helper spelling survived in the normative text and the checklist, and the design note left a superseded proposal unmarked | All four places now read `mayMutateReplay(snapshot, phase, expectedPosition)`, and the design note marks its per-continuation-snapshot proposal and its caller-trusted-position proposal as superseded by finding 3, pointing at what stands |

Evidence after the fix round: warning-free build, `ctest` **19/19**,
`tst_sessionreplayplayer` **49 QTest pass entries** (47 methods), and the suite's own failures caught
my two wrong expectations inside the direct predicate test before this round was dispatched.
