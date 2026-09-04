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

add_executable(komport-qt6
  komport/main.cpp
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
  komport/komport.qrc
)

target_include_directories(komport-qt6 PRIVATE komport)

target_compile_definitions(komport-qt6 PRIVATE
  KOMPORT_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(komport-qt6 PRIVATE
  Qt6::Widgets
  Qt6::PrintSupport
  Qt6::SerialPort
)

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
