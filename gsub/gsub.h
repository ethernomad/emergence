/**
 * @file
 * @copyright 1998-2004 Jonathan Brown <jbrown@bluedroplet.com>
 * @license https://www.gnu.org/licenses/gpl-3.0.html
 * @homepage https://github.com/bluedroplet/emergence
 */

#ifndef _INC_GSUB
#define _INC_GSUB


#define SURFACE_ALPHA8BIT			0
#define SURFACE_16BIT				1
#define SURFACE_16BITALPHA8BIT		2
#define SURFACE_24BIT				3
#define SURFACE_24BITALPHA8BIT		4
#define SURFACE_24BITPADDING8BIT	5
#define SURFACE_FLOATS				6
#define SURFACE_ALPHAFLOATS 		7
#define SURFACE_FLOATSALPHAFLOATS	8

/*
#define SURFACE_A8		0
#define SURFACE_565		1
#define SURFACE_565A8	2
#define SURFACE_888		3
#define SURFACE_888A8	4
#define SURFACE_888P8	5
#define SURFACE_fff		6
#define SURFACE_Af 		7
#define SURFACE_fffAf	8
*/

struct surface_t
{
	int flags;
	int pitch, alpha_pitch;
	int width, height;
	uint8_t *buf;
	uint8_t *alpha_buf;
};


struct blit_params_t
{
	uint8_t red, green, blue;
	uint8_t alpha;
	
	struct surface_t *source;
	int source_x;
	int source_y;
	
	struct surface_t *dest;
		
	union
	{
		int dest_x;
		int x1;
	};
	
	union
	{
		int dest_y;
		int y1;
	};
	
	union
	{
		int width;
		int x2;
	};
	
	union
	{
		int height;
		int y2;
	};
};

	
void *get_pixel_addr(struct surface_t *suface, int x, int y);
void *get_alpha_pixel_addr(struct surface_t *suface, int x, int y);


struct surface_t *new_surface(int flags, int width, int height);
struct surface_t *new_surface_no_buf(int flags, int width, int height);
void clear_surface(struct surface_t *surface);
void convert_surface_to_alpha8bit(struct surface_t *surface);
void convert_surface_to_16bit(struct surface_t *in);
void convert_surface_to_16bitalpha8bit(struct surface_t *surface);
void convert_surface_to_24bit(struct surface_t *surface);
void convert_surface_to_24bitalpha8bit(struct surface_t *surface);
void convert_surface_to_floats(struct surface_t *in);
struct surface_t *duplicate_surface(struct surface_t *in);
struct surface_t *duplicate_surface_to_24bit(struct surface_t *in);
void surface_flip_horiz(struct surface_t *surface);
void surface_flip_vert(struct surface_t *surface);
void surface_rotate_left(struct surface_t *surface);
void surface_rotate_right(struct surface_t *surface);
void surface_slide_horiz(struct surface_t *surface, int pixels);
void surface_slide_vert(struct surface_t *surface, int pixels);
void free_surface(struct surface_t *surface);

struct surface_t *read_png_surface(char *filename);
struct surface_t *read_png_surface_as_16bit(char *filename);
struct surface_t *read_png_surface_as_16bitalpha8bit(char *filename);
struct surface_t *read_png_surface_as_24bitalpha8bit(char *filename);
struct surface_t *read_png_surface_as_floats(char *filename);
struct surface_t *read_png_surface_as_floatsalphafloats(char *filename);
	
#ifdef USE_GDK_PIXBUF
struct surface_t *read_gdk_pixbuf_surface(char *filename);
struct surface_t *read_gdk_pixbuf_surface_as_16bit(char *filename);
struct surface_t *read_gdk_pixbuf_surface_as_16bitalpha8bit(char *filename);
struct surface_t *read_gdk_pixbuf_surface_as_24bitalpha8bit(char *filename);
struct surface_t *read_gdk_pixbuf_surface_as_floats(char *filename);
struct surface_t *read_gdk_pixbuf_surface_as_floatsalphafloats(char *filename);
#endif

struct surface_t *read_raw_surface(char *filename);
int write_png_surface(struct surface_t *surface, char *filename);
int write_raw_surface(struct surface_t *surface, char *filename);


#include <zlib.h>

struct surface_t *gzread_raw_surface(gzFile file);
int gzwrite_raw_surface(gzFile file, struct surface_t *surface);


extern int (*gsub_callback)();



#if defined(__i386__)
void fb_update_mmx(void*, void*, int, int, int) __attribute__ ((cdecl));
void bb_clear_mmx(void*, int, int) __attribute__ ((cdecl));
void convert_16bit_to_32bit_mmx(void*, void*, int) __attribute__ ((cdecl));
#elif defined(__x86_64__)
void fb_update_mmx(void*, void*, int, int, int);
void bb_clear_mmx(void*, int, int);
void convert_16bit_to_32bit_mmx(void*, void*, int);
#endif

void init_gsub();
void kill_gsub();



extern uint8_t *vid_alphalookup;
extern uint16_t *vid_graylookup;
extern uint16_t *vid_redalphalookup;
extern uint16_t *vid_greenalphalookup;
extern uint16_t *vid_bluealphalookup;



// blit_ops.cpp



void plot_pixel(struct blit_params_t *params);
void alpha_plot_pixel(struct blit_params_t *params);
void draw_rect(struct blit_params_t *params);
void alpha_draw_rect(struct blit_params_t *params);
void blit_surface(struct blit_params_t *params);
void blit_partial_surface(struct blit_params_t *params);
void alpha_blit_surface(struct blit_params_t *params);
void alpha_blit_partial_surface(struct blit_params_t *params);


// text.cpp

void init_text();


int blit_text(int x, int y, uint8_t red, uint8_t green, uint8_t blue, 
	struct surface_t *dest, const char *fmt, ...);
int blit_text_centered(int x, int y, uint8_t red, uint8_t green, uint8_t blue, 
	struct surface_t *dest, const char *fmt, ...);
int blit_text_right_aligned(int x, int y, uint8_t red, uint8_t green, uint8_t blue, 
	struct surface_t *dest, const char *fmt, ...);



// line.cpp

void draw_line(struct blit_params_t *params);
	


struct surface_t *resize(struct surface_t *src_texture, int dst_width, int dst_height);
struct surface_t *rotate_surface(struct surface_t *in_surface, 
	int scale_width, int scale_height, double theta);


#ifdef _INC_VERTEX

struct texture_verts_t
{
	struct surface_t *surface;
	struct vertex_t *verts;
	struct texture_verts_t *next;
};


struct texture_polys_t
{
	struct surface_t *surface;
	struct vertex_ll_t **polys;
	struct texture_polys_t *next;
};


struct surface_t *multiple_resample(struct texture_verts_t *texture_verts0, 
	struct texture_polys_t *texture_polys0, 
	int dst_width, int dst_height, int dst_posx, int dst_posy);

struct surface_t *resample(float *src_pixels, struct vertex_t *src_verts, 
	int src_width, int src_height, int dst_width, int dst_height);


#endif	// _INC_VERTEX

uint16_t convert_24bit_to_16bit(uint8_t red, uint8_t green, uint8_t blue);
//word convert_doubles_to_16bit(double red, double green, double blue);



#define convert_doubles_to_16bit(red, green, blue) \
((((uint16_t)(lround((red) * 31.0))) << 11) | \
((((uint16_t)(lround((green) * 63.0))) & 0x3f) << 5) | \
(((uint16_t)(lround((blue) * 31.0))) & 0x1f))

#define convert_double_to_8bit(alpha) ((uint8_t)lround((alpha) * 255.0))

#define get_double_red(in) ((double)((in) >> 11) / 31.0)
#define get_double_green(in) ((double)(((in) >> 5) & 0x3f) / 63.0)
#define get_double_blue(in) ((double)((in) & 0x1f) / 31.0)
#define get_double_from_8(in) ((double)in / 255.0)

#define get_float_red(in) ((float)((in) >> 11) / 31.0f)
#define get_float_green(in) ((float)(((in) >> 5) & 0x3f) / 63.0f)
#define get_float_blue(in) ((float)((in) & 0x1f) / 31.0f)
#define get_float_alpha(in) ((float)(in) / 255.0f)

#endif	// _INC_GSUB
