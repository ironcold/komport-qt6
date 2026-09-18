Review result: **not approved for commit yet**. Two HIGH findings must be fixed and independently re-reviewed.

1. **HIGH — the codec can emit headers outside ADR-006’s fixed v1 writer profile.**

   [sessionrecordcodec.h:83](/home/max/Development/misc/komport-qt6/komport/sessionrecordcodec.h:83)–[91] expose mutable `sourceId`, local identity strings, clock kind, and `referenceSessionTimestampNs`; [sessionrecordcodec.cpp:88](/home/max/Development/misc/komport-qt6/komport/sessionrecordcodec.cpp:88)–[103] writes them verbatim. This permits source ID 0 or non-1, arbitrary local-profile strings, and a non-zero session reference, contrary to ADR-006’s mandatory M9 profile.

   Worse, [sessionrecordcodec.cpp:92](/home/max/Development/misc/komport-qt6/komport/sessionrecordcodec.cpp:92)–[94] inserts caller-supplied configuration without validating its exhaustive schema. It can therefore persist private members such as `applyStatus`, omit required members, or use wrong types. The compliant fixture in [tst_sessionrecordcodec.cpp:49](/home/max/Development/misc/komport-qt6/tests/tst_sessionrecordcodec.cpp:49)–[71] does not exercise this.

   Minimal fix: make fixed profile values codec constants rather than caller-controlled facts; always emit source ID `1` and session reference `"0"`. Validate the supplied configuration before serialization: exact object/member sets, required strings, numeric 32-bit buffering fields, and no additional members. Refuse with an empty result and specific reason on failure.

2. **HIGH — size limits are checked after codec allocations/serialization.**

   [sessionrecordcodec.cpp:119](/home/max/Development/misc/komport-qt6/komport/sessionrecordcodec.cpp:119) serializes the complete header before its 16 MiB check at [120](/home/max/Development/misc/komport-qt6/komport/sessionrecordcodec.cpp:120). Similarly, [157](/home/max/Development/misc/komport-qt6/komport/sessionrecordcodec.cpp:157)–[159] serializes metadata before `recordSizesFit()` at [164](/home/max/Development/misc/komport-qt6/komport/sessionrecordcodec.cpp:164).

   ADR-010 §3 and SPEC-M9 §5.3 require these limits before allocating or serializing. The returned buffer is correctly empty on refusal, but an oversized JSON object can already have caused the forbidden allocation.

   Minimal fix: introduce a bounded JSON preflight/serializer which accounts for escaped UTF-8 bytes before growing an output buffer, then only allocate/assemble after the header/body limit is known to fit.

3. **MEDIUM — TEST GAP: the seven named tests exist, but do not independently prove all they claim.**

   - `recordPrefixIsFortyFourBytesInTheAgreedOrder` does independently pin the literal 44 at [203](/home/max/Development/misc/komport-qt6/tests/tst_sessionrecordcodec.cpp:203), uses literal offset 44 at [228](/home/max/Development/misc/komport-qt6/tests/tst_sessionrecordcodec.cpp:228), and would catch a dropped or reordered field through its cursor walk. That part is meaningful.
   - However, it and the header test decode using `qFromLittleEndian`, the counterpart of the implementation’s `qToLittleEndian`; see [116](/home/max/Development/misc/komport-qt6/tests/tst_sessionrecordcodec.cpp:116)–[119] and [208](/home/max/Development/misc/komport-qt6/tests/tst_sessionrecordcodec.cpp:208)–[226]. On the usual little-endian host, a mistaken native-endian writer can pass. Add a manually specified byte fixture using non-symmetric multi-byte values and assert exact bytes.
   - `headerProfileMatchesAdr006` only proves a valid input stays valid. It does not prove rejection of extra configuration fields, missing/wrong-typed fields, or hostile fixed-profile facts.
   - `sixtyFourBitJsonValuesAreCanonicalDecimalStrings` only exercises `reference.sourceTimestampNs` at [249](/home/max/Development/misc/komport-qt6/tests/tst_sessionrecordcodec.cpp:249)–[263]. It does not prove that a supplied non-zero `referenceSessionTimestampNs` cannot leak into a v1 header.
   - No codec test verifies rejection of a structurally invalid `SessionEvent`, despite the implementation doing so at [150](/home/max/Development/misc/komport-qt6/komport/sessionrecordcodec.cpp:150). It is not one of the seven table names, so no named test is missing, but this required §5.3 behavior remains untested in step 1.
   - `oversizedRecordIsRejected` proves the 64 MiB-body refusal; the u32 cases are only exercised through public helper `recordSizesFit()` in `lengthArithmeticIsChecked`.

4. **ARCHITECTURE QUESTION — `recordSizesFit()` should not be a public contract solely for testing.**

   [sessionrecordcodec.h:121](/home/max/Development/misc/komport-qt6/komport/sessionrecordcodec.h:121) and also `headerObject()` at [144](/home/max/Development/misc/komport-qt6/komport/sessionrecordcodec.h:144) expose implementation/testing seams not specified as public API. The methods are QtCore-only, so they do not violate ADR-009, but test convenience alone is weak justification for expanding the session-facing surface. Prefer private/internal helpers with a narrowly scoped test access mechanism, or explicitly document and accept them as public contracts.

What is correct:

- The start-block and record field widths/order in the implementation match ADR-006; the record prefix is 44 bytes and `recordLength` is correctly `40 + metadata + payload`.
- Flags are zero, empty metadata is encoded as zero length, and payload/serialized metadata are appended unchanged.
- JSON object member order is irrelevant; ADR-006 fixes names and shapes, not textual JSON order.
- The slice is otherwise disciplined: no file I/O, recorder, or application wiring. Adding the codec to `komport_core` and to the QtCore-only compile proof is correct and appropriately scoped.

I did not run CMake, the warning build, the Core-only proof, or ctest because this environment is read-only; the reported 14/14 result remains author-provided evidence. I also could not independently execute endian-sensitive tests on a big-endian platform.