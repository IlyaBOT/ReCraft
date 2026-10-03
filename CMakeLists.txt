cmake_minimum_required(VERSION 3.10)
project(ReCraft C)
set(CMAKE_C_STANDARD 99)
file(READ "${CMAKE_SOURCE_DIR}/VERSION" RECRAFT_VERSION)
string(STRIP "${RECRAFT_VERSION}" RECRAFT_VERSION)
if(NOT RECRAFT_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+([.+-][A-Za-z0-9.+-]+)?$")
    message(FATAL_ERROR "VERSION must contain a version such as 0.1.0-dev")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/VERSION")
configure_file(cmake/recraft_version.h.in generated/recraft_version.h @ONLY)
include(cmake/legacy_source_compat.cmake)
include_directories("${CMAKE_BINARY_DIR}/generated")
set(RECRAFT_RUNTIME_DIR "${CMAKE_SOURCE_DIR}/build" CACHE PATH "Persistent game directory")

# Modern host builds. The native Snow Leopard/i386 recipe stays in Makefile.
if(WIN32 AND (NOT MINGW OR NOT CMAKE_C_COMPILER_ID STREQUAL "GNU"))
    message(FATAL_ERROR "The Windows development build requires MSYS2 UCRT64 GCC")
endif()
if(WIN32 AND NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
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

# Build GLFW 3.1.2's platform backend directly. Its old CMake project sets
# CMP0042 to OLD, which CMake 4 rejects. This leaves the pinned source clean.
find_package(Threads REQUIRED)
set(GLFW_SOURCES context.c init.c input.c monitor.c window.c)
if(WIN32)
    list(APPEND GLFW_SOURCES win32_init.c win32_monitor.c win32_time.c win32_tls.c
        win32_window.c winmm_joystick.c wgl_context.c)
    set(GLFW_DEFINITIONS _GLFW_WIN32 _GLFW_WGL _GLFW_USE_OPENGL)
    set(PLATFORM_LIBRARIES opengl32 gdi32 user32 winmm ws2_32)
elseif(APPLE)
    add_definitions(-DGL_SILENCE_DEPRECATION)
    list(APPEND GLFW_SOURCES cocoa_init.m cocoa_monitor.m cocoa_window.m
        iokit_joystick.m mach_time.c posix_tls.c nsgl_context.m)
    set(GLFW_DEFINITIONS _GLFW_COCOA _GLFW_NSGL _GLFW_USE_OPENGL)
    set(PLATFORM_LIBRARIES "-framework Cocoa" "-framework OpenGL" "-framework IOKit"
        "-framework CoreFoundation" "-framework CoreVideo")
else()
    find_package(OpenGL REQUIRED)
    find_package(X11 REQUIRED)
    foreach(_extension Xrandr Xinerama Xcursor Xi Xxf86vm)
        if(NOT X11_${_extension}_LIB)
            message(FATAL_ERROR "Missing X11 ${_extension} development library")
        endif()
    endforeach()
    list(APPEND GLFW_SOURCES x11_init.c x11_monitor.c x11_window.c xkb_unicode.c
        linux_joystick.c posix_time.c posix_tls.c glx_context.c)
    set(GLFW_DEFINITIONS _GLFW_X11 _GLFW_GLX _GLFW_USE_OPENGL
        _GLFW_HAS_XINPUT _GLFW_HAS_XF86VM)
    set(PLATFORM_LIBRARIES ${OPENGL_LIBRARIES} ${X11_LIBRARIES}
        ${X11_Xrandr_LIB} ${X11_Xinerama_LIB} ${X11_Xcursor_LIB} ${X11_Xi_LIB}
        ${X11_Xxf86vm_LIB} ${CMAKE_DL_LIBS})
endif()
foreach(_source ${GLFW_SOURCES})
    set(_compiled_source "${GLFW_ROOT}/src/${_source}")
    if((WIN32 AND _source STREQUAL "wgl_context.c") OR
       (APPLE AND (_source STREQUAL "cocoa_window.m" OR _source STREQUAL "nsgl_context.m")))
        recraft_compat_source("${_compiled_source}" _compiled_source)
    endif()
    list(APPEND _glfw_files "${_compiled_source}")
    if(_source MATCHES "\\.m$")
        set_source_files_properties("${_compiled_source}"
            PROPERTIES LANGUAGE C COMPILE_FLAGS "-x objective-c")
    endif()
endforeach()
add_library(glfw STATIC ${_glfw_files})
target_include_directories(glfw PRIVATE "${GLFW_ROOT}/include" "${GLFW_ROOT}/src")
target_compile_definitions(glfw PRIVATE ${GLFW_DEFINITIONS})
target_include_directories(glfw PRIVATE ${X11_INCLUDE_DIR})
target_link_libraries(glfw PUBLIC ${PLATFORM_LIBRARIES} Threads::Threads)

set(RAYLIB_SOURCES
    core.c rlgl.c glad.c shapes.c text.c textures.c models.c audio.c
    utils.c camera.c gestures.c stb_vorbis.c)
foreach(_source ${RAYLIB_SOURCES})
    if(_source STREQUAL "core.c" OR _source STREQUAL "rlgl.c")
        recraft_compat_source("${RAYLIB_ROOT}/src/${_source}" _compatible)
        list(APPEND _raylib_files "${_compatible}")
    else()
        list(APPEND _raylib_files "${RAYLIB_ROOT}/src/${_source}")
    endif()
endforeach()
add_library(raylib_legacy STATIC ${_raylib_files})
set_source_files_properties("${RAYLIB_ROOT}/src/gestures.c" PROPERTIES COMPILE_DEFINITIONS RECRAFT_NO_WIN_SHIM)
set_source_files_properties("${CMAKE_SOURCE_DIR}/src/network/network.c" PROPERTIES COMPILE_DEFINITIONS RECRAFT_NO_WIN_SHIM)
set_source_files_properties("${CMAKE_SOURCE_DIR}/src/network/server_status.c" PROPERTIES COMPILE_DEFINITIONS RECRAFT_NO_WIN_SHIM)

if(APPLE)
    # raylib 1.4 includes AL/*.h; use its pinned headers with Apple's framework.
    set(OPENAL_INCLUDE_DIR "${RAYLIB_ROOT}/external/openal_soft/include")
    find_library(OPENAL_LIBRARY OpenAL REQUIRED)
else()
    find_path(OPENAL_INCLUDE_DIR NAMES AL/alc.h)
    find_library(OPENAL_LIBRARY NAMES openal)
endif()
find_package(ZLIB REQUIRED)
if(NOT OPENAL_INCLUDE_DIR OR NOT OPENAL_LIBRARY)
    message(FATAL_ERROR "Install x64 OpenAL Soft (for UCRT64: pacman -S mingw-w64-ucrt-x86_64-openal). Do not link raylib's bundled win32 import library in an x64 build.")
endif()
message(STATUS "OpenAL library: ${OPENAL_LIBRARY}")
message(STATUS "zlib library: ${ZLIB_LIBRARIES}")
target_include_directories(raylib_legacy PRIVATE
    "${RAYLIB_ROOT}/src" "${CMAKE_SOURCE_DIR}/src" "${GLFW_ROOT}/include"
    "${OPENAL_INCLUDE_DIR}"
    "${RAYLIB_ROOT}/external/glew/include")
target_compile_definitions(raylib_legacy PRIVATE PLATFORM_DESKTOP GRAPHICS_API_OPENGL_11)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(raylib_legacy PRIVATE -std=gnu99 -fgnu89-inline -include "${CMAKE_SOURCE_DIR}/src/win_gl_compat.h")
endif()

file(GLOB_RECURSE RECRAFT_SOURCES "${CMAKE_SOURCE_DIR}/src/*.c")
add_executable(ReCraft ${RECRAFT_SOURCES})
set_target_properties(ReCraft PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${RECRAFT_RUNTIME_DIR}")
if(APPLE)
    set(APP_NAME ReCraft)
    string(REGEX REPLACE "[-+].*" "" APP_VERSION "${RECRAFT_VERSION}")
    if(CMAKE_OSX_DEPLOYMENT_TARGET)
        set(MIN_MACOS "${CMAKE_OSX_DEPLOYMENT_TARGET}")
    else()
        execute_process(COMMAND sw_vers -productVersion OUTPUT_VARIABLE MIN_MACOS OUTPUT_STRIP_TRAILING_WHITESPACE)
    endif()
    configure_file(assets/Info.plist.in generated/Info.plist @ONLY)
    set_target_properties(ReCraft PROPERTIES MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_BINARY_DIR}/generated/Info.plist")
endif()
target_include_directories(ReCraft PRIVATE "${CMAKE_SOURCE_DIR}/src" "${RAYLIB_ROOT}/src" "${GLFW_ROOT}/include" "${OPENAL_INCLUDE_DIR}" "${ZLIB_INCLUDE_DIRS}")
target_compile_definitions(ReCraft PRIVATE PLATFORM_DESKTOP GRAPHICS_API_OPENGL_11)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(ReCraft PRIVATE -O2 -include "${CMAKE_SOURCE_DIR}/src/win_gl_compat.h")
endif()

# Only the files consumed by the current asset IDs are deployed. Rebuilding
# never removes saves or any other persistent runtime directory.
add_custom_target(deploy_assets ALL)
file(STRINGS "${CMAKE_SOURCE_DIR}/assets/runtime_assets.txt" RECRAFT_ASSETS)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/assets/runtime_assets.txt")
foreach(_asset ${RECRAFT_ASSETS})
    get_filename_component(_directory "${_asset}" DIRECTORY)
    add_custom_command(TARGET deploy_assets POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${RECRAFT_RUNTIME_DIR}/assets/${_directory}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/assets/${_asset}"
            "${RECRAFT_RUNTIME_DIR}/assets/${_asset}" VERBATIM)
endforeach()
add_dependencies(ReCraft deploy_assets)
target_link_libraries(ReCraft raylib_legacy glfw "${OPENAL_LIBRARY}" ${ZLIB_LIBRARIES}
    ${PLATFORM_LIBRARIES} Threads::Threads)
if(NOT WIN32)
    target_link_libraries(ReCraft m)
endif()

# Development convenience. The UCRT64 compiler runtime must also be on PATH.
get_filename_component(_openal_lib_dir "${OPENAL_LIBRARY}" DIRECTORY)
find_file(OPENAL_RUNTIME NAMES libopenal-1.dll OpenAL32.dll
    PATHS "${_openal_lib_dir}/../bin" NO_DEFAULT_PATH)
if(WIN32 AND OPENAL_RUNTIME)
    add_custom_command(TARGET ReCraft POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${OPENAL_RUNTIME}" "$<TARGET_FILE_DIR:ReCraft>")
endif()

enable_testing()
add_executable(beta_session_test tests/beta_session_test.c src/world/beta_session.c)
target_include_directories(beta_session_test PRIVATE src)
add_test(NAME beta_session_test COMMAND beta_session_test)
set_tests_properties(beta_session_test PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
add_executable(music_stream_test tests/music_stream_test.c src/audio/music_stream.c)
target_include_directories(music_stream_test PRIVATE src "${RAYLIB_ROOT}/src" "${OPENAL_INCLUDE_DIR}")
target_link_libraries(music_stream_test raylib_legacy glfw "${OPENAL_LIBRARY}" ${PLATFORM_LIBRARIES} Threads::Threads)
if(NOT WIN32)
    target_link_libraries(music_stream_test m)
endif()
add_test(NAME music_stream_test COMMAND music_stream_test
    "${CMAKE_SOURCE_DIR}/assets/sounds/random/click.ogg" "${CMAKE_SOURCE_DIR}/assets/music/calm1.ogg")
set_tests_properties(music_stream_test PROPERTIES ENVIRONMENT "ALSOFT_DRIVERS=null" SKIP_RETURN_CODE 77)
add_executable(audio_record_test tests/audio_record_test.c src/audio/audio.c src/audio/music_stream.c
    src/audio/sound_policy.c src/assets/assets.c src/assets/server_icon_png.c src/util/game_paths.c
    src/world/world.c src/world/storage.c src/world/beta_blocks.c)
target_include_directories(audio_record_test PRIVATE src "${RAYLIB_ROOT}/src" "${OPENAL_INCLUDE_DIR}")
target_compile_options(audio_record_test PRIVATE -include "${CMAKE_SOURCE_DIR}/src/win_gl_compat.h")
target_link_libraries(audio_record_test raylib_legacy glfw "${OPENAL_LIBRARY}" ${PLATFORM_LIBRARIES} ${ZLIB_LIBRARIES} Threads::Threads m)
add_test(NAME audio_record_test COMMAND audio_record_test "${CMAKE_SOURCE_DIR}")
set_tests_properties(audio_record_test PROPERTIES ENVIRONMENT "ALSOFT_DRIVERS=null" SKIP_RETURN_CODE 77)
add_executable(sound_policy_test tests/sound_policy_test.c src/audio/sound_policy.c)
add_executable(climate_test tests/climate_test.c src/world/climate.c)
target_include_directories(climate_test PRIVATE src)
target_link_libraries(climate_test m)
add_test(NAME climate_test COMMAND climate_test)
target_include_directories(sound_policy_test PRIVATE src)
add_test(NAME sound_policy_test COMMAND sound_policy_test)
add_executable(settings_test tests/settings_test.c src/game/settings.c)
target_include_directories(settings_test PRIVATE src)
add_test(NAME settings_test COMMAND settings_test)
add_executable(gameplay_test tests/gameplay_test.c src/world/world.c src/world/storage.c src/world/beta_blocks.c src/game/player.c)
target_include_directories(gameplay_test PRIVATE src)
target_link_libraries(gameplay_test m)
add_test(NAME gameplay_test COMMAND gameplay_test)
set_tests_properties(gameplay_test PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
add_executable(sign_test tests/sign_test.c src/world/world.c src/world/storage.c src/world/beta_blocks.c)
add_executable(transport_test tests/transport_test.c src/world/world.c src/world/storage.c src/world/beta_blocks.c)
target_include_directories(transport_test PRIVATE src)
target_link_libraries(transport_test m)
add_test(NAME transport_test COMMAND transport_test)
target_include_directories(sign_test PRIVATE src)
target_link_libraries(sign_test m)
add_test(NAME sign_test COMMAND sign_test)
set_tests_properties(sign_test PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
add_executable(mining_test tests/mining_test.c src/game/mining.c src/game/inventory.c)
target_include_directories(mining_test PRIVATE src)
target_link_libraries(mining_test m)
add_test(NAME mining_test COMMAND mining_test)
add_executable(crafting_test tests/crafting_test.c src/game/crafting.c src/game/inventory.c src/world/beta_blocks.c)
target_include_directories(crafting_test PRIVATE src)
add_test(NAME crafting_test COMMAND crafting_test)
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
        src/world/beta_blocks.c src/assets/assets.c src/util/game_paths.c src/renderer/texture_animation.c
        src/game/entity_render.c src/game/creative.c src/ui/pixel_font.c src/assets/server_icon_png.c)
    target_include_directories(renderer_test PRIVATE src "${RAYLIB_ROOT}/src")
    target_compile_definitions(renderer_test PRIVATE RECRAFT_TEST_ASSET_ROOT="${CMAKE_SOURCE_DIR}")
    target_compile_options(renderer_test PRIVATE -include "${CMAKE_SOURCE_DIR}/src/win_gl_compat.h")
    target_include_directories(renderer_test PRIVATE "${GLFW_ROOT}/include")
    target_link_libraries(renderer_test raylib_legacy glfw "${OPENAL_LIBRARY}"
        ${PLATFORM_LIBRARIES} ${ZLIB_LIBRARIES} m)
    add_test(NAME renderer_test COMMAND renderer_test)
    if(WIN32)
        add_test(NAME renderer_gl11_test COMMAND renderer_test --software-gl11)
    endif()
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/network_test.c")
    add_executable(network_test tests/network_test.c src/network/network.c
        src/network/protocols/protocol_beta_14.c src/world/world.c src/world/storage.c src/world/beta_blocks.c)
    target_include_directories(network_test PRIVATE src "${ZLIB_INCLUDE_DIRS}")
    target_compile_definitions(network_test PRIVATE RECRAFT_NO_WIN_SHIM)
    target_link_libraries(network_test ${ZLIB_LIBRARIES} Threads::Threads m)
    if(WIN32)
        target_link_libraries(network_test ws2_32)
    endif()
    add_test(NAME network_test COMMAND network_test)
endif()
add_executable(protocol_47_test tests/protocol_47_test.c src/network/protocols/protocol_1_8_47.c)
target_include_directories(protocol_47_test PRIVATE src "${ZLIB_INCLUDE_DIRS}")
target_link_libraries(protocol_47_test ${ZLIB_LIBRARIES})
add_test(NAME protocol_47_test COMMAND protocol_47_test)

add_executable(server_status_test tests/server_status_test.c src/network/server_status.c
    src/network/status_json.c src/network/protocols/protocol_1_8_47.c
    src/util/server_list.c src/assets/server_icon_png.c)
target_include_directories(server_status_test PRIVATE src "${ZLIB_INCLUDE_DIRS}")
target_compile_definitions(server_status_test PRIVATE RECRAFT_NO_WIN_SHIM)
target_link_libraries(server_status_test ${ZLIB_LIBRARIES} Threads::Threads)
if(WIN32)
    target_link_libraries(server_status_test ws2_32)
endif()
add_test(NAME server_status_test COMMAND server_status_test)

if(EXISTS "${CMAKE_SOURCE_DIR}/tests/beta_discovery_test.c")
    add_executable(beta_discovery_test tests/beta_discovery_test.c
        src/world/beta_discovery.c src/world/beta_level_io.c src/game/inventory.c src/nbt/nbt.c src/util/game_paths.c)
    target_include_directories(beta_discovery_test PRIVATE src "${ZLIB_INCLUDE_DIRS}")
    target_link_libraries(beta_discovery_test ${ZLIB_LIBRARIES} m)
    if(EXISTS "${CMAKE_SOURCE_DIR}/build/saves/New World/level.dat" AND
       EXISTS "${CMAKE_SOURCE_DIR}/build/saves/World1/level.dat")
        add_test(NAME beta_discovery_test COMMAND beta_discovery_test "${CMAKE_SOURCE_DIR}/build/saves")
    endif()
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/build/saves/World1/level.dat")
    add_executable(beta_level_test tests/beta_level_test.c
        src/world/beta_level.c src/world/beta_level_io.c src/world/beta_discovery.c
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

# Headless world tests use the same container persistence/smelting modules as
# the client. None of these sources depend on raylib or a graphics context.
foreach(_world_test world_test renderer_test network_test beta_region_test gameplay_test sign_test transport_test audio_record_test)
    if(TARGET ${_world_test})
        target_sources(${_world_test} PRIVATE src/world/block_entity.c src/world/environment.c src/world/climate.c src/world/redstone.c src/world/beta_session.c src/game/bed.c src/game/sign.c src/game/player.c
            src/world/entities.c src/world/mobs.c src/world/transport.c src/world/rail.c
            src/world/physics.c src/world/ticks.c src/world/fluid.c src/game/mining.c
            src/game/crafting.c src/game/inventory.c src/nbt/nbt.c)
    endif()
endforeach()

# Tests use assert(), including calls with side effects. Keep them active in CI Release.
add_executable(beta_recovery_test tests/beta_recovery_test.c src/world/beta_level.c
    src/world/beta_level_io.c src/world/beta_discovery.c src/game/inventory.c
    src/nbt/nbt.c src/util/game_paths.c)
target_include_directories(beta_recovery_test PRIVATE src "${ZLIB_INCLUDE_DIRS}")
target_link_libraries(beta_recovery_test ${ZLIB_LIBRARIES} m)
add_test(NAME beta_recovery_test COMMAND beta_recovery_test)
foreach(_level_test beta_level_test beta_recovery_test)
    if(TARGET ${_level_test})
        target_sources(${_level_test} PRIVATE src/world/beta_session.c)
    endif()
endforeach()

get_property(_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(_target ${_targets})
    if(_target MATCHES "_test$" AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${_target} PRIVATE -UNDEBUG)
    endif()
endforeach()
