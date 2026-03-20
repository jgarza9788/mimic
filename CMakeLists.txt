cmake_minimum_required(VERSION 3.16)
project(Mimic VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(X11 REQUIRED)

add_library(mimic_core
    src/MimicCommandRegistry.cpp
    src/MimicWorkspaceModel.cpp
    src/MimicLayoutEngine.cpp
    src/MimicOverviewController.cpp
)

target_include_directories(mimic_core PUBLIC include)
target_link_libraries(mimic_core PUBLIC X11::X11)

add_executable(mimic src/main.cpp)
target_link_libraries(mimic PRIVATE mimic_core)

install(TARGETS mimic RUNTIME DESTINATION bin)
install(FILES sessions/mimic.desktop DESTINATION share/xsessions)
install(FILES config/mimic.keys DESTINATION share/mimic/examples)

include(CTest)
if(BUILD_TESTING)
  add_executable(mimic_tests tests/test_main.cpp)
  target_link_libraries(mimic_tests PRIVATE mimic_core)
  add_test(NAME mimic_tests COMMAND mimic_tests)
endif()
