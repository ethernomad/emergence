# Architecture — `common/`

## 1. Overview

`common/` is a statically-linked C utility library shared by every buildable sub-project in the Emergence game engine (client, server, map editor, skin packager, and sprite generators). It provides the lowest level of the engine stack:

- Platform-independent type definitions
- Dynamic string construction and I/O
- Binary byte-buffer serialization for the custom UDP network protocol
- 2D geometry primitives and polygon clipping
- Filesystem path manipulation and runtime asset discovery
- Per-user directory initialization

The library is never compiled standalone. Each sub-project builds its own `libcommon.a` by symlinking source files from this directory at configure time.

---

## 2. Module Inventory

### Foundation Types

| File | Purpose |
|------|---------|
| `types.h` | Platform-independent integer aliases (`byte`, `word`, `dword`, `dwordlong`) and `NULL` |
| `vertex.h` | 2D point types: fixed `vertex_t` and linked-list `vertex_ll_t` |
| `llist.h` | Generic intrusive singly-linked list macros (`LL_ADD`, `LL_REMOVE`, `LL_REMOVE_ALL`, etc.) |
| `minmax.h` | `min(a, b)` and `max(a, b)` ternary macros |

### String & Buffer

| File | Purpose |
|------|---------|
| `stringbuf.h` / `stringbuf.c` | Dynamic string (`string_t`) with printf-style formatting, file I/O, and zlib I/O |
| `buffer.h` / `buffer.c` | Binary read/write buffer (`buffer_t`) for network packet construction and deserialization |

### Geometry

| File | Purpose |
|------|---------|
| `polygon.h` / `polygon.c` | Sutherland-Hodgman convex polygon clipping and area computation |
| `inout.h` / `inout.c` | Point-vs-directed-line classification (inside/outside half-plane test) |

### Filesystem

| File | Purpose |
|------|---------|
| `resource.h` / `resource.c` | Game asset discovery — searches multiple install directories for a named resource |
| `user.h` / `user.c` | Per-user directory setup (`~/.emergence/`, skins, maps) |
| `prefix.h` / `prefix.c` | Small path helper functions (`br_strcat`, `br_extract_dir`, `br_extract_prefix`) |
| `abs2rel.h` / `abs2rel.c` | Convert an absolute filesystem path to a relative path |
| `rel2abs.h` / `rel2abs.c` | Convert a relative filesystem path to an absolute path |

### Third-Party

| File | Origin |
|------|--------|
| `abs2rel.c` / `abs2rel.h` | Shigio Yamaguchi / Tama Communications (GPLv3) |
| `rel2abs.c` / `rel2abs.h` | Shigio Yamaguchi / Tama Communications (GPLv3) |

---

## 3. Key Data Structures

### `string_t` — Dynamic String

```c
struct string_t {
    char *text;  // always heap-allocated, NUL-terminated
};
```

Created via `new_string*()` constructors, destroyed with `free_string()`. Every append reallocates the entire buffer (no grow factor). Supports printf-style formatting (`new_string_text`, `string_cat_text`), typed appends for integers and doubles, and conditional file/zlib I/O.

### `buffer_t` — Binary Byte Buffer

```c
struct buffer_t {
    uint8_t *buf;
    int size;       // allocated capacity
    int writepos;   // current write offset
    int readpos;    // current read offset
};
```

Sequential write-then-read model for network packet construction and parsing. Grows in 4096-byte granularity. Strings are stored NUL-terminated when written via `buffer_cat_string()`, but not via `buffer_cat_text()`. All multi-byte integers are in host byte order. Read underflow returns `BUF_EOB` (-1 cast to the return type).

### `vertex_t` — Fixed 2D Point

```c
struct vertex_t {
    float x, y;
};
```

### `vertex_ll_t` — Linked-List 2D Point

```c
struct vertex_ll_t {
    float x, y;
    struct vertex_ll_t *next;
};
```

Used for arbitrary-vertex polygons where the fixed 8-vertex limit of `polygon_t` is too restrictive.

### `polygon_t` — Fixed-Size Convex Polygon

```c
struct polygon_t {
    int numverts;
    vertex_t vertex[8];  // max 8 vertices
};
```

Vertices must be in clockwise order. Clipping is destructive (modifies the input in-place).

### `llist.h` — Intrusive Linked List Macros

Each node struct must have a `next` pointer. Key macros:

| Macro | Action |
|-------|--------|
| `LL_ALLOC(type, head, node)` | `malloc` and prepend |
| `LL_CALLOC(type, head, node)` | `calloc` and prepend |
| `LL_CALLOC_TAIL(type, head, node)` | `calloc` and append |
| `LL_ADD(type, head, node)` | `memcpy` a value node and prepend |
| `LL_ADD_TAIL(type, head, node)` | `memcpy` and append to tail |
| `LL_REMOVE(type, head, node)` | Remove a specific node and free |
| `LL_REMOVE_ALL(type, head)` | Free entire list |
| `LL_NEXT(node)` | Advance iterator |

---

## 4. Dependency Graph

```
types.h            vertex.h           llist.h            minmax.h
    |                  |                  |                  |
    |                  v                  v                  |
    |            polygon.h/c --------> polygon.c            |
    |                                                      |
    v                                                      v
stringbuf.h/c <---------------------------------------- buffer.h/c
    ^                  ^
    |                  |
resource.h/c      user.h/c
inout.h/c           abs2rel.h/c         rel2abs.h/c
  (standalone)        (standalone)        (standalone)
```

Detailed include relationships:

- `buffer.c` includes `stringbuf.h`, `buffer.h`, `minmax.h`
- `polygon.c` includes `vertex.h`, `polygon.h`, `llist.h`
- `resource.c` includes `stringbuf.h`
- `user.c` includes `types.h`, `stringbuf.h` (conditionally `console.h`)
- `inout.c`, `abs2rel.c`, `rel2abs.c` — no project-internal includes

---

## 5. Build & Consumption Model

`common/` is consumed from the repository's top-level Meson project. `common/meson.build` declares the shared source list once, then the various sub-projects compile the parts they need into static libraries.

### Linking

Each final binary links one of the Meson-built static libraries:

| Binary | Link chain |
|--------|------------|
| `em-client` | `libcommon_game.a` ← `libshared_client.a` ← `libgsub_game.a` ← client objects |
| `em-server` | `libcommon_game.a` ← `libshared_server.a` ← server objects |
| `em-edit` | `libcommon_tools.a` ← `libgsub_tools.a` ← editor objects |
| `em-skin` | `libcommon_tools.a` ← `libgsub_misc.a` ← skin packager objects |
| `plasma` | `libcommon_misc.a` ← `libgsub_misc.a` ← generator objects |
| `shield` | `libcommon_misc.a` ← `libgsub_misc.a` ← generator objects |

### Compile Flags

| Sub-project | Flags applied to `common/` |
|-------------|---------------------------|
| `em-game` | `-DLINUX -DEMGAME -Wall` |
| `em-tools` | `-DLINUX -Wall` |
| `misc` | `-DLINUX` (builds a reduced source set, omitting `user.c`, `prefix.c`, `resource.c`) |

---

## 6. Design Patterns & Conventions

### Constructor / Destructor

Every heap-allocated type follows `new_X()` / `free_X()` naming. All destructors are NULL-safe (early return on NULL argument). No reference counting or ownership transfer — callers always own and must free.

### NULL-Safety

All public `free_*()`, `buffer_cat_*()`, and `string_cat_*()` functions check for NULL arguments and return early. This is a pervasive pattern across the library.

### Buffer Protocol (Network Serialization)

`buffer_t` uses a sequential write-then-read position model. All multi-byte values are stored in **host byte order** with no endian conversion, meaning the protocol is only correct between same-endian machines (in practice, x86-only). Strings are NUL-terminated when written via `buffer_cat_string()` but not via `buffer_cat_text()` — this distinction is part of the network protocol.

### Destructive Polygon Clipping

`poly_clip()` and `poly_line_clip()` modify the input polygon in-place. All polygons must be in clockwise vertex order. Area is computed via the shoelace formula.

### Conditional Compilation

| Flag | Effect |
|------|--------|
| `LINUX` | Linux-specific code paths, `asprintf` availability |
| `WIN32` | Windows type aliases (`unsigned __int64`), `itoa` fallbacks |
| `EMGAME` | Game-specific behavior (console logging in `user.c`) |
| `PKGDATADIR` | Compile-time install location used by `find_resource()` |
| `_GNU_SOURCE` | Enables GNU extensions (e.g., `vasprintf`) — defined at the top of most `.c` files |
| `_REENTRANT` | Enables reentrant/thread-safe libc functions — defined at the top of most `.c` files |

---

## 7. Third-Party Code

### `prefix.h` / `prefix.c`

Now a tiny internal path-helper module. It retains only portable string/path routines (`br_strcat`, `br_extract_dir`, `br_extract_prefix`) used by the rest of the tree; the old BinReloc `/proc/self/maps` logic is no longer present.

### `abs2rel.c` / `rel2abs.c`

By Shigio Yamaguchi, Tama Communications. GPLv3. Provide bidirectional absolute ↔ relative path conversion. Used exclusively by the map editor (`em-edit/map.c`) for resolving map asset paths. Use K&R-style function definitions and report errors via `errno`.

---

## 8. Known Limitations

- **No endian conversion in `buffer_t`.** Multi-byte values are stored in host byte order. The network protocol is only correct between same-endian machines.
- **`string_t` reallocates on every append.** No grow factor or buffer reuse — each `string_cat_*` call does `malloc` + `strcpy` + `free`. Inefficient for many small concatenations.
- **`find_resource()` returns a pointer to a static buffer.** Not thread-safe; the returned pointer is invalidated on the next call. Acknowledged in the source.
- **Unchecked `malloc`/`calloc`/`realloc` returns.** Allocation failures are not detected; NULL dereferences will follow on out-of-memory.
- **`string_cat_uint64()` is incomplete.** The Linux code path is commented out; the function will not produce correct output for 64-bit unsigned values on Linux.
- **`inout()` acknowledges a better algorithm exists.** The current implementation handles edge orientations explicitly rather than using a general cross-product test.
