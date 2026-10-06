add_executable(tst_system tst_system.cpp)
target_link_libraries(tst_system PRIVATE wintaskman_system Qt6::Test)
add_test(NAME tst_system COMMAND tst_system)

add_executable(tst_tablemodel tst_tablemodel.cpp ${PROJECT_SOURCE_DIR}/src/ui/tablemodel.cpp)
target_include_directories(tst_tablemodel PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(tst_tablemodel PRIVATE Qt6::Core Qt6::Test)
add_test(NAME tst_tablemodel COMMAND tst_tablemodel)
