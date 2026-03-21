cmake_minimum_required(VERSION 3.16)
project(mimic VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MIMIC_BUILD_TESTS "Build unit tests" ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(XCB REQUIRED xcb)

add_library(mimic_core
    src/config.cpp
    src/config_watcher.cpp
    src/event_loop.cpp
    src/ipc_server.cpp
    src/layout_engine.cpp
    src/logger.cpp
    src/rules_engine.cpp
    src/workspace.cpp
    src/workspace_manager.cpp
    src/monitor_manager.cpp
    src/compositor_bridge.cpp
    src/window_manager.cpp
    src/xcb_backend.cpp
)

target_include_directories(mimic_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${XCB_INCLUDE_DIRS}
)

target_compile_options(mimic_core PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(mimic_core PUBLIC ${XCB_LIBRARIES})

add_executable(mimic src/main.cpp)
target_link_libraries(mimic PRIVATE mimic_core)

add_executable(mimic-msg src/mimic_msg.cpp)
target_link_libraries(mimic-msg PRIVATE mimic_core)

install(TARGETS mimic mimic-msg RUNTIME DESTINATION bin)
install(DIRECTORY config/ DESTINATION share/mimic/config)
install(FILES README.md LICENSE DESTINATION share/doc/mimic)

if(MIMIC_BUILD_TESTS)
    enable_testing()
    add_executable(mimic_tests
        tests/test_layout.cpp
        tests/test_workspace.cpp
        tests/test_config.cpp
    )
    target_link_libraries(mimic_tests PRIVATE mimic_core)
    add_test(NAME mimic_tests COMMAND mimic_tests)
endif()
