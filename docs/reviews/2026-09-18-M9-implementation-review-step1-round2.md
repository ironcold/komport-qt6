Review result: **do not commit yet**. The original findings are addressed, but one new HIGH issue blocks approval.

- HIGH — fixed v1 profile: **closed**. Caller-controlled profile fields are gone from `SessionHeaderFacts` ([header:78](/home/max/Development/misc/komport-qt6/komport/sessionrecordcodec.h:78)); the codec writes source ID `1`, the fixed local identity, and session reference `"0"` itself ([codec:239](/home/max/Development/misc/komport-qt6/komport/sessionrecordcodec.cpp:239), [codec:245](/home/max/Development/misc/komport-qt6/komport/sessionrecordcodec.cpp:245)). Configuration validation enforces exact sections, members, and types before serialization ([codec:149](/home/max/Development/misc/komport-qt6/komport/sessionrecordcodec.cpp:149)). The rejection tests cover the required cases ([tests:383](/home/max/Development/misc/komport-qt6/tests/tst_sessionrecordcodec.cpp:383)).

- HIGH — limit checks after serialization/allocation: **closed as originally reported**. The JSON upper-bound walk precedes `QJsonDocument::toJson()` ([codec:45](/home/max/Development/misc/komport-qt6/komport/sessionrecordcodec.cpp:45), [codec:341](/home/max/Development/misc/komport-qt6/komport/sessionrecordcodec.cpp:341), [codec:387](/home/max/Development/misc/komport-qt6/komport/sessionrecordcodec.cpp:387)); record assembly only follows the size check ([codec:401](/home/max/Development/misc/komport-qt6/komport/sessionrecordcodec.cpp:401)).

- MEDIUM — test gaps: **closed for the requested coverage**. The exact record fixture, start-block fixture, configuration rejection cases, and invalid-event cases are present ([tests:334](/home/max/Development/misc/komport-qt6/tests/tst_sessionrecordcodec.cpp:334), [tests:365](/home/max/Development/misc/komport-qt6/tests/tst_sessionrecordcodec.cpp:365), [tests:383](/home/max/Development/misc/komport-qt6/tests/tst_sessionrecordcodec.cpp:383), [tests:434](/home/max/Development/misc/komport-qt6/tests/tst_sessionrecordcodec.cpp:434)). Correction to my round-1 wording: an exact fixture on a little-endian machine cannot distinguish explicit little-endian writing from an accidentally native-endian writer on that same machine; source inspection does confirm `qToLittleEndian` is used ([codec:28](/home/max/Development/misc/komport-qt6/komport/sessionrecordcodec.cpp:28)). A big-endian run would add confidence but is not available here.

- Architecture question: **closed; keep `recordSizesFit()` public.** `headerObject()` is correctly implementation-private ([codec:230](/home/max/Development/misc/komport-qt6/komport/sessionrecordcodec.cpp:230)). The stated public purpose of `recordSizesFit()`—a pure, allocation-free format predicate the future recorder can use when reporting refused-event sizes—is sound and directly tied to SPEC-M9 §5.3 ([header:122](/home/max/Development/misc/komport-qt6/komport/sessionrecordcodec.h:122)). It should remain public.

New findings:

1. **HIGH — conservative preflight wrongly refuses valid, in-limit records and headers.**  
   The six-times-per-character upper bound is treated as a refusal threshold ([codec:73](/home/max/Development/misc/komport-qt6/komport/sessionrecordcodec.cpp:73), [codec:92](/home/max/Development/misc/komport-qt6/komport/sessionrecordcodec.cpp:92)). Thus, for example, an approximately 12 MiB ASCII metadata string has an actual compact JSON encoding well below the 64 MiB record limit but an estimated size over 72 MiB and is refused before serialization ([codec:390](/home/max/Development/misc/komport-qt6/komport/sessionrecordcodec.cpp:390)). This violates the “one accepted event produces exactly one record” rule for events that actually fit ADR-006’s limits.

   Minimal fix: replace the conservative rejection threshold with an exact, allocation-free compact-JSON byte counter (including escaping and UTF-8 length), then serialize only after that exact count fits. Add accepted large-ASCII header/metadata boundary tests.

2. **MEDIUM — invalid creation times produce a nonconformant header.**  
   `createdUtc` is not validated ([header:86](/home/max/Development/misc/komport-qt6/komport/sessionrecordcodec.h:86)); an invalid `QDateTime` is serialized as an empty string ([codec:258](/home/max/Development/misc/komport-qt6/komport/sessionrecordcodec.cpp:258)), not ADR-006’s required ISO-8601 UTC timestamp.

   Minimal fix: reject invalid `createdUtc` before building the header, with a codec test for the refusal.

3. **MEDIUM — buffering-number validation can convert an out-of-range double to `qint64`.**  
   The integral check uses `qint64(value.toDouble())` without a range check ([codec:200](/home/max/Development/misc/komport-qt6/komport/sessionrecordcodec.cpp:200)). A finite JSON number outside `qint64` range can make that conversion undefined.

   Minimal fix: use finite/truncation checks without narrowing conversion, or explicitly range-check first; add a large-double test.

I did not build or run tests, including the author-reported warning-free build and 14/14 `ctest` result, because this review environment is read-only and execution was explicitly out of scope. I also could not execute the endian fixture on a big-endian target.