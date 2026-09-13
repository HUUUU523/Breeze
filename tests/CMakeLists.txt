find_package(Qt6 REQUIRED COMPONENTS Core)

qt_add_executable(test_sync test_sync.cpp ../syncmerge.cpp)
set_target_properties(test_sync PROPERTIES AUTOMOC OFF AUTOUIC OFF)
target_include_directories(test_sync PRIVATE ..)
target_link_libraries(test_sync PRIVATE Qt6::Core)

add_test(NAME test_sync COMMAND test_sync)
