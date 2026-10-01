# Network protocol status

The current adapter speaks the **Minecraft Beta 1.7.3 protocol, version 14**, to
offline-mode servers. It never sends account credentials. A server that returns an
online-authentication challenge is rejected. No protocol 47 adapter exists yet.

The transport resolves DNS on a worker thread, connects over nonblocking TCP,
buffers partial packets, and processes at most 128 packets and two zlib chunk
regions per `network_tick()`. It limits a compressed region to 128 KiB and checks
the exact decompressed length before writing blocks. Unknown packet IDs and
malformed lengths end the connection with an error instead of losing framing.

Implemented traffic:

| Direction | Packet IDs | Use |
| --- | --- | --- |
| Both | `0x00`, `0x02`, `0x01`, `0x03`, `0x0D` | Keepalive, offline handshake, login, chat, position/rotation |
| Client to server | `0x0E`, `0x0F`, `0x10` | Mining, placement, selected hotbar slot |
| Both | `0x09`, `0x65`, `0x6A` | Same-dimension respawn, close window, transaction |
| Client to server | `0x66` | Click Window with expected slot stack and action ID |
| Server to client | `0x08`, `0x14`-`0x18`, `0x1D`, `0x1F`-`0x22` | Health and basic entity events |
| Server to client | `0x32`-`0x35`, `0x3C` | Chunk visibility, zlib block regions, block changes, explosions |
| Server to client | `0x64`, `0x67`-`0x69`, `0xFF` | Open window, slot/inventory updates, furnace progress, disconnect |

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

## Verification

`tests/network_test.c` opens a local TCP listener, checks the actual outgoing
handshake/login, sends a deterministic protocol 14 session, and checks the
client's position acknowledgement, keepalive response, selected slot, zlib
chunk, single and multiple block changes, relighting, and inventory callbacks.
It checks that a wool ID and its color metadata survive the chunk packet.
It also checks server chest slots, rejected/accepted window clicks, cursor
resynchronization, furnace properties and the same-dimension respawn exchange.
It also checks packet truncation boundaries and rejects an oversized compressed
chunk length. The test runs with a synthetic server; **native interoperability
with a Beta 1.7.3 server has not been measured**.

The wire layouts were checked against the
[Technical Beta Wiki packet index](https://pixelbrush.dev/beta-wiki/networking/packets/),
especially its [login](https://pixelbrush.dev/beta-wiki/networking/packets/001-login),
[position](https://pixelbrush.dev/beta-wiki/networking/packets/013-player-position-and-rotation),
[chunk](https://pixelbrush.dev/beta-wiki/networking/packets/051-chunk-block-region),
[multi-block change](https://pixelbrush.dev/beta-wiki/networking/packets/052-set-multiple-blocks),
and [inventory](https://pixelbrush.dev/beta-wiki/networking/packets/104-fill-container)
descriptions.
