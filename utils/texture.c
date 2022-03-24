#include "../utils/glutil.h"
#include "texture.h"
#include "file.h"
#include "../candle.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#ifdef THREADED
#include "../third_party/tinycthread/source/tinycthread.h"
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ASSERT(x)
#include "../third_party/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../third_party/stb_image_write.h"

static int32_t texture_cubemap_frame_buffer(texture_t *self);
static int32_t texture_2D_frame_buffer(texture_t *self);
static void texture_disk_cacher_write(texture_t *self, tex_tile_t *tile);

/* texture_t *fallback_depth; */
int32_t g_tex_num = 0;

GLenum attachments[16] = {
	GL_COLOR_ATTACHMENT0,
	GL_COLOR_ATTACHMENT1,
	GL_COLOR_ATTACHMENT2,
	GL_COLOR_ATTACHMENT3,
	GL_COLOR_ATTACHMENT4,
	GL_COLOR_ATTACHMENT5,
	GL_COLOR_ATTACHMENT6,
	GL_COLOR_ATTACHMENT7,
	GL_COLOR_ATTACHMENT8,
	GL_COLOR_ATTACHMENT9,
	GL_COLOR_ATTACHMENT10,
	GL_COLOR_ATTACHMENT11,
	GL_COLOR_ATTACHMENT12,
	GL_COLOR_ATTACHMENT13,
	GL_COLOR_ATTACHMENT14,
	GL_COLOR_ATTACHMENT15
};

struct tpair {
	texture_t *tex;
	int32_t i;
};

texture_t *g_cache;
uint32_t g_cache_frame;
texture_t *g_probe_cache;
tex_tile_t **g_cache_bound;
texture_t *g_indir;
tex_tile_t *g_tiles;
const int g_cache_w = 64;
const int g_cache_h = 32;
const int g_indir_w = 128;
const int g_indir_h = 128;
uint32_t g_cache_n = 0;
uint32_t g_indir_n = 1;

typedef struct
{
#ifdef THREADED
	thrd_t thread;
	mtx_t mtx;
#endif
	tex_tile_t *tile;
} tile_worker_t;

tile_worker_t g_tile_workers[8];
int load_tex_tile_worker(void *usrptr);

bool_t *g_probe_tiles;
uint32_t g_min_shadow_tile = 64;
uint32_t g_max_probe_levels;
uint32_t g_num_shadows;
#ifdef THREADED
mtx_t g_indir_mtx;
#endif

uint32_t get_sum_tiles(uint32_t level)
{
	return 2 * (pow(2, level) - 1);
}

uint32_t g_probe_resolution = 1024;
void svp_init()
{
	int32_t max;
	uint32_t t;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max);
	max = floorf(((float)max) / 129);
#ifdef THREADED
	mtx_init(&g_indir_mtx, mtx_plain);
#endif
	g_cache = texture_new_2D(g_cache_w * 129, g_cache_h * 129, TEX_INTERPOLATE, 1,
	                         buffer_new("color", false, 4));
	g_cache_bound = calloc(g_cache_w * g_cache_h, sizeof(*g_cache_bound));
	g_indir = texture_new_2D(g_indir_w, g_indir_h, 0, 1,
	                         buffer_new("indir", false, 3));
	g_tiles = malloc(g_indir_w * g_indir_h * sizeof(tex_tile_t));

	g_probe_cache = texture_new_2D(g_probe_resolution * 4, g_probe_resolution * 6, 0, 2,
		buffer_new("depth", false, -1),
		buffer_new("color", false, 4));

	g_max_probe_levels = log2(g_probe_resolution / g_min_shadow_tile) + 1;
	g_num_shadows = get_sum_tiles(g_max_probe_levels);
	g_probe_tiles = malloc(g_num_shadows * sizeof(g_probe_tiles));
	for (t = 0; t < g_num_shadows; t++)
	{
		g_probe_tiles[t] = false;
	}
#ifdef THREADED
	for (t = 0; t < sizeof(g_tile_workers) / sizeof(*g_tile_workers); t++)
	{
		mtx_init(&g_tile_workers[t].mtx, mtx_plain);
		mtx_lock(&g_tile_workers[t].mtx);
		thrd_create(&g_tile_workers[t].thread, load_tex_tile_worker,
		            &g_tile_workers[t]);
	}
#endif

	glerr();
}

uint32_t get_level_y(uint32_t level)
{
	return 3 * ((int)pow(2, 11 - level)) * ((int)pow(2, level) - 1);
}

uint32_t get_level_size(uint32_t level)
{
	return (g_probe_resolution / pow(2, level));
}

probe_tile_t get_free_tile(uint32_t level, uint32_t *result_level)
{
	probe_tile_t result = {0};
	do
	{
		const uint32_t num_tiles_at_level = 2 * pow(2, level);
		const uint32_t y = get_level_y(level);
		const uint32_t first_tile = get_sum_tiles(level);
		const uint32_t size = get_level_size(level);
		uint32_t i;
		for (i = 0; i < num_tiles_at_level; i++)
		{
			if (!g_probe_tiles[first_tile + i])
			{
				g_probe_tiles[first_tile + i] = true;
				if (result_level)
					*result_level = level;
				result.id = first_tile + i + 1;
				result.pos = ivec2(2 * size * i, y);
				result.size = uvec2(size, size);
				return result;
			}
		}
		if (num_tiles_at_level + first_tile > g_num_shadows)
		{
			*result_level = ~0;
			break;
		}
		level++;
	} while(true);

	return result;
}

void release_tile(probe_tile_t tile)
{
	if (tile.id > 0)
		g_probe_tiles[tile.id - 1] = false;
}

void svp_destroy()
{
	texture_destroy(g_cache);
	texture_destroy(g_indir);
	texture_destroy(g_probe_cache);
	free(g_tiles);
}

static struct tpair *pair(texture_t *tex, int id)
{
	struct tpair *p = malloc(sizeof(*p));
	p->tex = tex;
	p->i = id;
	return p;
}
#define ID_2D 0
#define ID_CUBE 1
#define ID_3D 2
static void texture_update_gl_loader(texture_t *self)
{

	if(self->target == GL_TEXTURE_2D && !self->sparse_it)
	{
		glActiveTexture(GL_TEXTURE0 + ID_2D);
		glBindTexture(self->target, self->bufs[0].id);

		glTexSubImage2D(self->target, 0, 0, 0, self->width, self->height,
				self->bufs[0].format, GL_UNSIGNED_BYTE, self->bufs[0].data);
		glerr();
	}
	if(self->target == GL_TEXTURE_3D && !self->sparse_it)
	{
		glActiveTexture(GL_TEXTURE0 + ID_3D);
		glBindTexture(self->target, self->bufs[0].id);

		glTexSubImage3D(self->target, 0, 0, 0, 0,
				self->width, self->height, self->depth,
				self->bufs[0].format, GL_UNSIGNED_BYTE, self->bufs[0].data);
		glerr();
	}
	glBindTexture(self->target, 0); glerr();
	glActiveTexture(GL_TEXTURE0 + 0);
	glerr();
}

void texture_update_gl(texture_t *self)
{
	loader_push(g_candle->loader, (loader_cb)texture_update_gl_loader, self,
			NULL);
}

int32_t texture_save(texture_t *self, int id, const char *filename)
{
	uint32_t dims;
	uint8_t *data;
	uint32_t res;

	if(!self->framebuffer_ready)
	{
		printf("Cannot save framebufferless texture (why would you want to?)\n");
		return 0;
	}
	glFlush();
	glFinish();

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, self->frame_buffer[0]); glerr();

	if(self->depth_buffer) id--;

	dims = self->bufs[id].dims;
	data = malloc(self->width * self->height * dims);
	stbi_flip_vertically_on_write(1);

	glReadBuffer(GL_COLOR_ATTACHMENT0 + id);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glReadPixels(0, 0, self->width, self->height, self->bufs[id].format,
			GL_UNSIGNED_BYTE, data);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	glerr();

	res = stbi_write_png(filename, self->width, self->height, dims, data, 0);
	free(data);
	return res;
}

uint32_t texture_get_pixel(texture_t *self, int32_t buffer, int32_t x, int32_t y,
		float *depth)
{
	uint32_t data = 0;
	if(!self->framebuffer_ready) return 0;
	y = self->height - y - 1;

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); glerr();
	glBindFramebuffer(GL_READ_FRAMEBUFFER, self->frame_buffer[0]); glerr();

	{
		glReadBuffer(GL_COLOR_ATTACHMENT0 + buffer); glerr();

		glPixelStorei(GL_UNPACK_ALIGNMENT, 1); glerr();

		glReadPixels(x, y, 1, 1, self->bufs[buffer + self->depth_buffer].format,
		             GL_UNSIGNED_BYTE, &data); glerr();
	}
	if(depth)
	{
		/* float fetch_depth = 0.988937f; */

		/* glReadBuffer(GL_NONE); glerr(); */

		/* glReadPixels(x, y, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &fetch_depth); glerr(); */

		/* *depth = fetch_depth; */
	}

	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	glerr();
	return data;
}

static void texture_update_sizes(texture_t *self)
{
	int m;
	self->sizes[0].x = self->width;
	self->sizes[0].y = self->height;
	for(m = 1; m < MAX_MIPS; m++)
	{
		int w = floor(((float)self->sizes[m - 1].x) / 2.0f);
		int h = floor(((float)self->sizes[m - 1].y) / 2.0f);
		/* int w2, h2; */
		/* glGetTexLevelParameteriv(GL_TEXTURE_2D, m, GL_TEXTURE_WIDTH, &w2); */
		/* glGetTexLevelParameteriv(GL_TEXTURE_2D, m, GL_TEXTURE_HEIGHT, &h2); */
		self->sizes[m] = uvec2(w, h);

	}
}

static int32_t alloc_buffer_gl(struct tpair *data)
{
	int32_t i = data->i;
	texture_t *self = data->tex;
	uint32_t wrap = self->repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE;
	int32_t width = self->width;
	int32_t height = self->height;

	if(!self->bufs[i].id)
	{
		glGenTextures(1, &self->bufs[i].id); glerr();
	}
	if(width == 0) width = 1;
	if(height == 0) height = 1;

	glActiveTexture(GL_TEXTURE0 + ID_2D);
	glBindTexture(self->target, self->bufs[i].id); glerr();
	glTexImage2D(self->target, 0, self->bufs[i].internal,
			width, height, 0, self->bufs[i].format,
			self->bufs[i].type, NULL); glerr();

	glTexParameteri(self->target, GL_TEXTURE_WRAP_S, wrap);
	glTexParameteri(self->target, GL_TEXTURE_WRAP_T, wrap);
	glerr();

	if(self->mipmaped)
	{
		glTexParameteri(self->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(self->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glGenerateMipmap(self->target); glerr();

		texture_update_sizes(self);
	}
	else
	{
		glTexParameteri(self->target, GL_TEXTURE_MAG_FILTER, self->interpolate ?
				GL_LINEAR : GL_NEAREST);
		glTexParameteri(self->target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glerr();
	}

	glBindTexture(self->target, 0); glerr();

	self->bufs[i].fb_ready = 0;
	glActiveTexture(GL_TEXTURE0);
	self->bufs[i].ready = 1;

	if(data) free(data);
	return 1;
}

void texture_alloc_buffer(texture_t *self, int32_t i)
{
	self->last_depth = NULL;
	self->framebuffer_ready = 0;
	loader_push(g_candle->loader, (loader_cb)alloc_buffer_gl, pair(self, i),
			NULL);
}

static uint32_t round_power_of_two(uint32_t v)
{
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	return v + 1;
}

uint32_t texture_num_tiles_per_row(const texture_t *self)
{
	float max_dim = self->width > self->height ? self->width : self->height;
	return round_power_of_two(ceil(((float)max_dim) / 128));
}

uint32_t texture_max_mips(texture_t *self)
{
	uint32_t num_tiles = texture_num_tiles_per_row(self);
	return log2(num_tiles);
}

uint32_t _mip_to_tiles(uint32_t mip)
{
	const uint32_t max_mip_tiles = (uint32_t)((pow(4, mip) - 1) / 3);
	const uint32_t pad_tiles = MAX_MIPS - mip - 1;
	return max_mip_tiles + pad_tiles;
}

uint32_t texture_mips_to_tiles(texture_t *self)
{
	return _mip_to_tiles(texture_max_mips(self) + 1);
}
void test(float mip)
{
	uint32_t offset = 0, i;
	uint32_t tiles_per_row = round_power_of_two(16);
	uint32_t tpr = tiles_per_row;
	float max_mips = log2(tiles_per_row);
	float mm = max_mips < mip ? max_mips : mip;

	uint32_t map_tiles = (pow(4.,max_mips-mm+1.) * (pow(4.,mm) - 1.)) / 3u;

	if (mip > max_mips)
	{
		map_tiles += mip - max_mips;
	}

	for (i = 0u; i < MAX_MIPS && i < mip; i++)
	{
		offset += tpr * tpr;
		if (tpr > 1u)
			tpr /= 2u;
	}
}

void set_tile_location(tex_tile_t *tile,
                       const tex_cache_location_t *location) 
{
	uint32_t mip = tile->mip;
	uint32_t x = tile->x;
	uint32_t y = tile->y;
	int32_t m;
	uint32_t tiles_per_row, yy, xx, tex_tile;

	if (   tile->location.x   == location->x
	    && tile->location.y   == location->y
	    && tile->location.mip == location->mip)
	{
		return;
	}
	tile->location.mip = location->mip;
	tile->location.x   = location->x;
	tile->location.y   = location->y;
	texture_bind(g_indir, 0);
	glTexSubImage2D(g_indir->target, 0, tile->indir_x, tile->indir_y,
					1, 1, GL_RGB, GL_UNSIGNED_BYTE, location);

	if (mip == 0) return;
	mip -= 1;
	x *= 2;
	y *= 2;
	m = texture_max_mips(tile->tex) - mip;
	if (m < 0) m = 0;
	tiles_per_row = (uint32_t)pow(2, m);

	for (yy = y; yy <= y + 1; yy++)
	{
		if (yy >= tiles_per_row)
			break;

		for (xx = x; xx <= x + 1; xx++)
		{
			tex_tile_t *mtile;
			if (xx >= tiles_per_row)
				break;

			tex_tile = yy * tiles_per_row + xx;
			mtile = &tile->tex->bufs[0].mips[mip][tex_tile];
			assert(mtile->bound == 0);
			set_tile_location(mtile, location);
		}
	}
}

static tex_tile_t *get_parent_tile(tex_tile_t *tile)
{
	return texture_get_tile(tile->tex, tile->mip + 1,
	                        tile->x / 2, tile->y / 2);
}

static uint32_t svt_get_free_tile(void)
{
	uint32_t cache_tile;
	if (g_cache_n == g_cache_w * g_cache_h)
	{
		uint32_t i;
		uint32_t oldest_touched = ~0;
		tex_tile_t *old_tile;
		tex_tile_t *parent_tile;
		cache_tile = ~0;
		for (i = 0; i < g_cache_n; i++)
		{
			uint32_t touched;
			if (g_cache_bound[i]->bound != 1) continue;
			assert(g_cache_bound[i]->bound);
			touched = g_cache_bound[i]->touched;
			if (touched < oldest_touched)
			{
				cache_tile = i;
				oldest_touched = touched;
			}
		}

		old_tile = g_cache_bound[cache_tile];
		g_cache_bound[cache_tile] = NULL;

		parent_tile = get_parent_tile(old_tile);
		if (parent_tile)
		{
			assert(parent_tile->bound > 1);
			parent_tile->bound--;
			set_tile_location(old_tile, &parent_tile->location);
		}
		old_tile->location.x = 0;
		old_tile->location.y = 0;
		old_tile->location.mip = 0;
		old_tile->bound = 0;
	}
	else
	{
		cache_tile = g_cache_n++;
	}
	return cache_tile;
}

tex_tile_t *texture_get_tile(texture_t *self, uint32_t mip,
                             uint32_t x, uint32_t y)
{
	uint32_t tiles_per_row;

	if (mip >= MAX_MIPS) return NULL;
	tiles_per_row = self->bufs[0].num_tiles[mip];
	if (!tiles_per_row) return NULL;
	return &self->bufs[0].mips[mip][y * tiles_per_row + x];
}

void load_tile_frame_inc()
{
	g_cache_frame++;
}

uint32_t _load_tile(tex_tile_t *tile, uint32_t max_loads)
{
	tex_tile_t *parent_tile;
	uint32_t loads;

	if (!tile) return 0;

	tile->touched = g_cache_frame;

	parent_tile = get_parent_tile(tile);
	loads = _load_tile(parent_tile, max_loads);
	max_loads -= loads;

	if (max_loads == 0) return 0;
	if (tile->bound) return 0;
	if (parent_tile && !parent_tile->bound)
	{
		return 0;
	}

	if (!tile->loaded && tile->tex->cacher)
	{
		assert(tile->tex->usrptr);
		if (!tile->loading)
		{
			tile->tex->cacher(tile->tex, tile->mip, tile->x, tile->y);
		}
		return 0;
	}

	if (parent_tile)
	{
		assert(parent_tile->bound <= 4);
		parent_tile->bound++;
	}

	{
		tex_cache_location_t new_location;
		const uint32_t cache_tile = svt_get_free_tile();

		assert(tile->bound == 0);
		tile->bound = 1;

		g_cache_bound[cache_tile] = tile;

		texture_bind(g_cache, 0);
		new_location.mip = (uint8_t)tile->mip;
		new_location.x = cache_tile % g_cache_w;
		new_location.y = cache_tile / g_cache_w;

		glTexSubImage2D(g_cache->target, 0, new_location.x * 129,
		                new_location.y * 129, 129, 129, g_cache->bufs[0].format,
		                GL_UNSIGNED_BYTE, tile->bytes); glerr();

		set_tile_location(tile, &new_location);
	}

	glerr();
	return 1;
}

uint32_t load_tile(texture_t *self, uint32_t mip, uint32_t x, uint32_t y,
                   uint32_t max_loads)
{
	assert(mip < MAX_MIPS);
	return _load_tile(texture_get_tile(self, mip, x, y), max_loads);
}

uint32_t load_tile_by_id(uint32_t tile, uint32_t max_loads)
{
	if (tile == 0) return 0;
	if (tile >= g_indir_n)
	{
		/* printf("could not load tile %d %d\n", tile, g_indir_n); */
		return 0;
	}
	return _load_tile(&g_tiles[tile], max_loads);
}

static void texture_tile_filename(texture_t *self, uint32_t mip,
                                  uint32_t x, uint32_t y, char *str,
                                  uint32_t n)
{
	const char *path = (char*)self->usrptr;
	char *dot = strrchr(path, '.');

	sprintf(str, "%.*s_CACHE_%u_%u_%u%s",
	         (int)(dot - path), path, mip, x, y, dot);
}

static
void wrapped_read(const uint32_t *input, uint32_t size_in,
                  uint32_t *output, uint32_t size_out, uint32_t i)
{
	while (size_out)
	{
		const uint32_t read_count = min(size_out, size_in - i);
		memcpy(output, input + i, read_count * sizeof(uint32_t));
		i = (i + read_count) % size_in;
		output += read_count;
		size_out -= read_count;
	}
}

static int32_t texture_from_file_loader(texture_t *self)
{
	const uint32_t format = self->bufs[0].format;
	const uint32_t type = self->bufs[0].type;
	int32_t tiles_x, m, y, x;
	uint32_t tile_i, i;
	uint32_t *buffer;

	if (self->target != GL_TEXTURE_2D)
	{
		printf("PROBLEM '%s'\n", self->name);
		return 0;
	}
	glActiveTexture(GL_TEXTURE0 + ID_2D);

	self->mipmaped = true;
	glGenTextures(1, &self->bufs[0].id); glerr();
	glBindTexture(self->target, self->bufs[0].id); glerr();
	glerr();
	glTexParameterf(self->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(self->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexParameteri(self->target, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(self->target, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glTexImage2D(self->target, 0, self->bufs[0].internal, self->width,
			self->height, 0, format, type, self->bufs[0].data); glerr();

	glGenerateMipmap(self->target); glerr();
	texture_update_sizes(self);

	glTexParameteri(self->target, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(self->target, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glerr();

	glBindTexture(self->target, 0); glerr();

	/* glPixelStorei(GL_PACK_ROW_LENGTH, 129); */
	/* glPixelStorei(GL_PACK_SKIP_PIXELS, 0); */
	/* glPixelStorei(GL_PACK_SKIP_ROWS, 0); */

	texture_target(self, NULL, 0); glerr();
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0); glerr();

	tiles_x = texture_num_tiles_per_row(self);
	tile_i = self->bufs[0].indir_n;

	buffer = malloc(sizeof(*buffer) * self->width * self->height);

	for (m = 0; m < MAX_MIPS; m++)
	{

		if (self->frame_buffer[m] > 200)
		{
			continue;
		}
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); glerr();
		glBindFramebuffer(GL_READ_FRAMEBUFFER, self->frame_buffer[m]); glerr();
		glReadBuffer(GL_COLOR_ATTACHMENT0); glerr();
		glReadPixels(0, 0, self->sizes[m].x, self->sizes[m].y,
		             format, GL_UNSIGNED_BYTE, buffer); glerr();

		for (y = 0; y < tiles_x; y++) for (x = 0; x < tiles_x; x++)
		{
			int tty;
			tex_tile_t *tilep;
			int bx = x * 128;
			int by = y * 128;
			uint32_t i;

			assert(tile_i < g_indir_w * g_indir_h);
			tilep = &g_tiles[tile_i++];
			for (i = 0; i < 129 * 129; ++i)
				tilep->bytes[i] = 0xFF0000FF;

			tilep->location.x = 0;
			tilep->location.y = 0;
			tilep->x = x;
			tilep->y = y;
			tilep->bound = 0;
			tilep->tex = self;

			if (   self->sizes[m].x > bx
			    && self->sizes[m].y > by) {
				/* for (tty = 0; tty < min(129, ((int)self->sizes[m].y) - by); ++tty) */
				/* { */
				/* 	memcpy(tilep->bytes + tty * 129, */
				/* 		   buffer + (by + tty) * self->sizes[m].x + bx, */
				/* 		   sizeof(uint32_t) * min(129, self->sizes[m].x - bx)); */
				/* } */
				/* DEBUG SVT */
				/* for (tty = 0; tty < min(129, ((int)self->sizes[m].y) - by); ++tty) { */
				/* 	uint32_t ttx; */
				/* 	for (ttx = 0; ttx < min(129, ((int)self->sizes[m].x) - bx); ++ttx) */
				/* 	{ */
				/* 		if (tty == 0) */
				/* 			tilep->bytes[tty * 129 + ttx] = 0xff00ff00; */
				/* 		if (tty + 1 == min(129, ((int)self->sizes[m].y) - by)) */
				/* 			tilep->bytes[tty * 129 + ttx] = 0xffff0000; */

				/* 		if (ttx == 0) */
				/* 			tilep->bytes[tty * 129 + ttx] = 0xff00ff00; */
				/* 		if (ttx + 1 == min(129, ((int)self->sizes[m].x) - bx)) */
				/* 			tilep->bytes[tty * 129 + ttx] = 0xffff0000; */
				/* 	} */
				/* } */
				/* wrapped read */
				for (tty = 0; tty < 129; ++tty)
				{
					const uint32_t wrapped_y = (by + tty) % self->sizes[m].y;
					wrapped_read(buffer + wrapped_y * self->sizes[m].x, self->sizes[m].x,
					             tilep->bytes + tty * 129, 129, bx);
				}
			}
			/* DEBUG SVT */
			/* for (tty = 0; tty < 129; ++tty) { */
			/* 	uint32_t ttx; */
			/* 	for (ttx = 0; ttx < 129; ++ttx) { */
			/* 		if (((tty + ttx) & 1) && ( tty + by < 3 || tty >= (((int)self->sizes[m].y) - by - 3) */
			/* 								|| ttx + bx < 3 || ttx >= (((int)self->sizes[m].x) - bx - 3))) */
			/* 			tilep->bytes[tty * 129 + ttx] = 0xffff00ff; */
			/* 	} */
			/* } */
			tilep->loaded = true;
			tilep->loading = false;

			if (self->usrptr)
			{
				FILE *fp;
				char buffer[256];

				texture_tile_filename(self, m, tilep->x, tilep->y, buffer, sizeof(buffer));
				fp = fopen(buffer, "r");
				if (fp)
				{
					fclose(fp);
				}
				else
				{
					/* printf("Writing uncached %s\n", buffer); */
					texture_disk_cacher_write(self, tilep);
				}
			}
		}
		if (tiles_x > 1)
			tiles_x = tiles_x / 2;
	}
	free(buffer);
	glPixelStorei(GL_PACK_ROW_LENGTH, 0);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	glerr();

	if (self->target != GL_TEXTURE_2D)
	{
		printf("PROBLEM '%s'\n", self->name);
		return 0;
	}

	glBindTexture(self->target, 0); glerr();

	stbi_image_free(self->bufs[0].data);

	glDeleteFramebuffers(MAX_MIPS, self->frame_buffer);

	for(i = 0; i < self->bufs_size; i++)
	{
		if(self->bufs[i].id) glDeleteTextures(1, &self->bufs[i].id);
	}

	self->bufs[0].ready = 1;

	return 1;
}

buffer_t buffer_new(const char *name, int32_t is_float, int32_t dims)
{
	buffer_t buffer;
	memset(&buffer, 0, sizeof(buffer));
#ifdef __EMSCRIPTEN__
	is_float = false;
#endif

	strncpy(buffer.name, name, sizeof(buffer.name) - 1);
	buffer.ready = 0;

	if(dims == 4)
	{
		buffer.format   = GL_RGBA;
		buffer.internal = is_float ? GL_RGBA16F : GL_RGBA8;
		buffer.type = is_float ? GL_FLOAT : GL_UNSIGNED_BYTE;
	}
	else if(dims == 3)
	{
		buffer.format   = GL_RGB;
		buffer.internal = is_float ? GL_RGB16F : GL_RGB8;
		buffer.type = is_float ? GL_FLOAT : GL_UNSIGNED_BYTE;
	}
	else if(dims == 2)
	{
		buffer.format   = GL_RG;
		buffer.internal = is_float ? GL_RG16F : GL_RG8;
		buffer.type = is_float ? GL_FLOAT : GL_UNSIGNED_BYTE;
	}
	else if(dims == 1)
	{
		buffer.format   = GL_RED;
		buffer.internal = is_float ? GL_R16F : GL_R8;
		buffer.type = is_float ? GL_FLOAT : GL_UNSIGNED_BYTE;
	}
	else if(dims == -1)
	{
		buffer.format = GL_DEPTH_COMPONENT;
		buffer.internal = GL_DEPTH_COMPONENT24;
		buffer.type = GL_UNSIGNED_SHORT;
	}

	buffer.dims = dims;

	return buffer;
}

texture_t *texture_new_2D
(
	uint32_t width,
	uint32_t height,
	uint32_t flags,
	uint32_t buf_count,
	...
)
{
	va_list argptr;
	uint32_t i;
	texture_t *self = calloc(1, sizeof *self);

	self->target = GL_TEXTURE_2D;
	self->brightness = 0.3f;

	self->repeat = !!(flags & TEX_REPEAT);
	self->mipmaped = !!(flags & TEX_MIPMAP);
	self->interpolate = !!(flags & TEX_INTERPOLATE);
	self->sparse_it = !!(flags & TEX_SPARSE);

	self->width = width;
	self->height = height;
	self->sizes[0] = uvec2(width, height);

	va_start(argptr, buf_count);
	for (i = 0; i < buf_count; i++)
	{
		buffer_t buffer = va_arg(argptr, buffer_t);
		int32_t n = self->bufs_size++;
		self->bufs[n] = buffer;

		if (buffer.dims == -1)
		{
			if(i > 0) perror("Depth component must be added first\n");
			self->depth_buffer = 1;
		}

	}
	va_end(argptr);

	for (i = 0; i < buf_count; i++)
	{
		texture_alloc_buffer(self, i);
	}

	if(self->depth_buffer)
	{
		self->prev_id = 1;
		self->draw_id = 1;
	}

	return self;
}

static void texture_new_3D_loader(texture_t *self)
{
	glActiveTexture(GL_TEXTURE0 + ID_3D);

	glGenTextures(1, &self->bufs[0].id); glerr();
	glBindTexture(self->target, self->bufs[0].id); glerr();

	glTexParameterf(self->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(self->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(self->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(self->target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(self->target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
	glerr();

    /* glPixelStorei(GL_UNPACK_ALIGNMENT, 1); glerr(); */

	glTexImage3D(self->target, 0, self->bufs[0].internal,
			self->width, self->height, self->depth,
			0, self->bufs[0].format, GL_UNSIGNED_BYTE, self->bufs[0].data);
	glerr();

	glBindTexture(self->target, 0); glerr();
	glActiveTexture(GL_TEXTURE0);
	self->bufs[0].ready = 1;

	glerr();
}

texture_t *texture_new_3D
(
	uint32_t width,
	uint32_t height,
	uint32_t depth,
	int32_t dims
)
{
	bool_t is_float = false;
	const uint32_t i = 0;
	texture_t *self = calloc(1, sizeof *self);

	self->target = GL_TEXTURE_3D;

	self->bufs[0].dims = dims;
	if(dims == 4)
	{
		self->bufs[i].format   = GL_RGBA;
		self->bufs[i].internal = is_float ? GL_RGBA16F : GL_RGBA8;
		self->bufs[i].type = is_float ? GL_FLOAT : GL_UNSIGNED_BYTE;
	}
	else if(dims == 3)
	{
		self->bufs[i].format   = GL_RGB;
		self->bufs[i].internal = is_float ? GL_RGB16F : GL_RGB8;
		self->bufs[i].type = is_float ? GL_FLOAT : GL_UNSIGNED_BYTE;
	}
	else if(dims == 2)
	{
		self->bufs[i].format   = GL_RG;
		self->bufs[i].internal = is_float ? GL_RG16F : GL_RG8;
		self->bufs[i].type = is_float ? GL_FLOAT : GL_UNSIGNED_BYTE;
	}
	else if(dims == 1)
	{
		self->bufs[i].format   = GL_RED;
		self->bufs[i].internal = is_float ? GL_R16F : GL_R8;
		self->bufs[i].type = is_float ? GL_FLOAT : GL_UNSIGNED_BYTE;
	}
	else if(dims == -1)
	{
		if(i > 0) perror("Depth component must be added first\n");
		self->bufs[i].format = GL_DEPTH_COMPONENT;
		self->bufs[i].internal = GL_DEPTH_COMPONENT24;
		self->bufs[i].type = GL_UNSIGNED_BYTE;
		self->depth_buffer = 1;
	}

	self->width = width;
	self->height = height;
	self->depth = depth;

	self->bufs[0].data	= calloc(dims * width * height * depth, 1);

	loader_push(g_candle->loader, (loader_cb)texture_new_3D_loader, self,
			NULL);

	return self;
}

void texture_set_xyz(texture_t *self, int32_t x, int32_t y, int32_t z,
		GLubyte r, GLubyte g, GLubyte b, GLubyte a)
{
	int32_t i = (x + (y + z * self->height) * self->width) * 4;
	self->bufs[0].data[i + 0] = r;
	self->bufs[0].data[i + 1] = g;
	self->bufs[0].data[i + 2] = b;
	self->bufs[0].data[i + 3] = a;
}

void texture_set_xy(texture_t *self, int32_t x, int32_t y,
		GLubyte r, GLubyte g, GLubyte b, GLubyte a)
{
	int32_t i = (x + y * self->width) * 4;
	switch (self->bufs[0].dims)
	{
		case 4: self->bufs[0].data[i + 3] = a;
		case 3: self->bufs[0].data[i + 2] = b;
		case 2: self->bufs[0].data[i + 1] = g;
		case 1: self->bufs[0].data[i + 0] = r;
	}
}

static void texture_update_indir_info(texture_t *self)
{
	int32_t tiles_x, m, x, y, i;

	texture_update_sizes(self);
	switch(self->bufs[0].dims)
	{
		case 1:	self->bufs[0].format	= GL_RED;
				self->bufs[0].internal = GL_R8;
				self->bufs[0].type = GL_UNSIGNED_BYTE;
				break;
		case 2:	self->bufs[0].format	= GL_RG;
				self->bufs[0].internal = GL_RG8;
				self->bufs[0].type = GL_UNSIGNED_BYTE;
				break;
		case 3:	self->bufs[0].format	= GL_RGB;
				self->bufs[0].internal = GL_RGB8;
				self->bufs[0].type = GL_UNSIGNED_BYTE;
				break;
		case 4: self->bufs[0].format	= GL_RGBA;
				self->bufs[0].internal = GL_RGBA8;
				self->bufs[0].type = GL_UNSIGNED_BYTE;
				break;
	}

#ifdef THREADED
	mtx_lock(&g_indir_mtx);
#endif
	self->bufs[0].indir_n = g_indir_n;
	self->bufs[0].tiles = &g_tiles[g_indir_n];

	self->bufs_size = 1;
	strncpy(self->bufs[0].name, "color", sizeof(self->bufs[0].name) - 1);
	self->mipmaped = true;
	self->interpolate = true;

	tiles_x = texture_num_tiles_per_row(self);
	for (m = 0; m < MAX_MIPS; m++)
	{
		self->bufs[0].num_tiles[m] = tiles_x;
		self->bufs[0].mips[m] = &g_tiles[g_indir_n];
		for (y = 0; y < tiles_x; y++) for (x = 0; x < tiles_x; x++)
		{
			tex_tile_t *tilep = &g_tiles[g_indir_n];

			tilep->loaded = false;
			tilep->loading = false;
			tilep->bound = false;
			tilep->touched = 0;

			tilep->mip = m;
			tilep->x = x;
			tilep->y = y;
			tilep->indir_x = g_indir_n % g_indir_w;
			tilep->indir_y = g_indir_n / g_indir_w;

			tilep->location.mip = 0;
			tilep->location.x = 0;
			tilep->location.y = 0;

			tilep->tex = self;

			g_indir_n++;
			assert(g_indir_n < g_indir_w * g_indir_h);

			for (i = 0; i < sizeof(tilep->bytes) / sizeof(tilep->bytes[0]); i++)
			{
				tilep->bytes[i] = 0xFFFF8080;
			}
		}
		if (tiles_x > 1)
			tiles_x = tiles_x / 2;
	}
#ifdef THREADED
	mtx_unlock(&g_indir_mtx);
#endif

	self->sparse_it = true;
}


static
int32_t texture_load_from_memory(texture_t *self, const char *buffer,
                                 int32_t len)
{
	texture_t temp;
	memset(&temp, 0, sizeof(temp));

	temp.target = GL_TEXTURE_2D;

	temp.bufs[0].dims = 0;
	temp.bufs[0].data = stbi_load_from_memory((const unsigned char*)buffer,
	                                          len, (int32_t*)&temp.width,
	                                          (int32_t*)&temp.height,
	                                          &temp.bufs[0].dims, 4);
	temp.bufs[0].dims = 4;

	if(!temp.bufs[0].data)
	{
		printf("Could not load from memory!\n");
		return 0;
	}
	*self = temp;

	texture_update_indir_info(self);
	loader_push(g_candle->loader, (loader_cb)texture_from_file_loader,
	            self, NULL);

	return 1;
}

texture_t *texture_from_buffer(const char *name, void *buffer,
                               int32_t width, int32_t height, int32_t Bpp)
{
	texture_t *self = texture_new_2D(width, height, TEX_INTERPOLATE, 0);

	self->bufs[0].dims = Bpp;
	self->bufs[0].data = buffer;

	strncpy(self->name, name, sizeof(self->name) - 1);
	texture_update_indir_info(self);
	loader_push(g_candle->loader, (loader_cb)texture_from_file_loader,
	            self, NULL);
	return self;
}

texture_t *texture_from_memory(const char *name, const char *buffer, size_t len)
{
	texture_t *self = texture_new_2D(0, 0, TEX_INTERPOLATE, 0);
	strcpy(self->name, name);
	texture_load_from_memory(self, buffer, len);
	return self;
}

int32_t load_tex(texture_t *self)
{
	texture_t temp = *self;
	printf("loading cacheless %s\n", (char*)self->usrptr);

	temp.bufs[0].dims = 0;
	temp.bufs[0].data = stbi_load((char*)self->usrptr, (int32_t*)&temp.width,
			(int32_t*)&temp.height, &temp.bufs[0].dims, 4);
	temp.bufs[0].dims = 4;

	if(!temp.bufs[0].data)
	{
		printf("Could not find texture file: %s\n", (char*)self->usrptr);
		return 0;
	}
	*self = temp;

	loader_push(g_candle->loader, (loader_cb)texture_from_file_loader,
				self, NULL);
	return 1;
}

int32_t load_tex_tile(tex_tile_t *tile)
{
	int32_t w;
	int32_t h;
	int32_t dims = 0;
	char buffer[256];
	void *bytes;

	texture_tile_filename(tile->tex, tile->mip, tile->x, tile->y,
	                      buffer, sizeof(buffer));

	puts(buffer);
/* #ifdef __EMSCRIPTEN__ */
	/* EM_ASM_( */
	/* { */
		/* load_tile_js($0, $1, $2); */
	/* }, path, bytes, tile->loading); */
/* #else */
	bytes = stbi_load(buffer, &w, &h, &dims, 4);
/* #endif */
	dims = 4;
	assert(w == 129 && h == 129);
	memcpy(tile->bytes, bytes, w * h * 4);

	assert(tile->loading);
	assert(tile->bound == 0);
	tile->loaded = true;
	tile->loading = false;

	stbi_image_free(bytes);
	return 0;
}

int32_t save_tex_tile(tex_tile_t *tile)
{
	char buffer[256];

	texture_tile_filename(tile->tex, tile->mip, tile->x, tile->y,
	                      buffer, sizeof(buffer));
	stbi_write_png(buffer, 129, 129, 4, tile->bytes, 0);
	return 1;
}

static void texture_disk_cacher_write(texture_t *self, tex_tile_t *tile)
{
#ifdef THREADED
	thrd_t thrd;
	thrd_create(&thrd, (thrd_start_t)save_tex_tile, tile);
#else
#endif
}

int load_tex_tile_worker(void *usrptr)
{
	tile_worker_t *worker = usrptr;

	while (true) {
#ifdef THREADED
		mtx_lock(&worker->mtx);
#endif
		if (!worker->tile)
		{
			break;
		}
		load_tex_tile(worker->tile);
		worker->tile = NULL;
	}
	return 0;
}

static void load_tex_tile_assign_worker(tex_tile_t *tile)
{
	uint32_t t;
	for (t = 0; t < sizeof(g_tile_workers) / sizeof(*g_tile_workers); t++)
	{
		if (!g_tile_workers[t].tile)
		{
			tile->loading = true;
			g_tile_workers[t].tile = tile;
#ifdef THREADED
			mtx_unlock(&g_tile_workers[t].mtx);
#endif
			return;
		}
	}
}

static void texture_disk_cacher(texture_t *self, uint32_t mip,
                                uint32_t x, uint32_t y)
{
	char buffer[256];
	FILE *tile_fp;
	uint32_t tile_i;
	int32_t tiles_x, m, i;

	texture_tile_filename(self, mip, x, y, buffer, sizeof(buffer));

	tile_fp = fopen(buffer, "r");
	if (tile_fp)
	{
		tex_tile_t *tile = texture_get_tile(self, mip, x, y);
		fclose(tile_fp);
#ifdef THREADED
		load_tex_tile_assign_worker(tile);
#else
		tile->loading = true;
		load_tex_tile(tile);
#endif
		return;
	}
	assert(x < self->bufs[0].num_tiles[mip]);
	assert(y < self->bufs[0].num_tiles[mip]);

	tile_i = self->bufs[0].indir_n;
	tiles_x = texture_num_tiles_per_row(self);
	printf("cache not exists %s %d\n", buffer, tiles_x);
	for (m = 0; m < MAX_MIPS; m++)
	{
		for (i = 0; i < tiles_x * tiles_x; i++)
		{
			tex_tile_t *tilep = &g_tiles[tile_i++];
			if (!tilep->loaded)
			{
				tilep->loading = true;
			}
		}
		if (tiles_x > 1)
		{
			tiles_x = tiles_x / 2;
		}
	}
#ifdef THREADED
	{
		thrd_t thrd;
		thrd_create(&thrd, (thrd_start_t)load_tex, self);
	}
#else
	load_tex(self);
#endif

}

texture_t *texture_from_file(const char *filename)
{
	texture_t *self = texture_new_2D(0, 0, TEX_INTERPOLATE, 0);
	FILE *fp = fopen(filename, "r");
	stbi_info_from_file(fp, (int32_t*)&self->width, (int32_t*)&self->height,
	                    &self->bufs[0].dims);
	self->bufs[0].dims = 4;
	fclose(fp);

	strncpy(self->name, filename, sizeof(self->name) - 1);
	self->usrptr = malloc(strlen(filename) + 1);
	strcpy(self->usrptr, filename);
	self->cacher = texture_disk_cacher;

	texture_update_indir_info(self);

    return self;
}

static int32_t texture_2D_frame_buffer(texture_t *self)
{
	int32_t levels = self->mipmaped ? MAX_MIPS : 1;
	GLuint targ = self->target;
	int32_t i, m;

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	if(!self->frame_buffer[0])
	{
		glGenFramebuffers(levels, self->frame_buffer);
	}


	for(i = self->depth_buffer; i < self->bufs_size; i++)
	{
		if(!self->bufs[i].fb_ready)
		{
			for(m = 0; m < levels; m++)
			{
				glBindFramebuffer(GL_DRAW_FRAMEBUFFER, self->frame_buffer[m]);

				glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,
						GL_COLOR_ATTACHMENT0 + i - self->depth_buffer, targ,
						self->bufs[i].id, m);
			}
			self->bufs[i].fb_ready = 1;
		}
	}

	self->framebuffer_ready = 1;
	return 1;
}

int32_t texture_target_sub(texture_t *self, texture_t *depth, int32_t fb,
                          int32_t x, int32_t y, int32_t width, int32_t height) 
{
	bool_t ready;

	if (!self->bufs[self->depth_buffer].id)
		return 0;

	ready = self->framebuffer_ready;
	if(!ready)
	{
		if(self->target == GL_TEXTURE_2D)
		{
			texture_2D_frame_buffer(self);
		}
		else
		{
			texture_cubemap_frame_buffer(self);
		}
	}

	/* if(!depth) */
	/* { */
	/* 	if(!fallback_depth) */
	/* 	{ */
	/* 		fallback_depth = texture_new_2D(1, 1, 0, */
	/* 			buffer_new("depth",	1, -1) */
	/* 		); */
	/* 	} */
	/* 	depth = fallback_depth; */
	/* } */

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, self->frame_buffer[fb]); glerr();

	if(!ready || (self->last_depth != depth && self->target == GL_TEXTURE_2D))
	{
		GLuint status;

		glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
				self->target, depth ? depth->bufs[0].id : 0, 0); glerr();

		status = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
		if(status != GL_FRAMEBUFFER_COMPLETE)
		{
			printf("Failed to create framebuffer! tex_sub '%s'\n", self->name);
			switch(status)
			{
				/* case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT: */
				/* 	printf("incomplete dimensions\n"); break; */
				case GL_FRAMEBUFFER_UNSUPPORTED:
					printf("unsupported\n"); break;
				case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
					printf("missing attach\n"); break;
				case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
					printf("incomplete attach\n"); break;
			}
			exit(1);
		}

		glDrawBuffers(self->bufs_size - self->depth_buffer, attachments);
		glerr();
		self->last_depth = depth;
	}

	glViewport(x, y, width, height); glerr();
	return 1;
}

int32_t texture_target(texture_t *self, texture_t *depth, int32_t fb)
{
	int w = self->sizes[fb].x;
	int h = self->sizes[fb].y;
	/* if(w == 0) w = self->width; */
	/* if(h == 0) h = self->height; */
	return texture_target_sub(self, depth, fb, 0, 0, w, h);
}

void texture_draw_id(texture_t *self, int32_t tex)
{
	self->draw_id = tex;
}

void texture_bind(texture_t *self, int32_t tex)
{
	tex = tex >= 0 ? tex : self->draw_id;

	if (!self->bufs[tex].id) return;

	glBindTexture(self->target, self->bufs[tex].id); glerr();
}

void texture_destroy(texture_t *self)
{
	int32_t i;

	for (i = 0; i < self->bufs_size; i++)
	{
		if (self->bufs[i].id) glDeleteTextures(1, &self->bufs[i].id);
	}
	if (self->target == GL_TEXTURE_CUBE_MAP && self->frame_buffer[1])
	{
		glDeleteFramebuffers(6, &self->frame_buffer[0]);
	}
	else if (self->frame_buffer[0]) /* TEX2D */
	{
		int32_t levels = self->mipmaped ? MAX_MIPS : 1;
		glDeleteFramebuffers(levels, &self->frame_buffer[0]);
	}


	/* TODO: free gl buffers and tex */
	free(self);
}

int32_t texture_2D_resize(texture_t *self, int32_t width, int32_t height)
{
	int32_t i;
	if(self->width == width && self->height == height) return 0;
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glActiveTexture(GL_TEXTURE0 + ID_2D);
	self->width = width;
	self->height = height;
	self->sizes[0] = uvec2(width, height);

	for(i = 0; i < self->bufs_size; i++)
	{
		/* uint32_t Bpp = self->bufs[i].dims; */

		/* if(self->bufs[i].dims == -1) */ 
		/* { */
			/* Bpp = 1 * sizeof(float); */
		/* } */
		/* uint32_t imageSize = Bpp * self->width * self->height; */
		/* self->bufs[i].data = realloc(self->bufs[i].data, imageSize); */

		if(self->bufs[i].id)
		{
			glDeleteTextures(1, &self->bufs[i].id);
			self->bufs[i].id = 0;
			if (self->bufs[i].pbo)
			{
				glDeleteBuffers(1, &self->bufs[i].pbo);
			}
			self->bufs[i].pbo = 0;
			self->bufs[i].ready = 0;
		}

		texture_alloc_buffer(self, i);

		/* glBindTexture(targ, 0); */
		/* self->framebuffer_ready = 0; */
	}

	glBindTexture(self->target, 0);
	glerr();

	return 1;
}

static int32_t texture_cubemap_frame_buffer(texture_t *self)
{
	int32_t f, i;
	GLuint status;
	glActiveTexture(GL_TEXTURE0 + ID_CUBE);

	if(!self->frame_buffer[0])
	{
		glGenFramebuffers(6, self->frame_buffer);
	}
	for(f = 0; f < 6; f++)
	{
		GLuint targ = GL_TEXTURE_CUBE_MAP_POSITIVE_X + f;

		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, self->frame_buffer[f]);

		glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
				targ, self->bufs[0].id, 0);

		for(i = 1; i < self->bufs_size; i++)
		{
			glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,
					GL_COLOR_ATTACHMENT0 + i - 1,
					targ, self->bufs[i].id, 0);
		}
		self->sizes[f] = uvec2(self->width, self->height);
	}
	glDrawBuffers(self->bufs_size - self->depth_buffer, attachments);

	status = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
	if(status != GL_FRAMEBUFFER_COMPLETE)
	{
		printf("Failed to create framebuffer! cubemap\n");
		exit(1);
	}

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glerr();
	glActiveTexture(GL_TEXTURE0);

	self->framebuffer_ready = 1;
	return 1;
}

static int32_t texture_cubemap_loader(texture_t *self)
{
	int32_t i;
	glActiveTexture(GL_TEXTURE0 + ID_CUBE);

	glGenTextures(1, &self->bufs[0].id); glerr();
	glGenTextures(1, &self->bufs[1].id); glerr();

	if(self->depth_buffer)
	{
		glBindTexture(self->target, self->bufs[0].id); glerr();
		glTexParameteri(self->target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(self->target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(self->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(self->target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(self->target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		for(i = 0; i < 6; i++)
		{
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0,
					GL_DEPTH_COMPONENT16, self->width, self->height, 0,
					GL_DEPTH_COMPONENT, GL_FLOAT, NULL); glerr();
		}
	}


	/* if(self->color_buffer) */
	{
		glBindTexture(self->target, self->bufs[1].id); glerr();
		glTexParameteri(self->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(self->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(self->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(self->target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(self->target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		for(i = 0; i < 6; i++)
		{
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA32F,
					self->width, self->height, 0, GL_RGBA, GL_FLOAT, NULL);
			glerr();
		}
	}

	glBindTexture(self->target, 0); glerr();
	/* texture_cubemap_frame_buffer(self); */

	glActiveTexture(GL_TEXTURE0);
	self->bufs[0].ready = 1;
	self->bufs[1].ready = 1;

	glerr();
	return 1;
}

texture_t *texture_cubemap
(
	uint32_t width,
	uint32_t height,
	uint32_t depth_buffer
)
{
	texture_t *self = calloc(1, sizeof *self);
	self->id = g_tex_num++;
	self->target = GL_TEXTURE_CUBE_MAP;

	self->width = width;
	self->height = height;
	self->depth_buffer = depth_buffer;
	self->draw_id = 1;
	self->prev_id = 1;

	self->bufs[0].dims = -1;
	self->bufs[1].dims = 4;
	strncpy(self->bufs[0].name, "depth", sizeof(self->bufs[0].name) - 1);
	strncpy(self->bufs[1].name, "color", sizeof(self->bufs[1].name) - 1);

	self->bufs_size = 2;

	loader_push(g_candle->loader, (loader_cb)texture_cubemap_loader, self, NULL);

	return self;
}

void *tex_loader(const char *bytes, size_t bytes_num,
                 const char *name, uint32_t ext)
{
	texture_t *texture = texture_from_memory(name, bytes, bytes_num);
	return texture;
}

void textures_reg()
{
	sauces_loader(ref("png"), tex_loader);
	sauces_loader(ref("tga"), tex_loader);
	sauces_loader(ref("jpg"), tex_loader);
}
