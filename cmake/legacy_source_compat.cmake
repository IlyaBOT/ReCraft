# Narrow build-time fixes. Pinned upstream trees remain untouched, and generated
# copies retain upstream copyright notices. Fail rather than silently miss a fix.
function(recraft_replace needle replacement)
    string(FIND "${_compat_text}" "${needle}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "Pinned legacy source no longer matches compatibility patch")
    endif()
    string(REPLACE "${needle}" "${replacement}" _compat_text "${_compat_text}")
    set(_compat_text "${_compat_text}" PARENT_SCOPE)
endfunction()

function(recraft_compat_source source output)
    get_filename_component(name "${source}" NAME)
    file(READ "${source}" _compat_text)
    if(name STREQUAL "wgl_context.c")
        recraft_replace("int i, nativeCount, usableCount;" "int i, nativeCount, usableCount;\n    int allowSoftware = 0;")
        recraft_replace("    usableCount = 0;" "enumerate_formats:\n    usableCount = 0;")
        recraft_replace("if (getPixelFormatAttrib(window, n, WGL_ACCELERATION_ARB) =="
            "if (!allowSoftware && getPixelFormatAttrib(window, n, WGL_ACCELERATION_ARB) ==")
        recraft_replace("if (!(pfd.dwFlags & PFD_GENERIC_ACCELERATED) &&"
            "if (!allowSoftware && !(pfd.dwFlags & PFD_GENERIC_ACCELERATED) &&")
        recraft_replace("    if (!usableCount)"
            "    /* ReCraft: prefer hardware, then permit GDI OpenGL 1.1 if none exists. */\n    if (!usableCount && !allowSoftware)\n    {\n        allowSoftware = 1;\n        goto enumerate_formats;\n    }\n\n    if (!usableCount)")
    elseif(name STREQUAL "core.c")
        recraft_replace("window = glfwCreateWindow(" "window = recraft_glfw_create_window(")
        set(_compat_text "/* ReCraft: checked window creation for raylib 1.4. */\n#include \"util/legacy_window_guard.h\"\n${_compat_text}")
    elseif(name STREQUAL "rlgl.c")
        recraft_replace("    TraceLog(INFO, \"GPU: GLSL:     %s\", glGetString(GL_SHADING_LANGUAGE_VERSION));"
            "#if !defined(GRAPHICS_API_OPENGL_11)\n    TraceLog(INFO, \"GPU: GLSL:     %s\", glGetString(GL_SHADING_LANGUAGE_VERSION));\n#endif")
    endif()
    set(destination "${CMAKE_BINARY_DIR}/generated/legacy/${name}")
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated/legacy")
    # configure_file avoids changing the output timestamp when content is equal.
    set(_compat_template "${CMAKE_BINARY_DIR}/generated/legacy/${name}.in")
    file(WRITE "${_compat_template}" "${_compat_text}")
    configure_file("${_compat_template}" "${destination}" COPYONLY)
    set(${output} "${destination}" PARENT_SCOPE)
endfunction()
