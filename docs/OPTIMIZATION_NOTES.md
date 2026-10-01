Что обязательно сделать в ReCraft:
Здесь уже появляются voxel-specific правила.
Greedy meshing становится не просто оптимизацией, а центральной частью renderer
Было бы очень плохо:
1 блок = 6 faces = 24 vertices

Даже после hidden-face removal число вершин быстро вырастет.
Для GMA950 цепочка должна быть:
block array
   ↓
hidden-face elimination
   ↓
greedy meshing
   ↓
chunk static mesh
   ↓
VBO
   ↓
1 draw call

Например плоская стена:
16 × 16 blocks

не должна давать:
256 quads
1024 vertices

если её можно представить:
1 quad
4 vertices

При software/CPU vertex processing разница огромная.
Один terrain atlas
Для Beta-style графики:
terrain atlas:
256×256 RGBA

занимает всего:
256 KiB

Полная mip chain:
~341 KiB

Так что mipmaps на самом деле не страшны для 64 MiB VRAM.
Здесь я бы скорректировал наш прежний подход:
geometry LOD — не нужен;
mipmaps — вполне можно использовать.

Причём mipmap уровни лучше генерировать один раз при загрузке, а не постоянно runtime.
Legacy preset:
Mipmaps: OFF

для классического Minecraft pixel look.
Но настройку:
Mipmaps: 0–4

оставить, потому что на дальних текстурах они потенциально могут даже помочь texture cache и убрать aliasing.
4. Fast/Fancy для ReCraft должны реально менять renderer
Не просто декоративная настройка.
Fast
Leaves:
    GL_ALPHA_TEST
    никаких blended leaves

Clouds:
    OFF или очень простая сетка

Water:
    одна translucent pass

Lighting:
    face-direction + baked block light

Particles:
    жесткий limit

Entity shadows:
    OFF

Mipmaps:
    OFF

Render distance:
    4–6 chunks

Fancy
Можно добавить:
transparent leaves
clouds
smooth per-vertex lighting
больше particles
mipmaps

Но всё остальное всё равно fixed-function.
5. Не делать per-block lighting через OpenGL lights
На GMA950 вообще не нужна конструкция:
glEnable(GL_LIGHTING);
glLight...
glNormal...

для каждого voxel face.
Для Minecraft-style мира намного дешевле сразу запекать интенсивность в vertex color:
top:       100%
north:      80%
south:      80%
east/west:  70%
bottom:     50%

и умножать её на:
sky light 0..15
block light 0..15

Результат передавать:
glColor4ub(...)

Нормали тогда terrain mesh вообще хранить не обязан.
Это уменьшает vertex size.
6. Формат вершины ReCraft
Не:
struct Vertex {
    float x, y, z;
    float nx, ny, nz;
    float u, v;
    float r, g, b, a;
};

Это около 48 bytes/vertex.
Нам нужен условно:
struct VoxelVertex {
    int16_t x;
    int16_t y;
    int16_t z;

    uint16_t u;
    uint16_t v;

    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};

16 bytes.
Можно ещё ужать, но сначала надо benchmark.
На старом железе важнее простой и естественно выровненный формат, чем сэкономить последние два байта ценой дорогой распаковки CPU.
7. Draw calls
Цель для ReCraft:
opaque:
    ~1 draw call / visible chunk

cutout:
    желательно тот же chunk pass

transparent:
    максимум ещё 1 draw/chunk

При render distance 4:
9 × 9 ≈ 81 chunk columns

это очень нормально.
При distance 6:
13 × 13 ≈ 169

тоже ещё адекватно.
Не надо делать mesh отдельным VBO для каждого блока, texture, section и т.д.
8. Вертикальные sections — осторожно
В Beta 1.7.3 высота всего 128.
Я бы не делал по 8 draw calls на column, то есть:
16×16×16 section
× 8

в renderer.
Это легко превратит:
100 chunks

в:
800+ terrain draws

Что GMA950 не понравится.
Лучше логически держать dirty-sections для mesher:
Chunk 16×16×128
├── section dirty flags
└── один consolidated render mesh

То есть section помогает понять, что пересчитывать, но renderer видит преимущественно один chunk mesh.
9. CPU occlusion вместо GPU occlusion queries
Hardware occlusion queries я бы пока вообще не использовал.
Они могут заставлять CPU ждать GPU — ровно то, чего у нас и так много.
Позже можно сделать дешёвый Minecraft-style chunk visibility:
камера
 ↓
visible chunk
 ↓
какие стороны chunk соединены воздухом
 ↓
переходим только в потенциально видимые соседние chunks

То есть precomputed visibility/portal graph на CPU.
В пещерах это может очень сильно уменьшить количество chunk draws без GPU synchronization.
Но это уже второй этап после normal frustum culling.
10. ReCraft profiler
F3 overlay должен быть примерно таким:
ReCraft
60 FPS / 16.6 ms

CPU:
 Tick:       1.8 ms
 Meshing:    2.3 ms
 Render CPU: 4.2 ms

GPU:
 Intel GMA 950
 OpenGL 1.4 APPLE-1.6.36

 GPU Util:       N/A
 CPU Wait GPU:  63.2 %
 GL traffic:     7.8 MiB/s

 VRAM:          27.4 / 64 MiB
 GART:          26.1 / 256 MiB

World:
 Chunks loaded:   121
 Visible:          74
 Rendered:         61
 Dirty:             2

Renderer:
 Draw calls:       74
 Faces:         12,403
 Vertices:      49,612
 VBO uploads:       1
 Upload:          128 KiB
 Texture binds:     2

Вот такой profiler на GMA950 будет намного полезнее выдуманного % GPU.
11. Важная корректива после T7400
Теперь у нас два CPU, поэтому ReCraft можно сделать чуть умнее:
1-core CPU:
    meshing = main-thread incremental budget

2+ cores:
    main thread = game/render/OpenGL
    worker #1 = world generation + chunk meshing

Но никаких OpenGL вызовов из worker thread.
Worker выдаёт только готовый CPU-side mesh:
mesh job
 ↓
vertices/indexes in RAM
 ↓
main thread
 ↓
VBO upload

Один worker для T7400 будет практически идеален.