## M9 step 5b — independent review, round 1

**Decision: changes requested. Do not commit this slice yet.**

### Findings

- **HIGH — Damage leaves the UI claiming that recording is still active.**  
  [`updateRecordingUi()`](/home/max/Development/misc/komport-qt6/komport/komport.cpp:1508) uses `state() != Stopped`, so `Damaged` sets both the action checked and the permanent indicator visible at [1509–1512](/home/max/Development/misc/komport-qt6/komport/komport.cpp:1509). A recorder damage signal is delivered while the state is `Damaged` ([sessionrecorder.cpp:324–344](/home/max/Development/misc/komport-qt6/komport/sessionrecorder.cpp:324)); no further data is written in that state. This contradicts the required “visible exactly while running” behavior and leaves the new capture-risk indicator false-positive.

  Minimal fix: derive the UI state from `state() == SessionRecorder::State::Live`, not “not stopped.” This also unchecks the action before the damage report is shown.

- **MEDIUM — Damaged-recording status reports omit required finalisation facts.**  
  [`recordingSummary()`](/home/max/Development/misc/komport-qt6/komport/komport.cpp:1517) returns only the reason for a damaged report. It omits the target path, complete-record count, accepted byte count, and duration, although the report contains all four and SPEC-M9 requires them for stop/failure reporting ([SPEC-M9:195–200](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M9-live-session-recording.md:195), [ADR-010:83–86](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-010-live-session-recording.md:83)).

  Minimal fix: make the damaged branch say, in substance, “Recording ended damaged: N complete records, B bytes written in T s, to PATH: REASON.” Do not describe those bytes as durable/on-disk; they are the recorder’s accepted-byte count.

- **TEST GAP — The new UI test does not assert status reporting.**  
  The idle-refusal test verifies the return value, file absence, action, and indicator ([tst_sessionrecordingui.cpp:103–115](/home/max/Development/misc/komport-qt6/tests/tst_sessionrecordingui.cpp:103)), but not the required refusal message. Likewise, the success test calls the stop path but does not inspect the status text ([161–165](/home/max/Development/misc/komport-qt6/tests/tst_sessionrecordingui.cpp:161)). The source does route both through the existing status label ([komport.cpp:1359](/home/max/Development/misc/komport-qt6/komport/komport.cpp:1359), [1492](/home/max/Development/misc/komport-qt6/komport/komport.cpp:1492), [1461](/home/max/Development/misc/komport-qt6/komport/komport.cpp:1461)).

  Minimal fix: assert `hoverHintLabel` contains the refusal outcome and the clean-stop summary.

### Verified

- The new action is separately named, checkable, uses the author-chosen distinct `media-tape` icon, and appears in both Session menu and toolbar ([komport.cpp:217–221](/home/max/Development/misc/komport-qt6/komport/komport.cpp:217), [257–259](/home/max/Development/misc/komport-qt6/komport/komport.cpp:257), [285–287](/home/max/Development/misc/komport-qt6/komport/komport.cpp:285)). The existing text logger action remains untouched at [208–211](/home/max/Development/misc/komport-qt6/komport/komport.cpp:208).
- No recording state is persisted: the new action has no settings read/write path, and recording starts only through the explicit action/helper. The recorder remains inert otherwise.
- A permanent status-bar `QLabel` is an appropriate visible mechanism. `isHidden()` is the correct assertion for this unshown-window test: it tests the widget’s explicit hidden state, unlike `isVisible()`.
- Reading the configuration snapshot before the recorder checks `Live` is safe: the accessor is read-only ([komportserial.cpp:909–917](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:909)). The recorder validates live state and header before opening the target ([sessionrecorder.cpp:163–191](/home/max/Development/misc/komport-qt6/komport/sessionrecorder.cpp:163)), so an idle refusal cannot create or truncate a file.
- The zero-record text is accurate: it says no *session data* was recorded, without denying that a valid header-only file exists. The normal counts/duration line does not overclaim durability.
- The damage-path application test gap is acceptable for this slice. The recorder’s injected-sink tests own failure reproducibility; adding an application seam solely to force filesystem failure would widen the production boundary. Typed direct connection is present at [komport.cpp:396–397](/home/max/Development/misc/komport-qt6/komport/komport.cpp:396). Fixing the state predicate above makes inspection sufficient for this wiring.
- `Q_MOC_INCLUDE("sessionrecorder.h")` with the forward declaration is correct for the reference parameter ([komport.h:57–66](/home/max/Development/misc/komport-qt6/komport/komport.h:57)). This is a same-thread direct signal/slot connection; no separate metatype declaration is required here.
- German strings added for this UI are translated ([komport_de.ts:147–155](/home/max/Development/misc/komport-qt6/komport/translations/komport_de.ts:147), [476–509](/home/max/Development/misc/komport-qt6/komport/translations/komport_de.ts:476)). Leaving the five already-present unfinished M8/5a entries unchanged is proper scope control; the older unrelated `Visual Bell` unfinished entry also remains untouched ([900–902](/home/max/Development/misc/komport-qt6/komport/translations/komport_de.ts:900)).
- The UI test’s single PTY RX check is application-wiring coverage, not the deferred full byte-exact PTY end-to-end test from step 6.

After the two code fixes and the small status assertions, step 5b may be committed as one cohesive commit, subject to the stated build/test rerun. I could not independently run the build, tests, generated-QM verification, or interact with the native save-dialog overwrite behavior in this read-only environment.