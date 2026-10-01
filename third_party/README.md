# Pinned source dependencies

This directory can also contain user-provided original Minecraft resources and
reference files. They are development references only. The runtime reads only
the selected normalized files deployed from `assets/` into `build/assets/`.

The game source does not include either dependency. Place these exact tags in
`third_party/` for a native build, or in ignored `.deps/` for the Windows
development build:

| Dependency | Tag | Commit | License |
| --- | --- | --- | --- |
| raylib | `1.4.0` | `75a73d94171051037fcf670852877977d9251520` | zlib |
| GLFW | `3.1.2` | `30306e54705c3adae9fe082c816a3be71963485c` | zlib/libpng |

The raylib 1.4 tag contains GLFW 3.1 headers and prebuilt libraries, including
a **32-bit** Windows OpenAL import library. ReCraft compiles the raylib and
GLFW 3.1.2 source and does not link the prebuilt GLFW or Windows OpenAL files.
The Windows CMake build uses a matching UCRT64 OpenAL Soft package and zlib.
Its GLFW target compiles the pinned Win32 sources directly because GLFW
3.1.2's original CMake script uses a removed OLD policy in CMake 4; the
dependency source need not be patched. On Snow Leopard the Makefile links the
system OpenAL framework and zlib. GLFW 3.2 removed OS X 10.6 support; see the
[GLFW changelog](https://www.glfw.org/changelog).
raylib 1.4 defaults to `GRAPHICS_API_OPENGL_11` in its source Makefile.

To fetch and verify on a machine with current Git/TLS support:

```sh
git clone --depth 1 --branch 1.4.0 https://github.com/raysan5/raylib.git third_party/raylib-1.4.0
git clone --depth 1 --branch 3.1.2 https://github.com/glfw/glfw.git third_party/glfw-3.1.2
git -C third_party/raylib-1.4.0 rev-parse HEAD
git -C third_party/glfw-3.1.2 rev-parse HEAD
```

The final two values must match the table before a target build. Dependencies
are kept outside the ReCraft source tree's tracked files. Preserve their
upstream notices when distributing binaries.

Download instructions and target checks are in
[SNOW_LEOPARD_BUILD.md](../docs/SNOW_LEOPARD_BUILD.md).
# Host CI compatibility patches

Modern host CMake builds apply the small, checked transformations in
`cmake/legacy_source_compat.cmake` to generated copies. They allow the Windows
GDI OpenGL fallback, check raylib window creation, and skip its GLSL version
query for fixed-function GL. Upstream checkouts are not edited. Native
Snow Leopard still uses the Makefile and the legacy GLFW archive below.

