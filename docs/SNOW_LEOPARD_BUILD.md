# Snow Leopard i386 build

Target: Mac OS X 10.6.8, Xcode 3.2.6, Apple GCC 4.2, OpenGL 1.1 compatibility
context. This recipe uses no C++/Rust/Node/Python runtime and builds the game
with GNU make. **A native Snow Leopard build and GMA 950 run are still required
for validation.**

Install CMake 2.8.12 or another version that runs on 10.6 for the GLFW build.
GLFW 3.1.2 declares CMake 2.8.12 as its minimum. From the project root:

```sh
git clone --depth 1 --branch 1.4.0 https://github.com/raysan5/raylib.git third_party/raylib-1.4.0
git clone --depth 1 --branch 3.1.2 https://github.com/glfw/glfw.git third_party/glfw-3.1.2
git -C third_party/raylib-1.4.0 rev-parse HEAD
git -C third_party/glfw-3.1.2 rev-parse HEAD
```

Check the commits against [third_party/README.md](../third_party/README.md).
Build GLFW as a static i386 library:

```sh
mkdir third_party/glfw-3.1.2/build-legacy
cd third_party/glfw-3.1.2/build-legacy
cmake .. -DCMAKE_OSX_ARCHITECTURES=i386 -DCMAKE_OSX_DEPLOYMENT_TARGET=10.6 -DGLFW_BUILD_EXAMPLES=OFF -DGLFW_BUILD_TESTS=OFF -DGLFW_BUILD_DOCS=OFF
make
cd ../../..
make legacy
```

The project Makefile compiles raylib 1.4.0 and ReCraft from source with
`-DGRAPHICS_API_OPENGL_11 -arch i386 -mmacosx-version-min=10.6`, then creates
`build/ReCraft.app` plus the eight runtime assets in `build/assets/`. It links
the system OpenAL framework, zlib and
pthread. Change the visible name and bundle name with
`make legacy APP_NAME=YourName`.

The client version comes from the root `VERSION` file. Make generates a C99
header and the bundle version fields from it; changing VERSION rebuilds the
client. `build/ReCraft.app/Contents/MacOS/ReCraft --version` works without a
graphics window. The modern macOS build on Vesper is a separate x86-64 CI
artifact and does not establish 10.6/i386 compatibility; see [CI.md](CI.md).

Inspect the result on the target Mac:

```sh
file build/ReCraft.app/Contents/MacOS/ReCraft
otool -L build/ReCraft.app/Contents/MacOS/ReCraft
```

`file` must report i386. `otool -L` should refer only to frameworks/libraries
present on 10.6.8. Launch the app on the actual GMA 950 machine and inspect
the startup GPU capabilities log, game rendering, input, and F3 timings.
The build flags alone cannot establish runtime compatibility or performance.
Run `--smoke-test --frames 120 --no-audio` and one scene from
[BENCHMARK.md](BENCHMARK.md) on the target before claiming support. Compare
VBO Auto with `--client-arrays` and verify the reported CGL vertex path,
OpenGL extensions, GL errors, frame times and image output. The GMA 950
pipeline and memory accounting limits are in [PERFORMANCE.md](PERFORMANCE.md).
Do not treat reported VRAM or GART values as fixed available budgets.

The GLFW [compatibility guide](https://www.glfw.org/docs/3.1/compat.html) says
OS X 10.6 has no core profile and supports at most an OpenGL 2.1 context.
ReCraft requests the older fixed-function path. GLFW [3.2 removed 10.6
support](https://www.glfw.org/changelog).

Runtime directories, including `build/saves`, are beside the app bundle and
survive `make legacy` and `make clean`. The executable locates them through
`_NSGetExecutablePath` and `realpath`, so launching it from another working
directory does not change resource lookup. `mods/` is reserved and is not
created. The target Mac must still verify the native build and GUI atlas.
