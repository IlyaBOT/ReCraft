# GMA 950 renderer policy

The target is a Mac OS X 10.6.8 i386 machine with Intel GMA 950. Intel's
[GMA 950 brief](https://www.intel.com/content/dam/doc/sales-brief/gma-950-graphics-sales-brief.pdf)
states that vertex shader 3.0 and transform and lighting are supported in
software through its Processor Specific Geometry Pipeline. This is a reason to
keep the submitted vertex count small. It does **not** establish that every
fixed-function OpenGL operation on a particular Mac driver follows exactly the
same path. The native renderer's reported capabilities and measured frame times
must decide that. Apple's
[OpenGL Profiler guide](https://developer.apple.com/library/archive/documentation/GraphicsImaging/Conceptual/OpenGLProfilerUserGuide/Strategies/Strategies.html)
identifies `kCGLCPGPUVertexProcessing` as the path check on OS X. The game
uses fixed-function OpenGL without shaders.

The pipeline is block data -> hidden-face removal -> greedy meshing -> one cached
mesh per chunk column -> opaque/cutout/translucent passes. It uses neither
per-block OpenGL lighting nor GPU occlusion queries. Directional face shades
and block/sky light are baked into vertex colors. Camera distance and frustum
culling remove whole chunks before drawing. At most one draw call per nonempty
layer of a visible chunk is the target; translucent blocks need their own
blended pass. The atlas repeats each 16 px tile in a 4 x 4 cell, so greedy
rectangles currently stop at four blocks per axis to preserve tiled textures
with OpenGL 1.1 UVs. A 16 x 16 solid wall is therefore 16 quads rather than
one, while still reducing its 256 block faces substantially. Further merging
needs a different texture strategy and a benchmark on the target hardware.

The renderer's 16-byte local-coordinate vertex stores short positions and UVs
plus RGBA8 colors. It does not send normals or expand positions to floats on
the CPU for each draw. Its 1024 x 1024 RGBA atlas occupies 4 MiB at level zero;
the full generated mip chain is 5,592,404 bytes. The added slots hold Beta
wool colors, spruce/birch log sides, spruce leaves, planks, selected
ore/stone/Nether cube textures, plant cutouts, the four Beta slab materials, containers, cactus, torches and fluids.
Slot zero carries the stone slab top because air has no rendered faces. Half
slab faces and exposed cube faces share the cached opaque mesh. Static mip levels are made once at
atlas creation. CPU animation uploads only visible fluid/portal cells and levels
0-4; no animated cells are uploaded when none are visible. Source pixels come from selected CoterieCraft Beta terrain
tiles when `build/assets/textures/terrain.png` is present; procedural pixels
remain a missing-asset fallback. The default is mipmaps off for the pixel look, with levels
0-4 selectable where the current OpenGL context supports the required texture
level control. The UI disables mipmap levels on a plain GL 1.1 context without
that control. Geometry LOD is absent.

Plant cutouts use double-sided crossed quads in the cached chunk mesh. They
share the terrain atlas and cutout pass with foliage, so they add vertices but
no per-plant draw calls or texture switches.
Normal torches add five narrow quads per block to that cached cutout layer;
wall orientation comes from metadata without another draw call or texture.

The GUI uses a 256 px widgets atlas, a 128 px bitmap font atlas, a 16 px
repeating dirt texture and one static 512 px panorama. All are nearest
filtered and drawn with fixed-function textured quads; there is no shader,
FBO blur or post-process pass. VSync is the default at monitor refresh. Manual
FPS limits stop at the detected refresh rate and use a sleep rather than a
busy loop. Simulation stays at 20 Hz; the rendered camera interpolates between
simulation states. The debug FPS number is an average and cannot by itself
describe frame pacing.

VBO Auto uses the extension only when the extension and functions are present
and a storage budget permits it; it falls back to client arrays. The default
budget is 16 MiB and can be set to 4, 8, 16 or 32 MiB. It is independent of
driver-reported VRAM/GART. VBO On still requires actual capability. Whether a
VBO wins over client arrays on GMA 950 is an empirical question because the driver controls
placement and synchronization. The rendering path never assumes VBO means
dedicated GPU memory. When the VBO extension or entry points are absent, the
UI disables the VBO mode and budget controls.

Fast graphics uses cutout leaves; Fancy can use blended leaves. Smooth
vertex lighting is an independent option, OFF by default. Clouds, particles, entity shadows, and other incomplete
visual options are disabled in the UI. Water is drawn in one translucent pass.
The default render distance is deliberately short; 4-6 columns around the
camera is the first range to test on GMA 950. Chunk rebuilds are limited by a
per-frame count and a roughly 3 ms main-thread budget. There is no worker
meshing thread yet. If one is added for dual-core systems, it must only
produce CPU mesh data; GL uploads remain on the render thread.
Local fluid and falling-block updates defer block-light recomputation until
the end of each physics batch, so spreading water does not rescan an entire
chunk for every placed fluid cell.

## Memory and driver values

Intel's [DVMT description](https://www.intel.com/content/www/us/en/support/articles/000005472/graphics.html)
says integrated graphics can use dynamically allocated system memory, with
limits depending on the machine, operating system and driver. A reported
"VRAM" capacity on this chipset is not a reliable fixed pool of dedicated
memory or proof that a given VBO resides there. Apple's
[CGL VRAM note](https://developer.apple.com/library/archive/qa/qa1168/_index.html)
describes queries for the renderer's reported video memory; newer
`kCGLRPVideoMemoryMegabytes` is available only from OS X 10.7, so the 10.6
build must use older available properties and validate their results.

F3 and CSV can precisely account for **ReCraft-owned bytes**: world arrays,
CPU mesh buffers, requested VBO bytes and atlas upload bytes. These are
estimates of resources requested by the process, not residency measurements.
Any CGL VRAM or texture-memory number is labeled as a driver report, not
remaining/used VRAM. GART size and use are shown as unavailable; Apple's
[Driver Monitor parameter list](https://developer.apple.com/library/archive/documentation/GraphicsImaging/Conceptual/OpenGLDriverMonitorUserGuide/Glossary/Glossary.html)
describes GART counters in a separate diagnostics tool, not in the CGL
video-memory query. GPU utilization and
CPU-wait-for-GPU percentages also remain unavailable; ordinary frame timing
cannot derive them. Apple's
[OpenGL Driver Monitor guide](https://developer.apple.com/library/archive/documentation/GraphicsImaging/Conceptual/OpenGLDriverMonitorUserGuide/Using/UsingOGLDM.html)
describes separate driver counters for target-machine investigation.

The supplied [optimization notes](OPTIMIZATION_NOTES.md) include example
"VRAM 27.4 / 64 MiB" and "GART 26.1 / 256 MiB" overlay lines. They are
illustrative values, not observations or hard budgets for this project.
No GMA 950 FPS figure is asserted before a native run.

## Windows frame-pacing check, 2026-10-01

On the development RTX 3060 (NVIDIA 581.57), a 960 x 720, distance-8,
240-frame `bench_stream` run without VSync averaged 284.45 FPS with a 149.60
FPS 1% low. A separate 240-frame normal smoke run with VSync averaged 7.44 ms
per frame; its 99th-percentile duration was 15.45 ms, with 14 frames over
12 ms and none over 20 ms. The slow frames included 3-4 ms chunk rebuilds
and long buffer swaps, while terrain drawing was generally below 0.5 ms.
This explains why a 144 FPS counter can coexist with visible judder: a missed
refresh can double the presentation interval, and the old 20 Hz camera
movement added another source of uneven motion. Camera interpolation is now
active. Incremental chunk meshing and target-machine profiling remain open.
These Windows figures do not predict Snow Leopard/GMA 950 performance.

## Optional pause blur and smooth lighting

Both settings default OFF. Smooth lighting is baked during mesh creation, then
interpolated by GL_SMOOTH. Uniformly lit surfaces retain greedy merging;
nonuniform faces retain their interior vertices. Its extra geometry should be
measured on the target GMA hardware before enabling it by default.

Pause blur uses a maximum 512 x 512 RGB POT texture (0.75 MiB requested,
driver storage may be larger), four fixed-function fullscreen samples, no FBO,
shader, CPU readback or dynamic lights. Singleplayer reuses a static snapshot;
multiplayer redraws the background every frame. Clear snapshots are bounded to
2,097,152 texels; larger windows or allocation failure fall back to rendering
the paused world. Live multiplayer without blur needs no snapshot texture.

The 2026-10-01 pause smoke CSVs at 960 x 720 show terrain/update/stream time
zero after settling, with and without blur; the ~6.9 ms frame interval is mainly
VSync. Exclude the final screenshot frame, whose PNG export is synchronous.
A subsequent 240-frame distance-8 streaming run measured 232.829 FPS average,
126.411 FPS 1% low, with mean mesh time ~3.73 ms. These runs are not a controlled
before/after comparison; earlier timing above remains historical. Mesh work
continues to dominate this workload. No GMA performance result is claimed.

## Gameplay additions

Fluid shores use four shared corner heights and exposed side quads. Flat
interiors retain greedy merging. Narrow torches, cactus and container faces
stay in cached chunk meshes. Remote player models are limited to 64 visible
entities per frame; local item drawing is bounded by the 128-item render list.
Neither path uses shaders. The due-time block queue holds 4096 cells with hash
deduplication and a per-tick update budget; supported sand is not continuously
scheduled. Cactus random sampling is 80 positions per loaded chunk/tick.
All GPU memory numbers remain requests, not verified physical VRAM residency.

On the RTX 3060 Windows host, 300-frame uncapped runs on 2026-10-02 (first 100
frames excluded) measured median CPU frame intervals of 0.311 ms for `bench_flat`
and 0.323 ms for `bench_worstcase_transparency`; p95 was 0.454/0.398 ms.
Their median terrain draw counts were 23/41 with 6,480/46,420 submitted vertices.
These are CPU submission timings without a GPU completion fence, not sustained
displayed FPS or a GMA result. They also exclude simulation and first-person
rendering. Use normal play and `--profile-gpu` for those remaining workloads.
