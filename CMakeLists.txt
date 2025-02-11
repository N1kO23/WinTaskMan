cmake_minimum_required(VERSION 3.31)
project(Qt6SystemdServices)

set(CMAKE_CXX_STANDARD 17)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

add_executable(Qt6SystemdServices main.cpp)
target_link_libraries(Qt6SystemdServices Qt6::Core Qt6::Widgets)
