# Benchmark procedure

The executable can create seven deterministic, transient workloads:

| Scene | Workload |
| --- | --- |
| `bench_flat` | Flat opaque terrain |
| `bench_forest` | Generated terrain with fixed dense groves |
| `bench_caves` | An enclosed corridor with torches |
| `bench_chunk_updates` | Flat terrain with one deterministic edit per frame |
| `bench_worstcase_transparency` | Alternating leaves, glass and water |
| `bench_torch` | Close view of floor and wall torch geometry/UVs |
| `bench_stream` | Streams chunks during sampling to expose mesh rebuild cost |

The benchmark preloads the selected chunk radius and builds its initial
meshes before frame sampling, except `bench_stream`, which measures streaming
from an empty cache. It disables the FPS cap and VSync, uses a fixed
seed, and advances the view with a deterministic trajectory. Use identical
window size, distance, mipmap level, scene and graphics settings when comparing
two renderer modes. Record the GPU, driver/OpenGL string, OS, CPU and build
revision alongside the CSV. The first frames can still contain platform
startup effects, so compare repeated runs and report their spread.

For example, from PowerShell after the Windows build:

```powershell
$env:PATH = 'C:\msys64\ucrt64\bin;' + $env:PATH
build\ReCraft.exe --benchmark bench_flat --frames 600 --no-audio --window 960x720 --distance 4 --csv build/flat-vbo.csv
build\ReCraft.exe --benchmark bench_flat --frames 600 --no-audio --window 960x720 --distance 4 --client-arrays --csv build/flat-client.csv
build\ReCraft.exe --benchmark bench_flat --frames 600 --no-audio --window 960x720 --distance 4 --basic-mesh --csv build/flat-basic.csv
build\ReCraft.exe --benchmark bench_stream --frames 600 --no-audio --window 960x720 --distance 8 --csv build/stream.csv
```

`--mipmaps 0..4`, `--smooth-lighting 0|1`, `--fancy-leaves 0|1`,
`--chunk-budget 1..8`, and `--vbo-budget 4|8|16|32` select renderer workloads
explicitly. `--client-arrays` and `--basic-mesh` isolate the VBO and greedy
meshing choices.

Run the same executable and options on the native Mac, using
`build/ReCraft.app/Contents/MacOS/ReCraft` as the command. The CSV columns
are `frame`, frame/update/render/mesh milliseconds, loaded/visible/dirty
chunks, draw calls, submitted vertices and triangles, rendered entity count,
estimated world/CPU mesh/requested VBO/requested texture bytes, VBO state,
uploaded/client-vertex bytes this frame, configured VBO budget, and the
driver-reported VRAM capacity when available, plus stream, terrain, HUD, swap,
and optional GPU-wait phase times. The reported VRAM is not measured usage.
`--profile-gpu` calls `glFinish` and reports the blocking wait; it deliberately
changes frame pacing and is useful only for diagnosing GPU back pressure.
Render time includes platform drawing and buffer swap; it is not a GPU timer.
The printed average
FPS is frames divided by summed frame time. "1% low" is the FPS equivalent of
the mean duration of the slowest 1% of frames, rounded up to one sample.

The `--smoke-test` and `--menu-smoke` modes run for 120 frames by default;
`--frames N` overrides that limit. `--capture file.png` saves the last frame.
`--screen video` (also `main`, `worlds`, `create`, `multiplayer`, `add`,
`direct`, `inventory`, `pause`) opens a menu for a bounded UI smoke run.
`--world save-directory` opens a save directly, and `--fullscreen` starts in
the monitor-sized window. `--no-audio` avoids an
audio-device dependency in automated runs. Captures and CSVs should be kept
under ignored `build/` for local measurements.

All five 120-frame scenes completed on a Windows development machine with
an NVIDIA RTX 3060 on 2026-09-30. Those results are illustrative of test
execution only. No benchmark on a modern Windows GPU proves GMA 950
performance. Native
Snow Leopard and GMA 950 results belong in a separate run record with the
exact driver string and machine configuration.
