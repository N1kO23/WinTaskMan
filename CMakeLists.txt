cmake_minimum_required(VERSION 3.31)
project(WinTaskMan)

set(CMAKE_CXX_STANDARD 17)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Charts)

qt_add_resources(APP_RESOURCES resources.qrc)

add_executable(WinTaskMan 
src/main.cpp 
src/fetchfunctions.cpp 
src/helperutils.cpp
)

target_link_libraries(WinTaskMan Qt6::Core Qt6::Widgets Qt6::Charts)
target_sources(WinTaskMan PRIVATE ${APP_RESOURCES})
