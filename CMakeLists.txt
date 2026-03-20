cmake_minimum_required(VERSION 3.16)
project(Mimic VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(PC_X11 QUIET x11)
  if(NOT PC_X11_FOUND)
    message(FATAL_ERROR "Missing X11 development package (pkg-config module: x11). Install libX11 development headers (e.g. libx11-devel on Fedora, libx11 on Arch/CachyOS).")
  endif()
else()
  message(WARNING "pkg-config was not found; falling back to CMake X11 detection only.")
endif()

find_package(X11 REQUIRED)
if(NOT X11_X11_INCLUDE_PATH)
  message(FATAL_ERROR "X11 headers were not found. Install libX11 development files.")
endif()

add_library(mimic_core
    src/MimicCommandRegistry.cpp
    src/MimicConfig.cpp
    src/MimicWorkspaceModel.cpp
    src/MimicLayoutEngine.cpp
    src/MimicOverviewController.cpp
)

target_include_directories(mimic_core PUBLIC include)
target_link_libraries(mimic_core PUBLIC X11::X11)

add_executable(mimic src/main.cpp)
target_link_libraries(mimic PRIVATE mimic_core)

set(MIMIC_SESSION_EXEC "${CMAKE_INSTALL_FULL_BINDIR}/mimic")
configure_file(sessions/mimic.desktop.in ${CMAKE_CURRENT_BINARY_DIR}/mimic.desktop @ONLY)

install(TARGETS mimic RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/mimic.desktop DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/xsessions)
install(FILES config/mimic.keys DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/mimic/examples)
install(FILES config/mimic.toml DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/mimic/examples)
install(CODE "
  set(_mimic_user_config_dir \"\$ENV{HOME}/.config/mimic\")
  set(_mimic_user_toml \"\${_mimic_user_config_dir}/mimic.toml\")
  if(NOT EXISTS \"\${_mimic_user_toml}\")
    file(MAKE_DIRECTORY \"\${_mimic_user_config_dir}\")
    file(INSTALL DESTINATION \"\${_mimic_user_config_dir}\" TYPE FILE FILES \"${CMAKE_CURRENT_SOURCE_DIR}/config/mimic.toml\")
    message(STATUS \"Installed default user config: \${_mimic_user_toml}\")
  endif()
")

include(CTest)
if(BUILD_TESTING)
  add_executable(mimic_tests tests/test_main.cpp)
  target_link_libraries(mimic_tests PRIVATE mimic_core)
  add_test(NAME mimic_tests COMMAND mimic_tests)
endif()
