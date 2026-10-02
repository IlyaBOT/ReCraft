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
Packaging copies the executable and assets selected by
[`assets/runtime_assets.txt`](../assets/runtime_assets.txt): 28 PNG textures,
93 sound effects/variants and 12 music tracks (133 assets). The images include
the preferred Coterie sign texture, the original Beta sign fallback and the
original unknown-server icon. It also includes VERSION, README
and dependency/asset notices. Windows includes transitive non-system DLLs.
Linux/macOS use tar archives to preserve executable permissions and app layout.
User saves, options, server lists and `third_party` references are excluded.
Linux requires system OpenAL, zlib, X11 and OpenGL libraries.

Network CTests use loopback listeners only: the Beta test checks gameplay packet
exchanges, Unicode chat/sign text and tracked-player entries; the status test
checks modern status/pong/favicon queries, malformed replies, timeouts and
bounded PNG decoding. Beta list entries expose TCP reachability only, without
invented MOTD/player counts or roster ping. Protocol 47 is status-only; modern
gameplay and account authentication are not implemented. CI does not contact
public Minecraft servers and these tests do not prove real-server interoperability.
Received server icons, server lists and user data are not bundled in artifacts.

`cmake/legacy_source_compat.cmake` prepares checked build-only copies of four
upstream translation units. GLFW/WGL tries accelerated pixel formats first,
then permits GDI OpenGL 1.1 when none exists (including Windows CI machines).
raylib's window creation is checked before centering, and its fixed-function
path no longer asks GL 1.1 for an unsupported GLSL version enum. Cocoa startup
uses synchronous `NSApplication.finishLaunching`, then sets activation policy;
both are available in the 10.6 SDK. It does not enter the old unbounded nested
`NSApplication run` loop. Dependency sources remain pinned and unchanged.
The music EOF test stops/drains/refills the OpenAL queue deterministically;
Apple OpenAL ignores the OpenAL Soft null-driver environment and CI output
devices need not advance at wall-clock speed. No tests are disabled by this fix.
Run 37073222917 passed Windows/Linux and built macOS, but Vesper lost communication
with GitHub during CTest. macOS test completion remains unverified; this failure
has no published job log and requires restoring the runner's connection.
Run 37079001771 passed Windows (21/21) and Linux (20/20), while Vesper again lost
communication, this time during the build before CTest. No job log was uploaded.
This leaves the native macOS fix unverified; restoring runner connectivity is
required before further test diagnosis. Local-only world tests use private save
fixtures, so their count is higher than CI's.
The macOS renderer test also checks the actual CGL vertex-processing result.
CTest on Vesper has a 60-second per-test timeout and captures a bounded stack
sample after a startup failure. The dependency
checkouts and the native macOS GLFW archive remain untouched.

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
