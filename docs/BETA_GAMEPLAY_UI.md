# Двери, проводимость, лодки и меню

Этап от 3 октября 2026 года. Игровая цель — Minecraft Beta 1.7.3;
компоновка меню и языковые материалы — из 1.5.2. Это не заявление о полной
совместимости со всеми механиками Beta.

## Исправления

| Задача | Результат |
| --- | --- |
| Двери | Предметы 324/330 ставят блоки 64/71 двумя половинами. Деревянная дверь переключается ПКМ, железная — сигналом. Проверяются опора, свободное место, направления и соседняя дверь. |
| Столкновения | Горизонтальные импульсы между игроком и мобами, мобами, вагонетками; у лодки также учитывается твёрдая bounding box. Импульс больше не теряется при следующем обновлении управления/AI. |
| Редстоун | Исправлены уведомления после изменения ID/metadata источника. Башня «факел → блок → факел» инвертирует сигнал по этажам; двери реагируют на сигнал через соседний проводящий блок. |
| Повторитель | Пластина и два объёмных факела. Подвижный факел отражает четыре задержки. Состояние 93/94 меняется атомарно с сохранением metadata. |
| Лодки | Предмет 333, модель из оригинальной текстуры, плавучесть по пяти слоям, движение от импульса пассажира, посадка/выход, столкновения, разрушение и дроп 3 досок + 2 палок. Entity NBT `Boat`. |
| Камера | Симуляция и отображение используют один предел pitch ±90°, без разных ограничений и возврата камеры на следующем тике. |
| Источники | Восстанавливается удалённый поверхностный источник над источником воды при двух горизонтальных соседних источниках. |
| Options | Две колонки по скриншоту, Music/Sound, Invert/Sensitivity, FOV/Difficulty, никнейм по центру; переходы к Video, Controls, Language, Multiplayer и Texture Packs. |
| Паки | Папки и ZIP в двух runtime-каталогах, иконки/описание `pack.txt`, список со скроллом, выбор, fallback и обновление текстур открытого мира. |
| Окно | Разрешено изменение размеров и штатное разворачивание; live framebuffer size используется для viewport, камеры и GUI. Контекст сохраняется. |
| Главное меню | Собственный пиксельный ReCraft wordmark, раскладка 1.5.2, версия/ревизия/UTC-дата, указание фанатской пародии и прав Mojang, ссылка GitHub. Фон статичный. |
| Языки | Реестр 1.5.2, 61 исходная запись + English (US), native names, выделение, scrollbar, globe button, выбор и сохранение. Original `.lang` + Unicode bitmap pages. |

В сетевом режиме дополнительно разделены реестры мобов и объектов пакета
`0x17`: Boat=1, Minecart=10/11/12, Arrow=60. Объекты используют соответствующие
модели; лодки/вагонетки доступны для отправки `UseEntity`, а не рисуются как
неизвестные мобы. Это не добавляет завершённую сетевую физику транспорта.

## Legacy состояния и reference

У двери младшие два бита — направление, бит 4 — состояние, бит 8 — верхняя
половина. **В Beta верх повторяет metadata низа с добавлением 8**; современный
формат hinge bits не используется. Альтернативная петля парной двери
кодируется старой комбинацией направления и бита 4.

Repeater metadata: направление в младших двух битах, задержка в следующих
двух. Задержки 2/4/6/8 игровых тиков при 20 Hz. Redstone torch передаёт сильный
сигнал блоку сверху; остальные допустимые стороны получают слабый сигнал,
опора исключается. Сильное питание блока и слабое питание не смешиваются.

Лодка хранит vanilla `id`, `Pos`, `Motion`, `Rotation`, `FallDistance`, `Fire`,
`Air`, `OnGround`; Beta `EntityBoat` не добавляет собственных NBT-полей.
Пассажир не сохраняется как современная цепочка `Passengers`.

Сверены существующие C-модули, материалы third_party, vanilla Beta JAR,
классы ItemDoor/BlockDoor, BlockRedstoneTorch, BlockRedstoneRepeater,
RenderBlocks, Entity, ItemBoat/EntityBoat/ModelBoat/RenderBoat и NetClientHandler
в [Beta source mirror](https://github.com/jacobo-mc/mc_b1.7.3_release/tree/main/1.7.3-LTS/src/minecraft/net/minecraft/src).
LTS mirror может содержать изменения; спорные места нельзя считать vanilla
только по этой копии. Современные blockstates, charging bow и новые типы rails
не вводились. Директории установленных Minecraft использовались только для чтения.

В ветке источников mirror проверял metadata текущего блока внутри ветки,
в которой он уже положительный: это не восстанавливало яму над водой.
ReCraft проверяет нижний источник. Это намеренное исправление данного условия,
а не утверждение о побитовом совпадении с тем исходником.

## Ресурсы и конфигурация

`resource_pack.c` читает каталоги, ZIP central directory и stored/raw-deflate
записи через уже используемый zlib. Проверяются CRC, размеры и относительные
пути. Извлечения на диск нет. Неудачный выбор сохраняет предыдущий пак.

`assets.c` накладывает legacy пути (`terrain.png`, `gui/gui.png`, `font/default.png`)
и совместимые `assets/minecraft/textures/...` поверх bundled assets. Отдельные
блоковые PNG обновляют соответствующие клетки Beta terrain atlas. HD PNG
уменьшаются nearest до legacy canvas. Выбор сбрасывает GPU texture/font cache,
`renderer_reload_assets()` заменяет атлас, сохраняя мир и его meshes.
Собственная процедурная анимация не перезаписывает отдельные water/lava/portal
PNG из пака; полосы анимации пока используют первый кадр. Legacy общий
`terrain.png` сохраняет стандартные процедурные эффекты Beta.

Выбранные `texture_pack`, `language`, `player_name`, FOV, sensitivity и invert
сохраняются в `config/options.txt`. Корень берётся от исполняемого файла, либо
через `--data-dir`. Runtime не зависит от third_party или установленного Minecraft.

Unicode берётся из original glyph atlas + glyph_sizes, без TTF и сглаживания.
Разметка исходных 16x16 glyph cells сохранена; Unicode-шрифт тоньше основного
ASCII-шрифта. Кэш — максимум 16 страниц 256x256 (4 MiB RGBA), nearest filtering.
Языковые файлы доступны через единый resource manager с английским fallback.

## Файлы и сохранность

- Gameplay: `src/world/door.*`, `physics.c`, `world.c`, `fluid.c`, `entities.*`,
  `mobs.*`, `transport.*`, `src/game/player.*`, `entity_render.*`.
- Ресурсы/UI: `src/assets/assets.*`, `resource_pack.*`, `src/ui/language.*`,
  `pixel_font.*`, `ui.*`, `src/game/settings.c`, `src/main.c`.
- Окно/сеть: `src/util/display.*`, `game_paths.*`, `legacy_window_guard.h`,
  `src/network/network.*`.
- Build/docs: CMakeLists, runtime allowlist, version header/template/script,
  GitHub workflow, `.gitignore`, README и связанные документы.
- Проверки: gameplay, transport, settings, network, renderer tests;
  новый `resource_pack_test.c`.
- Добавлены оригинальные boat/globe PNG, `assets/lang/`, Unicode pages.

Существующие source/runtime-файлы не удалялись. Пользовательские снимки
`docs/Screenshots/` исключены из Git. Save directories и reference не очищались.

## Проверки и оставшиеся ограничения

Локальный UCRT64 build и 25/25 CTest прошли, включая software OpenGL 1.1.
Проверены обе двери/4 направления/ПКМ/сигнал/парная петля/reload, редстоуновая
башня, repeater input/output/delay, глубокая вода, предел pitch и pushing.
Лодка: размещение, плавучесть, движение пассажира, enter/exit, pushing,
столкновение с берегом, уничтожение и NBT roundtrip. ZIP: stored/deflate,
CRC, traversal rejection, partial fallback. GL: обновление/освобождение cache,
nearest и сохранность custom water. Config: сохранение выбранного пака/языка.
Windows resize/maximize/context check проходит; проверка resize включена также
в Linux CI smoke. GUI/models/русские подписи сняты и просмотрены запуском из
другой cwd. Пользовательские снимки не входят в эти generated captures.
После повторной сборки проверены SHA-256 всех 102 файлов `build/saves`:
нет изменений, удаления или новых файлов в этих save directories.

Остаются: полная локализация ReCraft-specific/gameplay текстов, Unicode вне
BMP, bidi/shaping, анимированные полосы пака, modern models/blockstates и
sound-pack JSON; ZIP64/encrypted ZIP не поддерживаются. Для лодок ещё нужны
полная логика fire/lava и сетевого пассажира/управления, визуальные splash/hit
эффекты. Естественный spawn, полный AI, точная генерация Beta и остальные
ранее перечисленные большие механики этим этапом не завершены. Snow Leopard
i386 требует отдельной проверки на исходном SDK/железе.
