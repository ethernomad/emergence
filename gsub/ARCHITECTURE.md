# gsub Architecture

gsub is the software 2D graphics subsystem for Emergence. It provides pixel-format-agnostic surface management, alpha compositing, drawing primitives, image I/O, and area-weighted resampling/rotation. All rendering is CPU-side with no GPU or SDL surface dependency; the library operates on raw byte buffers.

## Surface System

The central data structure is `surface_t` (`gsub.h:34`):

```c
struct surface_t {
    int flags;          // pixel format identifier
    int pitch;          // bytes per scanline (pixel data)
    int alpha_pitch;    // bytes per scanline (alpha data)
    int width, height;
    uint8_t *buf;       // pixel buffer
    uint8_t *alpha_buf; // separate alpha channel buffer (may be NULL)
};
```

Pixel and alpha data are stored in separate buffers. The `pitch` field allows scanlines to have padding (e.g., for 32-bit alignment). `get_pixel_addr()` and `get_alpha_pixel_addr()` compute addresses using `pitch`/`alpha_pitch` rather than assuming `width * bpp`, enabling sub-buffer views and aligned framebuffers.

### Pixel Formats

Defined as constants in `gsub.h:12-20`:

| Constant | Value | Pixel buffer | Alpha buffer |
|---|---|---|---|
| `SURFACE_ALPHA8BIT` | 0 | none | 1 byte/pixel |
| `SURFACE_16BIT` | 1 | RGB565 (2 bytes/pixel) | none |
| `SURFACE_16BITALPHA8BIT` | 2 | RGB565 (2 bytes/pixel) | 1 byte/pixel |
| `SURFACE_24BIT` | 3 | RGB888 (3 bytes/pixel) | none |
| `SURFACE_24BITALPHA8BIT` | 4 | RGB888 (3 bytes/pixel) | 1 byte/pixel |
| `SURFACE_24BITPADDING8BIT` | 5 | RGBX8888 (4 bytes/pixel) | none |
| `SURFACE_FLOATS` | 6 | 3 floats/pixel (RGB) | none |
| `SURFACE_ALPHAFLOATS` | 7 | none | 1 float/pixel |
| `SURFACE_FLOATSALPHAFLOATS` | 8 | 3 floats/pixel (RGB) | 1 float/pixel |

RGB data is stored in BGR memory order (blue at lowest address). The pitch helpers (`get_pitch`, `get_alpha_pitch`, `get_size`, `get_alpha_size`) in `surfaces.c:220-305` compute buffer sizes and strides for each format.

### Surface Lifecycle

- **Creation**: `new_surface()` allocates pixel and alpha buffers; `new_surface_no_buf()` allocates the struct only (used for framebuffers backed by external memory).
- **Destruction**: `free_surface()` frees both buffers and the struct.
- **Clearing**: `clear_surface()` zeroes-fill both buffers, handling pitch != `width * bpp` correctly.
- **Conversion**: `convert_surface_to_*()` functions transform between formats in-place (allocating new buffers, copying, freeing old ones). Supported conversions include 24-bit to 16-bit, 24-bit to floats, floats+alpha to 24-bit+alpha, etc.
- **Duplication**: `duplicate_surface()` deep-copies; `duplicate_surface_to_24bit()` copies with format conversion.
- **Transforms**: `surface_flip_horiz()`, `surface_flip_vert()`, `surface_rotate_left()`, `surface_rotate_right()`, `surface_slide_horiz()`, `surface_slide_vert()`.

## Alpha Compositing

gsub uses **precomputed lookup tables** instead of per-pixel division. This avoids the expensive `result = src * alpha + dst * (255 - alpha) / 255` computation in favor of table lookups.

### Lookup Tables

Built by `make_lookup_tables()` in `gsub.c:60-167`:

| Table | Size | Index | Purpose |
|---|---|---|---|
| `vid_alphalookup` | 65536 bytes | `(a << 8) \| b` | `a * b / 255` (8-bit alpha multiply) |
| `vid_graylookup` | 512 bytes | gray value | 16-bit gray RGB565 value |
| `vid_redalphalookup` | 16384 bytes | `(r5 << 8) \| alpha` | Red channel alpha blend result (5-bit, shifted to RGB565 position) |
| `vid_greenalphalookup` | 32768 bytes | `(g6 << 8) \| alpha` | Green channel alpha blend result (6-bit, shifted) |
| `vid_bluealphalookup` | 16384 bytes | `(b5 << 8) \| alpha` | Blue channel alpha blend result (5-bit, in position) |

Division uses banker's rounding: the remainder `q = r % 255` is compared to 128; values >= 128 round up.

### Blend Formula

For 8-bit alpha blending (24-bit/32-bit destinations):

```
result = vid_alphalookup[src << 8 | alpha] + vid_alphalookup[dst << 8 | ~alpha]
```

For 16-bit (RGB565) destinations, per-channel tables are used:

```
result = redalphalookup[r5 << 3 | alpha] + redalphalookup[old_r5 << 3 | ~alpha]
       | greenalphalookup[g6 << 3 | alpha] + greenalphalookup[old_g6 << 3 | ~alpha]
       | bluealphalookup[b5 << 8 | alpha] + bluealphalookup[old_b5 << 8 | ~alpha]
```

## Blit Pipeline

### blit_params_t

All drawing operations use `struct blit_params_t` (`gsub.h:44-78`):

```c
struct blit_params_t {
    uint8_t red, green, blue;   // solid color for rect/pixel operations
    uint8_t alpha;              // global transparency (255 = opaque)
    struct surface_t *source;   // source surface (for blits)
    int source_x, source_y;
    struct surface_t *dest;     // destination surface
    union { int dest_x; int x1; };  // dual-use: blit dest or line start
    union { int dest_y; int y1; };
    union { int width;  int x2; };   // dual-use: blit dims or line end
    union { int height; int y2; };
};
```

The union fields allow `blit_params_t` to serve both rect/surface blitting and line drawing.

### Dispatch Architecture

The blit operation dispatches through a two-level switch on `(source_format, dest_format)`. For each combination that's supported, a format-specific C function is called (e.g., `surface_alpha_blit_888A8_565_c` for blitting a 24-bit+alpha source onto a 16-bit destination with global alpha).

For performance-critical paths (rect fill, alpha rect, surface blit, alpha surface blit, pixel alpha plot), **function pointers** default to the C implementations but can be swapped to MMX/x86 assembly versions at runtime:

```c
// blit_ops.c:887-926
void (*pixel_alpha_plot_565)(...) = pixel_alpha_plot_565_c;
void (*rect_draw_888P8)(...)    = rect_draw_888P8_c;
void (*surface_blit_565_565)(...) = surface_blit_565_565_c;
// etc.
```

The top-level API functions (`plot_pixel`, `alpha_plot_pixel`, `draw_rect`, `alpha_draw_rect`, `surface_blit`, `surface_alpha_blit`) are format-agnostic dispatchers that switch on `dest->flags` (and `source->flags` for blits) to call the right format-specific function.

### Clipping

`clip_blit_coords()` (`blit_ops.c:929-959`) adjusts `source_x`, `source_y`, `dest_x`, `dest_y`, `width`, and `height` to clip the blit rectangle against the destination surface bounds. Negative source coordinates and overflow are handled by shrinking the rectangle.

### Public API

| Function | Purpose |
|---|---|
| `blit_surface()` | Copy entire source to dest at (dest_x, dest_y) |
| `blit_partial_surface()` | Copy a sub-rectangle of source to dest |
| `alpha_blit_surface()` | Like `blit_surface()` with global alpha |
| `alpha_blit_partial_surface()` | Like `blit_partial_surface()` with global alpha |

## Drawing Primitives

### Pixel and Rect

- `plot_pixel()` / `alpha_plot_pixel()`: Single-pixel operations dispatching on dest format.
- `draw_rect()` / `alpha_draw_rect()`: Filled rectangles with optional alpha blending.

### Line Drawing

`line.c` implements Bresenham's line algorithm with run-slicing optimization. Three format-specific implementations exist:

- `draw_line_888P8()` — 32-bit (24-bit with 8-bit padding) destination
- `draw_line_888()` — 24-bit destination
- `draw_line_565()` — 16-bit (RGB565) destination

The top-level `draw_line()` (`line.c:502-583`) clips coordinates against the destination surface bounds, then dispatches to the appropriate format-specific function.

### Text Rendering

`text.c` provides a bitmap font system. `init_text()` loads `smallfont.png` (a monospaced 8-pixel-wide font atlas) and populates a per-character width table (`charlengths[256]`).

Three rendering functions:
- `blit_text()` — left-aligned
- `blit_text_centered()` — horizontally centered
- `blit_text_right_aligned()` — right-aligned

Each character is blitted as a partial surface from the font atlas using `blit_partial_surface()`. The alpha channel in the font atlas provides anti-aliasing.

## Resampling and Rotation

### Area-Weighted Resampling

`resample.c` implements mathematically correct area-weighted resampling (not bilinear or nearest-neighbor). Each destination pixel's value is the weighted average of all source pixels it overlaps, weighted by the overlap area.

Four format-specific implementations:
- `resize_a8()` — alpha-only surfaces
- `resize_888()` — 24-bit RGB surfaces
- `resize_565()` — 16-bit RGB565 surfaces
- `resize_888a8()` — 24-bit RGB + 8-bit alpha (premultiplied alpha)

The top-level `resize()` dispatches on source format. All implementations support a `gsub_callback()` mechanism for long-running operations to be cancelled.

### Rotation

`rotate_surface()` performs scale + rotate in one pass. It maps each source pixel as a quad (four vertices) through a rotation+scale transformation, clips against destination pixel quads, and computes overlap areas. Uses the `polygon` library (`common/polygon.c`) for polygon clipping. Output is in `SURFACE_FLOATSALPHAFLOATS` format and then converted to `SURFACE_24BITALPHA8BIT` before return.

### Multi-Texture Resampling

`multiple_resample()` composites multiple textures with per-pixel vertex mapping onto a single destination surface. This is used for texture mapping where source pixels may be warped by arbitrary transformations. Two input chains are supported:
- `texture_verts_t` — vertex-mapped textures (each pixel is a quad)
- `texture_polys_t` — arbitrarily-polygon-mapped textures

## Surface I/O

### PNG Loading

`read_png_surface()` reads PNG files via libpng, producing surfaces in their native format:
- RGB PNG → `SURFACE_24BIT`
- RGBA PNG → `SURFACE_24BITALPHA8BIT`
- Grayscale PNG → `SURFACE_ALPHA8BIT`

Convenience loaders (`read_png_surface_as_16bit`, `_as_16bitalpha8bit`, `_as_24bitalpha8bit`, `_as_floats`, `_as_floatsalphafloats`) load then convert in one call.

### PNG Writing

`write_png_surface()` writes `SURFACE_24BIT` or `SURFACE_ALPHA8BIT` surfaces to PNG.

### GdkPixbuf Support

Optionally compiled with `USE_GDK_PIXBUF`, `read_gdk_pixbuf_surface()` loads images through GdkPixbuf, supporting any format GdkPixbuf can handle (JPEG, TIFF, etc.).

### Raw Format

`read_raw_surface()` / `write_raw_surface()` store surfaces as flat binary: `{flags, width, height}` header followed by pixel and alpha buffers. Gzip variants (`gzread_raw_surface`, `gzwrite_raw_surface`) compress with zlib.

## Assembly Optimizations

Seven `.S` files provide x86/MMX hand-optimized routines:

| File | Function | Purpose |
|---|---|---|
| `alpha_pixel_plot_x86.S` | `alpha_pixel_plot_x86` | Single-pixel alpha blend (stub) |
| `alpha_rect_draw_mmx.S` | `alpha_rect_draw_mmx` | Filled rect with alpha using MMX |
| `alpha_surface_blit_mmx.S` | `alpha_surface_blit_mmx` | Full-surface alpha blit using MMX |
| `bb_clear_mmx.S` | `bb_clear_mmx` | Backbuffer clear using MMX |
| `convert_16bit_to_32bit_mmx.S` | `convert_16bit_to_32bit_mmx` | 16-to-32-bit pixel conversion |
| `fb_update_mmx.S` | `fb_update_mmx` | Framebuffer update/copy using MMX |
| `surface_blit_mmx.S` | `surface_blit_mmx` | Non-alpha surface blit using MMX |

These are declared with `__attribute__((cdecl))` in `gsub.h:135-137` and are intended to replace the C function pointers at runtime for MMX-capable CPUs. The C fallbacks (`_c` suffix) serve as the default implementations.

## Initialization and Teardown

```c
init_gsub()    // gsub.c:205 — builds lookup tables, loads font via init_text()
kill_gsub()    // gsub.c:212 — frees lookup tables
```

`gsub_callback` (`gsub.h:131`) is an optional function pointer that resampling operations call periodically; returning non-zero cancels the operation (surfaces are freed and NULL is returned).

## Dependencies

| Dependency | Usage |
|---|---|
| libpng | PNG image loading and saving |
| zlib | Gzip-compressed raw surface I/O |
| `common/stringbuf.h` | String buffer utility |
| `common/vertex.h` | Vertex types for resample/rotation |
| `common/polygon.h` | Polygon clipping for rotation |
| `common/llist.h` | Linked list for `multiple_resample` |
| `common/minmax.h` | `min()`/`max()` macros for resample clipping |
| `common/resource.h` | Resource path resolution (for font loading) |