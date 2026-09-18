Round 4 verdict: **MEDIUM closed. No new findings.**

- The reader now requires `version` to be a JSON number exactly equal to `1.0`: [sessionrecordreader.h](/home/max/Development/misc/komport-qt6/tests/sessionrecordreader.h:119).
- It applies the same strict check to `sourceId`: [sessionrecordreader.h](/home/max/Development/misc/komport-qt6/tests/sessionrecordreader.h:143).
- The regression test rejects `sourceId: 1.5`, `version: 1.5`, and `version: "1"` with the intended profile violations: [tst_sessionrecorder.cpp](/home/max/Development/misc/komport-qt6/tests/tst_sessionrecorder.cpp:981).

This correctly prevents both truncating acceptance of fractional values and acceptance of strings. It conforms to ADR-006’s requirement that `version` and `sourceId` are JSON numbers with fixed value `1`.

The implementation slice may now be committed as one coherent commit: recorder, internal seam header, test-only reader, recorder tests, and the two CMake wiring changes. The prior carried findings remain closed, and this correction introduces no architecture or persisted-format change.

I could not run the build or tests in the read-only environment. The warning-free `-Wall -Wextra` build, 15/15 `ctest` result, and 20 green recorder methods remain author-provided evidence. Runtime behavior of `QFile::flush()` visibility and timer dispatch was not independently verifiable here.