# Regression tests, run via `ctest` (or `ctest --output-on-failure`) after
# building. Uses Qt Test (QTest) rather than a bespoke test runner - it's
# already part of any Qt installation that can build this project, and
# QTEST_MAIN gives each test its own QApplication for free, which the
# window-lifetime test in particular needs.
find_package(Qt6 6.2 REQUIRED COMPONENTS Test)

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
komport_add_test(tst_windowlifetime)
komport_add_test(tst_profileerror)
komport_add_test(tst_selection)
