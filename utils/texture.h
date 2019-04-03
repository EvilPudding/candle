#ifndef TEXTURE_H
#define TEXTURE_H

#include "macros.h"
#include "mafs.h"

#define MAX_MIPS 9
typedef struct
{
	ivec2_t pos;
	uvec2_t size;
} probe_tile_t;

typedef struct
{
	uint16_t cache_tile;
	uint8_t mip;
} tex_cache_location_t;

typedef struct tex_tile tex_tile_t;
typedef struct texture_t texture_t;

typedef struct tex_tile
{
	uint32_t bound;
	uint32_t touched;
	uint32_t indir_x;
	uint32_t indir_y;
	uint32_t bytes[129 * 129];

	uint32_t x;
	uint32_t y;
	tex_cache_location_t location;
	tex_tile_t *loaded_mip;
	texture_t *tex;
} tex_tile_t;

typedef struct
{
	uint32_t id;
	uint32_t fb_ready;
	int dims;
	uint32_t format;
	uint32_t internal;
	uint32_t type;
	char *name;
	uint8_t *data;
	uint32_t ready;

	tex_tile_t *mips[MAX_MIPS];
	uint32_t num_tiles_x[MAX_MIPS];
	uint32_t num_tiles_y[MAX_MIPS];
	tex_tile_t *tiles;
	int indir_n;
} buffer_t;

typedef struct texture_t
{
	char name[256];
	int id;

	uint32_t width;
	uint32_t height;
	uint32_t depth;
	uint32_t depth_buffer;
	void *last_depth;
	uint32_t repeat;

	buffer_t bufs[16]; /* 0 is depth, 1 is color, >= 2 are color buffers */

	int32_t bufs_size;

	float brightness;
	uint32_t track_brightness; 
	uint32_t target;

	uint32_t frame_buffer[MAX_MIPS];
	uvec2_t sizes[MAX_MIPS];

	char *filename;
	int32_t framebuffer_ready;
	int32_t draw_id;
	int32_t prev_id;
	int32_t mipmaped;
	int32_t interpolate;
	int32_t loading;
	bool_t sparse_it;
} texture_t;

texture_t *texture_from_file(const char *filename);
texture_t *texture_from_memory(void *buffer, int32_t len);
int32_t texture_load(texture_t *self, const char *filename);
int32_t texture_load_from_memory(texture_t *self, void *buffer, int32_t len);
int32_t texture_save(texture_t *self, int id, const char *filename);
texture_t *texture_from_buffer(void *buffer, int32_t width, int32_t height,
		int32_t Bpp);
#define TEX_MIPMAP			(1 << 0x01)
#define TEX_REPEAT			(1 << 0x02)
#define TEX_INTERPOLATE		(1 << 0x03)
#define TEX_SPARSE			(1 << 0x04)

texture_t *_texture_new_2D_pre(uint32_t width, uint32_t height, uint32_t flags);

extern __thread texture_t *_g_tex_creating;
#define texture_new_2D(w, h, f, ...) \
	(_texture_new_2D_pre(w, h, f),_texture_new(0, ##__VA_ARGS__))

texture_t *_texture_new(int32_t ignore, ...);

texture_t *texture_new_3D(uint32_t width, uint32_t height,
                          uint32_t depth, int32_t dims);

texture_t *texture_cubemap(uint32_t width, uint32_t height,
                           uint32_t depth_buffer);

int32_t texture_2D_resize(texture_t *self, int32_t width, int32_t height);

void texture_bind(texture_t *self, int32_t tex);

int32_t texture_target(texture_t *self, texture_t *depth, int32_t fb);
int32_t texture_target_sub(texture_t *self,
		texture_t *depth, int32_t fb, int32_t x, int32_t y,
		int32_t width, int32_t height);

void texture_destroy(texture_t *self);
uint32_t load_tile(texture_t *self, uint32_t mip, uint32_t x, uint32_t y,
                 uint32_t frame, uint32_t max_loads);

void texture_set_xy(texture_t *self, int32_t x, int32_t y,
		uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void texture_set_xyz(texture_t *self, int32_t x, int32_t y, int32_t z,
		uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void texture_update_gl(texture_t *self);

void texture_update_brightness(texture_t *self);
uint32_t texture_get_pixel(texture_t *self, int32_t buffer, int32_t x, int32_t y,
		float *depth);

void texture_draw_id(texture_t *self, int32_t tex);

int32_t buffer_new(const char *name, int32_t is_float, int32_t dims);

void textures_reg(void);
void svp_init(void);
void svp_destroy(void);

#endif /* !TEXTURE_H */
