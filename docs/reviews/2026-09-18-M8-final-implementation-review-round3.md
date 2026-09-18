## Round-3 verification verdict

1. **TEST GAP — Start Bits UI/persistence: Closed.**  
   The new real-application test seeds `StartBits=2`, loads the profile, verifies the staged request, saves, and re-reads `"2"`: [tst_profileerror.cpp](/home/max/Development/misc/komport-qt6/tests/tst_profileerror.cpp:114), [tst_profileerror.cpp](/home/max/Development/misc/komport-qt6/tests/tst_profileerror.cpp:123), [tst_profileerror.cpp](/home/max/Development/misc/komport-qt6/tests/tst_profileerror.cpp:143), [tst_profileerror.cpp](/home/max/Development/misc/komport-qt6/tests/tst_profileerror.cpp:148), [tst_profileerror.cpp](/home/max/Development/misc/komport-qt6/tests/tst_profileerror.cpp:152). The test-only protected-access seam is confined to the test at [tst_profileerror.cpp](/home/max/Development/misc/komport-qt6/tests/tst_profileerror.cpp:37). Production paths load, stage, and save the field at [komport.cpp](/home/max/Development/misc/komport-qt6/komport/komport.cpp:735), [komport.cpp](/home/max/Development/misc/komport-qt6/komport/komport.cpp:652), and [komport.cpp](/home/max/Development/misc/komport-qt6/komport/komport.cpp:606).

   I do **not** require a test that drives the modal combo box. The accepted §13 condition is now covered by the transport test plus this end-to-end profile path; dialog wiring remains directly inspectable at [komport.cpp](/home/max/Development/misc/komport-qt6/komport/komport.cpp:1210) and [komport.cpp](/home/max/Development/misc/komport-qt6/komport/komport.cpp:1241). Requiring modal-dialog automation would need a new UI-driving arrangement and exceeds this accepted M8 slice.

2. **TEST GAP — failed-with-change ordering: Closed.**  
   The test records both signals in one collector at [tst_transport.cpp](/home/max/Development/misc/komport-qt6/tests/tst_transport.cpp:852), executes two failed-with-change transactions, and asserts the complete required order at [tst_transport.cpp](/home/max/Development/misc/komport-qt6/tests/tst_transport.cpp:903). This matches the normative mapping at [SPEC-M8](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M8-session-transport-foundation.md:175) and the production emission order at [komportserial.cpp](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:858).

3. **DOCUMENTATION — cosmetic: Closed.**  
   The application-state comment now documents direct synchronous failure notification and suppression of the matching `QSerialPort` duplicate: [komport.h](/home/max/Development/misc/komport-qt6/komport/komport.h:268). The profile-error test comment no longer describes the obsolete `errorOccurred()` route: [tst_profileerror.cpp](/home/max/Development/misc/komport-qt6/tests/tst_profileerror.cpp:76). The implementation agrees: [komportserial.cpp](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:185), [komportserial.cpp](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:527).

## New findings

None.

## Completion

**M8 may be declared complete after the current step-6/step-7 slice, including the review record, is committed.** There is no remaining blocker or must-fix inside the accepted M8 scope.

In §14, tick all criteria except the first:

- Core-only contract target
- Additive binary observations / legacy API preservation
- Document-owned ordered byte-exact events
- Source ID and timing preservation
- QtCore-only public value contracts
- Observable lifecycle/configuration plus compatibility behavior
- NUL/all-byte PTY coverage
- Full CTest suite
- No excluded M8 features introduced

Leave the first checkbox open: no genuine Qt 6.3 build has been run. The self-review records only Qt 6.11.1 and explicitly leaves Qt 6.3 unverified: [self-review](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-18-M8-implementation-selfreview.md:153). The criterion itself requires an actual 6.3 build: [SPEC-M8](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M8-session-transport-foundation.md:465).

## Not independently verified

I could not execute CTest in this read-only environment: CTest attempts to write `build/Testing/Temporary/LastTest.log`. Therefore the reported warning-free build, 13/13 CTest result, and absence of skips remain recorded evidence rather than independently rerun results. I also could not verify a Qt 6.3 build, live hardware behavior, or delayed platform-specific `QSerialPort::errorOccurred()` delivery.