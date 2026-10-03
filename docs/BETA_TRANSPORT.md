# Beta 1.7.3: исправления, предметы и транспорт

Проверка: 3 октября 2026 года. Это отчёт о локальных механиках, сохранении и
известных отличиях; он не заявляет полную совместимость всей игры с Beta.

## Таблички и рычаг

**Причина бага Done:** после закрытия редактора удерживаемый левый клик
попадал в игровой ввод следующего кадра. Creative mining удалял табличку
либо блок за ней. ESC не создавал такого мышиного события.
Оба действия вызывают один `finish_sign_edit`: обновляются только строки
существующей Sign TileEntity либо отправляется серверный packet 0x82.
После возврата cursor capture игровой ввод ждёт отпускания обеих кнопок.
Placement/destruction повторно не вызываются. Standing Sign сохраняет
metadata 0..15, Wall Sign — 2/3/4/5, NBT — Text1..Text4 и неизвестные теги.

Рычаг — **ID 69**, младшие три бита задают ориентацию, **бит 8 — ON**:

| Metadata без ON | Опора / установка |
| --- | --- |
| 1 | опора −X, клик по её стороне +X |
| 2 | опора +X, клик по её стороне −X |
| 3 | опора −Z, клик по её стороне +Z |
| 4 | опора +Z, клик по её стороне −Z |
| 5 | пол, ручка вдоль Z |
| 6 | пол, ручка вдоль X |

Beta выбирает floor 5/6 случайно; используется Java Random мира. Потолочного
крепления нет. ID и metadata устанавливаются атомарно **до neighbor update**:
прежний промежуточный meta 0 заставлял настенный рычаг потерять ложную опору.
Основание использует cobblestone tile 16, ручка — tile 96, U=7..8.99,
V=6..15.99, отдельные торцы и наклон ±40°. Модель входит в cached chunk mesh
(12 quads), без отдельного draw call. Selection box зависит от ориентации;
body collision отсутствует. RMB переключает 8; weak power вокруг,
strong power опорному блоку. Удаление опоры даёт предмет 69.

## First person, сон и nickname

Пустая рука: skin 64×32, UV правой руки 40,16, ModelBiped размер 4×12×4 и
ItemRenderer pivot/transforms. Блоки объёмные; tools/sword/bow/food — sprite
толщиной 1/16, две плоскости и стороны 16 строк/столбцов с alpha test.
Общий Beta sprite pose отличается от block pose; современные charge/eating
позы не добавлены. Удочка имеет оригинальный дополнительный поворот 180°.
Equip/change item опускает и поднимает руку со скоростью .4/тик, смена
картинки происходит внизу. Swing использует sin/sqrt кривые. Bobbing/hurt
подключены к существующему лёгкому пути. GL matrices, texture binding,
depth range и атрибуты восстанавливаются.
Карта с двумя руками и точное directional item lighting остаются TODO;
неподдержанные формы блоков сохраняют ограничения общего block renderer.

`player_eye` вычисляет голову от **head half** и bed metadata 0..3:
центр + .4 вдоль кровати, высота bedY+1.0375, yaw зависит от направления.
Standing eye offset во сне не применяется, рука скрыта. Переход сна/пробуждения
.2 секунды — визуальное расширение ReCraft, не новый world state.

Имя: `UiOptions.player_name[17]`, `config/options.txt`, ключ `player_name`.
Options редактирует отдельный draft; invalid input не меняет активное имя.
Разрешены 1..16 ASCII букв/цифр/`_`, default `Player`. Player.name ссылается
на настройку; network_connect получает её же. Сохранение при обычном выходе.
UUID, authentication и приватные account NBT tags не добавлены.

## Jukebox

Block **84**, metadata **0** пустой / **1** содержит диск.
Vanilla TileEntity **RecordPlayer**, поле **Record: Int** — Item ID.

| Item ID | Диск | Runtime asset |
| --- | --- | --- |
| 2256 | 13 | assets/records/13.ogg |
| 2257 | cat | assets/records/cat.ogg |

RMB вставляет, следующий RMB извлекает в ItemEntity; разрушение тоже извлекает
содержимое. Survival расходует предмет, Creative сохраняет стек. Стороны
используют tile 74, верх — 75. Пути задаёт asset manager.
Пластинка выбрасывается с исходными случайными смещениями Beta: X/Z=.15+.7×random,
Y=.66+.7×random относительно блока, pickup delay — 10 тиков.
Один global positional streaming source, volume=.5×sound volume, радиус 64.
Новый диск заменяет старый, eject останавливает этот поток даже после запуска
другого jukebox, как SoundManager Beta. Начало пластинки прерывает background
music; её idle countdown приостанавливается. Четыре малых OpenAL buffers,
без декодирования полного OGG в RAM. После reload диск остаётся внутри,
но не запускается автоматически: оригинал играет при действии вставки.

## Лук и Arrow entity

Лук **261**, стрела **262**: мгновенный выстрел без charge и износа лука.
Нужна стрела среди 36 слотов; Survival расходует одну, Creative сохраняет стек.
Старт у глаза, боковой offset .16 и вертикальный −.1, yaw/pitch,
скорость **1.5 блока/тик**, Gaussian spread .0075, звук random.bow.

Arrow — SavedEntity с 20 Hz simulation, не hitscan. Проверяется весь отрезок
полёта против block box и expanded entity/cart boxes, ближайшее попадание
наносит **4 HP**. Drag .99 (вода .8), gravity .03 блока/тик².
В блоке сохраняются координаты/ID/metadata, shake=7; смена блока освобождает
стрелу. После **1200 ground ticks** despawn. Player-owned стрела после shake
подбирается, если есть место; skeleton-owned не подбирается.
Оригинальная texture 32×32, interpolation позиции, ориентация по velocity.

NBT: id=Arrow; Pos/Motion/Rotation; xTile/yTile/zTile: Short;
inTile/inData/shake/inGround/player: Byte, общие vanilla entity fields.
Motion на диске — блоки/тик, внутри движка — блоки/секунда. Pitch стрелы
сохраняет знак EntityArrow; yaw учитывает соглашение камеры. Ground/air
counters оригинал не сохраняет. Bubble particles/полное knockback остаются
в очереди effects/combat. Skeleton AI теперь создаёт настоящие стрелы с Beta
скоростью 0.6/tick, inaccuracy 12 и cooldown 30; см. [дополнение](BETA_EVENTS_AND_PROFILE.md).

## Rails и Minecart

| Block ID | Тип | Metadata |
| --- | --- | --- |
| 66 | обычные рельсы | 0..9, с углами |
| 27 | powered rail | 0..5 + бит 8 питания |
| 28 | detector rail | 0..5 + бит 8 обнаружения |

0=N/S, 1=E/W, 2=подъём E, 3=W, 4=N, 5=S;
6=угол S/E, 7=S/W, 8=N/W, 9=N/E. Activator Rail отсутствует.
rail.c хранит исходную MATRIX endpoints, ищет соседей на той же высоте/±1,
проверяет доступность двух соединений и пересчитывает topology при изменениях.
Перекрёсток выбирает угол по Beta приоритету с учётом redstone. Нужны опоры
снизу и у высокого конца уклона; отсутствие опоры даёт rail item.
Диагональные height neighbors тоже обновляются. Powered chain: дальность
8 соседних рельс, detector: пересечение cart box, recheck 20 тиков,
weak/strong power. Mesh — double-sided cutout plane со slope, без collider.

| Item ID | Minecart Type | Действие |
| --- | --- | --- |
| 328 | 0 | посадка RMB, выход Shift/RMB |
| 342 | 1 | контейнер 27 слотов |
| 343 | 2 | уголь добавляет Fuel=1200, PushX/PushZ от игрока |

Позиция проецируется на MATRIX segment, угол и slope; свободного steering нет.
Beta параметры: max movement .4 блока/тик, slope acceleration .0078125,
drag .96 без пассажира/.997 с ним, powered acceleration .06, торможение
без питания и wall start. Furnace: .8 friction/.04 push, random 1/4 fuel tick.
Вне рельс gravity/drag/block/entity collisions. Тонкие полы проверяются
подшагами с уточнением остановки. Игрок/камера следуют cart; внутри модели
виден сундук/печь. Destroy даёт 328 и для Type 1/2 дополнительно 54/61;
cargo тоже выпадает. Dead entity сразу исключается из сохранения.

NBT: id=Minecart, Type: Int, общие Pos/Motion/Rotation;
Type 1 — Items (Slot/id/Count/Damage), Type 2 — PushX/PushZ: Double, Fuel: Short.
Vanilla Beta не сохраняет mount link; после reload игрок не прикреплён
к runtime ID, приватный ReCraft tag не добавлен.

**Отличия:** NPC не садятся автоматически; общая AABB physics/resolution
не даёт bit-for-bit Java collision trajectories. Нет furnace smoke particles
и оригинальной damage rocking animation. Topology использует существующую
ограниченную scheduled очередь (обычно следующий 20 Hz тик), тогда как часть
Beta neighbor logic синхронная. Power bit по цепи обновляется в одном тике
с лимитом recursion 8. Незагруженные/отсутствующие Beta chunks не считаются
симулируемым воздухом. Точная генерация пропущенных chunks — отдельная задача.

## Файлы и проверки

- Новые src/world/rail.{c,h}, transport.{c,h}; интеграция entities.{c,h},
  world.{c,h}, physics.c, redstone.c, beta_blocks.c, block_entity.{c,h}, mobs.{c,h}.
- Renderer: src/game/entity_render.{c,h}, src/renderer/renderer.c.
- Player/UI: src/game/bed.{c,h}, player.{c,h}, container.h, modal_input.h,
  settings.{c,h}, src/ui/ui.{c,h}, src/config.h, src/main.c.
- Audio/assets: src/audio/audio.{c,h}, sound_assets.def,
  src/assets/assets.{c,h}, assets/runtime_assets.txt и пять новых assets.
- Tests: transport_test.c, audio_record_test.c, settings_test.c, sign_test.c,
  renderer_test.c, beta_region_test.c и CMakeLists.txt.
- CI fixes: cmake/legacy_source_compat.cmake, tests/music_stream_test.c.
  README, CI/assets/mechanics/matrix docs обновлены.

**Windows UCRT64: 24/24 CTest.** Проверены rail 10/6/6 shapes, lever floor/wall
и ON/OFF/support, power reach/off, detector; carts straight/corner/slope,
boarding/fuel/chest/drops/thin-floor; arrow consumption/block/mob hit/release,
pickup/full inventory/despawn/chunk transfer; NBT и четыре bed head positions.
Renderer тестирует lever UV/ON/OFF, skin/sprites/equip/swing и GL restoration
на native GL и software GL 1.1. Audio test — оба OGG, global source, gain,
position, replacement/eject/music suspension; EOF не зависит от device timing.
Настоящий .mcr тест пишет только clone: lever/sign/RecordPlayer/rail/chest cart/
Arrow читаются обратно. 102 исходных save-файла сохранены.
Снимки options/mechanics/hand-{empty,tool,sword,bow,food}/bed/bed-{west,north,east}
запущены из другой cwd, exit=0. Native Snow Leopard/i386/GMA 950 не проверены.

Vesper: старый unbounded NSApplication run заменён finishLaunching; music test
управляет queue без wall-clock sleep. Run **37073222917** собрал macOS клиент,
затем runner **lost communication** во время CTest, logs не опубликованы.
Повторный run **37079001771** также потерял связь с Vesper, уже во время сборки;
до CTest он не дошёл, job log недоступен.
Последующая диагностика обнаружила отсутствие GUI-сессии у runner. Renderer test
теперь использует нативный CGL software context и offscreen framebuffer только
для теста; игровой OpenGL 1.x путь не меняется. [Run 37115852726](https://github.com/IlyaBOT/ReCraft/actions/runs/37115852726)
прошёл **20/20 macOS CTest**, проверку версии, offscreen-отрисовку настоящего UI
и упаковку приложения. Оконный запуск требует графического входа и проверяется,
если такая сессия доступна.
Windows CI прошёл 21/21, Linux — 20/20; дополнительные локальные world tests
требуют read-only save fixtures, отсутствующих в CI.

## Reference

Сначала проверены C-код/архитектура, third_party и read-only установленная
Beta инстанция. Оригинальные terrain/player textures уже есть в pack.
Arrow/cart PNG — побайтно из vanilla JAR SHA-1
43db9b498cb67058d2e12d394e6507722e71bb45; discs/drr.ogg — из original resources.
Reference не попадает в runtime.

Beta source classes:
[BlockLever](https://github.com/jacobo-mc/mc_b1.7.3_release/blob/main/1.7.3-LTS/src/minecraft/net/minecraft/src/BlockLever.java),
[ItemRenderer](https://github.com/jacobo-mc/mc_b1.7.3_release/blob/main/1.7.3-LTS/src/minecraft/net/minecraft/src/ItemRenderer.java),
[ItemBow](https://github.com/jacobo-mc/mc_b1.7.3_release/blob/main/1.7.3-LTS/src/minecraft/net/minecraft/src/ItemBow.java),
[EntityArrow](https://github.com/jacobo-mc/mc_b1.7.3_release/blob/main/1.7.3-LTS/src/minecraft/net/minecraft/src/EntityArrow.java),
[RailLogic](https://github.com/jacobo-mc/mc_b1.7.3_release/blob/main/1.7.3-LTS/src/minecraft/net/minecraft/src/RailLogic.java),
[EntityMinecart](https://github.com/jacobo-mc/mc_b1.7.3_release/blob/main/1.7.3-LTS/src/minecraft/net/minecraft/src/EntityMinecart.java),
[TileEntityRecordPlayer](https://github.com/jacobo-mc/mc_b1.7.3_release/blob/main/1.7.3-LTS/src/minecraft/net/minecraft/src/TileEntityRecordPlayer.java),
[SoundManager](https://github.com/jacobo-mc/mc_b1.7.3_release/blob/main/1.7.3-LTS/src/minecraft/net/minecraft/src/SoundManager.java).
Эта LTS mirror может содержать свои изменения; эталон спорных мест — vanilla
JAR. Современные disc lists, charging, activator rails и blockstates не добавлены.
