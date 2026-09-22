cmake_minimum_required(VERSION 4.4.3)

project(sshf
  LANGUAGES CXX
  VERSION 1.0.0
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS_RELEASE "-Os -DNDEBUG")
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

# --- Fetch FTXUI --------------------------------------------------------------
include(FetchContent)

set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
set(FETCHCONTENT_QUIET OFF)

FetchContent_Declare(ftxui
  GIT_REPOSITORY https://github.com/arthursonzogni/ftxui.git
  GIT_TAG        v5.0.0
  GIT_PROGRESS   TRUE
  GIT_SHALLOW    TRUE
  EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(ftxui)
# ------------------------------------------------------------------------------

add_executable(
    sshf
    src/main.cpp
    src/sshf.cpp
    src/client/cli_tools.cpp
    src/client/net/cli_net.cpp
    src/config/ssh_config.cpp
)

target_include_directories(sshf PRIVATE src)

target_link_libraries(sshf
  PRIVATE ftxui::screen
  PRIVATE ftxui::dom
  PRIVATE ftxui::component
)

if (EMSCRIPTEN)
  string(APPEND CMAKE_CXX_FLAGS " -s USE_PTHREADS")
  string(APPEND CMAKE_EXE_LINKER_FLAGS " -s ASYNCIFY")
  string(APPEND CMAKE_EXE_LINKER_FLAGS " -s PROXY_TO_PTHREAD")

  foreach(file "index.html" "run_webassembly.py")
    configure_file("src/${file}" ${file})
  endforeach(file)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-Os -flto)
    add_link_options(-flto)
endif()

target_link_options(sshf PRIVATE -Wl,--strip-all)

install(TARGETS sshf DESTINATION bin)
install(FILES Makefile DESTINATION .)
install(PROGRAMS install.sh DESTINATION .)
install(PROGRAMS uninstall.sh DESTINATION .)

set(CPACK_PACKAGE_NAME "sshf")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "TUI SSH Manager")

set(CPACK_GENERATOR "TGZ")

include(CPack)
