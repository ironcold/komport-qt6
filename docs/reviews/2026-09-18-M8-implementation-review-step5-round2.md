Final verdict: **may be committed as one commit.** No new findings.

1. **HIGH — closed.**  
   [`komport.cpp:1279`](/home/max/Development/misc/komport-qt6/komport/komport.cpp:1279) retains the one `ConfigurationResult`, and [`komport.cpp:1288`](/home/max/Development/misc/komport-qt6/komport/komport.cpp:1288) implements the exact minimal guard: open only when closed *and* `storedOnly`.

   A live endpoint change calls `open()` internally ([`komportserial.cpp:611`](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:611)); a failure returns non-store-only ([`komportserial.cpp:617`](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:617)), so the preferences guard cannot retry it. The only other application connection path, [`applyConnectionSettings()`](/home/max/Development/misc/komport-qt6/komport/komport.cpp:625), closes first, stores its request while closed, then has one explicit `open()` at line 656. No other production `KomportApp` path opens the serial transport.

2. **TEST GAP — closed.**  
   [`tst_sessiondocument.cpp:146`](/home/max/Development/misc/komport-qt6/tests/tst_sessiondocument.cpp:146) opens a real PTY-backed document transport, changes its live endpoint to an invalid path, verifies closed/non-store-only state and exactly one session `Error(kind="open")` ([lines 180–184](/home/max/Development/misc/komport-qt6/tests/tst_sessiondocument.cpp:180)), then applies the same result-aware guard ([lines 186–192](/home/max/Development/misc/komport-qt6/tests/tst_sessiondocument.cpp:186)).

   Replacing that guard with the old unconditional closed-port guard would call `open()` again. `KomportSerial::open()` synchronously emits one `transportError` on every failed attempt ([`komportserial.cpp:181`](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:181), [`:185`](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:185)); the controller’s same-thread direct connection and failed-attempt admission accept the new, higher activation ID ([`sessioncontroller.cpp:57`](/home/max/Development/misc/komport-qt6/komport/sessioncontroller.cpp:57), [`:185`](/home/max/Development/misc/komport-qt6/komport/sessioncontroller.cpp:185)). It would therefore make the count at line 190 equal two, not pass due to absent events.

   I accept this as sufficient coverage for this call-site guard. Refactoring the modal GUI method solely to inject a dialog/test seam would exceed the accepted M8 scope.

3. **DOCUMENTATION — closed.**

   - The profile-path comment now limits “exactly once” to a successful open: [`komport.cpp:639-643`](/home/max/Development/misc/komport-qt6/komport/komport.cpp:639).
   - The preferences-path comment explicitly describes failed endpoint reopen as a failed attempt which leaves the port closed: [`komport.cpp:1259-1267`](/home/max/Development/misc/komport-qt6/komport/komport.cpp:1259).
   - The `settingsFailed()` timing comment distinguishes synchronous transaction reporting from the potentially asynchronous `QSerialPort::errorOccurred()` UI notification: [`komport.h:268-276`](/home/max/Development/misc/komport-qt6/komport/komport.h:268).

   The async caveat is pre-existing QSerialPort behavior, not introduced by this step: the transport itself already documents platform-dependent delivery ([`komportserial.cpp:176-180`](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:176)) and this slice does not modify that implementation.

**New findings:** none.

**Not independently verified:** I did not run the warning-enabled build or `ctest`, validate against a live serial device, or independently establish platform-specific Qt signal timing. I verified the supplied test/build claims only by reading the current code and test structure.