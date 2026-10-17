# M10 step 8 - adversarial review record (2026-10-17)

Scope: `tests/tst_sessionreplayendtoend.cpp` (new), `tests/CMakeLists.txt`. No production code changed.

## Focus (round 5, cumulative)

```
M10 step 8 (SPEC-M10 sections 7 and 11): new test tests/tst_sessionreplayendtoend.cpp (registered in tests/CMakeLists.txt), no production code changed. Check that the three spec'd tests really prove their claims over a real pty: recordPtySessionThenLoadAndReplayByteExactly (M9 recorder beside a live KomportApp, RX incl. every byte value, a terminal query answered live, TX; file parsed by the independent M9 test-side reader; loaded by SessionReader; replayed with Immediate; concatenated RX/TX payloads, event count, sequences, type, direction equal), replayingARecordedPtySessionTransmitsNothing (peer silent during load/replay/close, including the recorded query; the recording itself must hold the query and the live reply), theRecoveryRuleSurvivesARealRecording (cut inside the final record: prefix replays, recovery reported). Look for fixtures that pass without their critical content, weak/non-timeout peer checks, flakiness (timing), and gaps against the SPEC table. Mutation probes already done: replay writing TX to the port, reader not reporting recovery, reader dropping a payload byte - each fails a test.
Round 2: the no-transmit test now reads its own recording back (query in RX, live reply in TX); the recovery test compares every event field of the recovered prefix and of the delivered events against the complete recording and asserts the window's status text reports the dropped truncated record.
Round 3: the no-transmit test now holds the slave open, configures the window's port for the pty but leaves it closed before loading, installs the bytesWritten spy before load, asserts the port stays closed and the peer silent after load, then opens the port (seam) for replay/close observation. Probe: load opening the port fails the test.
Round 4: load-phase spies on opened/closed/configurationChanged/transportError plus the full configuration snapshot compare; checked non-blocking setup helper for the pty master. Probe: load that opens and closes the port fails.
Round 5: recording at 115200 baud vs the replay window pre-configured at 9600 (asserted different) so applying recorded settings during load is observable; binary TX (every byte value) via writeBytes, peer receipt verified, included in the expected recorded and replayed TX. Probe: load applying the recorded baud rate fails.
```

## Round 1

> # Codex Adversarial Review
> 
> Target: working tree diff
> Verdict: needs-attention
> 
> No-ship: two of the new end-to-end tests can pass without proving their required critical behavior.
> 
> Findings:
> - [medium] The no-transmit fixture never proves its own recording contains the query/reply (tests/tst_sessionreplayendtoend.cpp:285-287)
>   The asserted `recorded.rx`/`recorded.tx` are synthetic expected values assigned by `recordSession`, not bytes read back from this test's file. Thus this test still passes if this particular recording drops the query or its live reply: the live terminal can answer, the expected fields still contain both values, and replay has no query that could trigger a write. The first test validates a separately recorded file, so it does not close this per-fixture gap under timing-dependent loss.
>   Recommendation: Parse this recording with `readSessionRecordFile` (or inspect the loaded session) and assert concatenated RX contains `kQuery` and concatenated TX contains `kQueryReply` before replaying it.
> - [medium] Recovery test checks only counts, not complete-prefix replay or user-visible reporting (tests/tst_sessionreplayendtoend.cpp:350-370)
>   For the recovered prefix, the test compares only sequence and payload; it never verifies type, direction, timestamps, source/metadata, or that `eventDelivered` matches the recovered events at all—the replay assertion is only a count. The claimed window-level recovery report is likewise not inspected: successful loading and offline state pass even if the status text omits the truncation warning. A recovery-specific mapping/delivery regression or loss of the warning can therefore ship while this test passes.
>   Recommendation: Compare every delivered event field against the complete prefix of the whole session, and assert the window's status label contains the recovered/truncated-final-record message after loading the cut file.
> 
> Next steps:
> - Strengthen the two fixtures, then rerun the end-to-end target and full test suite.

## Round 2

> # Codex Adversarial Review
> 
> Target: working tree diff
> Verdict: needs-attention
> 
> No-ship: the new end-to-end no-transmit test does not observe the load phase it claims to cover.
> 
> Findings:
> - [high] Load-phase transmission is not connected to the PTY observation (tests/tst_sessionreplayendtoend.cpp:295-303)
>   `loadSessionForReplayFile()` runs before the serial port is configured for or opened on the test PTY, and `bytesWritten` is observed only afterward. Thus a regression that opens/configures the existing transport and transmits while loading can pass this test: it has no peer attached and no active signal spy during load. This fails the SPEC-M10 requirement that the peer read nothing during load, including for a recording containing a terminal query.
>   Recommendation: Before `loadSessionForReplayFile()`, configure the window's serial port to the PTY slave but leave it closed (so the offline-load precondition remains valid), install the `bytesWritten` spy, and poll the master during and immediately after load. Then retain the existing opened-port replay/close observations.
> 
> Next steps:
> - Amend the no-transmit fixture and rerun the end-to-end target plus the full test suite.

## Round 3

> # Codex Adversarial Review
> 
> Target: working tree diff
> Verdict: needs-attention
> 
> No-ship: the new no-transmit test can miss a transient transport activation/configuration during load, and its claimed bounded peer checks can block indefinitely if nonblocking setup fails.
> 
> Findings:
> - [medium] Load test misses transient port activation or reconfiguration (tests/tst_sessionreplayendtoend.cpp:305-311)
>   The test only observes final `isOpen()`, device name, writes, and peer bytes after `loadSessionForReplayFile()`. A regression that opens the port, applies recorded settings, and closes it again before the call returns emits no write and leaves these assertions passing. That violates the stated offline invariant and contradicts the comment's claim that opening/configuring would be observable.
>   Recommendation: Install `QSignalSpy` instances for `ITransport::opened`, `closed`, and `configurationChanged` before loading, and require all to remain empty through load (and close as applicable). Also compare the complete pre-load configuration snapshot after loading.
> - [medium] PTY timeout helpers are not actually bounded when nonblocking setup fails (tests/tst_sessionreplayendtoend.cpp:77-110)
>   `peerStaysSilent()` and `readPeer()` call `read()` directly; their elapsed-time loops only work if the master fd is nonblocking. Every `fcntl()` call that enables `O_NONBLOCK` ignores both failure paths. If that setup fails, a no-data `read()` blocks indefinitely, so the 5-second reply timeout and silence windows can hang the test runner instead of failing deterministically.
>   Recommendation: Add a checked helper that reads the existing flags, sets `O_NONBLOCK`, and fails the test on either `fcntl()` error; alternatively use `poll()` with the remaining deadline before each read.
> 
> Next steps:
> - Add transport lifecycle/configuration spies to the load phase.
> - Make nonblocking setup and peer-read deadlines enforceable.

## Round 4

> # Codex Adversarial Review
> 
> Target: working tree diff
> Verdict: needs-attention
> 
> No-ship: the new end-to-end gate can pass if load applies recorded settings, and it does not cover binary TX preservation required by the spec table.
> 
> Findings:
> - [medium] The configuration-mutation probe is blind to applying the recording (tests/tst_sessionreplayendtoend.cpp:311-324)
>   The recorder captures the configuration after setting this pty and 9600 baud, and the replay window is deliberately configured with that same pty and 9600 before its snapshot is taken. A defective loader that reapplies the recorded configuration while closed can therefore leave the snapshot unchanged and emit no configuration-change signal, passing every assertion here. This defeats the stated load-phase configuration check.
>   Recommendation: Record with a configuration deliberately different from the replay window's pre-load configuration (for example, recorder at 115200 and replay window at 9600), then assert the complete snapshot and endpoint remain at the pre-load values.
> - [medium] The byte-exact end-to-end test has no binary TX payload (tests/tst_sessionreplayendtoend.cpp:130-132)
>   `everyByteValue()` is sent only from the pty master, so it exercises RX. The recorded TX is limited to ESC[0n and `typed\r`; it contains no NUL or high-byte data. A regression that truncates, translates, or corrupts binary TX can pass while the SPEC-M10 end-to-end table requires binary payload coverage for TX and RX.
>   Recommendation: Write an explicit TX pattern containing NUL and high-byte values through `serial->writeBytes`, verify its accepted length and peer receipt, and include it in the expected recorded and replayed TX concatenation.
> 
> Next steps:
> - Make the recorded and pre-load serial configurations observably different.
> - Add a raw binary TX pattern to the shared live-recording fixture and rerun the end-to-end target.

## Round 5

> # Codex Adversarial Review
> 
> Target: working tree diff
> Verdict: approve
> 
> Ship: the new PTY target is registered and the three tests substantively cover the specified recording, byte-exact reader/player replay, passive load/replay, and truncated-final-record recovery claims. I found no material, defensible gap in this working-tree change.
> 
> No material findings.

## Resolution summary

- R1 (2 medium): the no-transmit test now reads its own recording back (query in RX, live reply in TX); the recovery test compares every event field of the recovered prefix and of the delivered events against the complete recording and asserts the window's status text reports the dropped record.
- R2 (high): load-phase observation added - slave held open, port configured but closed before load, spy before load, port still closed and peer silent after load.
- R3 (2 medium): opened/closed/configurationChanged/transportError spies and full configuration snapshot compare through load; checked non-blocking setup for the pty master.
- R4 (2 medium): recording made at 115200 baud vs the replay window at 9600 (asserted different); binary TX (all 256 byte values) recorded, verified at the peer and compared after replay.
- R5: approve, no findings.

## Evidence

- Mutation probes, each fails a test and was reverted: replay writes TX to the port; reader reports no recovery; reader drops a payload byte; load opens the port; load opens and closes the port; load applies the recorded baud rate.
- Full `ctest` 24/24, build warning-free with `-Wall -Wextra`.
