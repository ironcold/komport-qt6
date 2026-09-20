## Verdict

**Amend before adoption.** The direct amendments resolve the multi-event phase, final-stop baseline sequence, and cancellation-order finding, but a re-entrant `stateChanged(Paused)` case now leaves rule 9 contradictory. The text is not yet a safe basis for 3b; 3a remains cleanly separable.

## Disposition

| Item | Disposition | Evidence |
|---|---|---|
| Helper predicate / multi-event continuation | Resolved for the ordinary case | `StepContinue` supplies the missing state and position authorization, and the interruption test has concrete assertions. [§5.11:979](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:979), [§5.11:1014](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:1014), [§5.11:1056](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:1056) |
| Completion / final-delivery `stop()` | Partly resolved; new blocker below | The specified direct sequence, report, and no-op-from-`Finished` case are now explicit. [§5.11:1019](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:1019), [§5.11:1059](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:1059) |
| A2 evidence | Resolved as requested | The targeted interruption and no-op completion tests are named with assertions; the criterion lists the phase-aware helper. [§5.11:1053](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:1053), [§8:1289](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:1289) |
| Stop signal wording | Resolved | The normal `Playing → Paused` stop test correctly permits `stateChanged` while forbidding delivery/completion signals. [§7:1190](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:1190) |
| Sole location / closing claim | Not resolved | §6 still independently norms an unqualified delivery-boundary rule: “`stop()` takes effect at a delivery boundary.” [§6:1100](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:1100) |
| Cancellation ordering | Resolved operationally, but internally contradictory | Rule 10 gives the requested bump/cancel-before-state-change order. [§5.11:1027](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:1027) However, it directly conflicts with “Every mutation site calls it — … cancelling” and the `Cancel` phase. [§5.11:978](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:978), [§5.11:982](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:982) |

## New findings

1. **BLOCKER — rule 9’s exact sequence conflicts with a receiver of its own `stateChanged(Paused)`.**

   After final `eventDelivered`, `stop()` emits `stateChanged(Paused)` before the completion commit. A receiver of that signal may call `close()` or `start()`. Those operations change replay identity/session, so `Complete` must fail under the predicate; rule 4 then requires no further mutation or emission. But rule 9 unconditionally requires `stateChanged(Finished)` and `replayFinished`.

   Conflicting text:

   - “The observable sequence is: `eventDelivered`, … `stateChanged(Paused)`, … `stateChanged(Finished)`, `replayFinished`…” [§5.11:1019](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:1019)
   - `Complete` requires replay/session identity. [§5.11:964](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:964)
   - A failed continuation “delivers nothing, arms nothing, mutates nothing and emits nothing.” [§5.11:1003](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:1003)

   Define whether rule 9’s sequence is conditional on no supersession from the intervening `Paused` receiver, or whether completion is deliberately allowed to survive such a supersession. Add a test for `eventDelivered → stop → stateChanged(Paused) → close/start`.

2. **HIGH — the state table contradicts rules 8–10 about `stop()`.**

   The table accepts `stop()` only in `Playing`; it omits it in `Ready`, `Paused`, and `Finished`. Yet rule 8 says a receiver may `stop()` during a synchronous step, which can begin in `Ready` or `Paused`; rule 9 explicitly accepts it in `Finished`; rule 10 calls `stop()` a canceller that must work “in any state.”

   - [§5.5:490](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:490)
   - [§5.11:1014](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:1014)
   - [§5.11:1023](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:1023)
   - [§5.11:1027](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:1027)

   State the `Ready`/`Paused` outcomes and report explicitly, and make the table agree. In particular, explain how an otherwise no-op stop interrupts an outer synchronous step.

3. **LOW — one helper spelling still omits its required phase.**

   Rule 4 says `mayMutateReplay()` rather than `mayMutateReplay(snapshot, phase)`. [§5.11:1003](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:1003)

## Implementability and A1

- The five phases and rules provide an implementable normal path for automatic delivery, step continuation, completion, and direct final-delivery stop.
- The blocker above leaves one callback chain without a defined outcome; implementation would have to choose between rule 9 and the authorization predicate/rule 4.
- A1 remains pure: under the stated valid-mutation-context precondition it requires no identity counter, snapshot, or authorization guard. [§5.11:927](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:927), [§11:1440](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:1440)

I reviewed the specification and prior review record read-only. I did not review implementation code or execute tests.