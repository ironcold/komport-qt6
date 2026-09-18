## Round-3 verification verdict

**MEDIUM closed.** The amendment removes the API/ownership drift and faithfully matches the code:

- SPEC-M9’s current-state, lifecycle, ownership, and implementation-step text now names `KomportApp::closeEvent()` as the trigger and `KomportDoc::closeSession()` as the testable owner of the normative sequence.
- ADR-010 §1 now makes the same decision-level assignment.
- [`closeSession()`](/home/max/Development/misc/komport-qt6/komport/komportdoc.cpp:235) closes the transport before stopping any non-stopped recorder; [`closeEvent()`](/home/max/Development/misc/komport-qt6/komport/komport.cpp:940) delegates to it and handles a damaged report.
- The PTY regression exercises that production operation and checks that `TransportClosed` is written exactly once and is the final record.

No new findings.

The complete slice may now be committed as one cohesive commit: document-owned recorder, `closeSession()`, application delegation, PTY regression, and the SPEC-M9/ADR-010 amendments belong together.

I verified source-level behavior and `git diff --check` cleanliness. I could not run the warning-enabled build, the 15-test `ctest` suite, the PTY test, or independently repeat the order-reversal mutation experiment in this read-only environment; those remain author-provided evidence.