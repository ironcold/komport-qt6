# M10 step 6b - adversarial review (2026-10-16)

Slice: replay controls, timing ladder, result-value handling and the display adapter (SPEC-M10 5.7). Runner: codex-companion adversarial-review, scope working-tree.

## Focus (round 1)

Review M10 step 6b (SPEC-M10 5.7 replay controls, timing ladder, display adapter; 11 step 6b): komport/komport.cpp/.h and tests/tst_sessionreplayui.cpp (the rows after loadingIsNotPersistedAcrossRuns). Focus: (1) the display adapter: can a delivered event reach any transport/live entry point (RX goes only through KomportView::slotReplayReceivedChar, hex monitor only appendRx/appendTx), is TX correctly not echoed into the terminal view, are non-data events counted not rendered; (2) re-entrancy: the adapter/finished/state slots run synchronously inside the player's signals - do they call any mutating player operation or anything that can retire the player (SPEC 5.11); updateReplayControls reads the player only; (3) handlers consume result values: play/step/timing refusals reported, the timing selector written only from SessionReplayTimingChange::timing, showReplayTiming not re-triggering the handler; (4) subscription lifetime: connections made in slotOfflineSessionChanged(true) for the window that loaded the session; a second load after close, a player retired while a slot is running, window destroyed during replay; (5) enable/disable of the replay actions vs player state, hidden toolbar when not offline; (6) test quality - would each test fail if the behaviour were removed. KomportSerial and the legacy RX buffer must stay untouched; German/French translations of new strings are deliberately deferred to one translation step after this slice - do not flag that. Owner's foreign changes (AGENTS.md, CLAUDE.md, docs/project-context.md, docs/example-charsets/README.md, docs/translation-glossary.md) are out of scope.

## Round 1

# Codex Adversarial Review

Target: working tree diff
Verdict: needs-attention

No-ship: step-mode completion silently loses the required non-data-event accounting, and the new adapter’s no-transmit guarantee is not tested through the application wiring.

Findings:
- [medium] Step-driven replay never reports non-data events (komport/komport.cpp:1820-1884)
  Non-data events increment mReplayNonDataEvents, but that value is only displayed by slotReplayFinished(). A replay completed through Step Event/RX/TX transitions to Finished without replayFinished (by player contract), so its final step status contains only the advanced count. The existing step test delivers an Annotation and reaches Finished, but asserts only “No further TX”, masking this. Transport/error/configuration events can therefore be neither rendered nor disclosed to the user, contrary to the required status-summary accounting.
  Recommendation: Include the cumulative non-data count in each step outcome that reaches the end (or emit an equivalent completion summary for step completion), and add a test that completes a session containing non-data events solely via step actions and asserts the count.
- [medium] The display-adapter test cannot detect a transport write or replay-path bypass (tests/tst_sessionreplayui.cpp:842-885)
  The test verifies rendered bytes and only asserts that the serial port is closed. That does not prove this newly added adapter never calls a write/live entry point: a direct write attempt to a closed serial object is invisible here, and the fixture contains no terminal query that would expose an accidental live emulation route. The existing replay-entry test drives KomportView directly, not this SessionReplayPlayer-to-KomportApp adapter.
  Recommendation: Exercise the adapter with a loaded replay while its serial is connected to a pty, include terminal-query RX bytes, and assert both zero ITransport::bytesWritten emissions and a silent peer. Extend the source audit to extract and inspect KomportApp::slotReplayEventDelivered as the specification requires.

Next steps:
- Fix the step-completion status path and add the missing adapter-level no-write coverage before approving step 6b.

## Round 2

# Codex Adversarial Review

Target: working tree diff
Verdict: needs-attention

No-ship: stopping a replay can permanently hide delivered non-data events, which have no pane representation.

Findings:
- [medium] Stop status omits already-delivered non-data events (komport/komport.cpp:1810-1815)
  Non-data events are counted by the adapter but are rendered nowhere. A user can replay through annotations, transport errors, or configuration events, press Stop, then close the session; the only stop summary reports total delivered/remaining events and never discloses the non-data count. The new end-of-step fix covers only steps reaching the end, and replayFinished is never emitted after an explicit stop.
  Recommendation: Append mReplayNonDataEvents to the explicit-stop status summary (and any other terminal partial-replay summary intended to account for invisible events), then add a test that delivers a non-data event, stops before completion, and requires the count in the status text.

Next steps:
- Fix the stop-path disclosure and add the focused regression test.

## Round 3

# Codex Adversarial Review

Target: working tree diff
Verdict: needs-attention

No-ship: a partial step can deliver invisible non-data events without ever disclosing them to the user.

Findings:
- [medium] Partial step outcomes omit delivered non-data events (komport/komport.cpp:1833-1841)
  `mReplayNonDataEvents` is reported for a stopped replay and only for a step whose result reaches the end. A normal partial `Step Event`/`Step RX` can deliver an Annotation (or another non-data event), which the adapter intentionally renders nowhere, then leave more events pending. Its status message contains no non-data count; if the user closes the session next, the count is never disclosed. The existing tests cover a step that subsequently reaches the end and a separately stopped replay, so they do not expose this path.
  Recommendation: Include the non-data count (preferably the count delivered by that step, or clearly labelled cumulative count) in every successful step summary that delivered non-data events. Add a test that steps a non-data event without reaching the end, then closes the offline session, and asserts the step outcome disclosed it.

Next steps:
- Fix the partial-step status accounting and add the focused regression test.

## Round 4

# Codex Adversarial Review

Target: working tree diff
Verdict: approve

No material, defensible blocker found in the scoped 6b diff. The adapter stays on replay/display-only entries, result handling and synchronous player-signal slots do not mutate or retire the player, and lifecycle/control-state wiring is consistent with the reviewed contracts.

No material findings.

## Resolution summary

- Round 1 (step completion lost the non-data count; adapter not tested with a pty peer): fixed; new test `replayedTerminalQueriesThroughTheAdapterProduceNoReply`.
- Round 2 (stop summary omitted the count): fixed; `aStoppedReplayDisclosesTheNonDataEventsItDelivered`.
- Round 3 (partial steps omitted the count): fixed, every successful step with delivered non-data events states the cumulative count; `aPartialStepDisclosesTheNonDataEventsItDelivered`.
- Round 4: approve.
- Deferred on purpose: the source audit of the adapter (SPEC step 7), German/French translations of the new strings (one step after the slices).

## Evidence

- -Wall -Wextra build clean; ctest 23/23; tst_sessionreplayui 25 tests.
- Mutation probes, each caught: TX also rendered into the view; selector not restored after a refused timing; hex directions swapped; refused play not reported; adapter not subscribed; live entry instead of the replay entry; step-end accounting removed.
