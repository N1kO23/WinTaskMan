cmake_minimum_required(VERSION 3.16)
project(WinTaskMan LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Concurrent Widgets Charts OPTIONAL_COMPONENTS Test)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra)
endif()

# Reads the system's state from /proc, the service manager and the display server.
# Needs only Qt Core, so the unit tests can link it without the UI.
add_library(wintaskman_system STATIC
  src/system/applications.cpp src/system/applications.h
  src/system/procfs.cpp src/system/procfs.h
  src/system/runtool.cpp src/system/runtool.h
  src/system/services.cpp src/system/services.h
  src/system/trackers.cpp src/system/trackers.h
)
target_include_directories(wintaskman_system PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(wintaskman_system PUBLIC Qt6::Core)

add_executable(WinTaskMan
  src/main.cpp
  src/ui/applicationspage.cpp src/ui/applicationspage.h
  src/ui/backgroundjob.h
  src/ui/mainwindow.cpp src/ui/mainwindow.h
  src/ui/performancepage.cpp src/ui/performancepage.h
  src/ui/processespage.cpp src/ui/processespage.h
  src/ui/refreshablepage.h
  src/ui/rundialog.cpp src/ui/rundialog.h
  src/ui/servicespage.cpp src/ui/servicespage.h
  src/ui/tablemodel.cpp src/ui/tablemodel.h
  src/ui/tableview.cpp src/ui/tableview.h
)
target_link_libraries(WinTaskMan PRIVATE wintaskman_system Qt6::Concurrent Qt6::Widgets Qt6::Charts)
qt_add_resources(WinTaskMan icons
  PREFIX /
  BASE src/assets
  FILES src/assets/icons/taskmgr.ico
)

option(BUILD_TESTING "Build the unit tests (needs Qt Test)" ON)
if(BUILD_TESTING AND TARGET Qt6::Test)
  enable_testing()
  add_subdirectory(tests)
endif()
