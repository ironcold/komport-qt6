Round 3 verdict: **not ready to commit** due to one new MEDIUM finding. All items carried from round 2 are closed.

| Item | Verdict | Evidence |
|---|---|---|
| HIGH: zero-valued clock duration | Closed | `mRecordingStarted` replaces the zero sentinel, and damaged duration selects `mEndNs` by state: [sessionrecorder.cpp](/home/max/Development/misc/komport-qt6/komport/sessionrecorder.cpp:225), [durationNs](/home/max/Development/misc/komport-qt6/komport/sessionrecorder.cpp:301). Zero-start clean and damaged cases are covered: [test](/home/max/Development/misc/komport-qt6/tests/tst_sessionrecorder.cpp:854). |
| MEDIUM: canonical references and UTC creation time | Closed for the reported variants | The reader requires decimal strings, rejects signs/whitespace/leading zeroes, and permits `"0"`: [sessionrecordreader.h](/home/max/Development/misc/komport-qt6/tests/sessionrecordreader.h:76). It validates a parseable UTC ISO date: [sessionrecordreader.h](/home/max/Development/misc/komport-qt6/tests/sessionrecordreader.h:119). Mutations cover the reported cases: [test](/home/max/Development/misc/komport-qt6/tests/tst_sessionrecorder.cpp:927). |
| MEDIUM: zero-record status | Closed | Clean zero-record stop supplies the exact required reason: [sessionrecorder.cpp](/home/max/Development/misc/komport-qt6/komport/sessionrecorder.cpp:370). The report contract documents it: [sessionrecorder.h](/home/max/Development/misc/komport-qt6/komport/sessionrecorder.h:79). Exact assertion: [test](/home/max/Development/misc/komport-qt6/tests/tst_sessionrecorder.cpp:341). |
| Gap: empty metadata | Closed | A Data record asserts empty metadata bytes and `40 + payload` record length: [test](/home/max/Development/misc/komport-qt6/tests/tst_sessionrecorder.cpp:382). |
| Gap: start-flush visibility via real file handle | Closed | The production sink is exercised through a separate `QFile` before stop, then re-read after the exact payload is written: [test](/home/max/Development/misc/komport-qt6/tests/tst_sessionrecorder.cpp:885). |

New finding:

- **MEDIUM — profile reader accepts fractional fixed numeric fields.** `version` and `sourceId` are checked via `toInt() == 1`, which accepts a fractional JSON number such as `1.5` after conversion rather than requiring the fixed numeric value itself: [sessionrecordreader.h](/home/max/Development/misc/komport-qt6/tests/sessionrecordreader.h:117), [sessionrecordreader.h](/home/max/Development/misc/komport-qt6/tests/sessionrecordreader.h:140). This lets the independent structural reader accept a non-conformant v1 writer profile.

  Minimal fix: require `isDouble()` and `toDouble() == 1.0` for both fields, then add negative `1.5` mutations for `version` and `sourceId` in [tst_sessionrecorder.cpp](/home/max/Development/misc/komport-qt6/tests/tst_sessionrecorder.cpp:930).

Therefore this slice should **not** be committed yet; make that local reader/test correction, then re-review.

I could not run the build or tests in this read-only environment. The reported warning-free `-Wall -Wextra` build, 15/15 `ctest`, and 20 green recorder methods remain author evidence. I also could not independently verify actual `QFile::flush()` visibility or timer dispatch at runtime. The deliberately deferred >64 MiB recorder case and character-signal PTY property remain unverified.