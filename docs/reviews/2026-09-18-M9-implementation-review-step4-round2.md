## Round-2 verdict

- **Original MEDIUM: resolved.** The production helper closes transport before finalising the recorder at [komportdoc.cpp:235](/home/max/Development/misc/komport-qt6/komport/komportdoc.cpp:235) and [komportdoc.cpp:242](/home/max/Development/misc/komport-qt6/komport/komportdoc.cpp:242). `closeEvent()` uses that exact helper at [komport.cpp:940](/home/max/Development/misc/komport-qt6/komport/komport.cpp:940). The PTY test invokes it at [tst_sessiondocument.cpp:317](/home/max/Development/misc/komport-qt6/tests/tst_sessiondocument.cpp:317) and verifies the terminal record is present and final at [tst_sessiondocument.cpp:340](/home/max/Development/misc/komport-qt6/tests/tst_sessiondocument.cpp:340). This now guards the production operation, not a duplicate sequence.

- **Original LOW: resolved.** The member-order comment is now accurate at [komportdoc.h:134](/home/max/Development/misc/komport-qt6/komport/komportdoc.h:134), and the refused-start target is isolated with `QTemporaryDir` at [tst_sessiondocument.cpp:255](/home/max/Development/misc/komport-qt6/tests/tst_sessiondocument.cpp:255).

- **New MEDIUM — unapproved public-API/spec drift.** The behavior is architecturally sound, but the accepted specification explicitly assigns the close call to `KomportApp::closeEvent()` at [SPEC-M9 §5.8](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M9-live-session-recording.md:217), as does ADR-010 at [§1](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-010-live-session-recording.md:51). The change introduces public `KomportDoc::closeSession()` at [komportdoc.h:107](/home/max/Development/misc/komport-qt6/komport/komportdoc.h:107) and relocates the sequence there.

  This is a good design, but it is not merely an implicit reading of §5.8: it adds public API and changes the named owner of the sequence. The binding spec-before-code rule requires an amendment and review before accepting it.

  Minimal fix: amend SPEC-M9 §5.6/§5.8 and ADR-010 §1 to state that `closeEvent()` initiates closure by invoking the document’s `closeSession()` operation, which closes transport, finalises any non-stopped recorder, and returns its report. The application remains responsible for timing and presentation. Then update the test-plan name if desired.

## D7 recommendation

Confirmed. Step 5 should first specify a read-only `KomportSerial` accessor returning the exact four-member applied snapshot by value (`requested`, `effective`, `localBuffering`, `compatibility`), with no transaction, mutation, or emitted observation. The application supplies that value to the recorder.

Do not derive production snapshots by subtracting fields from `ConfigurationResult::toMetadata()`: that function is expressly transaction metadata at [transportconfiguration.h:118](/home/max/Development/misc/komport-qt6/komport/transportconfiguration.h:118), and the current test helper reaches it through `applyConfiguration()` at [tst_sessiondocument.cpp:82](/home/max/Development/misc/komport-qt6/tests/tst_sessiondocument.cpp:82). Document the accessor as D7’s source in the M9 amendment and the frozen M8 contract, analogous to D8.

## Commit assessment

**Not yet commit-ready as-is** because of the MEDIUM specification/API drift. Once the amendment is accepted, this remains one cohesive commit: document ownership, close helper, application delegation, and the PTY regression belong together.

I verified the source-level order and `git diff --check` cleanliness. I could not run the build/tests, observe the PTY runtime behavior, or independently verify the reported mutation-test failure in this read-only environment.