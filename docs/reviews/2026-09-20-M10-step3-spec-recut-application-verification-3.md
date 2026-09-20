## Verdict

**Amend before adopting 3b.** Step 3a remains a clean, implementable core. §5.11 still leaves a re-entrant final-delivery path undefined, and §6 retains the unqualified invariant previously identified.

## Disposition

| Item | Disposition |
|---|---|
| Rule 9, `Paused` receiver `start()` / `close()` | Resolved. The old completion is refused and the receiver’s state stands. |
| Rule 9 full operation matrix | **Not resolved.** A `stop()` or step from that `stateChanged(Paused)` receiver has no defined outcome. |
| `Finished` receiver | `start()`/`close()` are covered by rule 7; `stop()` by rule 9; a step is refused by §5.5. |
| Cancellation / no `Cancel` phase | Structurally resolved: cancellation is explicitly outside helper authorization and `close()` cancellation agrees with rule 10. |
| §6 delivery-boundary correction | **Not resolved; the correction claim is wrong.** |
| Mutation-authorization singularity / A1 purity | Resolved. §5.11 is the sole authorization rule; 3a requires no IDs, snapshots, or guard. |

The claimed §6 text is still present, unqualified:

> “`stop()` takes effect at a delivery boundary.”

at [§6:1105-1107](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:1105). It is the third occurrence: §5.6 and §9 are scoped, but §6 is not. I was right about this finding; the assertion that only two occurrences exist is incorrect.

## New findings

1. **BLOCKER — final-delivery `Paused` callback remains under-specified.**

   Rule 9 specifies `start()` and `close()` from its transient `stateChanged(Paused)` receiver, but not `stop()` or a step. This matters because rule 9 says the stop report has position at end and zero remaining events, while the state table defines `Paused` as having events remaining and accepts steps there. Rule 1 simultaneously says `Paused` is never a state with no events remaining. [§5.5:493](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:493), [§5.11:989-992](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:989), [§5.11:1020-1031](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:1020)

   Define whether a second `stop()`, `stepNext*()`, and—also currently unspecified—`play()` from this observable transient `Paused` state are refused or no-ops, and whether completion still occurs. Add direct tests for each stated outcome.

2. **HIGH — `stop()` remains inconsistent across §5.5 and rule 10.**

   The state table accepts `stop()` only in `Playing`; it is a no-op in `Idle`, while the operation bullet adds `Ready`; `Paused` and normal `Finished` are omitted. Rule 10 instead requires a canceller to work “in any state.” [§5.5:490-494](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:490), [§5.5:507-513](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:507), [§5.11:1032-1037](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:1032)

   Make the table and operation contract explicitly define the Paused and Finished reports/no-op behavior. This also resolves finding 1’s second-stop branch.

3. **MEDIUM — acceptance evidence does not prove the new conditional branch.**

   The named final-stop test still reads `aStopAtTheLastDeliveryStillCompletesTheReplay`, while rule 9 now correctly permits no completion after a `Paused` receiver closes or restarts. Add explicit tests for `eventDelivered → stop → stateChanged(Paused) → start/close`, plus the newly defined stop/step branches. [§5.11:1020-1026](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:1020), [§5.11:1046-1064](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:1046)

`close()` from a delivery slot, restart during `StepContinue`, and an arm after supersession are otherwise specified sufficiently by rules 4, 6, 8, and 10.

I verified the current specification and prior review record read-only. I did not inspect implementation code or run tests.