# M10 step 4, independent adversarial review, round 3

Record of the third independent adversarial round of the M10 step-4 slice. The reviewer's own
text is reproduced verbatim below the metadata; nothing in it has been edited (review records
are never retro-edited).

## Round metadata

| | |
| --- | --- |
| Slice | M10 step 4 - SPEC-M10 §11 step 4, §5.7 (reply-free rendering entry), §5.8 clause 3 (reply choke point), §5.7.1 (key-input suppression) |
| Repository / branch | `/home/max/Development/misc/komport-qt6`, `v2/m10-session-reader-replay` |
| Reviewed revision | working tree, HEAD `b804055` (step 3b, committed and pushed); the slice itself uncommitted |
| Scope of the round | `--scope working-tree` |
| Slice files in the reviewed revision | `komport/komportemulation.{h,cpp}`, `komport/komportview.{h,cpp}`, **`komport/komportserial.cpp`** (the RX-overflow flush - removed again by the fix round, see below), `tests/CMakeLists.txt`, `tests/tst_replayreply.cpp`, `tests/tst_replayreplyui.cpp` |
| Foreign changes in the same working tree | `AGENTS.md`, `CLAUDE.md`, `docs/project-context.md`, `docs/example-charsets/README.md`, `docs/translation-glossary.md` - owner's uncommitted changes, not part of the slice, named as such in the focus |
| Focus (the reviewer's brief) | `/tmp/m10-step4-focus3.txt`, sha256 `1fe5ccaca4815f4f36c0eef7d348cb0cf988f6a1b2213eafd061555ee6d4d4ef` |
| Runner | `/tmp/run_m10_step4_review3.sh` (codex-companion `adversarial-review --wait --scope working-tree`) |
| Source log | `/tmp/codex-m10-step4-review3.log`, sha256 `6f058aaf5e6cb5cd459b33ae9b322c8aa4de7a60ec20801f9bc369cad2f74a3b`, 9310 bytes |
| Run | 2026-09-29T10:14:33+02:00 → 2026-09-29T10:17:00+02:00, `EXIT=0` |
| Verdict | **needs-attention** - one high blocking finding |

## Reporter evidence given to the round (unchanged by the round)

- `cmake -B build -DCMAKE_CXX_FLAGS="-Wall -Wextra" && cmake --build build -j$(nproc)`: 0 warnings, 0 errors.
- `cd build && ctest --output-on-failure`: 100% tests passed out of 21, 0 failed.
- `QT_QPA_PLATFORM=offscreen ./tests/tst_replayreplyui`: 4 passed. The same 4 pass under
  `DISPLAY=:99 QT_QPA_PLATFORM=xcb` with a private `Xvfb`, i.e. where `QClipboard::Selection` is real.
- Mutation probes on the suppressed branch of `KomportView::keyPressEvent()` (one at a time,
  product file restored byte-identically afterwards): guard removed → red on
  `QCOMPARE(pressed.count(), 0)`; guard calls `deselect()` → red on
  `QCOMPARE(view->selectedText(), selectedBefore)`; guard takes `resetScroll()`/`slotScroll()` → red
  on `QCOMPARE(bar->value(), scrollBefore)`.
- The test-side selection question of the handover, with its diagnosis (both named candidates
  refuted, the real cause in the test's own scroll-dependent cell resolution):
  `docs/reviews/2026-09-20-M10-step4-selection-fix-note.md`.

## Editor's note on two reviewer statements (corrections list)

- The finding says "No changed test observes `receivedChar`". That is imprecise:
  `tests/tst_replayreplyui.cpp:281` (a file of this slice) connects `KomportSerial::receivedChar`
  and waits for the byte count, and `tests/tst_transport.cpp:921` spies on it (pre-existing). The
  accurate statement, and the one the finding's substance rests on, is: no test pins the *overflow*
  delivery semantics of the legacy display path (order, the drop, its interaction with the flush
  timer and with re-entrant receivers) - and the slice's own observation of that signal is only a
  count, which the RX-overflow change is what makes reachable at all.
- The finding's scope claim was verified before it was adopted: SPEC-M10 §3 (non-goals, lines 81-88)
  requires that `KomportSerial`, the legacy character signals and the legacy RX buffer stay
  unchanged, and that "the existing RX buffer keep [its] present behaviour, which §7's regression
  tests pin"; AGENTS.md additionally forbids changing the legacy RX buffer as a drive-by fix. The
  claim holds.
- The third cause names the test, its remedy is a test-side bound: SPEC-M8 §6.2 (quoted in
  `komportserial.cpp:505-510`, pre-existing) states the event stream is loss-free where the *display*
  path is deliberately lossy, so the drop is accepted legacy behaviour, not a defect this slice may
  repair.

## Codex Adversarial Review (verbatim)

# Codex Adversarial Review

Target: working tree diff
Verdict: needs-attention

Do not commit as one M10 step-4 commit. The replay/rendering change is defensible, but the included RX-buffer rewrite violates the accepted step scope and has no regression proof for its changed legacy delivery semantics.

Findings:
- [high] Blocking — unapproved, unpinned rewrite of legacy RX delivery (komport/komportserial.cpp:506-514)
  Quoted: `if ( mRxBuffer.size() > mRxQueueMax ) { ... slotFlushRxBuffer(); }`. This changes `KomportSerial` and the legacy RX buffer even though SPEC-M10 explicitly forbids both and §11 step 4 lists only the reply-free emulation/view work. It also changes more than loss: `receivedChar` is now emitted synchronously from `readyRead` on overflow rather than by the flush timer, affecting terminal, hex view, logger, and transfer consumers. No changed test observes `receivedChar`; `tst_transport.cpp:273` observes only `bytesReceived`, which is emitted before buffering. There is no overflow-order, repeated-overflow, timer, or synchronous-receiver/re-entrancy regression test. Blocking; classification: outside the documented threat model (must be fixed).
  Recommendation: Remove this serial change from the step-4 commit. If the loss fix is required now, first add an accepted spec amendment and deliver it as a separately reviewed slice with scripted RX tests that assert exact `receivedChar` sequence/order across overflow and its interaction with the timer/re-entrant receivers.

Next steps:
- Attack log — corrected selection test: non-vacuity is now real (`selectedText()` and both selected cell flags are asserted immediately before the event). The scroll-position dependency of `getCell()` is controlled because the selection is created after scrolling to the fixed position; post-event scroll value/maximum are checked before the cell flags. A scroll mutation therefore fails its own comparison, not a misleading selection assertion. The immediate snapshot is the correct measurement point; taking it earlier would attribute focus/event-loop changes to the key event.
- Attack log — suppressed input paths: `keyPressEvent()` returns before reset/scroll and `keyPressed`; key release is empty; auto-repeat repeats the guarded press path; minimap/wheel paths are independent of key press; the auto-scroll timer requires an active mouse selection. `slotReplayReceivedChar()` performs only emulation rendering and live-equivalent scroll bookkeeping. The targeted test friend adds no callable product path. No supported path found that changes focus, selection, or scrolling during the suppressed press.
- Attack log — emulation: all current terminal reply sites route through the private choke point; scoped restoration handles nested replay and re-entrant live rendering; view ordering matches the two live signal connections. The structural audit now covers the view and correctly states its indirect/meta-object-call limit. The synchronous-destruction hazard remains unenforced, but it is explicitly an accepted residual outside this slice's supported render-callback contract, not a new blocker.
- Attack log — validation: local replay dynamic checks could not be reproduced because the read-only sandbox cannot create serial lock files: emulation PTY cases skipped and UI PTY cases failed at opening the PTY. The source audit passed. This environmental result is not used as a finding; reporter-provided PTY evidence remains the available dynamic evidence.

## After this round (reporter's note, not part of the reviewer's text)

The fix round implemented the one named finding; no other change was made:

- `komport/komportserial.cpp` was restored to `HEAD` (`git checkout --`), so the slice no longer
  touches `KomportSerial` or the legacy RX buffer. The removed patch is preserved for the owner as
  `/tmp/m10-step4-serial-rx-overflow.diff`, sha256
  `ee5bac7e29d23479b944305ef0ef6520d121a664c87ca9696e97c9b4c24fcc84`, to be spun into its own
  spec-amended slice if the owner wants it.
- `tests/tst_replayreplyui.cpp` no longer needs it: the live-vs-replay stream was shortened to 831
  bytes (150 short lines, the queries spread across them, ending mid-line) and the test asserts that
  bound (`< 1024`, the `rxQueue` default) with the reason on the assertion, because comparing the two
  renders requires that the live path delivered every byte it was given. Measured on the fixed
  revision: stream 831 bytes, `receivedChar` count 831, history 176 rows (144 scrollback + 32
  visible), so the scrollback the round-2 finding asked for is deeper than before. The comments that
  attributed the earlier "1024 of 1971 bytes" divergence to a half-delivered render were corrected to
  name both causes (incomplete synchronisation and the over-mark burst).
- Re-verified after the fix: `-Wall -Wextra` build 0 warnings/errors; `ctest` 21/21;
  `tst_replayreplyui` 4/4 offscreen and 4/4 under `xcb`/`Xvfb`; the three mutation probes still red on
  exactly their own assertions. The slice's own record of this fix round is the section added to
  `docs/reviews/2026-09-20-M10-step4-implementation-selfreview.md`.

Reviewed revisions and hashes at the time of the round: the tracked working-tree diff as reviewed is
reconstructed by appending the saved serial patch (`/tmp/m10-step4-serial-rx-overflow.diff`) to the
current `git diff`; that reconstruction hashes to
`1feffe1d49c1d43c38ad13be2ce4e4faa3837687d6d35478602c875147825dcf`. The two test files as reviewed
were superseded by this fix round: `tests/tst_replayreply.cpp`
(sha256 `aa6e9b351d698840c1505424031dae30802e2871f81696fa08db30b4e6130023`) is unchanged by it,
`tests/tst_replayreplyui.cpp` is now
`aa889da511bc539f8bdbc8499a712042fe9d19de10accf16bde2b46624942043`.
