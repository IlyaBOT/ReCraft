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
runtime worlds. The runner must be online, with Xcode command line tools. If CMake is
missing the workflow installs it using the runner's existing Homebrew.
macOS renderer tests use a native legacy CGL context and Apple's software
renderer, so every GL/pixel assertion runs without a desktop login. The test
also renders the actual `ui_frame` menu using game textures and the bitmap font.
`GL_EXT_framebuffer_object` supplies the test's offscreen color/depth surface;
it is not a runtime renderer requirement and adds no game effects or shaders.
If the runner user has an Aqua session, `tools/run_macos_gui.sh` runs the client
window smoke through `launchctl asuser`. Otherwise the menu capture uses the
same UI in the CGL test drawable. This checks offscreen rendering rather than
pretending to test a visible window. No CTest is disabled or skipped.

The workflow fetches raylib 1.4.0 and GLFW 3.1.2 at verified commits, builds
Release, runs CTest with assertions enabled, checks `--version`, renders the
main menu and uploads a runtime package. Action revisions are pinned too.
Packaging copies the executable and assets selected by
[`assets/runtime_assets.txt`](../assets/runtime_assets.txt): 30 PNG textures,
94 sound effects/variants, 12 music tracks and two Beta records (138 assets). The images include
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

`cmake/legacy_source_compat.cmake` prepares checked build-only copies of five
upstream translation units. GLFW/WGL tries accelerated pixel formats first,
then permits GDI OpenGL 1.1 when none exists (including Windows CI machines).
raylib's window creation is checked before centering, and its fixed-function
path no longer asks GL 1.1 for an unsupported GLSL version enum. Cocoa startup
uses synchronous `NSApplication.finishLaunching`, then sets activation policy;
both are available in the 10.6 SDK. It does not enter the old unbounded nested
`NSApplication run` loop. Dependency sources remain pinned and unchanged.
NSGL includes `NSOpenGLPFAAllowOfflineRenderers` (available in the 10.6 SDK): a
headless Mac may report both its GPU and software renderer as offline even
though usable contexts exist. It first requests an accelerated pixel format. If that fails for
a legacy OpenGL 1.x/2.x context, it retries without the acceleration requirement,
allowing Apple's system software renderer. Modern core requests are not relaxed.
This applies to the GLFW client backend; the renderer test uses its own CGL
drawable. Context errors and actual GL vendor/renderer/version are printed. Tests still
fail if no usable context exists.
The music EOF test stops/drains/refills the OpenAL queue deterministically;
Apple OpenAL ignores the OpenAL Soft null-driver environment and CI output
devices need not advance at wall-clock speed. No tests are disabled by this fix.
Earlier runs 37073222917 and 37079001771 lost runner communication before native
test verification. Local-only world tests use private save fixtures, so their
count is higher than CI's.
The runner subsequently completed run 37080121986: both audio tests passed, but
the renderer could not create its pixel format. Diagnostic run 37113861381
confirmed GLFW error 0x10009, `NSGL: Failed to find a suitable pixel format`.
Native probe run 37114246885 found two renderers, both marked `online=0`.
Default pixel formats failed; allowing offline renderers created a usable
context. An NSWindow drawable still failed (`CGSNewWindow failed with 1000`):
run 37114775471 confirmed no `gui/501` session and console account `root`.
The native CGL test drawable removes that WindowServer requirement from CTest.
Verification on 3 October 2026: [run 37115852726](https://github.com/IlyaBOT/ReCraft/actions/runs/37115852726)
passed all 20 macOS CTests on Vesper, the version check and offscreen menu render,
then packaged and uploaded the macOS client. The native renderer was Apple
Software Renderer, OpenGL 2.1; this is not a hardware/window smoke verification.
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
