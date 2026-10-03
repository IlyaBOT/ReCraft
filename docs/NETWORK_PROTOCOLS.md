# Network protocol status

Vehicle/object registry handling and its loopback regressions are described in
[the gameplay and UI update](BETA_GAMEPLAY_UI.md).

The current adapter speaks the **Minecraft Beta 1.7.3 protocol, version 14**, to
offline-mode servers. It never sends account credentials. A server that returns an
online-authentication challenge is rejected. Protocol 47 supports modern
**server-list status queries**, including MOTD, player counts, ping and favicon.
It is not a usable modern gameplay client; selecting modern status does not
enable modern gameplay login. Optional Microsoft profile authentication is a
separate feature, described in [account setup](MICROSOFT_ACCOUNT.md); it does not
enable online-mode Beta joining or modern gameplay.

The transport resolves DNS on a worker thread, connects over nonblocking TCP,
buffers partial packets, and processes at most 128 packets and two zlib chunk
regions per `network_tick()`. It limits a compressed region to 128 KiB and checks
the exact decompressed length before writing blocks. Unknown packet IDs and
malformed lengths end the connection with an error instead of losing framing.
Small action packets use TCP_NODELAY. macOS sockets use SO_NOSIGPIPE, and status
query deadlines use the monotonic `mach_absolute_time()` available on 10.6.

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
| Both | `0x00`, `0x02`, `0x01`, `0x03`, `0x0D` | Keepalive, offline handshake, login, chat, position/rotation |
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
does not handle dimension changes or online authentication. Same-dimension death
respawns retain chunks and wait for the server's new player position. A number of Beta block
IDs have no ReCraft model yet. Their original ID and metadata nibble now stay
in the chunk; rendering and collision still use solid or hidden placeholders.
IDs outside the stock 0..96 registry end the connection with an error.
Entity events are exposed to the application;
the renderer does not yet draw all entity types. This is not a complete Beta
gameplay client.

`network_send_position()` accepts **feet Y** and Beta yaw/pitch in **degrees**.
The serverbound wire order is X, camera Y (`feet Y + 1.62`), feet Y, Z. A
clientbound `NETWORK_EVENT_POSITION` returns feet Y and Beta degrees. Local
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
It also checks named-entity roster creation/removal, Unicode chat and sign
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
