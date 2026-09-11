cmake_minimum_required(VERSION 3.16)

# Komport-Qt6: a maintained Qt6/QSerialPort fork of the original KDE3/Qt3
# "Komport" serial terminal (Mike Sharkey, 2003), whose upstream project is
# no longer maintained. Versioning starts fresh at 1.0.0 here rather than
# continuing the inherited 0.4.6 - see CLAUDE.md/TODO.md for the full
# migration history.
project(komport-qt6
  VERSION 1.0.0
  DESCRIPTION "Komport-Qt6 - serial port communicator and terminal emulator"
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Codex review finding (Milestone 6): the declared 6.2 minimum was already
# inaccurate before this commit - qt_standard_project_setup() right below
# was only introduced in Qt 6.3, so the project has never actually built
# against a genuine Qt 6.2. Corrected to reflect the real floor. Unrelated
# to the round's own qt6_add_translation()-vs-qt_add_translations() choice
# below - qt_add_translations() itself has existed since 6.2 too (as a
# CMake technology preview whose interface changed at its 6.7
# stabilization/rename), so the lower-level, stable-since-6.2
# qt6_add_translation()/qt6_add_resources() primitives used below were
# chosen to avoid depending on a pre-6.7 tech-preview API surface, not
# because of any hard version-floor requirement.
find_package(Qt6 6.3 REQUIRED COMPONENTS Widgets PrintSupport SerialPort LinguistTools)

qt_standard_project_setup()

# Everything except main.cpp, as an object library: lets tests/ link against
# the exact same compiled application classes (KomportCellArray,
# KomportEmulation, KomportSerial, KomportApp, ...) without a second,
# separately-compiled copy of the sources, and without main() itself pulling
# in a conflicting entry point.
add_library(komport_core OBJECT
  komport/komport.cpp
  komport/komport.h
  komport/komportdoc.cpp
  komport/komportdoc.h
  komport/komportview.cpp
  komport/komportview.h
  komport/komportcell.cpp
  komport/komportcell.h
  komport/komportcellarray.cpp
  komport/komportcellarray.h
  komport/komportscrollbuffer.cpp
  komport/komportscrollbuffer.h
  komport/komportfilescrollbuffer.cpp
  komport/komportfilescrollbuffer.h
  komport/komportserial.cpp
  komport/komportserial.h
  komport/komportemulation.cpp
  komport/komportemulation.h
  komport/komportcharset.cpp
  komport/komportcharset.h
  komport/komporttransfer.cpp
  komport/komporttransfer.h
  komport/komportupload.cpp
  komport/komportupload.h
  komport/komportdownload.cpp
  komport/komportdownload.h
  komport/komportscript.cpp
  komport/komportscript.h
  komport/settingsdialog.cpp
  komport/settingsdialog.h
  komport/komporthexview.cpp
  komport/komporthexview.h
  komport/komportmacrobar.cpp
  komport/komportmacrobar.h
  komport/komportsessionlogger.cpp
  komport/komportsessionlogger.h
  komport/komportminimap.cpp
  komport/komportminimap.h
  komport/komport.qrc
)

target_include_directories(komport_core PUBLIC komport)

target_compile_definitions(komport_core PUBLIC
  KOMPORT_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(komport_core PUBLIC
  Qt6::Widgets
  Qt6::PrintSupport
  Qt6::SerialPort
)

# Milestone 6: internationalization. Source strings are wrapped in tr()
# throughout (see TODO.md section 0/CLAUDE.md); komport/translations/*.ts
# holds the actual translations, maintained by hand/lupdate, not
# regenerated as part of every build. qt6_add_translation() only compiles
# each .ts into a .qm at build time (lrelease) - -nounfinished drops any
# translation Qt Linguist still marks "unfinished" (a string lupdate added
# but nobody has translated/confirmed yet) from the compiled .qm entirely,
# so KomportApp falls back to the English source text for exactly those
# strings at runtime instead of shipping a blank/wrong translation - see
# main.cpp's QTranslator loading comment for the runtime side of this.
# The resulting .qm files are embedded directly into the binary via the
# Qt Resource System (like komport.qrc's icons already are) rather than
# relying on an install-time search path, so translations work identically
# from the build directory and from an installed prefix.
set(KOMPORT_TS_FILES
  komport/translations/komport_de.ts
)
qt6_add_translation(KOMPORT_QM_FILES ${KOMPORT_TS_FILES} OPTIONS -nounfinished)
qt6_add_resources(komport_core "translations"
  PREFIX "/translations"
  BASE "${CMAKE_CURRENT_BINARY_DIR}"
  FILES ${KOMPORT_QM_FILES}
)

add_executable(komport-qt6 komport/main.cpp)
target_link_libraries(komport-qt6 PRIVATE komport_core)

install(TARGETS komport-qt6 RUNTIME DESTINATION bin)
install(FILES komport/komport.desktop DESTINATION share/applications)
install(FILES komport/lo32-app-komport.png
  DESTINATION share/icons/hicolor/32x32/apps
  RENAME komport-qt6.png
)
install(FILES komport/lo16-app-komport.png
  DESTINATION share/icons/hicolor/16x16/apps
  RENAME komport-qt6.png
)

# Regression tests for crash classes past code reviews already found and
# fixed once (profile/array-size validation, unbounded escape-sequence
# buffers, window lifetime/serial-port teardown, ...) - see TODO.md's
# Codex-review section for the incidents these guard against. BUILD_TESTING
# is defined by include(CTest) below (default ON); pass
# -DBUILD_TESTING=OFF to skip needing Qt6::Test.
include(CTest)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

# Architecture-decision spikes (see TODO.md section 1 / docs/architecture-
# decisions/). Off by default: throwaway code that pulls in dependencies
# (e.g. qtermwidget6) the normal build/install/test path never needs.
option(KOMPORT_BUILD_SPIKES "Build experimental architecture-decision spikes (not part of the shipped app)" OFF)
if(KOMPORT_BUILD_SPIKES)
  add_subdirectory(spike/qtermwidget-bridge)
endif()
