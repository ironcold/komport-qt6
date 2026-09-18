## Verification verdict

1. **LOW — DOCUMENTATION / frozen API: partially closed.**  
   The implementation has the public constructor at [itransport.h:61](/home/max/Development/misc/komport-qt6/komport/itransport.h:61), defined with QObject parenting at [itransport.cpp:25](/home/max/Development/misc/komport-qt6/komport/itransport.cpp:25). `KomportSerial` uses it at [komportserial.cpp:101](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:101), and the `ITransport` test double inherits it at [tst_sessioncontroller.cpp:40](/home/max/Development/misc/komport-qt6/tests/tst_sessioncontroller.cpp:40). ADR-003 still omits it at [ADR-003:18](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-003-transport-abstraction-v1.md:18).

   The proposed ADR text may be applied as given:

   ```cpp
   class ITransport : public QObject {
       Q_OBJECT
   public:
       // The contract is a QObject so an implementation can be parented in the
       // owning object's tree; it still owns byte movement only, never a session.
       explicit ITransport(QObject *parent = nullptr);
       virtual ~ITransport() = default;
       virtual bool open() = 0;
   ```

   Keep `virtual ~ITransport() = default;` in ADR-003. The shipped header declares an overriding destructor and defines it out-of-line as defaulted at [itransport.cpp:30](/home/max/Development/misc/komport-qt6/komport/itransport.cpp:30); that implementation-placement detail does not alter the normative virtual-destructor contract.

   Correction to the supplied rationale: `tests/tst_transport.cpp` does **not** use `ITransport::ITransport`; its double derives from `KomportSerial` at [tst_transport.cpp:44](/home/max/Development/misc/komport-qt6/tests/tst_transport.cpp:44). The Core-only probe also inherits the constructor at [session_contract_compile.cpp:49](/home/max/Development/misc/komport-qt6/tests/session_contract_compile.cpp:49).

   Once this normative amendment is applied, the transport interface may stand unchanged as the frozen M9 contract.

2. **LOW — TEST GAP: closed.**  
   The Core-only object target includes all public-surface implementation units—`itransport.cpp`, `transportconfiguration.cpp`, and `sessioncontroller.cpp`—at [tests/CMakeLists.txt:52](/home/max/Development/misc/komport-qt6/tests/CMakeLists.txt:52)-[56](/home/max/Development/misc/komport-qt6/tests/CMakeLists.txt:56), and declares only `Qt6::Core` at [tests/CMakeLists.txt:59](/home/max/Development/misc/komport-qt6/tests/CMakeLists.txt:59). It names the complete header inventory at [tests/CMakeLists.txt:45](/home/max/Development/misc/komport-qt6/tests/CMakeLists.txt:45)-[51](/home/max/Development/misc/komport-qt6/tests/CMakeLists.txt:51).

   `transportconfiguration.cpp` includes only its contract header plus Qt Core headers (`QJsonArray`, `QLatin1String`) at [transportconfiguration.cpp:18](/home/max/Development/misc/komport-qt6/komport/transportconfiguration.cpp:18)-[21](/home/max/Development/misc/komport-qt6/komport/transportconfiguration.cpp:21). Static inspection supports Core-only compilation.

3. **COSMETIC — DOCUMENTATION: closed.**  
   `putChar()` now accurately limits success to API acceptance and explicitly excludes physical delivery at [komportserial.h:155](/home/max/Development/misc/komport-qt6/komport/komportserial.h:155)-[160](/home/max/Development/misc/komport-qt6/komport/komportserial.h:160). `sentChar()` is correctly described as an ordered character of an accepted write at [komportserial.h:193](/home/max/Development/misc/komport-qt6/komport/komportserial.h:193)-[194](/home/max/Development/misc/komport-qt6/komport/komportserial.h:194).

   This matches ADR-003’s acceptance boundary at [ADR-003:39](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-003-transport-abstraction-v1.md:39)-[43](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-003-transport-abstraction-v1.md:43) and [ADR-003:119](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-003-transport-abstraction-v1.md:119)-[123](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-003-transport-abstraction-v1.md:123). The implementation emits `bytesWritten` and `sentChar` only for the accepted prefix at [komportserial.cpp:406](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:406)-[413](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:413). No neighbouring `KomportSerial` header comment still makes the same physical-delivery claim.

The self-review correction is factually correct: [implementation-selfreview.md:29](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-18-M8-implementation-selfreview.md:29) now accurately describes the complete source set and records the prior overstatement.

## New findings

None.

## Not verified

I did not execute configure, build, or `ctest`; the claimed warning-free build and 13/13 result remain recorded evidence only. I also could not verify Qt 6.3 compilation, device-level serial behavior, or platform-specific `QSerialPort` timing.