# em-tools Architecture

em-tools is the tooling package for Emergence, a 2D multiplayer space combat game. It contains two programs — a GTK2 map editor (`em-edit`) and a CLI skin packager (`em-skin`) — along with shared static libraries for graphics (`gsub`) and general utilities (`common`).

---

## 1. Build System

em-tools is built from the repository's top-level Meson project. `em-tools/meson.build` builds `em-skin` whenever zlib and libpng are available, and builds the editor only when GTK+ 2.0 is available and the `editor` feature is enabled or left at `auto`.

### Sub-projects

| Directory | Output | Status |
|-----------|--------|--------|
| `em-edit/` | `em-edit` binary | Built when `gtk+-2.0` is available |
| `em-skin/` | `em-skin` binary | Built when zlib and libpng are available |
| `gsub/` | `libgsub_tools.a`, `libgsub_misc.a` | Static libraries shared with the editor and skin packager builds |
| `common/` | `libcommon_tools.a` | Static library shared with both tool binaries |
| `pixmaps/` | `emergence.png` | Desktop icon |
| `desktop/` | `em-edit.desktop` | Freedesktop .desktop file |
| `share/` | PNG/SVG assets | Splash screen, node/point icons, stock textures |

### Library Split

The tools binaries link against Meson-built static libraries from the top-level `gsub/` and `common/` directories. `em-edit` uses the tools-specific gsub library compiled with `-DUSE_GDK_PIXBUF`, while `em-skin` links the non-GTK `libgsub_misc.a` because it only needs PNG and raw-surface helpers. The common library is shared with the rest of the tree. Editor runtime assets are installed under `${datadir}/emergence`, with the desktop icon also installed under `${datadir}/pixmaps`.

### Link Chain

```
em-edit: libcommon_tools.a + libgsub_tools.a + em-edit objects + GTK2 + zlib + libpng + libm + pthread
em-skin: libcommon_tools.a + libgsub_misc.a + em-skin objects + zlib + libpng + libm
```

For the internal architecture of `libcommon.a` and `libgsub.a`, see [`../../common/ARCHITECTURE.md`](../common/ARCHITECTURE.md) and [`../../gsub/ARCHITECTURE.md`](../gsub/ARCHITECTURE.md).

---

## 2. em-edit — Map Editor

em-edit is a WYSIWYG 2D map editor built on GTK2. It renders maps directly to a `GdkImage` backbuffer via the gsub surface system, and uses a cooperative worker thread for expensive background operations (BSP tree construction, texture resampling, tile rendering).

### Map Data Model

The map is a layered graph structure. All entity lists are intrusive singly-linked lists using the `->next` pointer convention from `common/llist.h`.

```
node_t ──sat──> conn_t ──belongs to──> curve_t
  │                │                       │
  │                └── parameterized by ──> t_t (sample points along curve)
  │
  ├── point_t (placed along curves)
  │       │
  │       ├── fill_t (closed polygon defined by points)
  │       └── line_t (wall segment between two points, doors/switches)
  │
  ├── tile_t (render cache region, occludes conns/nodes/fills)
  │
  └── object_t (independent game entities: weapons, spawn points, etc.)
```

#### `node_t` — Junction Points (`nodes.h`)

The fundamental building block. Each node has a position `(x, y)` and four **satellites** (`sats[4]`) — direction handles that control the shape and orientation of connections. Each satellite also has a **wall width** (`width[4]`).

```c
struct node_t {
    uint32_t index;
    float x, y;
    struct sat_t sats[4];          // direction handles {x, y}
    float width[4];                // wall width per satellite
    uint8_t fill_type;             // NODE_NOTHING or NODE_TEXTURE
    struct string_t *texture_filename;
    struct surface_t *texture_surface;
    float effective_x[4], effective_y[4];  // computed satellite endpoints
    uint8_t sat_conn_type[4];      // UNCONN, STRAIGHT, CONIC, or BEZIER
    int num_conns;
    uint32_t bsp_index[4];
    struct node_t *next;
};
```

Nodes are the only way to create connections. A satellite's `sat_conn_type` determines what kind of connection emanates from it.

#### `conn_t` — Connections (`conns.h`)

Edges between two node satellites. Three types:

| Type | Description |
|------|-------------|
| `CONN_TYPE_STRAIGHT` | Direct line between two satellite endpoints |
| `CONN_TYPE_CONIC` | Conic section curve (ellipse arc) |
| `CONN_TYPE_BEZIER` | Cubic Bezier curve |

```c
struct conn_t {
    uint32_t index;
    uint8_t type;                  // STRAIGHT, CONIC, or BEZIER
    struct node_t *node1, *node2;
    uint8_t sat1, sat2;            // which satellite on each node
    struct t_t *t0;                 // fine sampling points
    struct t_t *bigt0;              // coarse sampling points
    uint32_t t_count;
    float t_length;
    struct vertex_t *verts;         // computed polygon vertices
    struct surface_t *squished_texture;
    uint8_t tiled, fully_rendered;
    struct conn_t *next;
};
```

Each connection is parameterized by `t_t` sample points along its curve, which store positions and tangent vectors. These are used for vertex generation and texture mapping.

#### `curve_t` — Wall Appearance (`curves.h`)

Defines the visual appearance of connection walls. Multiple connections can share the same curve (same wall style). Curves are either solid color or textured, with control over tiling, offset, flip, rotation, and width lock.

```c
struct curve_t {
    uint32_t index;
    struct conn_pointer_t *connp0;  // connections using this curve
    struct string_t *texture_filename;
    uint8_t red, green, blue, alpha;
    uint8_t fill_type;             // CURVE_SOLID or CURVE_TEXTURE
    uint32_t reps;                 // texture repeat count
    float texture_length;
    uint8_t width_lock_on;
    float width_lock;
    struct curve_t *next;
};
```

#### `point_t` — Curve Points (`points.h`)

Points placed along curves. They define the vertices of fills and the endpoints of lines. Each point tracks its position on the curve (`pos` in 0..1) and its computed world coordinates.

```c
struct point_t {
    uint32_t index;
    struct curve_t *curve;
    float pos;                     // 0..1 along the curve
    float x, y;
    float deltax, deltay;          // perpendicular to curve at this point
    float left_width, right_width;
    struct conn_t *conn;           // which connection this point is on
    float t;                       // parameter value on connection
    int t_index;
    struct point_t *next;
};
```

#### `fill_t` — Filled Regions (`fills.h`)

Closed polygonal regions defined by a chain of `fill_edge_t` segments between points. Edges can either be straight lines or follow the curve they're on. Fills are solid color or textured with stretch/offset/friction properties.

```c
struct fill_t {
    uint8_t type;                  // FILL_TYPE_SOLID or FILL_TYPE_TEXTURE
    uint8_t red, green, blue, alpha;
    struct string_t *texture_filename;
    float stretch_horiz, stretch_vert;
    float offset_horiz, offset_vert;
    float friction;
    struct fill_edge_t *edge0;     // boundary edges
    struct texture_verts_t *texture_verts0;
    struct texture_polys_t *texture_polys0;
    uint8_t texture_tiled;
    struct fill_t *next;
};
```

#### `line_t` — Wall Segments (`lines.h`)

Segments between two points on a curve. Lines can carry door and/or switch behavior. Doors have color, width, energy, open/close timeouts, and an initial state. Switches have color, width, and cross-reference lists of doors to control on enter/leave.

```c
struct line_t {
    uint8_t status;                // LINE_STATUS_DOOR, LINE_STATUS_SWITCH, or both
    struct point_t *point1, *point2;
    // door fields: color, width, energy, initial_state, timeouts, index
    // switch fields: color, width, door cross-reference lists
    struct line_t *next;
};
```

#### `tile_t` — Render Cache (`tiles.h`)

The map is divided into rectangular tiles for occlusion-based rendering. Each tile caches a rendered image of all the connections, node textures, and fills it contains. When a connection/node/fill is modified, affected tiles are marked dirty and re-rendered by the worker thread.

```c
struct tile_t {
    int x1, y1, x2, y2;
    struct surface_t *surface;         // rendered image (NULL if not yet rendered)
    struct surface_t *scaled_surface;  // zoom-scaled version
    struct conn_pointer_t *connp0;     // connections occluded by this tile
    struct node_pointer_t *nodep0;     // node textures occluded by this tile
    struct fill_pointer_t *fillp0;     // fills occluded by this tile
    struct tile_t *next;
};
```

#### `object_t` — Game Objects (`objects.h`)

Independent entities placed on the map. Nine types with type-specific data in a union:

| Type | Data |
|------|------|
| `OBJECTTYPE_PLASMACANNON` | plasmas, angle, respawn_delay |
| `OBJECTTYPE_MINIGUN` | bullets, angle, respawn_delay |
| `OBJECTTYPE_ROCKETLAUNCHER` | rockets, angle, respawn_delay |
| `OBJECTTYPE_RAILS` | quantity, angle, respawn_delay |
| `OBJECTTYPE_SHIELDENERGY` | energy, angle, respawn_delay |
| `OBJECTTYPE_SPAWNPOINT` | texture, width/height, angle, teleport_only, index |
| `OBJECTTYPE_SPEEDUPRAMP` | texture, width/height, angle, activation_width, boost |
| `OBJECTTYPE_TELEPORTER` | texture, width/height, angle, radius, spawn_point_index, rotation |
| `OBJECTTYPE_GRAVITYWELL` | texture, width/height, angle, strength, enclosed |

Objects have a `texture_surface` (original) and a `scaled_texture_surface` (zoom-adjusted), both managed by the worker thread.

### Threading Model

em-edit uses a single cooperative worker thread to offload expensive operations from the GTK main loop.

#### Architecture

```
┌──────────────────┐           ┌──────────────────┐
│   GTK Main Thread│           │   Worker Thread   │
│                  │           │                    │
│  GTK event loop  │  signal   │  pthread_cond_wait │
│       │          │ ────────> │       │            │
│  start_working() │  (cond)   │  start()           │
│       │          │           │       │            │
│  stop_working()  │  trylock  │  check_stop_callback()
│       │          │ <──────── │  (via gsub_callback)│
│                  │           │       │            │
│                  │  write()  │  post_job_finished()│
│  on_worker_      │ <───────  │       │            │
│  callback()      │  (pipe)   │  pthread_cond_wait │
│       │          │           │                    │
│  job_complete_   │           └──────────────────┘
│  callback()      │
│       │          │
│  start_working() │  (if restarting)
└──────────────────┘
```

Three components cooperate:

1. **`worker_thread.c`** — Manages the pthread and inter-thread notification:
   - A `pthread_cond_t` signals the worker to start a job
   - A `pipe(mypipe[])` carries a single byte from worker to GTK when a job finishes
   - `gdk_input_add(mypipe[0], ...)` integrates the pipe into the GTK event loop as a `GDK_INPUT_READ` source

2. **`worker.c`** — Job priority queue and state machine:
   - 12 job types (`JOB_TYPE_SLEEP` through `JOB_TYPE_CAMERON`)
   - `start_working()` scans jobs in priority order; the first pending job wins
   - `stop_working()` acquires `main_lock` to block the worker, sets `stopping` flag
   - `job_complete_callback()` is called from GTK main thread via pipe notification; it runs completion handlers and restarts the next job if `restarting` is set
   - `gsub_callback` is set to `check_stop_callback()`, allowing long-running gsub operations (resampling, rotation) to be interrupted when the user modifies the map

3. **`main_lock.c`** — A `pthread_mutex_t` that serializes access to shared data structures:
   - The GTK thread acquires the lock when it needs to stop or coordinate with the worker; it is not held continuously
   - The worker thread periodically calls `worker_try_enter_main_lock()` (via `gsub_callback`) to check if it should stop
   - For expensive operations (surface rotation, resizing), `leave_main_lock_and_rotate_surface()` and `leave_main_lock_and_resize_surface()` duplicate the surface, release the lock, perform the operation lock-free, then return the result

#### Job Priority Order

`start_working()` checks jobs in this order. The first pending job wins and all others wait:

1. UI BSP tree generation (for interactive hit testing)
2. Object resampling (load textures at original resolution)
3. Object scaling (scale to current zoom)
4. Tile scaling (scale rendered tiles to current zoom)
5. Connection vertex generation (compute wall polygons)
6. Connection texture squishing (perspective-correct texture mapping)
7. Fill vertex generation
8. Node vertex generation
9. Tiling (compute tile assignments for connections/fills/nodes)
10. Tile rendering (rasterize tiles in worker thread)
11. Object resampling (second pass)
12. BSP tree generation (for compiled map output)
13. Object scaling (low priority)
14. Cameron (splash screen scaling)

When all jobs are complete, the map is considered "compiled" and `compile()` may run if requested.

### Rendering Pipeline

The rendering flow from data change to screen update:

1. **Invalidate** — A user edit (move node, change texture, etc.) calls `stop_working()` then invalidates affected entities (marks tiles dirty, sets `generate_bsp`, clears `tiled`/`fully_rendered` flags)
2. **Restart** — `start_working()` is called; it selects the highest-priority pending job and signals the worker thread
3. **Worker executes** — The worker thread runs the job function (e.g., `tile()`, `render_tile()`, `generate_bsp_tree()`) while periodically calling `check_stop_callback()` via `gsub_callback`
4. **Notify** — When the job finishes, the worker writes a byte to `mypipe[1]`
5. **GTK callback** — `on_worker_callback()` reads the byte and calls `job_complete_callback()`, which runs type-specific completion handlers and sets `restarting = 1`
6. **Draw** — If the job produced visible changes, `update_client_area()` is called, which runs `draw()` → `clear_surface(s_backbuffer)` → `draw_all()` → `gdk_draw_image()` to blit the backbuffer to the GDK window
7. **Next job** — `start_working()` is called again for the next pending job

#### Backbuffer Rendering

The backbuffer (`s_backbuffer`) is a gsub `surface_t` whose pixel buffer is the same memory as a `GdkImage`:

```c
// main.c:208-219 (on_configure)
image = gdk_image_new(GDK_IMAGE_FASTEST, visual, vid_width, vid_height);
s_backbuffer = new_surface_no_buf(SURFACE_24BITPADDING8BIT, vid_width, vid_height);
s_backbuffer->buf = image->mem;
s_backbuffer->pitch = image->bpl;
```

This zero-copy mapping means all gsub drawing operations write directly into the GdkImage's pixel memory. The 16-bit path uses `SURFACE_16BIT` instead.

### BSP Tree (`bsp.c`)

A Binary Space Partition tree provides spatial indexing for two purposes:

1. **Interactive hit testing** — `get_curve_bsp()`, `get_node_bsp()`, `get_fill_bsp()` use the UI BSP tree to quickly find entities under the mouse cursor
2. **Map compilation** — The compiled BSP tree is written to the `.cmap` output file for the game engine

BSP tree lines (`bsp_line_t`) carry typed references to curves, nodes, or fills on each side. The tree is built using the `inout()` half-plane test from `common/inout.c`. Two separate trees exist: the **UI BSP** (fast, for hit testing) and the **compile BSP** (complete, for output).

### Interface (`interface.c`)

The interface module manages:

- **Coordinate transforms** — `world_to_screen()` and `screen_to_world()` convert between world coordinates (float, centered on view) and screen pixels, accounting for zoom and pan
- **View state** — A bitmask (`view_state`) controls visibility of grid, objects, tiles, BSP tree, outlines, switches/doors, and nodes
- **Zoom** — A float `zoom` factor (default 1.0); satellite display mode toggles between vector (`SATS_VECT`) and width (`SATS_WIDTH`)
- **Interaction state machine** — States include `STATE_DRAGGING_VIEW`, `STATE_DRAGGING_NODE`, `STATE_DRAGGING_SAT`, `STATE_DRAGGING_POINT`, `STATE_DRAGGING_OBJECT`, `STATE_SPACE_MENU`, `STATE_DEFINING_FILL`, and more
- **Mouse/key handlers** — `left_button_down()`, `right_button_down()`, `mouse_move()`, `key_pressed()`, etc. dispatch to the current state handler
- **Drawing** — `draw_all()` renders all visible layers to the backbuffer in the correct order

### Cameron (`cameron.c`)

The splash screen system. Loads `splash.png` on init, maintains a scaled version (`s_scaled_cameron`) that preserves aspect ratio within the window. Scaling is done by the worker thread via `JOB_TYPE_CAMERON`. Drawn centered in the viewport when no map is active.

### UI Dialogs

Property editors and dialogs are built with **Glade** (`em-edit.glade`). `glade-2` generates `glade.c`/`glade.h` containing `create_*_dialog()` functions. `callbacks.h` declares signal handlers for each dialog widget. Dialogs exist for:

- Every object type (rocket launcher, minigun, plasma cannon, spawn point, teleporter, gravity well, speedup ramp, shield energy, rails)
- Door/switch properties
- Fill properties and wall properties
- End node, crossover node, and straight-through node properties
- Compile progress, corrupt file, not saved, and help dialogs

`support.c`/`support.h` provide `lookup_widget()` for Glade widget tree navigation.

### Map Save/Load

Maps are saved as gzip-compressed files (`.map`) using `gzwrite_*`/`gzread_*` functions from gsub and common. The save file writes entities in this order: nodes, connections, curves, points, fills, lines, and objects. The BSP tree is rebuilt after load rather than stored in the `.map` file. A compiled version (`.cmap`) is produced by `compile()` and placed in `~/.emergence/maps/`.

File paths for textures are stored as relative paths (via `abs2rel`/`rel2abs` from `common/`) so maps are portable.

---

## 3. em-skin — Skin Packager

A CLI tool that reads PNG textures from a named directory and writes a gzip-compressed `.skin` file. Meson builds it whenever zlib and libpng are available.

Usage: `em-skin <directory>`

For each directory argument, it reads:
- `craft.png`
- `rocket-launcher.png`
- `minigun.png`
- `plasma-cannon.png`

Each is loaded as a 24-bit+8-bit-alpha surface via `read_png_surface_as_24bitalpha8bit()` and written with `gzwrite_raw_surface()`. The output file is `<directory>.skin` compressed at level 9.

em-skin depends only on gsub/common helpers, zlib, and libpng — no GTK dependency.

---

## 4. Module Dependency Graph

```
                    main.c
                   /   |   \
                  /    |    \
           interface  map    cameron
          /  |  |  \   |       |
      grid  bsp nodes objects  gsub
             |   |  \     |
          bezier conns  tiles
                   |      |
                curves  worker_thread
                   |        |
                points  main_lock
               /     \      |
            fills    lines  worker
               \     /       |
                tiles   ─────┘
                   |
               (render cache)

  ┌─────────────────────────────────────┐
  │  gsub (surface_t, blit, resample)   │
  └──────────────┬──────────────────────┘
                 │
  ┌──────────────┴──────────────────────┐
  │  common (llist, vertex, polygon,    │
  │    stringbuf, buffer, user, etc.)   │
  └─────────────────────────────────────┘
```

---

## 5. File Reference

### em-edit/

| File | Responsibility |
|------|---------------|
| `main.c` / `main.h` | GTK window setup, event handlers, backbuffer management, initialization |
| `interface.c` / `interface.h` | Coordinate transforms, view state, interaction state machine, `draw_all()` |
| `map.c` / `map.h` | Map lifecycle (new/open/save/compile/clear), file dialogs |
| `worker.c` / `worker.h` | Job priority queue, start/stop/restart state machine |
| `worker_thread.c` / `worker_thread.h` | pthread management, pipe-based GTK notification |
| `main_lock.c` / `main_lock.h` | Mutex for GTK/worker thread coordination |
| `nodes.c` / `nodes.h` | Node CRUD, satellite manipulation, vertex generation, texture tiling |
| `conns.c` / `conns.h` | Connection CRUD, t-value generation, vertex generation, texture squishing |
| `curves.c` / `curves.h` | Curve (wall style) CRUD, connection-to-curve assignment |
| `points.c` / `points.h` | Point CRUD along curves |
| `fills.c` / `fills.h` | Fill CRUD, fill definition flow, vertex generation, texture tiling |
| `lines.c` / `lines.h` | Line CRUD, door/switch management |
| `objects.c` / `objects.h` | Game object CRUD, resampling, scaling |
| `tiles.c` / `tiles.h` | Tile render cache, collation, scaling, rendering |
| `bsp.c` / `bsp.h` | BSP tree construction, spatial queries, compiled output |
| `bezier.c` / `bezier.h` | Cubic Bezier evaluation and parameterization |
| `conic.c` / `conic.h` | Conic section evaluation and parameterization |
| `grid.c` / `grid.h` | Grid drawing and snap-to-grid |
| `cameron.c` / `cameron.h` | Splash screen loading and scaling |
| `floats.c` / `floats.h` | Floating image I/O for compiled maps |
| `glade.c` / `glade.h` | Auto-generated dialog constructors (from Glade) |
| `callbacks.h` | Auto-generated dialog signal handlers (from Glade) |
| `support.c` / `support.h` | Auto-generated Glade helper functions |
| `em-edit.glade` / `em-edit.gladep` | Glade UI definition files |

### em-skin/

| File | Responsibility |
|------|---------------|
| `main.c` / `main.h` | CLI entry point, directory to .skin conversion |

---

## 6. Design Patterns and Conventions

### Constructor / Destructor

Every module follows `init_*()` / `kill_*()` naming for setup and teardown. Called in sequence from `main()`:

```c
init_user();
init_gsub();
init_interface();
init_map();
init_cameron();
init_worker();    // creates main_lock and worker thread
```

Destruction follows reverse order in the `on_destroy` handler.

### Linked Lists

All entity collections use intrusive singly-linked lists with a `->next` pointer, managed via `common/llist.h` macros (`LL_CALLOC`, `LL_REMOVE`, `LL_REMOVE_ALL`). Head pointers follow the `entity0` naming convention (e.g., `node0`, `conn0`, `curve0`, `point0`, `fill0`, `line0`, `object0`).

### Pointer Lists

Secondary collections use `_pointer_t` structs (e.g., `node_pointer_t`, `conn_pointer_t`, `fill_pointer_t`) that hold a reference to an entity and a `->next` pointer. Used for tile occlusion lists, selection lists, and curve connection lists.

### Cooperative Cancellation

`gsub_callback` is set to `check_stop_callback()` at init time. Any long-running gsub operation (resampling, rotation) calls this function pointer periodically. If the GTK thread has called `stop_working()`, it holds `main_lock`, so `worker_try_enter_main_lock()` fails and the gsub operation returns a cancellation indicator (typically freeing its output and returning NULL).

### gzwrite / gzread Serialization

All entity types implement `gzwrite_*(gzFile)` and `gzread_*(gzFile)` functions for save/load, conditionally compiled with `#ifdef ZLIB_H`. This pattern is used consistently across nodes, connections, curves, points, fills, lines, objects, BSP trees, and surfaces.

### Conditional Compilation

| Flag | Effect |
|------|--------|
| `LINUX` | Linux-specific code paths |
| `USE_GDK_PIXBUF` | Enable GdkPixbuf image loading (in gsub) |
| `_GNU_SOURCE` | GNU extensions (vasprintf) |
| `_REENTRANT` | Thread-safe libc functions |
| `ZLIB_H` | Enable gzwrite/gzread functions |

---

## 7. External Dependencies

| Library | Usage |
|---------|-------|
| GTK2 / GDK2 | Window system, event handling, file chooser/color button widgets, image display |
| zlib | Gzip-compressed map/skin file I/O |
| libpng | PNG texture loading |
| pthread | Worker thread, mutex |
