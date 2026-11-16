# Handover: M10 step 4 (reply-free rendering path), session of 2026-09-20

Working tree handover. Nothing here is committed: HEAD is `b804055` (M10 step 3b, approved and
pushed). Branch `v2/m10-session-reader-replay`; `master` untouched.

## State in one paragraph

Step 4 of SPEC-M10 §11 (the reply-free rendering entry, §5.7/§5.8 clause 3) is implemented and
reviewed through two adversarial rounds, both of which asked for more evidence. The emulation-level
evidence is complete and green; the view-level evidence is green except **one assertion** that has
turned out to be a real open question rather than a test detail (see below). The approved plan and
all review records are in `docs/reviews/2026-09-20-M10-step4-*`.

## What is implemented (uncommitted)

- `komport/komportemulation.{h,cpp}` - the private choke point `sendTerminalReply(const QByteArray &)`
  (the emulation's only terminal-answer site; all three reply sites - `CSI 5 n`, `CSI 6 n`,
  `CSI c`/`ESC Z` - route through it), the reply mode `mTerminalRepliesAllowed` (default `true`, live
  safe), the shared private `renderReceivedChar(char)`, and the two public entries
  `slotReceivedChar()` (mode: replies allowed) and `slotReplayReceivedChar()` (mode: suppressed),
  both through `KomportEmulationReplyModeScope` - forward-declared and friended in the header,
  **defined privately in the .cpp**, `QPointer`-guarded so a render that destroys the emulation
  cannot make the scope's exit write into freed memory.
- `komport/komportview.{h,cpp}` - `slotReplayReceivedChar(char)` (forwards to the emulation first,
  then the live slot's scroll bookkeeping), `setKeyInputEnabled(bool)`/`keyInputEnabled()` with
  `mKeyInputEnabled` (default `true`); `keyPressEvent()` consumes the event without emitting
  `keyPressed` while suppressed. `TstReplayReplyUi` is a **targeted friend** (same
  friend-for-testability pattern as `TstAppearance`/`TstCharset`, no product API added) - used for
  the view's own `mScrollBar` (`KomportMinimapScrollBar`) and the selection cell flag.
- `komport/komportserial.cpp` - **real product fix**: `slotDataAvailable()` used to drop the oldest
  bytes when the RX buffer exceeded `rxQueue` (default 1024), so a burst larger than that lost its
  beginning before the 250 ms flush timer ever ran - bytes that `readAll()` had returned, with no gap
  marker. It now flushes the buffer instead: the high-water mark bounds the buffer, not the stream.
- `tests/tst_replayreply.cpp` (7/7 green) - live entries still answer (exact reply bytes per query
  form), replayed queries rendered but never answered (no `bytesWritten`, peer silent in a bounded
  window), suppression never survives the call, nested replay stays suppressed while a re-entrant
  live call answers, and a source audit of **both** `komportemulation.cpp` and `komportview.cpp`
  (comments/strings stripped, transmit identifiers detected independently of `.`/`->`, brace-aware
  function extraction, every occurrence required to lie in an allow-listed parsed body; the audit
  states its own limit - indirect/`QMetaObject` calls are invisible to it).
- `tests/tst_replayreplyui.cpp` (3/4) - app-based offscreen: the strict live-vs-replay comparison
  (full scrollback via `totalHistoryRows()`/`cellAtHistoryRow()`, visible grid, cursor, scroll
  value/maximum, non-bottom position and auto-follow; geometry asserted equal; live side waits for
  the byte count, not a text marker) and the key-input test.

## The one open item

`suppressedKeyInputNeitherSignalsNorWrites` fails on its last assertion: after the suppressed key
event the selection is **gone** (`selectedText()` is `""` where it was `"x \n"`), i.e. the promise
"focus, selection and scroll position unchanged" does not hold today. The selection itself is created
through the established `tst_selection` route (`view->select(QPoint(0,0), QPoint(1,0), true)`,
checked via `selectedText()`/the cell flag) - that part works.

Two candidate explanations, and the decision is the next step:

1. **The test does not mirror the real caller completely.** `select(clip=true)`'s own comment says
   `QClipboard::selectionChanged()` can be delivered **synchronously** into `slotSelectionChanged()`,
   which calls `deselect()` unless `mInSelection` is set - and the real caller (`mouseReleaseEvent`)
   sets that flag before calling `select()`. If that is the cause, the test must set it too.
2. **A real suppression gap.** Then the product must keep the view state untouched for a key event
   that is supposed to have no effect at all - including the key *release* path
   (`KomportView::keyReleaseEvent`, not read to its end yet).

Do not pick one by guessing; read `select()`/`slotSelectionChanged()`/`keyReleaseEvent` and decide.
The assertion must not be weakened either way.

## Next steps

1. Settle the selection question above (product fix or complete the caller mirroring).
2. Run the whole suite: `cmake --build build -j$(nproc)` then `cd build && ctest` (expect 21/21;
   build with `-DCMAKE_CXX_FLAGS="-Wall -Wextra"`, no warnings from the changed files).
3. Third adversarial review round (the brief pattern is `/tmp/m10-step4-adversarial2.md`, result
   record `docs/reviews/2026-09-20-M10-step4-adversarial-review-3.md`), then the fix round.
4. Commit only after an approving round; **no push** without an explicit instruction. Stage only the
   files of this slice - `AGENTS.md`, `CLAUDE.md`, `docs/project-context.md`,
   `docs/example-charsets/README.md` and `docs/translation-glossary.md` are foreign, uncommitted
   changes of the owner and must stay untouched.

## Standing constraints from the owner

- The strict comparisons stay as they are; a newly red test with a real defect is never softened -
  stop and report it.
- No artificial reproduction of historical `readAll()` chunk boundaries: "same display bookkeeping"
  means semantic equivalence of the result, not identical call frequency.

## Earlier milestones of this branch (all committed and pushed)

`006a607`+`18a3943` (Slice 1, reader) - `64bd85c` (spec re-cut 3a/3b) - `41460f8` (3a, replay core)
- `b804055` (3b, mutation authorization).
