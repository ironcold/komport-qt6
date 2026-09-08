# QTermWidget bridge spike - see docs/architecture-decisions/ADR-001-terminal-engine-strategy.md
#
# Deliberately NOT part of komport_core/komport-qt6: this is throwaway
# architecture-decision code to inform ADR-001, not a shipped feature. Only
# built when -DKOMPORT_BUILD_SPIKES=ON is passed to CMake (see top-level
# CMakeLists.txt), so the normal build/install/test path never needs
# qtermwidget6 installed.
find_package(qtermwidget6 REQUIRED)

add_executable(qtermwidget-bridge-spike main.cpp)
target_link_libraries(qtermwidget-bridge-spike PRIVATE
  Qt6::Widgets
  Qt6::SerialPort
  qtermwidget6
)
