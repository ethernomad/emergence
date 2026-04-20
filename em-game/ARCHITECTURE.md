# Emergence Architecture

Emergence is a fast-paced multiplayer top-down arcade space-combat game. Players pilot crafts (spaceships) in 2D arenas, picking up weapons, fighting opponents, using teleporters, speedup ramps, and gravity wells. Written in C, licensed under GPL-3.0.

## Architecture Overview

Client-server model with an **authoritative server**. The server derives game time at **200 Hz** (200 ticks/second) from the shared timer code, processes client input commands, and broadcasts entity state changes to all connected clients. Clients run local prediction using the same shared physics code and render the scene.

```
┌─────────────┐  UDP (custom reliable protocol)  ┌─────────────┐
│  em-client   │◄────────────────────────────────►│  em-server  │
│              │                                  │             │
│  Rendering   │  Control input ─────────►       │  Game loop  │
│  Prediction  │  ◄──────── Entity events         │  Physics     │
│  Input       │  ◄──────── Player info           │  Auth       │
│  Sound       │                                  │  Console    │
└─────────────┘                                  └─────────────┘
```

Three static libraries provide shared code:

- **libcommon.a** — general utilities (buffers, polygons, paths, resources)
- **libgsub.a** — software 2D graphics primitives (MMX-optimized blitting)
- **libshared.a** — game logic, network protocol, BSP collision (compiled into both client and server with `EMCLIENT`/`EMSERVER` preprocessor guards)

## Directory Structure

```
em-game/
├── common/          libcommon.a — buffers, polygons, linked lists, resource paths
├── gsub/            libgsub.a — software 2D rasterizer with MMX assembly
├── shared/          Code shared between client/server (network, physics, BSP, cvars)
├── em-client/       Client executable — rendering, input, sound, prediction
├── em-server/       Server executable — game loop, propagation, authentication
├── share/           Game data — maps, demos, sounds, skins, configs
│   ├── stock-maps/        Compiled .cmap map files
│   ├── stock-sounds/      Ogg Vorbis sound effects
│   ├── stock-object-textures/  PNG textures for entities
│   ├── stock-skins/       Player skin definitions
│   └── demos/             Recorded gameplay demos
├── pixmaps/         Desktop icon (emergence.png)
├── desktop/         .desktop files for client and server
└── meson.build       Meson build rules for the client/server targets and assets
```

## Build System

Meson-based (`meson setup build && meson compile -C build`). Relevant options:

- `-Dnonauthenticating=true` — builds the server without public-key authentication (`-DNONAUTHENTICATING`)

The build generates `share/stock-sounds/*.ogg` from the checked-in `.wav` sources using `oggenc` when available, or `ffmpeg` as a fallback, then installs those generated files under `${datadir}/emergence/stock-sounds`.

Client links static `gsub`, `shared`, and `common` libraries plus SDL, OpenSSL/libcrypto, X11/Xext/Xrandr, OpenGL/GLU, ALSA, Vorbis/Ogg, zlib, libpng, libm, and pthreads.

Server links static `shared` and `common` libraries plus OpenSSL/libcrypto, zlib, libm, and pthreads.

The client's server-browser cache is stored in `~/.emergence/rumoured.client`; it is not seeded from an installed data file.

## Server Architecture

Entry point: `entry.c` — parses CLI options (`-d` daemonize, `-m` map, `-n` max players, `-p` port default 45420, `-e` exec script).

### Main Loop

`poll()` on three file descriptors:

1. `net_out_pipe[0]` — network events from the network thread
2. `key_out_pipe[0]` — key authentication results
3. `STDIN_FILENO` — console input (if not daemonized)

### Connection State Machine

```
CONN_STATE_VIRGIN ──► CONN_STATE_AUTHENTICATING ──► CONN_STATE_VERIFYING ──► CONN_STATE_JOINED
      (connect)              (key exchange)              (key verified)          (in game)
```

Local connections and private-network connections skip authentication. Public Internet connections require authentication unless built with `NONAUTHENTICATING`.

### Game Loop

`process_game_timer()` runs from the shared alarm thread. That thread is a busy loop that repeatedly calls registered listeners; `update_game()` converts wall time into 200 Hz game ticks and catches up simulation work as needed:

1. `tick_game()` → `s_tick_entities()` — shared physics for all entities
2. `tick_player()` — per-player pulse events
3. `tick_map()` — pickup respawns
4. `propagate_entities()` — sends entity state updates to clients
5. `propagate_player_info()` — sends player rankings

## Client Architecture

Entry point: `entry.c` — initializes the client process, signal handling, and X11-based video path.

### Main Loop

`poll()` on five file descriptors:

1. `x_render_pipe[0]` — X11 render events
2. `console_pipe[0]` — console input
3. `key_out_pipe[0]` — key authentication results
4. `download_out_pipe[0]` — map download progress
5. `net_out_pipe[0]` — network events (suspended during map download)

### Client Game State Machine

```
GAMESTATE_DEAD (0)
GAMESTATE_DEMO (1)
GAMESTATE_CONNECTING (2) ──► GAMESTATE_JOINING (3) ──► GAMESTATE_CREATING_SESSION (4)
GAMESTATE_AWAITING_APPROVAL (5) ──► GAMESTATE_JOINED (6)
GAMESTATE_SPECTATING (7) ──► GAMESTATE_PLAYING (8)
```

## Network Protocol

Custom **reliable UDP** protocol on raw UDP sockets (max packet 512 bytes).

### Packet Header (32 bits)

| Bits    | Field  |
|---------|--------|
| 0–26    | Index (sequence number, 0 to 0x07FFFFFF) |
| 27–29   | Flags |
| 30–31   | Class |

### Packet Classes

| Class | Value | Purpose |
|-------|-------|---------|
| `EMNETCLASS_CONTROL` | 0x0 | Connection management |
| `EMNETCLASS_STREAM` | 0x1 | Reliable data stream |
| `EMNETCLASS_MISC` | 0x3 | Unreliable (ping/pong, server info) |

### Connection Handshake (anti-spoofing 3-way)

1. Client → Server: `CONNECT` with magic number `0x02708d11`
2. Server → Client: `COOKIE` with random value (proves return address)
3. Client → Server: echo `COOKIE` back
4. Server → Client: `CONNECT` with starting index (connection established)

### Reliability

- All `STREAM` and `DISCONNECT` packets are acknowledged
- Unacknowledged packets retransmitted every 100ms
- Connect packets retransmitted every 500ms
- Connection timeout: 5 seconds
- Max 400 unacknowledged outbound packets per connection

### Threading

Network runs in a dedicated `pthread` using `poll()` on the UDP socket + kill pipe. Communicates with the main thread via `net_out_pipe` (pipe-based message passing) protected by a recursive mutex.

## Game Protocol

Protocol version: `EM_PROTO_VER 0x05`

### Message Classes (top 2 bits of first byte)

| Class | Value | Description |
|-------|-------|-------------|
| `EMMSGCLASS_STND` | Standard | Written to demo files |
| `EMMSGCLASS_NETONLY` | Network-only | Not recorded in demos |
| `EMMSGCLASS_EVENT` | Event | Timed game events with tick number |

### Server → Client Messages (key messages)

| Message | Class | Description |
|---------|-------|-------------|
| `EMMSG_PROTO_VER` | STND | Protocol version negotiation |
| `EMMSG_LOADMAP` | STND | Load map (name, size, SHA1) |
| `EMMSG_LOADSKIN` | STND | Load a skin |
| `EMMSG_PLAYING` | STND | Enter playing state |
| `EMMSG_SPECTATING` | STND | Enter spectating state |
| `EMNETMSG_AUTHENTICATE` | NETONLY | Request authentication |
| `EMNETMSG_JOINED` | NETONLY | Join accepted |
| `EMNETMSG_FAILED` | NETONLY | Join failed |
| `EMNETMSG_MATCH_BEGUN` | NETONLY | Match started |
| `EMNETMSG_PLAYER_INFO` | NETONLY | Player name/ready/frags |
| `EMNETMSG_MATCH_OVER` | NETONLY | Match ended |
| `EMNETMSG_LOBBY` | NETONLY | Return to lobby |
| `EMEVENT_PULSE` | EVENT | Tick sync (every 200 ticks) |
| `EMEVENT_SPAWN_ENT` | EVENT | Create entity on client |
| `EMEVENT_UPDATE_ENT` | EVENT | Update entity state |
| `EMEVENT_KILL_ENT` | EVENT | Remove entity |
| `EMEVENT_FOLLOW_ME` | EVENT | Camera follow entity |
| `EMEVENT_CARCASS` | EVENT | Craft becomes carcass |
| `EMEVENT_RAILTRAIL` | EVENT | Railgun trail visual |
| `EMEVENT_EXPLOSION` | EVENT | Explosion particles |

### Client → Server Messages (key messages)

| Message | Description |
|---------|-------------|
| `EMMSG_JOIN` | Request to join |
| `EMMSG_SESSION_KEY` | Auth key response |
| `EMMSG_THRUST` | Thrust amount (0–1) |
| `EMMSG_BRAKE` / `EMMSG_NOBRAKE` | Start/stop braking |
| `EMMSG_ROLL` | Roll amount (continuous float) |
| `EMMSG_ROLL_LEFT/RIGHT` | Key-based roll |
| `EMMSG_FIRERAIL` | Fire railgun |
| `EMMSG_FIRELEFT` / `EMMSG_FIRERIGHT` | Fire weapons |
| `EMMSG_DROPMINE` | Drop mine |
| `EMMSG_READY` | Toggle ready status |
| `EMMSG_SAY` | Chat message |
| `EMMSG_SUICIDE` | Self-destruct |

## Entity System

Eight entity types with distinct physics:

| Type | Constant | Radius | Mass | Description |
|------|----------|--------|------|-------------|
| 0 | `ENT_CRAFT` | 56.569 | 100.0 | Player spaceship |
| 1 | `ENT_WEAPON` | 35.355 | 75.0 | Pickup weapon |
| 2 | `ENT_PLASMA` | 5.0 | — | Plasma projectile |
| 3 | `ENT_BULLET` | point | — | Minigun bullet |
| 4 | `ENT_ROCKET` | 25.0 | — | Rocket with thrust |
| 5 | `ENT_MINE` | 60.0 | — | Proximity mine |
| 6 | `ENT_RAILS` | 20.0 | 15.0 | Railgun ammo pickup |
| 7 | `ENT_SHIELD` | 20.0 | — | Shield energy pickup |

### Weapons

| Weapon | Velocity | Fire Rate | Damage | Notes |
|--------|----------|-----------|--------|-------|
| Plasma Cannon | 8 | 20/sec (1 per 10 ticks) | 0.1 | Slow projectiles |
| Minigun | 24 | 50/sec | 0.05 | Fast point bullets |
| Rocket Launcher | thrust phase 400 ticks | — | splash, force 4000 | Explodes on contact |
| Railgun | instant (line walk) | — | 0.5 | Max 20 rails, requires ammo pickup |

### Physics

- **Space friction**: 0.99935/tick (very slight drag)
- **Brake friction**: 0.98/tick
- **Collision friction**: 0.75 on bounce
- **Craft rolling speed**: 0.024 rad/tick (key), continuous via mouse
- **Craft thrust**: up to 0.025 acceleration units
- **Gravity wells**: inverse-square attraction, can be confined by BSP walls
- **Weapon auto-attach**: weapons orbit at ideal distance 100, max 150

### Collision Detection

- **Circle–circle**: `circles_intersect()` — distance < sum of radii
- **Point–circle**: `point_in_circle()` — for bullet hits
- **Line–circle**: `line_in_circle()` — discriminant test for line-segment vs circle
- **Circle–BSP wall**: `circle_walk_bsp_tree()` — recursive traversal for entity-wall collision
- **Line–BSP wall**: `line_walk_bsp_tree()` — for railgun hits and entity-wall collision
- **Elastic bouncing**: velocity reflection along collision normal with mass-dependent momentum transfer and friction

### Match System

1. **Lobby** — players join, mark ready
2. **Match begins** — when all players ready
3. **Match duration** — configurable (default 10 minutes)
4. **Match end** — highest frags wins; 10 seconds later, return to lobby
5. **Scoring** — kill = +1 frag; death (self/accident) = -1 frag

### Damage Model

- Craft shields start at 1.0
- Shield < 0 but > -0.25: craft becomes "carcass" (can be pushed)
- Shield < -0.25: craft explodes
- Splash damage: inverse-square from explosion point

## Map System

### .cmap Format (gzip-compressed binary)

1. **Format ID**: uint16 (must equal 0)
2. **BSP tree**: recursive wall geometry for collision
3. **Objects**: typed array (spawn points, teleporters, speedup ramps, gravity wells, weapon/shield/rails pickups)
4. **Map tiles**: array of (x, y, raw_surface) for background rendering
5. **Floating images**: additional decorative images

### BSP Tree

```c
struct bspnode_t {
    float x1, y1, x2, y2;    // Line segment defining the partition
    float tstart, tend;       // Parameter range for wall segment
    float dtstart, dtend;     // Delta parameter range
    struct bspnode_t *front, *back;
};
```

Used for wall collision (circle and line walks) and railgun hit detection.

### Map Loading

1. Server sends `EMMSG_LOADMAP` with map name, file size, SHA1 hash
2. Client checks home directory → stock-maps → auto-download from server
3. Map tiles scaled to match video resolution (native: 1600×1200; other resolutions cache as `.cmap.cache<width>`)
4. Integrity verified via SHA1 hash and file size
5. Network message processing suspended during download

## Rendering Pipeline

Entirely **software-based** using the `gsub` 2D rasterizer, output to screen via X11 shared-memory blits.

```
┌────────────────────────────────────────────────┐
│ clear_surface(s_backbuffer)                    │  16-bit RGB565 software buffer
│ render_game()                                  │  Tiles, entities, effects
│ render_servers() / render_console() / etc.     │  UI overlays
│ update_frame_buffer()                          │  XShmPutImage() → XFlush()
└────────────────────────────────────────────────┘
```

### World-to-Screen Mapping

```
screenx = floor((worldx - viewx) * (vid_width / 1600.0)) + vid_width/2
screeny = vid_height/2 - 1 - floor((worldy - viewy) * (vid_width / 1600.0))
```

Reference resolution: 1600×1200. Non-native resolutions scale map tiles accordingly.

### X11 Presentation

- Creates a fullscreen undecorated X11 window
- Allocates an `XImage` via `XShmCreateImage()` and backs `s_backbuffer` with the shared-memory pixel buffer
- Presents frames with `XShmPutImage()` followed by `XFlush()`
- Uses XRandR to query and switch display modes

## Input System

| Input | Method |
|-------|--------|
| Mouse | Primary: raw `/dev/input/mice` (3-byte protocol). Fallback: X11 mouse events |
| Keyboard | X11 `KeyPress` events processed in `process_x()` |
| DGA | XFree86 Direct Graphics Access for mouse grabbing |

### Default Bindings

```
W/Up    = thrust          A      = fire_left
S/Down  = brake           D      = fire_right
A/Left  = roll_left       Space  = drop_mine
D/Right = roll_right      R      = fire_rail
Z       = fire_left       X      = fire_rail
Mouse axis_0 = roll       Mouse btn_0 = fire_rail
                         Mouse btn_1 = brake
```

## Authentication

- **Internet play**: OpenSSL-based challenge-response authentication requiring purchased keys
- **LAN/private play**: Local and private-network connections bypass authentication
- **`NONAUTHENTICATING` compile flag**: disables authentication for public connections as well

## Timer System

- Uses `rdtsc` on x86 and a monotonic-clock fallback on other architectures
- x86 builds calibrate timer frequency from `/proc/cpuinfo`
- Tick rate: 200 Hz (`get_tick_from_wall_time()` converts elapsed counts to game ticks)
- Wall time: `get_wall_time()` returns seconds as double

## Dependencies

| Library | Client | Server | Purpose |
|---------|--------|--------|---------|
| OpenGL/GLU | ✓ | — | Linked by the client build |
| X11/Xrandr | ✓ | — | Window management, input, display modes |
| SDL 1.2.7 | ✓ (static, audio-only) | — | Audio output |
| zlib | ✓ | ✓ | Gzip compression (maps, demos) |
| libpng | ✓ | — | PNG image loading |
| OpenSSL | ✓ | ✓ | Key authentication, SHA1 hashing |
| libvorbis/libogg | ✓ | — | Ogg Vorbis audio decoding |
| ALSA (libasound) | ✓ | — | Audio output |
| pthreads | ✓ | ✓ | Threading |
