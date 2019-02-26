#include "texture.h"
#include "file.h"
#include <candle.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

void world_changed(void);
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ASSERT(x)
#include <utils/stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <utils/stb_image_write.h>

static int32_t texture_cubemap_frame_buffer(texture_t *self);
static int32_t texture_2D_frame_buffer(texture_t *self);

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
tex_tile_t **g_cache_bound;
texture_t *g_indir;
tex_tile_t *g_tiles;
const int g_cache_w = 64;
const int g_cache_h = 32;
const int g_indir_w = 256;
const int g_indir_h = 256;
int g_cache_n = 0;
int g_indir_n = 0;
void svp_init()
{
	int max;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max);
	printf("Max texture size %d\n", max);
	max = floorf(((float)max) / 128);
	g_cache = texture_new_2D(g_cache_w * 128, g_cache_h * 128, TEX_INTERPOLATE,
	                         buffer_new("color", false, 4));
	g_cache_bound = calloc(g_cache_w * g_cache_h, sizeof(*g_cache_bound));
	g_indir = texture_new_2D(g_indir_w, g_indir_h, 0,
	                         buffer_new("indir", false, 3));
	g_tiles = malloc(g_indir_w * g_indir_h * sizeof(tex_tile_t));

	/* glGetIntegerv(GL_TEXTURE_MAX_LEVEL, &max); */
	/* printf("MAX LEVELS %d\n", max); */
	glerr();
}

static struct tpair *pair(texture_t *tex, int id)
{
	struct tpair *p = malloc(sizeof(*p));
	p->tex = tex;
	p->i = id;
	return p;
}
#define ID_2D 31
#define ID_CUBE 64
#define ID_3D 33
static void texture_update_gl_loader(texture_t *self)
{

	if(self->target == GL_TEXTURE_2D)
	{
		glActiveTexture(GL_TEXTURE0 + ID_2D);
		glBindTexture(self->target, self->bufs[0].id);

		glTexSubImage2D(self->target, 0, 0, 0, self->width, self->height,
				self->bufs[0].format, GL_UNSIGNED_BYTE, self->bufs[0].data);
		glerr();
	}
	if(self->target == GL_TEXTURE_3D)
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

	uint32_t dims = self->bufs[id].dims;
	uint8_t *data = malloc(self->width * self->height * dims);
	stbi_flip_vertically_on_write(1);

	glReadBuffer(GL_COLOR_ATTACHMENT0 + id);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glReadPixels(0, 0, self->width, self->height, self->bufs[id].format,
			GL_UNSIGNED_BYTE, data);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	glerr();

	int32_t res = stbi_write_png(filename, self->width, self->height, dims,
			data, 0);
	free(data);
	return res;
}

uint32_t texture_get_pixel(texture_t *self, int32_t buffer, int32_t x, int32_t y,
		float *depth)
{
	if(!self->framebuffer_ready) return 0;
	uint32_t data = 0;
	y = self->height - y;
	glFlush();
	glFinish();

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, self->frame_buffer[0]); glerr();

	{
		glReadBuffer(GL_COLOR_ATTACHMENT0 + buffer);

		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

		glReadPixels(x, y, 1, 1, GL_RG, GL_UNSIGNED_BYTE, &data);
	}
	if(depth)
	{
		float fetch_depth = 0;

		/* texture_bind(self, DEPTH_TEX); */
		glReadBuffer(GL_NONE);

		/* texture_2D_frame_buffer(self); */
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

		glReadPixels(x, y, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &fetch_depth);

		*depth = fetch_depth;
	}

	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	glerr();
	return data;
}

void texture_update_brightness(texture_t *self)
{
	texture_bind(self, 0);

	uint8_t data[300];
	glGetTexImage(self->target, 9, self->bufs[0].format, GL_UNSIGNED_BYTE,
			data);
	uint8_t r = data[0];
	uint8_t g = data[1];
	uint8_t b = data[2];
	self->brightness = ((float)(r + g + b)) / (255.0f * 3.0f);
	if(self->brightness <= 0) self->brightness = 1;
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
	texture_t *self = data->tex;
	int32_t i = data->i;

	if(!self->bufs[i].id)
	{
		glGenTextures(1, &self->bufs[i].id); glerr();
	}
	uint32_t wrap = self->repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE;
	int32_t width = self->width;
	int32_t height = self->height;
	if(width == 0) width = 1;
	if(height == 0) height = 1;

	glActiveTexture(GL_TEXTURE0 + ID_2D);
	glBindTexture(self->target, self->bufs[i].id); glerr();
	glTexImage2D(self->target, 0, self->bufs[i].internal,
			width, height, 0, self->bufs[i].format, self->bufs[i].type, NULL); glerr();

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

bool_t load_tile(texture_t *self, tex_tile_t *tile, int mip, int frame)
{
	uint32_t cache_tile;

	tile->touched = frame;

	if (tile->bound) return false;

	if (g_cache_n >= g_cache_w * g_cache_h)
	{
		cache_tile = 0;
		uint32_t oldest_touched = ~0;
		for (uint32_t i = 0; i < g_cache_n; i++)
		{
			const uint32_t touched = g_cache_bound[i]->touched;
			if (touched < oldest_touched)
			{
				cache_tile = i;
				oldest_touched = touched;
			}
		}
		if (g_cache_bound[cache_tile])
		{
			g_cache_bound[cache_tile]->bound = false;
		}
	}
	else
	{
		cache_tile = g_cache_n++;
	}
	g_cache_bound[cache_tile] = tile;

	tile->bound = true;

	struct {
		uint16_t cache_tile;
		uint8_t mip_level;
	} indir_info = {cache_tile, mip};


	int vx = (indir_info.cache_tile % g_cache_w) * 128;
	int vy = (indir_info.cache_tile / g_cache_w) * 128;
	texture_bind(g_cache, 0);
	glTexSubImage2D(self->target, 0, vx, vy, 128, 128,
			self->bufs[0].format, GL_UNSIGNED_BYTE,
			tile->bytes); glerr();

	texture_bind(g_indir, 0);
	/* glCopyTexSubImage2D(GL_TEXTURE_2D, */
	/* 		0, (g_cache_n % g_cache_w) * 128, */
	/* 		  (g_cache_n / g_cache_w) * 128, x, y, 128, 128); */
	glTexSubImage2D(g_indir->target, 0, tile->indir_x, tile->indir_y, 1, 1,
			GL_RGB, GL_UNSIGNED_BYTE, &indir_info);
	glerr();
	return true;
}

#include<utils/shader.h>
static int32_t texture_from_file_loader(texture_t *self)
{
	glActiveTexture(GL_TEXTURE0 + ID_2D);

	glGenTextures(1, &self->bufs[0].id); glerr();
	glBindTexture(self->target, self->bufs[0].id); glerr();
	glerr();
	glTexParameterf(self->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(self->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	/* if(0) */
	/* { */

		/* GLfloat anis = 0; */
		/* glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &anis); glerr(); */
		/* /1* printf("Max anisotropy filter %f\n", anis); *1/ */
		/* if(anis) */
		/* { */
		/* 	glTexParameterf(self->target, GL_TEXTURE_MAX_ANISOTROPY_EXT, 8); */
		/* } */
		/* glerr(); */
	/* } */
	glTexParameteri(self->target, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(self->target, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glTexImage2D(self->target, 0, self->bufs[0].internal, self->width,
			self->height, 0, self->bufs[0].format,
			self->bufs[0].type, self->bufs[0].data); glerr();

	glGenerateMipmap(self->target); glerr();
	texture_update_sizes(self);

	glTexParameteri(self->target, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(self->target, GL_TEXTURE_WRAP_T, GL_REPEAT);
	/* glTexParameteri(self->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR); */
	/* glTexParameteri(self->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); */
	glerr();

	glActiveTexture(GL_TEXTURE0);
	self->bufs[0].ready = 1;

	self->bufs[0].indir_n = g_indir_n;
	self->bufs[0].tiles = &g_tiles[g_indir_n];
	const uint32_t format = self->bufs[0].format;
	const uint32_t type = self->bufs[0].type;

	glPixelStorei(GL_PACK_ROW_LENGTH, 128);
	glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
	glPixelStorei(GL_PACK_SKIP_ROWS, 0);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	texture_target(self, NULL, 0); glerr();

	int tiles_x = ceilf(((float)self->width) / 128);
	int tiles_y = ceilf(((float)self->height) / 128);
	for (int m = 0; m < MAX_MIPS; m++)
	{
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); glerr();
		glBindFramebuffer(GL_READ_FRAMEBUFFER, self->frame_buffer[m]); glerr();
		glReadBuffer(GL_COLOR_ATTACHMENT0); glerr();

		self->bufs[0].num_tiles[m] = tiles_x * tiles_y;
		self->bufs[0].mips[m] = &g_tiles[g_indir_n];
		for (int y = 0; y < tiles_y; y++) for (int x = 0; x < tiles_x; x++)
		{
			tex_tile_t *tilep = &g_tiles[g_indir_n];
			tilep->bound = false;
			tilep->indir_x = g_indir_n % g_indir_w;
			tilep->indir_y = g_indir_n / g_indir_w;
			g_indir_n++;

			int tx = x * 128;
			int ty = y * 128;
			int w = fmin(128, self->sizes[m].x - tx);
			int h = fmin(128, self->sizes[m].y - ty); 
			if (self->sizes[m].x <= 0 || w <= 0) continue;
			if (self->sizes[m].y <= 0 || h <= 0) continue;

			/* printf("%d %d %d %d %d\n", m, tx, ty, self->sizes[m].x, self->sizes[m].y); */
			glReadPixels(tx, ty, w, h, format, type, tilep->bytes); glerr();
		}
		tiles_x = ceilf(0.5 * tiles_x);
		tiles_y = ceilf(0.5 * tiles_y);

	}
	glPixelStorei(GL_PACK_ROW_LENGTH, 0);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	glerr();

	/* for (int m = 0; m < MAX_MIPS; m++) */
	/* { */
	/* 	for (int tile = 0; tile < self->bufs[0].num_tiles[m]; tile++) */
	/* 	{ */
	/* 		load_tile(self, &self->bufs[0].mips[m][tile], m, 0); */
	/* 	} */
	/* } */

	/* glGenerateMipmap(self->target); glerr(); */

	stbi_image_free(self->bufs[0].data);
	for(int i = 0; i < self->bufs_size; i++)
	{
		if(self->bufs[i].id) glDeleteTextures(1, &self->bufs[i].id);
	}
	if(self->frame_buffer[0]) /* TEX2D */
	{
		int32_t levels = self->mipmaped ? MAX_MIPS : 1;
		glDeleteFramebuffers(levels, &self->frame_buffer[0]);
	}
	/* self->bufs[0].ready = 0; */
	/* self->bufs[0].id = 0; */
	self->bufs[0].id = self->bufs[0].indir_n;

	world_changed();

	return 1;
}
/* static int32_t texture_from_file_loader(texture_t *self) */
/* { */
/* 	self->mipmaped = 1; */
/* 	self->interpolate = 1; */
/* 	self->repeat = 1; */

/* 	/1* { *1/ */

/* 	/1* 	GLfloat anis = 0; *1/ */
/* 	/1* 	glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &anis); glerr(); *1/ */
/* 	/1* 	/2* printf("Max anisotropy filter %f\n", anis); *2/ *1/ */
/* 	/1* 	if(anis) *1/ */
/* 	/1* 	{ *1/ */
/* 	/1* 		glTexParameterf(self->target, GL_TEXTURE_MAX_ANISOTROPY_EXT, 8); *1/ */
/* 	/1* 	} *1/ */
/* 	/1* 	glerr(); *1/ */
/* 	/1* } *1/ */

/* 	texture_alloc_buffer(self, 0); */

/* 	glerr(); */
/* 	return 1; */
/* } */

int32_t buffer_new(const char *name, int32_t is_float, int32_t dims)
{
	texture_t *texture = _g_tex_creating;

	int32_t i = texture->bufs_size++;
	texture->bufs[i].name = strdup(name);
	texture->bufs[i].ready = 0;

	if(texture->depth_buffer)
	{
		texture->prev_id = 1;
		texture->draw_id = 1;
	}

	if(dims == 4)
	{
		texture->bufs[i].format   = GL_RGBA;
		texture->bufs[i].internal = is_float ? GL_RGBA32F : GL_RGBA8;
		texture->bufs[i].type = is_float ? GL_FLOAT : GL_UNSIGNED_BYTE;
	}
	else if(dims == 3)
	{
		texture->bufs[i].format   = GL_RGB;
		texture->bufs[i].internal = is_float ? GL_RGB32F : GL_RGB8;
		texture->bufs[i].type = is_float ? GL_FLOAT : GL_UNSIGNED_BYTE;
	}
	else if(dims == 2)
	{
		texture->bufs[i].format   = GL_RG;
		texture->bufs[i].internal = is_float ? GL_RG32F : GL_RG8;
		texture->bufs[i].type = is_float ? GL_FLOAT : GL_UNSIGNED_BYTE;
	}
	else if(dims == 1)
	{
		texture->bufs[i].format   = GL_RED;
		texture->bufs[i].internal = is_float ? GL_R32F : GL_R8;
		texture->bufs[i].type = is_float ? GL_FLOAT : GL_UNSIGNED_BYTE;
	}
	else if(dims == -1)
	{
		if(i > 0) perror("Depth component must be added first\n");
		texture->bufs[i].format = GL_DEPTH_COMPONENT;
		texture->bufs[i].internal = GL_DEPTH_COMPONENT16;
		texture->bufs[i].type = GL_UNSIGNED_SHORT;
		texture->depth_buffer = 1;
	}

	texture->bufs[i].dims = dims;

	texture_alloc_buffer(texture, i);


	return i;
}

__thread texture_t *_g_tex_creating = NULL;

texture_t *_texture_new_2D_pre
(
	uint32_t width,
	uint32_t height,
	uint32_t flags
)
{
	texture_t *self = calloc(1, sizeof *self);
	_g_tex_creating = self;

	self->target = GL_TEXTURE_2D;
	self->brightness = 1.0f;

	self->repeat = !!(flags & TEX_REPEAT);
	self->mipmaped = !!(flags & TEX_MIPMAP);
	self->interpolate = !!(flags & TEX_INTERPOLATE);
	self->sparse_it = !!(flags & TEX_SPARSE);

	self->width = width;
	self->height = height;
	self->sizes[0] = uvec2(width, height);

	return self;
}

texture_t *_texture_new(int32_t ignore, ...)
{
	texture_t *self = _g_tex_creating;
	_g_tex_creating = NULL;
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
	texture_t *self = calloc(1, sizeof *self);

	self->target = GL_TEXTURE_3D;

	self->bufs[0].dims = dims;
	if(dims == 4)
	{
		self->bufs[0].format	= GL_RGBA;
		self->bufs[0].internal = GL_RGBA32F;
	}
	else if(dims == 3)
	{
		self->bufs[0].format	= GL_RGB;
		self->bufs[0].internal = GL_RGB32F;
	}
	else if(dims == 2)
	{
		self->bufs[0].format	= GL_RG;
		self->bufs[0].internal = GL_RG32F;
	}

	self->width = width;
	self->height = height;
	self->depth = depth;

	self->bufs[0].data	= calloc(dims * width * height * depth, 1);

	/* float x, y, z; */
	/* for(x = 0; x < width; x++) */
	/* { */
		/* for(y = 0; y < height; y++) */
		/* { */
			/* for(z = 0; z < depth; z++) */
			/* { */
				/* texture_set_xyz(self, x, y, z, 255, 255, 255, 255); */
			/* } */
		/* } */
	/* } */


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

int32_t texture_load_from_memory(texture_t *self, void *buffer, int32_t len)
{
	texture_t temp = {.target = GL_TEXTURE_2D};

	temp.bufs[0].dims = 0;
	temp.bufs[0].data = stbi_load_from_memory(buffer, len, (int32_t*)&temp.width,
			(int32_t*)&temp.height, &temp.bufs[0].dims, 0);

	if(!temp.bufs[0].data)
	{
		printf("Could not load from memory!\n");
		return 0;
	}
	*self = temp;

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


	strncpy(self->name, "unnamed", sizeof(self->name));
	self->filename = "unnamed";
	self->bufs_size = 1;
	self->bufs[0].name = strdup("color");
	self->mipmaped = 1;
	self->interpolate = 1;

	loader_push(g_candle->loader, (loader_cb)texture_from_file_loader, self, NULL);
	return 1;
}

int32_t texture_load(texture_t *self, const char *filename)
{
	texture_t temp = {.target = GL_TEXTURE_2D};

	temp.bufs[0].dims = 0;
	temp.bufs[0].data = stbi_load(filename, (int32_t*)&temp.width,
			(int32_t*)&temp.height, &temp.bufs[0].dims, 0);

	if(!temp.bufs[0].data)
	{
		printf("Could not find texture file: %s\n", filename);
		return 0;
	}
	*self = temp;

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

	strncpy(self->name, filename, sizeof(self->name));
	self->filename = strdup(filename);
	self->bufs_size = 1;
	self->bufs[0].name = strdup("color");
	self->mipmaped = 1;
	self->interpolate = 1;

	loader_push(g_candle->loader, (loader_cb)texture_from_file_loader, self,
			NULL);
	return 1;
}

texture_t *texture_from_buffer(void *buffer, int32_t width, int32_t height,
		int32_t Bpp)
{
	texture_t *self = texture_new_2D(width, height, TEX_INTERPOLATE);

	self->bufs[0].dims = Bpp;
	self->bufs[0].data = buffer;

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

	strncpy(self->name, "unnamed", sizeof(self->name));
	self->filename = "unnamed";
	self->bufs_size = 1;
	self->bufs[0].name = strdup("color");
	self->mipmaped = 1;
	self->interpolate = 1;

	loader_push(g_candle->loader, (loader_cb)texture_from_file_loader, self, NULL);
	return self;
}

texture_t *texture_from_memory(void *buffer, int32_t len)
{
	texture_t *self = texture_new_2D(0, 0, TEX_INTERPOLATE);
	texture_load_from_memory(self, buffer, len);

	return self;
}
texture_t *texture_from_file(const char *filename)
{
	texture_t *self = texture_new_2D(0, 0, TEX_INTERPOLATE);
	texture_load(self, filename);
    return self;
}

static int32_t texture_2D_frame_buffer(texture_t *self)
{
	int32_t levels = self->mipmaped ? MAX_MIPS : 1;
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	if(!self->frame_buffer[0])
	{
		glGenFramebuffers(levels, self->frame_buffer);
	}
	GLuint targ = self->target;

	int32_t i, m;

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

int32_t texture_target_sub(texture_t *self, int32_t width, int32_t height,
		texture_t *depth, int32_t fb) 
{
	if(!self->bufs[self->depth_buffer].id)
	{
		/* This condition is testing if there is a color buffer */
		/* it's misleading and should be changed */
		return 0;
	}
	if(!self->framebuffer_ready)
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

	if(depth && self->last_depth != depth && self->target == GL_TEXTURE_2D)
	{
		glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
				self->target, depth->bufs[0].id, 0);
		self->last_depth = depth;

		glDrawBuffers(self->bufs_size - self->depth_buffer, attachments);

		GLuint status = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
		if(status != GL_FRAMEBUFFER_COMPLETE)
		{
			printf("Failed to create framebuffer!\n");
			switch(status)
			{
				case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT:
					printf("incomplete dimensions\n"); break;
				case GL_FRAMEBUFFER_UNSUPPORTED:
					printf("unsupported\n"); break;
				case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
					printf("missing attach\n"); break;
				case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
					printf("incomplete attach\n"); break;
			}
			exit(1);
		}
	}

	glViewport(0, 0, width, height); glerr();
	return 1;
}

int32_t texture_target(texture_t *self, texture_t *depth, int32_t fb)
{
	int w = self->sizes[fb].x;
	int h = self->sizes[fb].y;
	/* if(w == 0) w = self->width; */
	/* if(h == 0) h = self->height; */
	return texture_target_sub(self, w, h, depth, fb);
}

void texture_draw_id(texture_t *self, int32_t tex)
{
	self->draw_id = tex;
}

void texture_bind(texture_t *self, int32_t tex)
{
	/* printf("This shouldn't be used anymore\n"); exit(1); */
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
	if(self->width == width && self->height == height) return 0;
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glActiveTexture(GL_TEXTURE0 + ID_2D);
	self->width = width;
	self->height = height;
	self->sizes[0] = uvec2(width, height);

	int32_t i;
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
			self->bufs[i].ready = 0;
		}

		texture_alloc_buffer(self, i);

		/* glBindTexture(targ, 0); */
		/* self->framebuffer_ready = 0; */
	}

	glActiveTexture(GL_TEXTURE0);
	glerr();

	return 1;
}

static int32_t texture_cubemap_frame_buffer(texture_t *self)
{
	int32_t f;
	glActiveTexture(GL_TEXTURE0 + ID_CUBE);

	if(!self->frame_buffer[0])
	{
		glGenFramebuffers(6, self->frame_buffer);
	}
	for(f = 0; f < 6; f++)
	{
		int32_t i;
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

	GLuint status = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
	if(status != GL_FRAMEBUFFER_COMPLETE)
	{
		printf("Failed to create framebuffer!\n");
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
	self->bufs[0].name = strdup("depth");
	self->bufs[1].name = strdup("color");

	self->bufs_size = 2;

	loader_push(g_candle->loader, (loader_cb)texture_cubemap_loader, self, NULL);

	return self;
}

int32_t load_tex(texture_t *texture)
{
	char buffer[256];
	strcpy(buffer, texture->name);
	texture_load(texture, buffer);
	return 1;
}

void *tex_loader(const char *path, const char *name, uint32_t ext)
{
	texture_t *texture = texture_new_2D(0, 0, TEX_INTERPOLATE);
	strcpy(texture->name, path);

	SDL_CreateThread((int32_t(*)(void*))load_tex, "load_tex", texture);

	return texture;
}

void textures_reg()
{
	sauces_loader(ref("png"), tex_loader);
	sauces_loader(ref("tga"), tex_loader);
	sauces_loader(ref("jpg"), tex_loader);
}
