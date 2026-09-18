## Findings

- **HIGH** — preferences retries a failed live endpoint change, opening twice for one user action.  
  `komport.cpp:1275` can reopen inside `applyConfiguration()` when the endpoint changes (`komportserial.cpp:611-615`). If that reopen fails, it returns non-store-only and leaves the port closed (`komportserial.cpp:617-627`); `komport.cpp:1280` then calls `open()` again. This violates (b) and produces a second failed activation/error.  
  Minimal fix: retain the result and open only for the initial closed-port store-only case:

  ```cpp
  const ConfigurationResult result = serial->applyConfiguration(request);
  if (!serial->isOpen() && result.storedOnly) serial->open();
  ```

- **TEST GAP** — existing tests cover the transport-level failed endpoint change (`tests/tst_transport.cpp:585-615`), but not the application preferences path that adds the unconditional closed-port retry. Add a regression that changes a live preferences endpoint to an invalid path and asserts one failed open attempt/error.

- **DOCUMENTATION** — comments overclaim failure behavior.  
  `komport.cpp:639-642` says the subsequent `open()` applies the request “exactly once”; an invalid/empty endpoint fails before any configuration transaction. `komport.cpp:1258-1263` likewise says a live port is reconfigured in exactly one transaction, which is false on failed endpoint reopen and especially misleading while the second retry remains. Qualify these as successful-open behavior.  
  Also, `komport.h:272-275` says `open()` emits `settingsFailed()` synchronously, while failed `open()` itself emits only `transportError` (`komportserial.cpp:185-193`) and relies on `QSerialPort::errorOccurred` → `slotPortError()` for `settingsFailed()` (`komportserial.cpp:512-515`); the implementation itself notes this signal can be asynchronous (`komportserial.cpp:177-180`). Correct the comment or make the guarantee real.

No issues found by static inspection for (a), profile-path close/open behavior, scroll-buffer clamping, persistence/live appearance handling, or removal of application calls to the listed legacy setters. `settingsChanged()` has no production control connection; `settingsFailed()` remains connected only to UI error/status handling.

## Unauthorized behavior change

None newly introduced beyond the authorized migration was found. The double failed-open retry is retained from the legacy preferences flow, but M8 explicitly requires that it be eliminated.

## Verdict

**Do not commit yet.** Must fix the failed live-endpoint retry, add coverage, and correct the overclaiming comments.

## Not independently verified

I did not run the build or tests, validate against a live serial device, or verify platform-specific `QSerialPort` signal timing. The supplied 0-warning/13-test evidence was reviewed as stated, not re-executed.