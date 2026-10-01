cmake_minimum_required(VERSION 3.10)
project(ReCraft C)
set(CMAKE_C_STANDARD 99)

# Convenience build for Windows development. Snow Leopard uses the Makefile.
if(NOT WIN32)
    message(FATAL_ERROR "Use 'make legacy' on Mac OS X 10.6")
endif()
if(NOT MINGW OR NOT CMAKE_C_COMPILER_ID STREQUAL "GNU")
    message(FATAL_ERROR "The Windows development build requires MSYS2 UCRT64 GCC")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "The Windows development build requires the x86-64 UCRT64 toolchain")
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/.deps/raylib-1.4.0/src/raylib.h")
    set(_raylib_default "${CMAKE_SOURCE_DIR}/.deps/raylib-1.4.0")
else()
    set(_raylib_default "${CMAKE_SOURCE_DIR}/third_party/raylib-1.4.0")
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/.deps/glfw-3.1.2/src/internal.h")
    set(_glfw_default "${CMAKE_SOURCE_DIR}/.deps/glfw-3.1.2")
else()
    set(_glfw_default "${CMAKE_SOURCE_DIR}/third_party/glfw-3.1.2")
endif()
set(RAYLIB_ROOT "${_raylib_default}" CACHE PATH "raylib 1.4.0 source directory")
set(GLFW_ROOT "${_glfw_default}" CACHE PATH "GLFW 3.1.2 source directory")
if(NOT EXISTS "${RAYLIB_ROOT}/src/raylib.h" OR NOT EXISTS "${GLFW_ROOT}/src/internal.h")
    message(FATAL_ERROR "Fetch the pinned sources described in third_party/README.md")
endif()

# Build GLFW 3.1.2's Win32 backend directly. Its old CMake project sets
# CMP0042 to OLD, which CMake 4 rejects. This leaves the pinned source clean.
set(GLFW_WIN32_SOURCES
    context.c init.c input.c monitor.c window.c
    win32_init.c win32_monitor.c win32_time.c win32_tls.c
    win32_window.c winmm_joystick.c wgl_context.c)
foreach(_source ${GLFW_WIN32_SOURCES})
    list(APPEND _glfw_files "${GLFW_ROOT}/src/${_source}")
endforeach()
add_library(glfw STATIC ${_glfw_files})
target_include_directories(glfw PRIVATE "${GLFW_ROOT}/include" "${GLFW_ROOT}/src")
target_compile_definitions(glfw PRIVATE _GLFW_WIN32 _GLFW_WGL _GLFW_USE_OPENGL)

set(RAYLIB_SOURCES
    core.c rlgl.c glad.c shapes.c text.c textures.c models.c audio.c
    utils.c camera.c gestures.c stb_vorbis.c)
foreach(_source ${RAYLIB_SOURCES})
    list(APPEND _raylib_files "${RAYLIB_ROOT}/src/${_source}")
endforeach()
add_library(raylib_legacy STATIC ${_raylib_files})
set_source_files_properties("${RAYLIB_ROOT}/src/gestures.c" PROPERTIES COMPILE_DEFINITIONS RECRAFT_NO_WIN_SHIM)
set_source_files_properties("${CMAKE_SOURCE_DIR}/src/network/network.c" PROPERTIES COMPILE_DEFINITIONS RECRAFT_NO_WIN_SHIM)

find_path(OPENAL_INCLUDE_DIR NAMES AL/alc.h)
find_library(OPENAL_LIBRARY NAMES openal)
find_package(ZLIB REQUIRED)
if(NOT OPENAL_INCLUDE_DIR OR NOT OPENAL_LIBRARY)
    message(FATAL_ERROR "Install x64 OpenAL Soft (for UCRT64: pacman -S mingw-w64-ucrt-x86_64-openal). Do not link raylib's bundled win32 import library in an x64 build.")
endif()
message(STATUS "OpenAL library: ${OPENAL_LIBRARY}")
message(STATUS "zlib library: ${ZLIB_LIBRARIES}")
target_include_directories(raylib_legacy PRIVATE
    "${RAYLIB_ROOT}/src" "${GLFW_ROOT}/include"
    "${OPENAL_INCLUDE_DIR}"
    "${RAYLIB_ROOT}/external/glew/include")
target_compile_definitions(raylib_legacy PRIVATE PLATFORM_DESKTOP GRAPHICS_API_OPENGL_11)
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_options(raylib_legacy PRIVATE -std=gnu99 -fgnu89-inline -include "${CMAKE_SOURCE_DIR}/src/win_gl_compat.h")
endif()

file(GLOB_RECURSE RECRAFT_SOURCES "${CMAKE_SOURCE_DIR}/src/*.c")
add_executable(ReCraft ${RECRAFT_SOURCES})
set_target_properties(ReCraft PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build")
target_include_directories(ReCraft PRIVATE "${CMAKE_SOURCE_DIR}/src" "${RAYLIB_ROOT}/src" "${GLFW_ROOT}/include" "${OPENAL_INCLUDE_DIR}" "${ZLIB_INCLUDE_DIRS}")
target_compile_definitions(ReCraft PRIVATE PLATFORM_DESKTOP GRAPHICS_API_OPENGL_11)
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_options(ReCraft PRIVATE -O2 -include "${CMAKE_SOURCE_DIR}/src/win_gl_compat.h")
endif()

# Only the files consumed by the current asset IDs are deployed. Rebuilding
# never removes saves or any other persistent runtime directory.
add_custom_target(deploy_assets ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/build/assets/gui"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/build/assets/fonts"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/build/assets/textures"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/assets/gui/widgets.png" "${CMAKE_SOURCE_DIR}/build/assets/gui/widgets.png"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/assets/gui/background.png" "${CMAKE_SOURCE_DIR}/build/assets/gui/background.png"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/assets/gui/icons.png" "${CMAKE_SOURCE_DIR}/build/assets/gui/icons.png"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/assets/gui/inventory.png" "${CMAKE_SOURCE_DIR}/build/assets/gui/inventory.png"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/assets/gui/items.png" "${CMAKE_SOURCE_DIR}/build/assets/gui/items.png"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/assets/gui/panorama.png" "${CMAKE_SOURCE_DIR}/build/assets/gui/panorama.png"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/assets/textures/terrain.png" "${CMAKE_SOURCE_DIR}/build/assets/textures/terrain.png"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/assets/fonts/ascii.png" "${CMAKE_SOURCE_DIR}/build/assets/fonts/ascii.png"
    VERBATIM)
target_link_libraries(ReCraft raylib_legacy glfw "${OPENAL_LIBRARY}" ${ZLIB_LIBRARIES}
    opengl32 gdi32 user32 winmm ws2_32)

# Development convenience. The UCRT64 compiler runtime must also be on PATH.
get_filename_component(_openal_lib_dir "${OPENAL_LIBRARY}" DIRECTORY)
find_file(OPENAL_RUNTIME NAMES libopenal-1.dll OpenAL32.dll
    PATHS "${_openal_lib_dir}/../bin" NO_DEFAULT_PATH)
if(OPENAL_RUNTIME)
    add_custom_command(TARGET ReCraft POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${OPENAL_RUNTIME}" "$<TARGET_FILE_DIR:ReCraft>")
endif()

enable_testing()
add_executable(inventory_test tests/inventory_test.c src/game/inventory.c src/game/creative.c)
target_include_directories(inventory_test PRIVATE src)
add_test(NAME inventory_test COMMAND inventory_test)
add_executable(beta_blocks_test tests/beta_blocks_test.c src/world/beta_blocks.c)
target_include_directories(beta_blocks_test PRIVATE src)
add_test(NAME beta_blocks_test COMMAND beta_blocks_test)
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/world_test.c")
    add_executable(world_test tests/world_test.c src/world/world.c src/world/storage.c
        src/world/beta_blocks.c src/game/player.c)
    target_include_directories(world_test PRIVATE src)
    target_link_libraries(world_test m)
    add_test(NAME world_test COMMAND world_test)
    set_tests_properties(world_test PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/nbt_test.c")
    add_executable(nbt_test tests/nbt_test.c src/nbt/nbt.c)
    target_include_directories(nbt_test PRIVATE src)
    add_test(NAME nbt_test COMMAND nbt_test)
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/renderer_test.c")
    add_executable(renderer_test tests/renderer_test.c src/world/world.c src/world/storage.c
        src/world/beta_blocks.c src/assets/assets.c src/util/game_paths.c)
    target_include_directories(renderer_test PRIVATE src "${RAYLIB_ROOT}/src")
    target_compile_options(renderer_test PRIVATE -include "${CMAKE_SOURCE_DIR}/src/win_gl_compat.h")
    target_link_libraries(renderer_test raylib_legacy glfw "${OPENAL_LIBRARY}"
        opengl32 gdi32 user32 winmm m)
    add_test(NAME renderer_test COMMAND renderer_test)
    add_test(NAME renderer_gl11_test COMMAND renderer_test --software-gl11)
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/network_test.c")
    add_executable(network_test tests/network_test.c src/network/network.c
        src/network/protocols/protocol_beta_14.c src/world/world.c src/world/storage.c)
    target_include_directories(network_test PRIVATE src "${ZLIB_INCLUDE_DIRS}")
    target_compile_definitions(network_test PRIVATE RECRAFT_NO_WIN_SHIM)
    target_link_libraries(network_test ${ZLIB_LIBRARIES} ws2_32 m)
    add_test(NAME network_test COMMAND network_test)
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/beta_discovery_test.c")
    add_executable(beta_discovery_test tests/beta_discovery_test.c
        src/world/beta_discovery.c src/game/inventory.c src/nbt/nbt.c src/util/game_paths.c)
    target_include_directories(beta_discovery_test PRIVATE src "${ZLIB_INCLUDE_DIRS}")
    target_link_libraries(beta_discovery_test ${ZLIB_LIBRARIES} m)
    if(EXISTS "${CMAKE_SOURCE_DIR}/build/saves/New World/level.dat" AND
       EXISTS "${CMAKE_SOURCE_DIR}/build/saves/World1/level.dat")
        add_test(NAME beta_discovery_test COMMAND beta_discovery_test "${CMAKE_SOURCE_DIR}/build/saves")
    endif()
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/build/saves/World1/level.dat")
    add_executable(beta_level_test tests/beta_level_test.c
        src/world/beta_level.c src/world/beta_discovery.c
        src/game/inventory.c src/nbt/nbt.c src/util/game_paths.c)
    target_include_directories(beta_level_test PRIVATE src "${ZLIB_INCLUDE_DIRS}")
    target_link_libraries(beta_level_test ${ZLIB_LIBRARIES} m)
    add_test(NAME beta_level_test COMMAND beta_level_test
        "${CMAKE_SOURCE_DIR}/build/saves/World1")
    set_tests_properties(beta_level_test PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/build/saves/New World/region/r.0.0.mcr" AND
   EXISTS "${CMAKE_SOURCE_DIR}/build/saves/World1/region/r.-1.0.mcr")
    add_executable(beta_region_test tests/beta_region_test.c
        src/world/beta_region.c src/world/world.c src/world/storage.c
        src/world/beta_blocks.c src/nbt/nbt.c)
    target_include_directories(beta_region_test PRIVATE src "${ZLIB_INCLUDE_DIRS}")
    target_link_libraries(beta_region_test ${ZLIB_LIBRARIES} m)
    add_test(NAME beta_region_test COMMAND beta_region_test
        "${CMAKE_SOURCE_DIR}/build/saves/New World"
        "${CMAKE_SOURCE_DIR}/build/saves/World1")
    set_tests_properties(beta_region_test PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()
