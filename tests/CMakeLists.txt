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
komport_add_test(tst_windowlifetime)
komport_add_test(tst_profileerror)
komport_add_test(tst_selection)
komport_add_test(tst_appearance)
komport_add_test(tst_charset)
komport_add_test(tst_i18n)
