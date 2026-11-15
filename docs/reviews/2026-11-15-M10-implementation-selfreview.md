# M10 implementation self-review (2026-11-15)

M10 is "session reader and passive replay": the reader that turns a `.kpsession` v1 file
into one immutable offline session, the replay player that distributes its events, the
reply-free rendering path, and the application state that makes the offline session
explicit and process-wide. This document is my own review before the independent closing
review (SPEC-M10 §11 step 9). It states what shipped, maps the §8 criteria to real tests,
and lists the deviations and open items I know of, including the unflattering ones.

## 1. What was implemented

| Step | Commit | Content |
|---|---|---|
| Gate | `be2020a` | ADR-011 and SPEC-M10 accepted (five review rounds; owner ratified the process-wide offline state) |
| 1 | `006a607` | `SessionReader`: ADR-006 header/record validation, recovery rule, load outcome |
| 3a | `41460f8` | `SessionReplayPlayer` core: five states, seven operations, timing ladder, scheduler/clock seams |
| 3b | `b804055` | one mutation authorization (`mayMutateReplay`), snapshots, re-entrancy tests |
| 4 | `e2e5775` | reply-free rendering path (`sendTerminalReply()` as the single choke point) |
| 5 | `76c6d51` | `KomportDoc` owns the offline session and its player; process-wide preconditions |
| 6a | `a18b4a3` | offline state in the application windows: indicator, `Close Session`, control enumeration, guards |
| 6b | `88a127a` | replay controls, timing ladder, result-value handling, display adapter |
| 7 | `a94999b` | source/dependency audits and the dynamic no-write evidence |
| 8 | `8115b5b` | end-to-end over a real pty: record, load, replay, byte-exact, no transmission |
| 9 | this slice | self-review, §8 mapping, one added close-phase assertion (see §4) |

Step 2 (value types and the widget-free contract entry) has no separate commit; it was
delivered inside steps 1 and 3a together with `tst_sessioncontract`.

## 2. §8 acceptance criteria

All 13 criteria of `SPEC-M10` §8 are now ticked. A mechanical check of every backticked
test name in §8 against the `void` slots of `tests/*.cpp` found **five names that do not
exist as written**; §8 now carries the shipped names (same rule as SPEC-M9 §8):

| §8 name (planned) | Status |
|---|---|
| `liveTerminalQueriesStillAnswer` | renamed: `theLiveEntryPointsStillAnswer` (`tst_replayreply`) |
| `replayedTerminalQueriesProduceNoReply` | renamed: `everyReplayedQueryIsRenderedButNotAnswered` (`tst_replayreply`) and `replayedTerminalQueriesThroughTheAdapterProduceNoReply` (`tst_sessionreplayui`) |
| `theOfflineStateNeverOpensConfiguresOrWritesTheTransport` | **split, not shipped under this name**: load is `aValidFileEntersTheOfflineStateWithoutReplayingOrOpeningAPort` plus `recordedSettingsAreNeverApplied`; load/replay/close observed over a real pty by `replayingARecordedPtySessionTransmitsNothing` (`tst_sessionreplayendtoend`) |
| `theReaderNeverTouchesTheLivePath` | **deferred in step 1 and never shipped as its own test.** Its content is covered by the document-level load test and the load phase of the pty test (a configured, closed port is observed through the load: no open, no configure, no write, peer silent). It is not a test of the *reader class in isolation* next to a live document, which is what the plan described. |
| `noSeekOperationExists` | **static inspection only**, as the spec says: the public surface of `SessionReplayPlayer` is `start`, `play`, `stop`, `stepNextEvent/Rx/Tx`, `close`, `setTiming` plus read-only getters and `retire`; no position setter, seek or range operation exists. There is no automated test, so a future added seek would not fail the suite. |

The rest of §8 maps one-to-one to existing tests (counts in §3).

## 3. Test inventory (QTest entries, includes `initTestCase`/`cleanupTestCase`)

| Target | Entries | Covers |
|---|---|---|
| `tst_sessionreader` | 38 | header/record validation, limits, recovery, byte-exact payloads |
| `tst_sessionreplayplayer` | 52 | states, timing, steps, re-entrancy/authorization |
| `tst_sessionofflinedocument` | 24 | document ownership, process-wide preconditions, teardown re-entrancy |
| `tst_replayreply`, `tst_replayreplyui` | 8, 4 | reply-free path, live entry points still answer, source audit |
| `tst_sessionreplayui` | 28 | indicator, control enumeration, guards, ladder, adapter |
| `tst_sessionreplayendtoend` | 5 | record, load, replay over a pty; recovery; no transmission |
| `tst_sessioncontract` | 12 | widget-free contract check |

## 4. Deviations, weaknesses and open items

1. **The two gaps in §2 above** (`theReaderNeverTouchesTheLivePath`, `noSeekOperationExists`).
2. **Close phase was not observed at the port.** §8's no-transport criterion names load,
   replay and close, but the pty test only asserted peer silence after
   `slotCloseOfflineSession()`. Found during this review; I added assertions for
   `isOpen()`, the applied configuration snapshot, `written`, `closed`, `opened` and
   `transportError` around the close. It passes on the current code. Mutation probe: a
   deliberate `getSerial()->close()` at the top of `KomportDoc::closeOfflineSession()` makes
   `replayingARecordedPtySessionTransmitsNothing` fail (4/5); the probe was reverted
   (`git diff komport` empty) and the suite is 24/24 again.
3. **`tst_replayreplyui` depends on the platform plugin.** Run directly without
   `QT_QPA_PLATFORM=offscreen`, `suppressedKeyInputNeitherSignalsNorWrites` fails (empty
   selection on a real display). Under `ctest` it passes. This is a test-environment
   assumption, not a product defect, but a developer who runs the binary by hand will see a
   red test.
4. **Qt 6.3 build not verified** (local Qt 6.11.1, no CI), as for M8 and M9. Not a §8 line
   of M10, but the same standing open item.
5. **Memory of a loaded session is not measured.** The reader keeps the whole session in
   memory (the design, ADR-011); the 16 MiB header and per-record limits bound a unit, not
   the file. No test loads a multi-hundred-MiB file. Same class of gap as M9's
   statically derived memory claim.
6. **M8/M9 carry-overs unchanged:** the legacy RX-buffer flush, and the recorder's memory
   claim.

## 4a. Closing-review findings and their disposition (2026-11-15)

Two local/external passes were run after the self-review draft: deepseek-v4-flash (read-only,
source reading; the qwen3.8 session's backend was unreachable and did not run) and Codex.

| # | Source | Finding | Disposition |
|---|---|---|---|
| 1 | Codex, high | `KomportView::slotSimKeyPressed()` was a public, unguarded transmit door while offline | **fixed in two rounds**: round 1 guarded the view slot like `keyPressEvent()`; Codex round 2 (critical/high) showed the emulation's own entries and the live RX entry (a query with the port open) were still open, so the flag now also drives `KomportEmulation::setTransmitEnabled()`, consulted at all three transmit sites (`slotKeyPressed`, `slotSimKeyPressed`, `sendTerminalReply`). Test `suppressedSimulatedKeyInputNeitherSignalsNorWrites` covers view slot, both emulation entries and `ESC[5n`/`ESC[6n`/`ESC[c`/`ESC Z` on the live entry; one mutation probe per site (three) fails it. Note: the port cannot be opened while offline through the UI (every open/connect path is guarded); only the test seam does. **Codex round 3 (high) then objected that the gate's public setter can be set back to true by in-process code. Not changed**: the setter is how the application reopens the gate when the offline state ends, so it cannot be made un-callable; the claim is stated accordingly. Making the gate unspoofable would mean deriving it from the process-wide offline predicate inside the emulation, a coupling M10 does not warrant. Left to the owner's call. A raw `KomportSerial::putChar` call on the document's transport remains a public primitive by design: the claim is "no user-facing path and no view/emulation path", as §8 says. |
| 2 | Codex, medium | `KomportApp` load/close/play/stop/step slots used `this` after synchronous signals that a receiver may answer by deleting the window | **fixed**: `QPointer` guard after each signal-emitting call; test `aReceiverThatDeletesTheWindowDuringAnAppSlotIsHarmless` (load, close, play, stop, three steps). Probe: without the guards an AddressSanitizer build reports heap-use-after-free in `slotStatusMsg`; with them the target is clean under ASAN. |
| 3 | deepseek, medium | legacy RX flush: bytes buffered before a close are rendered through the live entry after a session is loaded; replies reach `putStr()` and are dropped only because the port is closed | **accepted, open**: documented M8 behaviour that SPEC-M10 §5.8 leaves unchanged; display-only while the port is closed. A fix (clear the buffer and stop the timer in `close()`) changes M8 behaviour and needs its own decision. |
| 4 | deepseek, low | the source audit cannot see `KomportTransfer` writes; "the only transmitting function" wording was too strong | **spec wording corrected**; the transfer slots are guarded and driven by `invokingEachOfflineGuardedEntryPointReachesNoTransport` |
| 5 | deepseek, low | `FileByteSource::read()` treats an I/O error that coincides with `atEnd()` as a clean end | **open**, unverified whether `QFile` produces that combination; not changed |
| 6 | deepseek, low | `loadOfflineSession()` can return ok after a receiver already closed the session | **open**, contract sharp edge, no receiver does it today (the app slot now tolerates window deletion, not this) |

Codex round 2 also found that the close-phase assertions did not compare the `configurationChanged`
spy; it is now compared against its count before the close.

## 5. Verification evidence (2026-11-15)

- Fresh configure with `-Wall -Wextra`, full build: **0 warnings**.
- `ctest`: **24/24** pass, both in the normal build tree and in the `-Wall -Wextra` tree.
- The close-phase addition passes in `tst_sessionreplayendtoend` (5/5).
- Mechanical name check of §8 against `tests/*.cpp`: five mismatches, all resolved in §2.
