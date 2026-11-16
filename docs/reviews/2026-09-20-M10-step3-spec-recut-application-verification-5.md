Verdict: amend before adopting 3b. Step 3a is clear to implement first; it remains free of identity, snapshot, and authorization logic.

| Item | Disposition |
|---|---|
| Transient `Paused` semantics | Resolved in normative text for declared `setTiming()` and refused `start(nullptr)`. |
| Undisturbed signal sequence | Resolved in the §5.11 test-plan row: it now requires `eventDelivered → Paused → Finished → replayFinished`. |

New findings:

1. **HIGH — the four rule-9 tests still do not cover the newly defined branches.**  
   `aStopFromTheTransientPaused…` covers second `stop`, steps, and `play`; it does not assert declared `setTiming()` or `start(nullptr)`. The other three rows cover successful `start`/`close`, the stop-only undisturbed branch, and the later `Finished` receiver. Extend the former test (or add one) to assert timing changes, captured reports retain their timing, null start refuses without ending completion, and the completion sequence continues.

2. **MEDIUM — §8’s 3b acceptance gate is stale.**  
   It omits `aStopFromTheTransientPaused…`, `aReceiverOfTheTransientPaused…`, and `anInterruptedMultiEventStep…`, despite §5.11 requiring them. Update the acceptance criterion so it actually mandates the branch evidence. See [§5.11](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:1070) and [§8](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:1311).

3. **LOW — invalid timing remains ambiguous in the transient window.**  
   Rule 9 says `setTiming()` is accepted, while §5.5 defines an out-of-range enum as refused. Say explicitly that the transient acceptance applies to declared ladder values only; an unknown value refuses unchanged and does not interrupt completion.

The state-changing surface is otherwise complete and consistent: successful `start`/`close` end the old completion; stop, all steps, and play preserve it as specified; accessors simply observe the live transient `Paused` facts. The rule numbering order (1–6, 8–10, 7) is editorially confusing but not contradictory.

Could not verify a future 3b implementation or execution. The current test source still lacks the new transient test methods and its existing `aStopAtTheLastDeliveryStillCompletesTheReplay` does not yet record the stated signal log; that is expected to be addressed when 3b is implemented.