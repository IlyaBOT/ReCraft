# Creative inventory, lighting and pause menus

Implemented 2026-10-01, preserving the C99 / OpenGL 1.1 path.

- Creative: 9 x 5 scrollable catalogue, right-hand draggable scrollbar,
  metadata variants, and hotbar with counts. Click a hotbar slot (or 1-9),
  then a catalogue item to replace that slot with a stack. Survival retains
  its separate 36-slot inventory. Granting is disabled for network sessions.
- Item icons use Beta Item registry coordinates, including tools, dyes and
  records. Catalogue visibility is not a claim of implemented item behavior.
- Block light crosses loaded chunk borders, including negative coordinates
  and diagonals. Removing emitters and inserting/removing opaque blocks also
  invalidates affected neighbour meshes. Native chunk arrival synchronizes
  edge light. Imported McRegion light is retained until an actual edit.
- Smooth Lighting: OFF by default; averages four light samples at each face
  corner and uses fixed-function vertex color interpolation. Greedy merging
  remains enabled for uniformly lit surfaces. Fancy does not toggle this option.
- Menu Blur: OFF by default; Esc and its nested options show the world.
  Singleplayer ticks stop and the frame is cached after mesh work settles.
  Multiplayer network, player ticks, streaming and world drawing continue.
  Blur uses a reduced viewport, GPU texture copy and four bilinear samples;
  font and buttons remain sharp. No shaders, FBO or CPU framebuffer readback.

## Checks

`ctest --test-dir build/windows-ucrt --output-on-failure` covers ten test
executables/configurations. New assertions cover catalogue bounds/variants,
Creative versus Survival granting, item sprite coordinates, border light
addition/removal, loading a new neighbour, occlusion, shared smooth-light
vertex colors, and cached versus live background refresh. The GPU tests run
on both the NVIDIA context and Microsoft GDI Generic OpenGL 1.1.

Example visual checks (disposable in-memory world):

```powershell
build\ReCraft.exe --smoke-test --screen inventory --no-audio --frames 30 --capture build/creative.png
build\ReCraft.exe --smoke-test --screen pause --menu-blur 0 --no-audio --frames 30 --capture build/pause-clear.png
build\ReCraft.exe --smoke-test --screen pause --menu-blur 1 --no-audio --frames 30 --capture build/pause-blur.png
build\ReCraft.exe --smoke-test --screen video --no-audio --frames 30 --capture build/pause-video.png
```

The executable was also launched with `F:\` as working directory. Native
Snow Leopard/GMA tests and a live server menu session are still needed; the
automated live-background test verifies renderer refresh, not server interoperability.

During development, the original McRegion read test exposed a save-on-close
side effect: a light recomputation changed 234 BlockLight bytes in chunk 4,1
of New World's r.0.0.mcr. Blocks, metadata and other NBT were unchanged. The
region was restored byte-for-byte from the pre-existing test-runtime copy
(SHA-256 `5F7F8CFE69F82A0BAB6862140942366DF294F6927648AC452CCE48727B869BC1`).
The test now reads originals directly without attaching them to a save-capable
chunk cache; write tests still use disposable copies.

## Reference boundary

The Beta Item/ItemDye/Block registries were inspected in the development-only
[Beta source reference](https://github.com/jacobo-mc/mc_b1.7.3_release/tree/main/1.7.3-LTS/src/minecraft/net/minecraft/src).
This is an LTS reference tree and may contain changes; it is not proof of exact
vanilla behavior for every subsystem. The local Beta client was also inspected
read-only. ReCraft's Creative catalogue and menu blur are extensions; do not
describe them as a pixel-identical vanilla Beta 1.7.3 screen.
