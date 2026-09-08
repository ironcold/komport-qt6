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

find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets PrintSupport SerialPort)

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
