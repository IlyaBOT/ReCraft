cmake_minimum_required(VERSION 3.10)
find_package(Git REQUIRED)
get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

function(fetch name url tag revision)
    set(destination "${ROOT}/.deps/${name}")
    if(NOT EXISTS "${destination}")
        execute_process(COMMAND "${GIT_EXECUTABLE}" clone --quiet --depth 1 --branch "${tag}"
            "${url}" "${destination}" RESULT_VARIABLE result)
        if(NOT result EQUAL 0)
            message(FATAL_ERROR "Could not fetch ${name}")
        endif()
    endif()
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${destination}" rev-parse HEAD
        OUTPUT_VARIABLE head OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE result)
    if(NOT result EQUAL 0 OR NOT head STREQUAL revision)
        message(FATAL_ERROR "${name} must be at ${revision}; existing directory was left untouched")
    endif()
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${destination}" diff --quiet HEAD
        RESULT_VARIABLE modified)
    if(NOT modified EQUAL 0)
        message(FATAL_ERROR "${name} has local changes; existing directory was left untouched")
    endif()
    message(STATUS "${name}: ${head}")
endfunction()

fetch(raylib-1.4.0 https://github.com/raysan5/raylib.git 1.4.0
    75a73d94171051037fcf670852877977d9251520)
fetch(glfw-3.1.2 https://github.com/glfw/glfw.git 3.1.2
    30306e54705c3adae9fe082c816a3be71963485c)
