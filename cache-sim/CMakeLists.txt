cmake_minimum_required(VERSION 3.16)
project(CacheSim VERSION 1.0 DESCRIPTION "FIFO vs LRU cache simulator" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # for editors and clangd

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Debug, Release, RelWithDebInfo or MinSizeRel" FORCE)
endif()

option(CACHESIM_BUILD_GUI "Build the Allegro 5 GUI (cachesim_gui)" ON)

function(cachesim_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endfunction()

# ---------------------------------------------------------------------------
# Core library: everything under src/core. It has no graphics code, so the
# command-line runner and the tests build without Allegro.
# ---------------------------------------------------------------------------
add_library(cachesim_core STATIC
  src/core/Cache.cpp
  src/core/CacheConfig.cpp
  src/core/Format.cpp
  src/core/MainMemory.cpp
  src/core/MemRequest.cpp
  src/core/Options.cpp
  src/core/PolicyFactory.cpp
  src/core/ReplacementPolicy.cpp
  src/core/Simulator.cpp
  src/core/TraceGenerator.cpp
  src/core/TraceReader.cpp
)
target_include_directories(cachesim_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
cachesim_warnings(cachesim_core)

# ---------------------------------------------------------------------------
# Command-line runner: prints the hit-rate table.
# ---------------------------------------------------------------------------
add_executable(cachesim_cli src/cli/main.cpp)
target_link_libraries(cachesim_cli PRIVATE cachesim_core)
cachesim_warnings(cachesim_cli)

# ---------------------------------------------------------------------------
# Tests: one file per area (tests/test_<area>.cpp), each registered with ctest.
# To add an area, add its name to this list.
# ---------------------------------------------------------------------------
enable_testing()
set(CACHESIM_TEST_AREAS
  format request config memory policies cache trace_reader trace_generator simulator options)
set(test_sources tests/test_main.cpp)
foreach(area IN LISTS CACHESIM_TEST_AREAS)
  list(APPEND test_sources tests/test_${area}.cpp)
endforeach()
add_executable(cachesim_tests ${test_sources})
target_link_libraries(cachesim_tests PRIVATE cachesim_core)
target_compile_definitions(cachesim_tests PRIVATE CACHESIM_TRACE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/traces")
cachesim_warnings(cachesim_tests)
foreach(area IN LISTS CACHESIM_TEST_AREAS)
  add_test(NAME ${area} COMMAND cachesim_tests ${area})
endforeach()

# ---------------------------------------------------------------------------
# GUI: needs Allegro 5 with the font, TTF, primitives and image addons.
# ---------------------------------------------------------------------------
if(CACHESIM_BUILD_GUI)
  set(allegro_libraries "")

  # pkg-config first: it works for Homebrew, Ubuntu's apt package and MSYS2.
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(ALLEGRO QUIET IMPORTED_TARGET allegro-5 allegro_main-5 allegro_font-5
                      allegro_ttf-5 allegro_primitives-5 allegro_image-5)
    if(ALLEGRO_FOUND)
      set(allegro_libraries PkgConfig::ALLEGRO)
    endif()
  endif()

  # Otherwise Allegro's own CMake package (vcpkg, builds from source), if it has every
  # library we need.
  if(NOT allegro_libraries)
    find_package(Allegro CONFIG QUIET)
    set(wanted_targets Allegro::allegro Allegro::allegro_main Allegro::allegro_font
                       Allegro::allegro_ttf Allegro::allegro_primitives Allegro::allegro_image)
    set(have_all_targets ${Allegro_FOUND})
    foreach(target IN LISTS wanted_targets)
      if(NOT TARGET ${target})
        set(have_all_targets FALSE)
      endif()
    endforeach()
    if(have_all_targets)
      set(allegro_libraries ${wanted_targets})
      # Some builds (Homebrew's 5.2.11, for one) list include folders that no longer exist,
      # which CMake rejects. Keep only the folders that are really there.
      foreach(target IN LISTS allegro_libraries)
        get_target_property(dirs ${target} INTERFACE_INCLUDE_DIRECTORIES)
        if(dirs)
          set(existing_dirs "")
          foreach(dir IN LISTS dirs)
            if(EXISTS "${dir}")
              list(APPEND existing_dirs "${dir}")
            endif()
          endforeach()
          set_target_properties(${target} PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${existing_dirs}")
        endif()
      endforeach()
    endif()
  endif()

  if(allegro_libraries)
    add_executable(cachesim_gui src/gui/main.cpp src/gui/App.cpp src/gui/Input.cpp src/gui/Renderer.cpp)
    target_link_libraries(cachesim_gui PRIVATE cachesim_core ${allegro_libraries})
    target_compile_definitions(cachesim_gui PRIVATE CACHESIM_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
    cachesim_warnings(cachesim_gui)
  else()
    message(WARNING
      "Allegro 5 was not found, so cachesim_gui will not be built. The core library, cachesim_cli "
      "and cachesim_tests do not need it. To build the GUI, install Allegro (macOS: brew install "
      "allegro pkgconf; Ubuntu: sudo apt install liballegro5-dev pkg-config) and configure again, "
      "or pass -DCACHESIM_BUILD_GUI=OFF to silence this warning.")
  endif()
endif()
