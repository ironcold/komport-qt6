## Review result — round 1

Two findings; no BLOCKER or HIGH finding.

- **MEDIUM — TEST GAP:** The new PTY test proves that its *hand-written sequence* works, but does not exercise `KomportApp::closeEvent()`. It calls `document.getSerial()->close()` and `recorder->stop()` itself at [tst_sessiondocument.cpp:310](/home/max/Development/misc/komport-qt6/tests/tst_sessiondocument.cpp:310) and [tst_sessiondocument.cpp:315](/home/max/Development/misc/komport-qt6/tests/tst_sessiondocument.cpp:315). Reversing those two lines would fail the terminal-record assertion, but reversing the production order at [komport.cpp:936](/home/max/Development/misc/komport-qt6/komport/komport.cpp:936)-[948](/home/max/Development/misc/komport-qt6/komport/komport.cpp:948) would not fail this test.

  Minimal fix: add an application-close-path test, or extract the non-UI close-finalisation sequence into a small callable helper used by `closeEvent()` and test that helper with the PTY.

- **LOW — DOCUMENTATION / test isolation:** The ownership comment says the recorder is “declared before the controller and the transport” at [komportdoc.h:126](/home/max/Development/misc/komport-qt6/komport/komportdoc.h:126), but `mSerial` is actually declared first at [line 125](/home/max/Development/misc/komport-qt6/komport/komportdoc.h:125). The code order is correct; the comment is not. Also, the idle-start test uses a fixed `/tmp/never-written.kpsession` path at [tst_sessiondocument.cpp:256](/home/max/Development/misc/komport-qt6/tests/tst_sessiondocument.cpp:256), so an unrelated pre-existing file makes it fail without testing the invariant.

  Minimal fixes: say “declared after `mSerial` and before the controller”; use `QTemporaryDir` for the refused-start target.

## Verified

- Ownership and destruction are correct. Declaration order is `mSerial`, recorder, controller, whose natural reverse destruction is controller → recorder → serial; explicit resets enforce the same result at [komportdoc.cpp:61](/home/max/Development/misc/komport-qt6/komport/komportdoc.cpp:61)-[66](/home/max/Development/misc/komport-qt6/komport/komportdoc.cpp:66). This also handles the by-value transport correctly. Construction is controller first, recorder second at [lines 44-48](/home/max/Development/misc/komport-qt6/komport/komportdoc.cpp:44).

- The recorder intentionally exists briefly after its controller is reset, leaving its raw pointer dangling, but it does not dereference it during destruction: its destructor only cancels the scheduler and flushes/closes the sink at [sessionrecorder.cpp:135](/home/max/Development/misc/komport-qt6/komport/sessionrecorder.cpp:135)-[144]. The raw controller pointer is used only by `start()` at [lines 163-168](/home/max/Development/misc/komport-qt6/komport/sessionrecorder.cpp:163). The timer is cancelled before teardown; no event loop turn occurs between the two explicit resets. Current same-thread construction also means the normal recorder connection is direct in practice. No UAF found.

- Direct document destruction records no final close event: `KomportSerial::~KomportSerial()` closes its underlying port directly and emits no `closed` signal at [komportserial.cpp:141](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:141)-[148].

- The application closes the same transport observed by the document controller: `KomportView::getSerial()` returns `getDocument()->getSerial()` at [komportview.cpp:506](/home/max/Development/misc/komport-qt6/komport/komportview.cpp:506). The close is therefore correctly before recorder finalisation. `stop()` runs for both `Live` and `Damaged` states; only `Stopped` is skipped. The `qWarning` for a damaged close is appropriate technical reporting for this slice and does not prematurely invent step-5 UI.

- The PTY test checks the important file semantics: exactly one terminal `TransportClosed`, no `TransportOpened`, final record is closed, and report count equals parsed-file count at [tst_sessiondocument.cpp:324](/home/max/Development/misc/komport-qt6/tests/tst_sessiondocument.cpp:324)-[342]. It would fail if its local close/stop order were reversed.

- No action, indicator, translated status text, reader/replay, or other step-5/6 UI scope leaks in. The `closeEvent()` ordering is formally listed under implementation step 5 in the spec, but the supplied slice explicitly includes this narrow normative portion; no broader wiring was added.

## Step-5 decision: configuration snapshot

**ARCHITECTURE QUESTION — resolve before production wiring.** The test helper’s subtraction is valid as a test of the current M8 metadata shape and ADR-006 profile. It must not become the production mechanism: [tst_sessiondocument.cpp:82](/home/max/Development/misc/komport-qt6/tests/tst_sessiondocument.cpp:82) calls `applyConfiguration()`, which is explicitly the mutating transaction entry point, even though the current equal-configuration path is a no-op ([komportserial.cpp:705](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:705)-[710]).

Recommendation: add a specified, read-only `KomportSerial` accessor returning the four-member applied-configuration snapshot by value, with no transaction fields and no emitted observation. Document it as the D7 source before implementing step 5; do not make application wiring derive a snapshot by subtracting fields from transaction metadata.

## Commit assessment

This is one cohesive commit, but I would not approve it **as-is** because the normative production close ordering lacks a regression test. After the MEDIUM test gap and LOW cleanup are addressed, it may be committed as one commit.

I did not run the build or tests in the read-only environment, so the reported warning-free build and 15/15 test result remain author-provided evidence rather than independently verified.