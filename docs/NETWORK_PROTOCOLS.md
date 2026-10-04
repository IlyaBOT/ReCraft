# Network protocol status

Vehicle/object registry handling and its loopback regressions are described in
[the gameplay and UI update](BETA_GAMEPLAY_UI.md).

The current adapter speaks **Minecraft Beta 1.7.3 protocol 14**. Offline-mode
servers work as before. With a signed-in Java profile, an online challenge starts
an asynchronous session join using the official UUID/token; login waits for
HTTP 204. The game server receives no access/refresh token. Account/application
approval and real-server authentication remain unverified; see
[account setup and limits](MICROSOFT_ACCOUNT.md).

Protocol 47 supports modern **server-list status queries**, including MOTD,
player counts, ping and favicon. Modern gameplay, encrypted login and modern
player-info packets are not implemented. Remote skin/cape lookup accepts Beta
names and future UUIDs through the public Mojang profile API and a bounded
worker/cache. Server entity metadata `0x28` and mob-spawn metadata drive the
burning flag; the fire HUD is not a client-side damage authority.
Note-block packet `0x36` delivers the five Beta instruments and pitch 0..24;
the client plays the server event with the original note sounds.

The transport resolves DNS on a worker thread, connects over nonblocking TCP,
buffers partial packets, and processes at most 128 packets and two zlib chunk
regions per `network_tick()`. It limits a compressed region to 128 KiB and checks
the exact decompressed length before writing blocks. Unknown packet IDs and
malformed lengths end the connection with an error instead of losing framing.
Small action packets use TCP_NODELAY. macOS sockets use SO_NOSIGPIPE, and status
query deadlines use the monotonic `mach_absolute_time()` available on 10.6.

## Teleports, login plugins and commands

The Beta packet order was checked against the installed, read-only b1.7.3
client's `Packet13PlayerLookMove`, `EntityClientPlayerMP` and `NetClientHandler`.
Previously the movement writer reversed feet Y and stance, and the mock server
expected the same mistake. On goldenage.keii.dev this prevented the server from
accepting the teleport acknowledgement and streaming terrain. The corrected
client loaded the room with signs and the public chest as IlyaBOT on 2026-10-04.
The automated test sent no password, chat command, block interaction or inventory
action. The user then entered the password manually and the server reported
successful login, but the player stayed in the room. Captured position events
still pointed to the room; return to the former house/mine is unresolved and
needs comparison with the original client. Later server teleports use the same
corrected path; the loopback test also covers a second teleport and its ACK.
Position-only `0x0B` and look-only `0x0C` corrections are now handled as well
as combined `0x0D`, using the corresponding packet type for acknowledgement.
An event's position/rotation bits prevent a partial correction from resetting
the omitted fields. Their loopback cases use a distant negative-Z destination;
this is protocol coverage, not proof that the live server uses a partial packet.

Teleport handling resets velocity, fall distance and the previous interpolation
pose. Simulation waits for received map data under the player's footprint;
an empty chunk allocated by a block update or a partial map update is insufficient. This readiness flag
is runtime-only and never changes a save format. Teleport acknowledgements are
sent immediately, independently of terrain readiness, to avoid a deadlock.

Received network chunks are kept until the server sends PreChunk Unload. Unload
removes the chunk, render data and tile entities instead of retaining an empty
cache slot. If all cache entries contain received terrain, the network cache
grows within the existing 4096-chunk limit instead of silently evicting terrain
that the server will not resend. Empty speculative entries can still be evicted.
The offline cache and persistent saves retain their existing policy. A regression
test fills a two-entry cache, checks growth and stable chunks, then checks server
unload and mesh destruction; network_test exercises the actual unload packet.

T opens chat in singleplayer or multiplayer; `/` opens it with a slash already
entered. Multiplayer sends commands unchanged in packet `0x03`, including
plugin commands such as `/login`. They are not parsed or executed locally and
are not logged. Server permissions and command syntax remain authoritative;
the client does not grant creative mode or implement a server plugin by itself.

Singleplayer has a separate compact command handler in `game/commands.c`:
`/tp [name] x y z`, `/gamemode survival|creative|s|c|0|1 [name]`,
`/time set day|night|ticks`, `/time add ticks`, `/timeset day|night|ticks`,
`/weather clear|rain|thunder [seconds]`, `/seed` and `/help`.
Only the local player can be targeted. Inventory contents are preserved when
changing modes. Time and weather update existing world fields and use the
existing save path. Game mode persists in native ReCraft worlds; imported Beta
worlds retain it only for the current session, since vanilla Beta level.dat
does not have a creative game-mode field. No newer block IDs, adventure mode or protocol
framing are added. These local convenience commands are an extension to Beta;
the mode/time/coordinate rules were compared with the installed 1.5.2 client
command classes. Day/night aliases set 0/12500 ticks; weather duration uses
20 ticks per second; relative coordinates and absolute X/Z block centering are
supported. Local teleport Y is bounded to -4096..4096 for the legacy simulator.

Regression checks cover acknowledgement before chunk delivery, teleport feet
Y, readiness at chunk edges, raw forwarding of several server/plugin commands,
local command validation, mode changes, clock overflow and weather state.

## Server list and latency

The server editor stores protocol `14` (Beta gameplay) or `47` (modern status).
Existing three-column server-list files load as protocol 14; the fourth column
stores the choice. Addresses accept `host[:port]`, `[IPv6][:port]` or bare IPv6,
with port 25565 by default. DNS SRV resolution is not implemented.

`server_status_create/request/tick/get` manages up to 64 entries and four
background queries. DNS and TCP waits run outside the render thread. Each query
has a four-second deadline. An abandoned DNS lookup retains its worker slot until
the OS returns, so repeated Refresh clicks cannot create an unlimited number of
resolver threads. Results expose a revision for updating cached icons.

| Selected protocol | Query | Information shown |
| --- | --- | --- |
| Beta 14 | TCP connect, then close without login | TCP reachability and connection time only; MOTD, slots and gameplay ping remain unknown |
| Modern status 47 | STATUS handshake, JSON request, eight-byte ping and matching pong | Reported version, MOTD, online/max when present, round-trip ping and optional favicon |

The modern exchange follows the original
[1.8.9 server pinger](https://raw.githubusercontent.com/Marcelektro/MavenMCP-1.8.9/master/src/main/java/net/minecraft/client/network/OldServerPinger.java).
The requested protocol and the server's reported protocol are separate fields.
Servers may omit player counts, and the reported protocol may be `-1`; those
values do not fabricate compatibility or population. MOTD string/object/array
components preserve text, `extra` children, named colors and legacy section
color codes. Full translated/styled chat-component rendering remains incomplete.

Status JSON is bounded to 64 KiB, 2048 tokens and 24 nesting levels. A favicon
must be a `data:image/png;base64,` string containing a 64x64 PNG of at most 16 KiB.
Chunk CRCs, dimensions and a 64 KiB decompression bound are checked before the
existing raylib PNG decoder runs. Invalid or missing icons use the original
Minecraft unknown-server texture. Uploads and texture disposal stay on the
render thread through `assets_set_server_icon/get_server_icon/clear_server_icons`;
the client does not write received icons into the user's game directory.

Ping bars use the modern thresholds: five below 150 ms, four below 300, three
below 600, two below 1000, otherwise one. Unknown ping is displayed separately.
Beta TCP connection time is labelled `TCP`; it is not presented as a ping.

## Beta gameplay packets

Implemented traffic:

| Direction | Packet IDs | Use |
| --- | --- | --- |
| Both | `0x00`, `0x02`, `0x01`, `0x03`, `0x0A`-`0x0D` | Keepalive, offline handshake, login, chat, position/rotation corrections |
| Client to server | `0x0E`, `0x0F`, `0x10` | Mining, placement, selected hotbar slot |
| Client to server | `0x07`, `0x12`, `0x13` | Entity interaction/attack, arm swing and player-action APIs (sneak/leave bed) |
| Both | `0x09`, `0x65`, `0x6A` | Same-dimension respawn, close window, transaction |
| Client to server | `0x66` | Click Window with expected slot stack and action ID |
| Server to client | `0x08`, `0x14`-`0x18`, `0x1D`, `0x1F`-`0x22` | Health and basic entity events |
| Server to client | `0x04`, `0x46` | 64-bit world time and begin/end rain; local clock interpolates server time |
| Server to client | `0x32`-`0x35`, `0x3C` | Chunk visibility, zlib block regions, block changes, explosions |
| Server to client | `0x64`, `0x67`-`0x69`, `0xFF` | Open window, slot/inventory updates, furnace progress, disconnect |
| Both | `0x82` | Four-line standing/wall sign text updates |

Other known protocol 14 packets are framed and skipped when they do not affect
the current client. The adapter rejects a dimension other than the Overworld and
does not handle dimension changes. Online challenges use asynchronous session join. Same-dimension death
respawns retain chunks and wait for the server's new player position. A number of Beta block
IDs have no ReCraft model yet. Their original ID and metadata nibble now stay
in the chunk; rendering and collision still use solid or hidden placeholders.
IDs outside the stock 0..96 registry end the connection with an error.
Entity events are exposed to the application;
the renderer does not yet draw all entity types. This is not a complete Beta
gameplay client.

`network_send_position()` accepts **feet Y** and Beta yaw/pitch in **degrees**.
The serverbound wire order is X, feet Y, stance (`feet Y + 1.62`), Z.
The clientbound teleport assigns the first Y to the original player's
`Entity.posY`, which includes the 1.62 eye offset. The adapter subtracts it;
`NETWORK_EVENT_POSITION` returns feet Y and Beta degrees. Local
`Player` angles use radians and different axes: when sending,
`BetaYaw = LocalYaw * 180/pi + 180`, `BetaPitch = -LocalPitch * 180/pi`.
The reverse conversion applies to a received position. Inventory events carry
the raw window ID in `entity_type`, raw slot number in `slot`, item ID (`-1`
means empty), amount, and damage. Window 0 hotbar occupies slots 36-44.
Player, workbench, chest (27/54) and furnace containers use server slot layouts.
One click transaction is pending at a time. A rejected transaction is acknowledged,
and further clicks wait for the server's full Window Items resynchronization.
Shift-click transfers and the remaining server container types are not implemented.

`T` opens the chat draft; Enter sends it and Escape closes it. The bounded chat
history preserves Unicode and section colors supported by the bitmap font.
Packet strings are encoded as UTF-16 big endian from validated UTF-8; the chat
limit is 119 UTF-16 units. Glyphs absent from the current atlas use its fallback.

`network_player_list()` supplies self and currently tracked named entities for
the Tab overlay. It does **not** supply all connected players: vanilla protocol
14 has neither a global player-list packet nor per-player latency data. The UI
labels this as nearby players and displays unknown latency as `--`. Entity
despawn removes the corresponding entry. Modern server-list player counts are
not a substitute for an in-game roster.

`network_send_sign_update()` writes Beta packet `0x82`; incoming packets emit
`NETWORK_EVENT_SIGN` with coordinates and four copied lines. Each line is limited
to 15 UTF-16 units, including surrogate pairs, rather than 15 UTF-8 bytes.
The application applies server text through `sign_text_receive()` only to an
existing sign in a network world. The server remains authoritative for blocks,
sign text, inventory changes, damage and entity interactions.

## Verification

`tests/network_test.c` opens a local TCP listener, checks the actual outgoing
handshake/login, sends a deterministic protocol 14 session, and checks the
client's position acknowledgement, keepalive response, selected slot, zlib
chunk, single and multiple block changes, relighting, and inventory callbacks.
It checks that a wool ID and its color metadata survive the chunk packet.
It also checks server chest slots, rejected/accepted window clicks, cursor
resynchronization, furnace properties and the same-dimension respawn exchange.
The loopback session runs both offline and online with a mock session-join
callback that stays pending before allowing login; self/remote burning metadata
are checked too. It also checks named-entity roster creation/removal, Unicode chat and sign
round trips, entity attack, arm swing and sneak packet fields. Packet truncation
boundaries and oversized compressed chunk lengths are covered.

`tests/server_status_test.c` opens local TCP listeners and verifies modern
handshake/status/pong traffic, Unicode/color MOTD components, optional counts,
favicon bytes, malformed JSON/UTF-8, wrong pong tokens, oversized frames and
timeouts. It checks PNG CRC and decompression-bomb rejection, Beta TCP-only
semantics, IPv6 address parsing and backwards-compatible protocol persistence.
These tests use synthetic servers; **interoperability with a real Beta server
or modern server has not been measured**. Native Snow Leopard execution also
remains unverified.

The wire layouts were checked against the
[Technical Beta Wiki packet index](https://pixelbrush.dev/beta-wiki/networking/packets/),
especially its [login](https://pixelbrush.dev/beta-wiki/networking/packets/001-login),
[position](https://pixelbrush.dev/beta-wiki/networking/packets/013-player-position-and-rotation),
[chunk](https://pixelbrush.dev/beta-wiki/networking/packets/051-chunk-block-region),
[multi-block change](https://pixelbrush.dev/beta-wiki/networking/packets/052-set-multiple-blocks),
and [inventory](https://pixelbrush.dev/beta-wiki/networking/packets/104-fill-container)
descriptions.
