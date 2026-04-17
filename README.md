# Emergence

Fast-paced multiplayer top-down 2D arcade space-combat game.

Players pilot craft in a space arena, pick up weapons, and battle for frags. Timed matches with a leaderboard determine the winner. Inspired by games like Subspace/Continuum.

**Version:** 0.9  
**Author:** Jonathan Brown \<jbrown@bluedroplet.com\>  
**License:** GNU GPLv3

## Features

- Client-server multiplayer with authoritative server model
- Three weapon types: plasma cannon, minigun, rocket launcher
- Additional pickups: rails (railgun), mines, shield energy
- Map features: walls (BSP collision), teleporters, speedup ramps, gravity wells
- Per-player customizable colors for smoke, shields, plasma, and rail trails
- Demo recording and playback (zlib-compressed)
- Auto-downloading of maps from servers with checksum verification
- OpenSSL key-based authentication for Internet play
- LAN/Internet server browser with ping measurement
- Remote console (rcon) for server administration
- In-game console with cvar system for configuration

## Project Structure

```
emergence/
├── em-game/              # Main game (client + server)
│   ├── em-client/         # Game client binary
│   ├── em-server/         # Game server binary
│   ├── shared/            # Shared game logic (physics, networking, cvars)
│   ├── gsub/              # Graphics subsystem (software renderer)
│   ├── common/            # Utility library
│   ├── share/             # Game assets (maps, skins, sounds, demos)
│   ├── pixmaps/           # Icons and pixmaps
│   └── desktop/           # Desktop entry files
├── em-tools/              # Editing tools
│   ├── em-edit/           # Map editor (GTK+/GNOME)
│   └── em-skin/           # Skin packager
├── common/                # Cross-project utility library
├── gsub/                  # Graphics subsystem (shared copy)
├── misc/                  # Utility programs
│   ├── plasma/            # Plasma projectile sprite generator
│   ├── shield/            # Shield bubble sprite generator
│   ├── random/            # Key generation utilities
│   └── blocksize/         # I/O block size utility
└── stock-object-textures/ # Base object texture assets
```

## Architecture

### Client-Server Model

The server is the sole authority on game state. Clients send control inputs (thrust, brake, roll, fire) and the server runs physics, collision detection, and state changes, propagating updates to all connected clients.

### Shared Game Logic (`sgame.c`)

Compiled into both client and server via `#ifdef EMSERVER` / `#ifdef EMCLIENT`. Contains:

- Entity management and physics simulation
- Collision detection (circle-circle, line-circle, point-in-circle)
- BSP tree traversal for wall bouncing
- Weapon mechanics (plasma, minigun, rocket, mine, rail)
- Gravity wells, teleporters, speedup ramps

### Network Protocol

Custom UDP-based protocol on port 45420 (max packet 512 bytes):

- Cookie-based connection handshake
- Multiple stream types: timed (ordered, timestamped), untimed, and out-of-order variants
- Message classes: standard (demo-recorded), network-only, event (timestamped)
- OpenSSL key authentication for public Internet play

### Graphics Subsystem (`gsub`)

Custom software 2D renderer with:

- Multiple pixel formats (8-bit alpha, 16-bit RGB565, 24-bit, 32-bit, floating point)
- Alpha blending with precomputed lookup tables
- PNG image I/O
- Text rendering, line drawing, image resampling/rotation
- x86/MMX assembly optimizations for blitting, alpha compositing, and frame buffer updates

## Building

Each sub-project uses GNU Autotools:

```bash
# Build the game (client + server)
cd em-game
./configure
make

# Build the map editor
cd em-tools
./configure
make

# Build misc utilities
cd misc
./configure
make
```

### Dependencies

- SDL (audio)
- OpenSSL (key authentication)
- zlib (compression)
- libpng (image loading)
- X11 / XRandr / DGA (video output)
- ALSA (sound output)
- GTK+ / GNOME / Glade (map editor only)
- pthreads (multithreading)

## Running

### Server

```bash
em-server
```

Configure via console commands or `default-server.autoexec`.

### Client

```bash
em-client
```

Configure via in-game console or `default-client.autoexec`. Default controls are defined in `default-controls.config`.

### Map Editor

```bash
em-edit
```

Node-based editor with Bezier/conic curves, lines, fills, tiles, and object placement. Compiles maps to `.cmap` format (gzip-compressed BSP trees).

## Game Mechanics

- **Craft** have thrust, brake, and roll (rotation) with space friction
- **Weapons** attach to craft (left/right slots) and use ammo
- **Pickups** spawn at map-designated points and respawn after a delay
- **Shields** absorb damage for both craft and weapons
- **Matches** are timed; players must ready up before a match begins
- **Frags** are scored by destroying opponent craft (self-destructs subtract a frag)

## Distribution

The project includes a `.deb` package target for i686 Linux. Assets (maps, skins, sounds, demos) are installed to the shared data directory.

## History

- **0.9** — Various bugfixes and improvements
- **0.8** — Auto-downloading, SDL audio, compiled map versioning, checksums
- **0.7** — Key authentication, timed matches, player rankings, bandwidth reductions
- **0.6** — Improved stability/performance, customizable colors, new weapons in default skin