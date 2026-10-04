# Механизмы, эффекты и аккаунт — 4 октября 2026

## Что изменено

| Область | Реализация |
| --- | --- |
| Creative | 216 уникальных ItemStack. Основа — настоящий `Item.getSubItems` из установленного vanilla 1.5.2 JAR, отфильтрованный до Beta ID/metadata. Нет воды/лавы как блоков, провода 55, служебных состояний повторителя/двери/печи/поршня, locked chest и заполненной карты. Beta деревянная плита остаётся `44:2`. Имена вариантов берутся из `.lang`; пустой слот не имеет подписи |
| Провод | Компонента пыли пересчитывает силы сразу при изменении соседей; при поиске внешнего питания вся пыль исключается как источник. Это устраняет самопитание кольца повторителей после подключения ответвления. Спад 15→0 и подъёмы сохранены |
| Повторитель | Оригинальные Beta tiles 131/147 в комплекте по умолчанию, четыре поворота UV, два факела с задержкой 2/4/6/8 тиков. Нет body collision. Пользовательский пак сохраняет свои текстуры |
| Кнопка и плиты | Каменная кнопка 77 на 20 тиков; каменная плита 70 реагирует на living entities, деревянная 72 также на предметы/транспорт/снаряды. Сигнал 0/1 в metadata, weak/strong power, проверка опоры и звуки |
| Раздатчик | ID 23, `Trap` TileEntity, 9 слотов. Питание самого блока или блока над ним вызывает действие спустя 4 тика. Случайный непустой слот; стрелы, снежки и яйца летят как entities, остальные предметы выбрасываются. Ведро воды выбрасывается предметом, как в Beta. Dropper в этой версии отсутствовал |
| Поршни | ID 29/33, шесть направлений, предел 12 перемещаемых блоков, sticky pull и запреты на obsidian/контейнеры/расширенный поршень. ID 34 — головка, ID 36 — движущийся блок с `Piston` TileEntity. Продвижение на 0.5 за тик; сущности выталкиваются с собственными размерами и проверкой блоков. База при втягивании остаётся на месте |
| Нотный блок | ID 25, `Music` TileEntity, `note` byte 0..24. ПКМ меняет ноту; ЛКМ играет её. Redstone играет по фронту сигнала. Над блоком должен быть воздух. Пять инструментов определяются материалом блока снизу; pitch `2^((note-12)/12)`, громкость 3. В multiplayer инструмент/нота приходят из `0x36`, без повторной локальной симуляции |
| Огонь | Player HUD и слои пламени вокруг горящих существ. Маленькая 16×32 RGBA-текстура, nearest, CPU-анимация по тикам, fixed-function GL 1.1. Серверный metadata bit 0 управляет огнём в multiplayer |
| TNT/песок | PrimedTnt остаётся непрозрачной текстурированной сущностью, мигает белым и увеличивается перед взрывом. Песок/гравий после задержки 2 тика превращаются в FallingSand; движение и отсечение AABB сверены с vanilla 1.5.2. Столбик оседает без потери блоков. См. [проверки рендера и падения](ENTITY_RENDER_REGRESSIONS.md) |
| Меню | Основные кнопки центрированы независимо от панели персонажа; панель сдвинута влево. Минимальный размер содержимого окна 640×480 задаётся нативно для Windows/Cocoa/X11 без замены GL-контекста. Иконки хотбара сдвинуты на 4 логических пикселя влево |
| Профиль | Имя редактируется только в Player Profile; после Microsoft-входа поле заблокировано и серое. Активные skin/cape получаются из Java profile; плащ уже существовал в Beta, его 10×16×1 модель и UV используются без новых shaders |

## Legacy данные и сохранение

- Поршень: metadata `0=down, 1=up, 2=north, 3=south, 4=west, 5=east`,
  bit 8 — расширен; у головки bit 8 означает sticky. `Piston` NBT:
  `blockId`, `blockData`, `facing` int, `progress` float, `extending` byte.
- `Trap.Items`: `Slot` byte, `id`/`Damage` short, `Count` byte, 9 ячеек.
- `Music.note` — byte; предыдущий уровень питания остаётся runtime-состоянием,
  как в оригинале, и не записывается в выдуманный modern block state.
- `PrimedTnt.Fuse` и `FallingSand.Tile` — byte. Для возобновления падения также
  сохраняются vanilla 1.5.2 byte-поля `Data`/`Time`; Beta их игнорирует.
  Beta-записи только с `Tile` продолжают загружаться. Координаты/Motion
  сохраняются обычными Entity lists; признак удаления исходного блока — runtime-only.
- Неизвестные исходные поля TileEntity/Entity NBT сохраняются при переписывании.
  Проверки работают с отдельными временными мирами, а не с `build/saves/`.

## Microsoft и multiplayer

Online challenge protocol 14 запускает асинхронный POST
`sessionserver.mojang.com/session/minecraft/join` с Java access token,
`selectedProfile` UUID и server ID. Login отправляется только после HTTP 204.
Токен не передаётся игровому серверу. При необходимости сессия обновляется.
Offline challenge `-` сохраняет прежнее поведение.

Общая ошибка Minecraft Services заменена на HTTP-код и безопасную диагностику.
Если ответ именно `403 / Invalid app registration`, нужна авторизация Client ID
со стороны Minecraft, а не смена никнейма. Это невозможно исправить локальным
переключением online-mode. Инструкция и ограничения:
[MICROSOFT_ACCOUNT.md](MICROSOFT_ACCOUNT.md).

Для чужих скинов/плащей используется публичный Mojang profile: имя Beta
разрешается в UUID, затем читается property `textures`. URL ограничены
`textures.minecraft.net/texture/<64 hex>`. Один фоновый worker, максимум 64
записи на соединение; PNG не более 64 KiB, skin 64×32/64×64, cape 64×32.
GL upload/освобождение выполняется только в основном потоке. Запрос по UUID
уже предусмотрен, но игровой адаптер protocol 47 ещё не реализован. Другие
клиенты показывают наш официальный плащ через свою обычную profile-систему;
собственный пакет «передачи плаща» не требуется.

## Основные изменённые файлы

- `src/world/redstone.c`, новые `mechanisms.c`/`piston.c`, `block_entity.c`,
  `transport.c`, `physics.c`, `ticks.c`, `world.c`, `entities.c`, `mobs.c`:
  механизмы, физика, legacy NBT и render snapshots.
- `src/renderer/renderer.c`, `src/game/entity_render.c`, `src/assets/assets.c`:
  пыль/повторитель/поршень, огонь, TNT, skin/cape и кеш текстур.
- `src/account/account*.c`, новый `player_textures.c`, `src/network/network.c`:
  диагностика входа, профиль/плащи, session join, metadata огня и удалённые скины.
- `src/ui/ui.c`, `language.c`, `src/game/creative.c`, новый
  `creative_catalogue.def`, `src/util/display.c`, `src/main.c`:
  меню, минимум окна, локализация, Creative и интеграция.
- `CMakeLists.txt`, `VERSION`, `assets/runtime_assets.txt`,
  `src/audio/sound_assets.def`, документация и regression tests.

Файлы пользовательских миров, аккаунтов и `docs/Screenshots/` не включаются
в commit. Оригинальные reference-каталоги не менялись; файлов не удалено.

## Источники

До изменений сопоставлены текущие world/renderer/container/network пути с
Beta `BlockRedstoneWire`, `BlockRedstoneRepeater`, `BlockDispenser`,
`TileEntityDispenser`, `BlockPressurePlate`, `BlockPistonBase`,
`TileEntityPiston`, `TileEntityRendererPiston`, `BlockNote`, `TileEntityNote`,
`EntityFallingSand`, `EntityTNTPrimed`, `RenderTNTPrimed`, `RenderPlayer`,
`Render` и `ItemRenderer`.
Readable reference: [Beta 1.7.3 source](https://github.com/jacobo-mc/mc_b1.7.3_release/tree/main/1.7.3-LTS/src/minecraft/net/minecraft/src).

Creative registry/sub-items проверены рефлексией над установленным
`D:/MultiMC/libraries/com/mojang/minecraft/1.5.2/minecraft-1.5.2-client.jar`,
без изменения инстанции. Итоговая таблица —
[`creative_catalogue.def`](../src/game/creative_catalogue.def); JAR/Java не нужны runtime.
Звуки `note/harp`, `bassattack`, `bd`, `snare`, `hat`, а также piston in/out
скопированы из read-only оригинальных ресурсов. Оригинальные repeater tiles
выделены из vanilla Beta terrain atlas. Только этот небольшой набор внесён
в `runtime_assets.txt`.

## Проверки и границы

- Release Windows UCRT64: 28/28 CTest, включая native NVIDIA GL и software GL 1.1.
- `mechanisms_test`: уровни провода; одинаковые фазы кольца четырёх повторителей
  delay 2 с ответвлением/без него на протяжении 160 тиков; кнопка/плиты;
  dispenser delay и четыре направления; поршни во всех шести направлениях,
  предел 12 и сохранение движения; пять инструментов note block/нотный NBT.
- `transport_test`: падение вместо мгновенной перестановки, приземление,
  FallingSand/PrimedTnt NBT и передача fuse в render snapshot.
- `renderer_test`: winding/границы пыли, четыре UV направления и оба состояния
  повторителя, nearest fire/cape/remote skin, HUD/пламя/TNT flash и GL state.
- `account_test`: service errors, request bodies, UUID session join, cape в
  account schema, публичный texture payload, name/UUID resolution и кеш.
- `network_test`: offline и online loopback с ожиданием session join,
  metadata огня, пять инструментов/ноты `0x36`, контейнеры/чат/таблички/health/respawn.
- Снимки 640×480, online profile, Creative, dispenser, repeaters и effects;
  запуск из `C:/Windows/Temp`; resize/maximize сохраняет GL texture.
- Повторная сборка: SHA-256 всех 1213 текущих файлов `build/saves/` совпал
  со свежим контрольным снимком; исходные миры в тестах не записываются.

Не доказана полная Java parity: порядок сложных redstone callbacks, тонкости
piston collision/short rod, все формы блоков/AI/частицы и note particle остаются
для дополнительных golden/integration проверок. Fire из custom pack использует
первый кадр, как остальные текущие pack animations. Slim/Alex и полный cape
chasing-position animation пока отсутствуют. Microsoft-вход реального аккаунта,
online Beta-сервер, Snow Leopard/i386/GMA 950 требуют проверки на соответствующих
сервисах и компьютере; локальные mocks этого не заменяют.
