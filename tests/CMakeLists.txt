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
# M9: the application wiring of the recording (action, indicator, reporting).
# Builds a real KomportApp offscreen and uses a pty, so it links libutil.
komport_add_test(tst_sessionrecordingui)
target_link_libraries(tst_sessionrecordingui PRIVATE util)
# M9: the end-to-end proof over a real pty - byte-exact RX/TX in a recording and
# the chunking property (one record per accepted unit, not per character).
komport_add_test(tst_sessionrecordingendtoend)
target_link_libraries(tst_sessionrecordingendtoend PRIVATE util)
# M10: the `.kpsession` reader (ADR-006 applied, ADR-011 D1-D4). Uses the
# injected byte-source seam and real temp files; no pty, hence no libutil.
komport_add_test(tst_sessionreader)
# M10: the passive replay player (ADR-011 D6-D10, SPEC-M10 5.5/5.6). Uses the
# scripted scheduler and the counting clock, so no wall-clock time and no pty.
komport_add_test(tst_sessionreplayplayer)
# M10 step 5: the document-level offline state (SPEC-M10 5.7/5.9) - ownership,
# the process-wide loading preconditions and the close path. Uses a pty for the
# live/recording preconditions, hence libutil.
komport_add_test(tst_sessionofflinedocument)
target_link_libraries(tst_sessionofflinedocument PRIVATE util)
# M10 step 8: the end-to-end proof over a real pty - M9 recorder, M10 reader and
# player, byte-exact replay, a silent peer and the recovery rule.
komport_add_test(tst_sessionreplayendtoend)
target_link_libraries(tst_sessionreplayendtoend PRIVATE util)
# M10: the reply-free rendering path (SPEC-M10 5.7/5.8) - the live entry points
# still answer, a replayed query is rendered but not answered. Uses a real pty as
# the peer and ITransport::bytesWritten as the observation, so it links libutil.
komport_add_test(tst_replayreply)
target_link_libraries(tst_replayreply PRIVATE util)
# The source audit part of that test reads the two emulation files, so their
# paths are compiled in rather than guessed from the working directory.
# M10 step 4 at the view level: a real KomportApp offscreen drives the replay
# rendering through KomportView::slotReplayReceivedChar() and the key-input
# suppression with a real key event. Uses a pty as the peer, so it links libutil.
komport_add_test(tst_replayreplyui)
target_link_libraries(tst_replayreplyui PRIVATE util)
target_compile_definitions(tst_replayreply PRIVATE
  KOMPORT_EMULATION_SOURCE="${CMAKE_SOURCE_DIR}/komport/komportemulation.cpp"
  KOMPORT_EMULATION_HEADER="${CMAKE_SOURCE_DIR}/komport/komportemulation.h"
  KOMPORT_VIEW_SOURCE="${CMAKE_SOURCE_DIR}/komport/komportview.cpp"
  KOMPORT_APP_SOURCE="${CMAKE_SOURCE_DIR}/komport/komport.cpp"
  KOMPORT_SOURCE_DIR="${CMAKE_SOURCE_DIR}/komport"
  # the compiler-aware part of the offline-unit dependency audit: the compiler that
  # builds the project lists every header the four units really pull in
  KOMPORT_CXX_COMPILER="${CMAKE_CXX_COMPILER}"
  # the compile definitions of the audited units, mirrored from the build's own
  # flags.make (komport_core: the five Qt module guards; the contract check: QT_CORE_LIB).
  # Not read from the targets by generator expression: referencing komport_core's
  # COMPILE_DEFINITIONS here drops the Qt guards from this very test, whose QTEST_MAIN
  # then picks the wrong application class. The list is a documented mirror: if the build
  # ever gains a module guard, update it here (the audit's limit, see SPEC-M10 5.8).
  KOMPORT_APP_DEFINES="QT_CORE_LIB|QT_GUI_LIB|QT_WIDGETS_LIB|QT_PRINTSUPPORT_LIB|QT_SERIALPORT_LIB"
  KOMPORT_OFFLINE_DEFINES="QT_CORE_LIB"
  KOMPORT_CORE_INCLUDE_DIRS="$<JOIN:$<TARGET_PROPERTY:Qt6::Core,INTERFACE_INCLUDE_DIRECTORIES>,|>"
  # the same for the application translation unit, which needs the widget modules
  KOMPORT_APP_INCLUDE_DIRS="$<JOIN:$<TARGET_PROPERTY:Qt6::Widgets,INTERFACE_INCLUDE_DIRECTORIES>,|>|$<JOIN:$<TARGET_PROPERTY:Qt6::Gui,INTERFACE_INCLUDE_DIRECTORIES>,|>|$<JOIN:$<TARGET_PROPERTY:Qt6::PrintSupport,INTERFACE_INCLUDE_DIRECTORIES>,|>|$<JOIN:$<TARGET_PROPERTY:Qt6::SerialPort,INTERFACE_INCLUDE_DIRECTORIES>,|>|$<JOIN:$<TARGET_PROPERTY:Qt6::Core,INTERFACE_INCLUDE_DIRECTORIES>,|>")
# M10 step 6a: the application wiring of the offline state (SPEC-M10 5.7/5.7.1) in
# real offscreen windows - indicator, controls in every window, the offline guard
# inside every entry point. Uses a pty as the peer, so it links libutil.
komport_add_test(tst_sessionreplayui)
target_link_libraries(tst_sessionreplayui PRIVATE util)
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
  ${CMAKE_SOURCE_DIR}/komport/sessionreader.cpp
  ${CMAKE_SOURCE_DIR}/komport/sessionreplayplayer.cpp
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
