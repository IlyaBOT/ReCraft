# Client builds and version

`VERSION` is the single source for the client version (currently `0.1.0-dev`).
CMake and the legacy Makefile generate `recraft_version.h`; do not edit generated
headers. Changing VERSION rebuilds the menu footer and `ReCraft --version`.
The macOS bundle also receives the numeric part in its version fields.

GitHub Actions builds trusted pushes to `main`, `v*` tags and manual runs:

| Job | Host | Build |
| --- | --- | --- |
| Windows | `windows-2022`, MSYS2 UCRT64 | x86-64 GCC, fixed-function OpenGL |
| Linux | `ubuntu-24.04`, X11/Mesa | x86-64 GCC, fixed-function OpenGL |
| macOS | `self-hosted, macOS, X64, Vesper` | x86-64 Apple Clang, fixed-function OpenGL |

Pull requests build on Windows/Linux. Vesper runs trusted pushes/manual runs
only. Checkout there uses `clean: false`; rebuilding does not clean persistent
runtime worlds. The runner must be online, with Xcode command line tools and a
logged-in graphical session for the window/renderer smoke checks. If CMake is
missing the workflow installs it using the runner's existing Homebrew.

The workflow fetches raylib 1.4.0 and GLFW 3.1.2 at verified commits, builds
Release, runs CTest with assertions enabled, checks `--version`, renders the
main menu and uploads a runtime package. Action revisions are pinned too.
Packaging copies only the executable, eight required textures, VERSION, README
and dependency/asset notices. Windows includes transitive non-system DLLs.
Linux/macOS use tar archives to preserve executable permissions and app layout.
User saves, options, server lists and `third_party` references are excluded.
Linux requires system OpenAL, zlib, X11 and OpenGL libraries.

The Vesper artifact targets its installed macOS SDK, **not Snow Leopard**.
Native 10.6/i386 remains the separate [legacy Makefile recipe](SNOW_LEOPARD_BUILD.md),
requiring the original SDK and toolchain; it is not yet tested in CI.

## Local modern Linux/macOS build

Linux prerequisites (Ubuntu):

```sh
sudo apt-get install build-essential cmake ninja-build libgl1-mesa-dev libopenal-dev zlib1g-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libxxf86vm-dev xvfb xauth
```

macOS needs Xcode command line tools and CMake. Apple's OpenAL/OpenGL frameworks
are used with the pinned raylib headers. No new graphics library is introduced.

```sh
cmake -P tools/fetch_dependencies.cmake
cmake -S . -B build/host -DCMAKE_BUILD_TYPE=Release
cmake --build build/host --parallel 2
ctest --test-dir build/host --output-on-failure
```

For headless Linux run CTest with `xvfb-run -a`. Use
`-DRECRAFT_RUNTIME_DIR=/absolute/game/root` to deploy executable/assets into an
isolated game directory. Default output stays `build/`, preserving `build/saves/`.
