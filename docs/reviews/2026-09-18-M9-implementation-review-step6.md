Verdict: **Approved. This step-6 slice may be committed.**

The test is a genuine end-to-end proof: it uses a real pty, live `KomportDoc` session, production recorder and filesystem, then parses the result with the independent test reader. Its byte-exact RX/TX, record ordering/count, non-empty RX payload, header, and chunking assertions are appropriate. CMake wiring correctly adds the target and links the existing pty dependency.

Not asserting a fixed or bounded number of pty observations is the right trade. Observation chunking belongs to OS/pty/Qt scheduling and is not a stable recorder contract. The relevant assertion is already present: recorded RX units are substantially fewer than bytes (`records * 16 < bytes`) while their concatenated payload is exact. That catches the prohibited character-signal path without encoding platform timing into the test.

This closes the specification’s pty end-to-end criterion and provides the intended practical evidence for the no-session-sized-buffer acceptance criterion, alongside the prior recorder-level one-event-per-event test.

“No terminal behaviour changed” is not established by this new test alone, nor should it be: it is an additive recorder-path test. Within M9, the pre-existing terminal/serial/emulation suites plus the dedicated recording-UI/logger regression tests are the appropriate coverage. The reported warning-free build and 17/17 `ctest` result satisfy that acceptance evidence, subject to the normal trust in the supplied execution report.