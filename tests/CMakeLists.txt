# Regression tests, run via `ctest` (or `ctest --output-on-failure`) after
# building. Uses Qt Test (QTest) rather than a bespoke test runner - it's
# already part of any Qt installation that can build this project, and
# QTEST_MAIN gives each test its own QApplication for free, which the
# window-lifetime test in particular needs.
# Kept consistent with the top-level CMakeLists.txt's own Qt6 version floor
# (6.3 - see its comment on why 6.2 was never actually reachable).
find_package(Qt6 6.3 REQUIRED COMPONENTS Test)

function(komport_add_test name)
  add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} PRIVATE komport_core Qt6::Test)
  add_test(NAME ${name} COMMAND ${name})
  # These tests create real (offscreen) widgets/windows - default to the
  # "offscreen" QPA platform so `ctest` also works without a display (CI,
  # SSH sessions, ...); a developer can still override it in their own shell
  # (e.g. QT_QPA_PLATFORM=xcb ctest), this only sets ctest's own default.
  set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

komport_add_test(tst_cellarray)
komport_add_test(tst_emulation)
komport_add_test(tst_serial)
# openpty() (used by the length-aware putStr() regression test, Milestone 7
# round 3) lives in libutil on glibc - link it only for this one test, no
# other target needs it.
target_link_libraries(tst_serial PRIVATE util)
# M8: local transport contract (observations, activation identity, accepted
# prefix). Uses the same openpty() pair, hence the same libutil link.
komport_add_test(tst_transport)
target_link_libraries(tst_transport PRIVATE util)
# M8: session controller (FIFO delivery, activation filter, ADR-005 mapping).
# No libutil: it uses the deterministic transport double, not a pty.
komport_add_test(tst_sessioncontroller)
# M8: document ownership of the controller against the real transport (pty).
komport_add_test(tst_sessiondocument)
target_link_libraries(tst_sessiondocument PRIVATE util)
# M9: the `.kpsession` v1 byte assembly (header block, record prefix, limits).
# Pure byte-level work, no file I/O and no pty, so no libutil.
komport_add_test(tst_sessionrecordcodec)
# M9: the live recorder (streaming, start-flush, periodic flush, damage paths).
# Uses the scripted sink and the manual flush scheduler, so no disk and no
# wall-clock time - and no pty, hence no libutil.
komport_add_test(tst_sessionrecorder)
komport_add_test(tst_windowlifetime)
komport_add_test(tst_profileerror)
komport_add_test(tst_selection)
komport_add_test(tst_appearance)
komport_add_test(tst_charset)
komport_add_test(tst_i18n)

# M8 (SPEC-M8 section 14, ADR-009): the new public session and transport
# contracts must not depend on widgets. This object target compiles every
# implementation unit of that public surface against Qt6::Core alone - no
# komport_core, no Qt6::Widgets, no Qt6::Test - so a widget or
# executable-specific type creeping into sessionevent.h / itransport.h /
# transportconfiguration.h / sessioncontroller.h breaks the build. The assertion
# is about the units, not only about including the headers.
add_library(komport_session_contract_check OBJECT
  session_contract_compile.cpp
  ${CMAKE_SOURCE_DIR}/komport/itransport.cpp
  ${CMAKE_SOURCE_DIR}/komport/transportconfiguration.cpp
  ${CMAKE_SOURCE_DIR}/komport/sessioncontroller.cpp
  ${CMAKE_SOURCE_DIR}/komport/sessionrecordcodec.cpp
  ${CMAKE_SOURCE_DIR}/komport/sessionrecorder.cpp
)
target_include_directories(komport_session_contract_check PRIVATE ${CMAKE_SOURCE_DIR}/komport)
target_link_libraries(komport_session_contract_check PRIVATE Qt6::Core)

# Behavioural tests for the same value contracts (Qt Core + Test only). The
# value-type implementation unit is compiled in directly instead of linking
# komport_core, so this target stays free of the widget dependency as well.
add_executable(tst_sessioncontract
  tst_sessioncontract.cpp
  ${CMAKE_SOURCE_DIR}/komport/transportconfiguration.cpp
)
target_include_directories(tst_sessioncontract PRIVATE ${CMAKE_SOURCE_DIR}/komport)
target_link_libraries(tst_sessioncontract PRIVATE Qt6::Core Qt6::Test)
add_test(NAME tst_sessioncontract COMMAND tst_sessioncontract)
