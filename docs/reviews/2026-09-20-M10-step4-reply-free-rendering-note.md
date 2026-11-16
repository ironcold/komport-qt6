# M10 step 4 implementation note: the reply-free rendering path

Written 2026-09-20, before any code, as the reviewable input for the slice's gate. Scope:
`docs/specs/SPEC-M10-session-reader-passive-replay.md` §11 step 4, implementing §5.7's
reply-free rendering entry and §5.8 clause 3 (the choke point). The normative text already
exists and is accepted - this note does not amend it, it plans the implementation.

## What the spec requires (quoted, not restated)

- The emulation's terminal-generated replies have exactly **one private choke point**,
  `KomportEmulation::sendTerminalReply(const QByteArray &)`, and the replay entry suppresses
  "that choke point for the duration of the call only (a flag that is set on entry and cleared
  on every exit, so it can never persist into the live path). The live entry points keep their
  replies, which a test pins (`liveTerminalQueriesStillAnswer`)." (§5.7)
- `KomportView::slotReplayReceivedChar(char)` (new) "forwards the byte to the emulation's
  replay entry point and performs the same display bookkeeping as the live
  `slotReceivedChar(char)`. It is a separate slot, so the live connection is untouched and the
  replay path is unreachable from the live signal." (§5.7)
- `KomportView::setKeyInputEnabled(false)` "makes `keyPressEvent()` consume the event without
  emitting `keyPressed`, so the view keeps its focus, its selection and its scrolling but
  produces no bytes" (§5.7.1, offline row for key input).
- A replayed `CSI 5 n`, `CSI 6 n`, `CSI c` or `ESC Z` "is displayed, not answered" (§5.8.3).

## Inventory (verified in the tree, 2026-09-20)

Reply sites - all in `komport/komportemulation.cpp`, all currently `serial()->putStr(...)`:

| Site | Query it answers | Code |
| --- | --- | --- |
| `doDeviceStatusReport()` | `CSI 5 n` | `komportemulation.cpp:1271` - `serial()->putStr("\x1b[0n")` |
| `doDeviceStatusReport()` | `CSI 6 n` | `komportemulation.cpp:1276-1277` - builds `CSI r;c R` from `cellArray()->cursor()`, `serial()->putStr` |
| `doDeviceAttributes()` | **all CSI `c` forms the dispatcher currently accepts** - `CSI c`, `CSI 0 c` and private and noncanonical variants included, because the dispatcher selects the handler on the final `c` and `doDeviceAttributes()` reads no parameters at all (`komportemulation.cpp:1265`) - and the classic **`ESC Z`** | `komportemulation.cpp:1292` - `serial()->putStr("\x1b[?62c")`; the old VT100 form `ESC Z` reaches it from the escape dispatcher at `komportemulation.cpp:1452` |

Dispatch / receive path: the two calls sit at `komportemulation.cpp:1403` (status report) and
`:1406` (device attributes) inside the CSI dispatcher; the escape dispatcher's `case 'Z'` is at
`:1452`; the live receive entry is `KomportEmulation::slotReceivedChar(char)` at `:1465`.

View side: `KomportView::slotReceivedChar(char)` performs the view's own scroll bookkeeping only
(`komportview.cpp:497`) - it does **not** forward to the emulation, which receives the serial
signal independently (`komportemulation.cpp:655`); both live connections and bodies stay exactly
as they are. A stored sequence reaching the replay entry has no path to key simulation
(`komportemulation.cpp:696`), simulated paste (`:1581`), macro output (`komport.cpp:1402`), a
transfer (`komporttransfer.cpp:56`) or the view's `keyPressed` - those are active user paths and
remain the offline guards' business (steps 5-6). `KomportView::keyPressEvent()` emits `keyPressed`, which the
emulation turns into `putChar()`/`putStr()`.

Not in scope: the other `serial()->putStr/putChar/write` uses in the file are ordinary text
output (printing the character stream), not terminal-generated replies; the spec's choke point
is about replies. Whether future active-output work should route through it is explicitly left
to a later milestone (§5.7: "the single place future active-output work would have to pass
through").

## Design

1. **The choke point.** A private `void KomportEmulation::sendTerminalReply(const QByteArray &)`
   that does exactly one thing: if replies are currently suppressed, return; otherwise
   `serial()->putStr(...)`. The three reply sites above are rewritten to call it. This is the
   only place the emulation answers the terminal, so the property is structural: a new reply
   site that forgets the choke point is visible as a `putStr` outside the three known ones.
2. **Suppression is a scoped reply mode, save-and-restore in *both* entries.** A plain boolean
   that the replay entry sets true and clears on exit is wrong in two directions, because
   rendering emits synchronous signals and therefore re-enters: a nested replay call would clear
   the flag when its inner scope exits and let the outer replay answer, and a re-entrant *live*
   call during a replay would inherit the suppression and fail to answer, which §5.7 forbids. The
   flag is a reply *mode* restored by scope instead: both entries wrap the shared work in a
   `QScopedValueRollback<bool>` - the live entry installing "replies allowed", the replay entry
   installing "replies suppressed" - so an outer replay keeps suppressing across an inner scope,
   an inner live call answers again inside it, and every exit restores its caller's state. The
   shared work moves into a private `renderReceivedChar(char)`; the entries differ only in the scope
   they install, and the member is explicitly initialised to "replies allowed" so the live path is
   never suppressed by default. Tests pin both directions.
3. **The view entry.** `KomportView::slotReplayReceivedChar(char)` mirrors
   `slotReceivedChar(char)`'s display bookkeeping exactly (the same view-side work, per §5.7)
   but forwards to `KomportEmulation::slotReplayReceivedChar()` instead of the live slot, and
   never emits `keyPressed` or touches the transport. Its own slot, so the live connection is
   untouched. Order matters and is copied, not invented: the view forwards to the emulation
   **before** its own bookkeeping, exactly as the live path already does (the emulation is
   connected first, `komportview.cpp:88`; the view's bookkeeping second, `:497`).
4. **Key input.** `KomportView::setKeyInputEnabled(bool)` sets a `bool mKeyInputEnabled` that
   `keyPressEvent()` consults first: when false, the event is consumed (`event->accept()`)
   and no `keyPressed` is emitted. Default true, so nothing changes for the live path.

## The one open design question (for the reviewer)

**How does a test observe "no byte was written"?** The emulation holds a concrete
`KomportSerial` (`serial()`), not an interface, so there is no injection point today. The
options are (a) a narrow test seam on the emulation - the precedent is M9's scheduler/clock
seams and the M10 player's `SessionReplayPlayerSeamsForTest`, a friend-for-testability type that
installs an observer instead of a transport; (b) the §5.8 dynamic test against a real PTY
(`openpty`, as `tst_serial` already does), which proves the property end-to-end but cannot
distinguish "no write" from "write to a peer we are not reading"; (c) an `ITransport`-shaped
write seam on the emulation, which would widen a public boundary ADR-003 keeps closed.

**I was wrong here, and the correction is the spec's own decision:** §5.8 expressly chooses *no*
transport-substitution seam and fixes the evidence set - the existing `ITransport::bytesWritten`
signal (emitted from the sole accepted-write primitive, `komportserial.cpp:408`), the **source
audit**, and the **PTY peer** staying silent. Option (a) is therefore rejected: no new
friend-for-testability observer in the emulation, and (c) stays rejected. The dynamic test
subscribes to `bytesWritten` and asserts it never fires while the stored queries are replayed,
with the PTY peer read proving silence end to end.

## Test plan (the spec's names, plus the unit pins)

| Test | Claim |
| --- | --- |
| `liveTerminalQueriesStillAnswer` (the spec names it) | with the live entry, `CSI 5 n`, `CSI 6 n`, `CSI c` and `ESC Z` each produce their reply through the choke point |
| `replayedQueriesAreRenderedButNotAnswered` | the same four sequences through the replay entry: zero writes, while the display state changes as it would live (cursor report case) |
| `suppressionNeverSurvivesTheReplayCall` | a live query immediately after a replay call answers again (the flag is cleared on every exit, including when the replay byte is not a query) |
| `replayedQueriesOfEveryAcceptedFormAreNotAnswered` | driven through **`KomportView::slotReplayReceivedChar()`** (the §5.7 view entry), with printable sentinels beside the four canonical queries and one private `c` variant: the screen and cursor change exactly as for live input, the view's scroll bookkeeping matches the live slot's, no `bytesWritten` fires and the PTY peer stays silent. The view forwards to the emulation **before** its own bookkeeping, matching the live order (the emulation is connected first, `komportview.cpp:88`; the view's bookkeeping second, `:497`) |
| `replayEntriesAndReplySitesPassTheSourceAudit` | the spec's audit, broader than the dispatchers: both replay entries and, once added, the display adapter contain no direct write; the **two reply functions** (`doDeviceStatusReport()`, `doDeviceAttributes()`) call `sendTerminalReply()` at all three of their sites; that choke point is the only direct reply write in the emulation. Staged: the step-4 commit audits the two replay entries and the reply sites; the display adapter joins the same audit when it exists (steps 6-7) |
| `aNestedReplayStaysSuppressedAndAReentrantLiveCallAnswers` | synchronous re-entry **created explicitly** by a `cellChanged` receiver - that signal is emitted synchronously from `drawChar()` (`komportcellarray.cpp:251`) - which issues a nested replay call, and in the other direction a live query, *while the outer replay render is active*: the outer stays suppressed, the inner live call still answers, and the reply mode is restored to its caller's value after each inner call |
| `replayRenderingNeverEmitsKeyInput` | the replay entry leaves `mKeyInputEnabled` alone and emits no `keyPressed`; `setKeyInputEnabled(false)` consumes a key press without a byte (unit + the view's own test) |

## Order of work

1. This note reviewed by the slice's gate (it decides the seam question above).
2. Emulation: choke point + suppression guard + replay entry (small, self-contained).
3. View: the replay slot + `setKeyInputEnabled`.
4. Tests per the table, then the self-review with the "no hidden reply path" argument (the
   mirror of 3a's "no hidden re-entrancy" argument).
5. Gate, fix cycle, commit - then §11 steps 5-8 (Doc ownership, offline UI wiring, audits,
   end-to-end PTY test).
