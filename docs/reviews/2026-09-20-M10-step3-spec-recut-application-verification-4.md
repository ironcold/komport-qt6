## Verdict

**Amend before adopting step 3b.** Step **3a is a sound basis to implement first**: it needs no replay identity, snapshot, or authorization guard. The 3b matrix still leaves two observable transient-`Paused` calls to inference.

## Disposition

| Previous item | Disposition |
|---|---|
| Final-delivery transient `Paused` matrix | **Partly resolved.** `stop`, all steps, `play`, successful `start`, and `close` are now addressed. |
| `stop()` consistency across states | **Resolved.** The table and normative bullet agree: it is accepted in every state; non-`Playing` calls are no-ops. |
| Conditional-branch evidence | **Partly resolved.** The two new test descriptions cover the successful `start`/`close` and no-op/refusal branches, but the undisturbed test does not yet pin the required signal sequence. |
| §6 delivery-boundary invariant | **Resolved.** Its valid-context and §5.11 qualification is now correct. |
| §5.11 singularity / A1 purity | **Resolved.** Mutation authorization is normed in §5.11; §5.5/§5.6/§6 defer receiver behavior there. |

## New findings

1. **BLOCKER — the transient matrix still omits `setTiming()` and a refused `start(nullptr)`.**

   From transient `Paused`, §5.5 normally accepts `setTiming()`, while rule 9 defines no outcome for it: whether the timing changes, whether completion still commits, and whether captured stop/completion reports retain their pre-callback timing. Rule 9’s “only a `start()` or `close()` … ends the sequence” supports the intended outcome, but §5.5 expressly restricts its normal operation contract to a valid mutation context.

   More directly, §5.5 says `start(nullptr)` refuses and changes nothing, whereas rule 9 says a `start()` from transient `Paused` ends the sequence. A refused start cannot replace the replay, so it should leave the completion intact—but that must be stated, not inferred. See [§5.5](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:498) and [rule 9](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:1023).

   Specify that a successful `start(non-null)` ends the old completion; a refused null start, accepted timing change, and refused unknown timing value leave the loaded replay and its completion intact. State the report-timing rule explicitly.

2. **HIGH — the three tests do not yet prove rule 9’s required observable order.**

   `aStopAtTheLastDeliveryStillCompletesTheReplay` proves eventual completion, but its stated assertion does not prove the required intermediate signal:
   `eventDelivered → stateChanged(Paused) → stateChanged(Finished) → replayFinished`.

   It would pass if an implementation silently omitted `stateChanged(Paused)`. Strengthen that test with the exact signal log. Extend the transient-no-op test to cover `setTiming()` and refused `start(nullptr)`, or add a dedicated test.

3. **MEDIUM — §8’s 3b acceptance criterion is stale.**

   The criterion lists earlier 3b evidence but omits both new transient-`Paused` tests (and also the interrupted-multi-event-step test). Add them so the acceptance gate actually requires the amended branches. See [§8](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:1306).

The apparent `Paused` table conflict is otherwise adequately scoped: §5.5 applies under a valid mutation context, while the transient receiver exception is explicitly delegated to §5.11.

## Could not verify

I reviewed the current specification and existing test source read-only; I did not run tests or assess a 3b implementation. The newly named transient tests are specification-plan entries, not present in the current test source, so their eventual fixtures/assertions remain to be reviewed.