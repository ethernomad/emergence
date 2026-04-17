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
| `prefix.h` / `prefix.c` | BinReloc — runtime discovery of the executable's install prefix |
| `abs2rel.h` / `abs2rel.c` | Convert an absolute filesystem path to a relative path |
| `rel2abs.h` / `rel2abs.c` | Convert a relative filesystem path to an absolute path |

### Third-Party

| File | Origin |
|------|--------|
| `prefix.h` / `prefix.c` | BinReloc by Mike Hearn & Hongli Lai (public domain) |
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
    ^
    |
prefix.h/c

inout.h/c           abs2rel.h/c         rel2abs.h/c
  (standalone)        (standalone)        (standalone)
```

Detailed include relationships:

- `buffer.c` includes `stringbuf.h`, `buffer.h`, `minmax.h`
- `polygon.c` includes `vertex.h`, `polygon.h`, `llist.h`
- `resource.c` includes `stringbuf.h`, `prefix.h`
- `user.c` includes `types.h`, `stringbuf.h` (conditionally `console.h`)
- `inout.c`, `abs2rel.c`, `rel2abs.c` — no project-internal includes

---

## 5. Build & Consumption Model

`common/` has no `configure.in` or top-level `Makefile.am`. It is never built as an independent sub-project. Instead, each consuming sub-project builds its own `libcommon.a`:

### Symlink Pattern

Each sub-project contains a `common/Makefile.am` that declares the source list and creates symlinks back to this directory:

```makefile
noinst_LIBRARIES = libcommon.a
libcommon_a_SOURCES = buffer.c polygon.c stringbuf.c ...
BUILT_SOURCES = $(libcommon_a_SOURCES)
CLEANFILES = $(libcommon_a_SOURCES)

$(libcommon_a_SOURCES):
    ln -s ../../common/$@
```

This ensures a single canonical copy of each source file while allowing each sub-project to compile with its own flags.

### Linking

Each final binary links `libcommon.a` via `LDADD`:

| Binary | Link chain |
|--------|------------|
| `em-client` | `libcommon.a` ← `libshared.a` ← `libgsub.a` ← client objects |
| `em-server` | `libcommon.a` ← `libshared.a` ← server objects |
| `em-edit` | `libcommon.a` ← `libgsub.a` ← editor objects |
| `em-skin` | `libcommon.a` ← `libgsub.a` ← skin packager objects |
| `plasma` | `libcommon.a` ← `libgsub.a` ← generator objects |
| `shield` | `libcommon.a` ← `libgsub.a` ← generator objects |

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
| `ENABLE_BINRELOC` | Activates BinReloc `/proc/self/maps` scanning in `prefix.c` |
| `_GNU_SOURCE` | Enables GNU extensions (e.g., `vasprintf`) — defined at the top of most `.c` files |
| `_REENTRANT` | Enables reentrant/thread-safe libc functions — defined at the top of most `.c` files |

---

## 7. Third-Party Code

### BinReloc (`prefix.h` / `prefix.c`)

By Mike Hearn and Hongli Lai. Public domain. Provides runtime discovery of the executable's installation prefix by scanning `/proc/self/maps`. When `ENABLE_BINRELOC` is defined, the macros `SELFPATH`, `PREFIX`, `BINDIR`, `DATADIR`, and `LIBDIR` resolve to the actual installation paths. Without it, the functions fall back to portable directory/prefix extraction helpers.

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
