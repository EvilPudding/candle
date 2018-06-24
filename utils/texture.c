#include "texture.h"
#include "file.h"
#include <candle.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

int LoadTGA(texture_t *self, const char * filename);
static int texture_cubemap_frame_buffer(texture_t *self);
static int texture_2D_frame_buffer(texture_t *self);

int g_tex_num = 0;

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

#define ID_2D 31
#define ID_CUBE 32
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
}

void texture_update_gl(texture_t *self)
{
	loader_push(g_candle->loader, (loader_cb)texture_update_gl_loader, self,
			NULL);
}

uint texture_get_pixel(texture_t *self, int buffer, int x, int y,
		float *depth)
{
	uint data = 0;
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

	glGetTexImage(self->target, 9, self->bufs[0].format, GL_UNSIGNED_BYTE,
			self->bufs[0].data);
	int r = *(char*)self->bufs[0].data;
	int g = *(char*)self->bufs[1].data;
	int b = *(char*)self->bufs[2].data;
	self->brightness = ((float)(r + g + b)) / 256.0f;
}

void texture_alloc_buffer(texture_t *self, int i)
{
	glBindTexture(self->target, self->bufs[i].id); glerr();
	glTexImage2D(self->target, 0, self->bufs[i].internal,
			self->width, self->height,
			0, self->bufs[i].format,
			(self->bufs[i].dims == -1) ? GL_FLOAT : GL_UNSIGNED_BYTE, NULL);

	self->framebuffer_ready = 0;
	self->bufs[i].ready = 0;
	self->last_depth = NULL;
	glerr();
}

static int texture_from_file_loader(texture_t *self)
{
	glActiveTexture(GL_TEXTURE0 + ID_2D);

	glGenTextures(1, &self->bufs[0].id); glerr();
	glBindTexture(self->target, self->bufs[0].id); glerr();
	glerr();

	glTexParameterf(self->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(self->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	/* if(0) */
	{

		GLfloat anis = 0;
		glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &anis); glerr();
		/* printf("Max anisotropy filter %f\n", anis); */
		if(anis)
		{
			glTexParameterf(self->target, GL_TEXTURE_MAX_ANISOTROPY_EXT, 8);
		}
		glerr();
	}
	glTexParameteri(self->target, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(self->target, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glTexImage2D(self->target, 0, self->bufs[0].internal, self->width,
			self->height, 0, self->bufs[0].format,
			GL_UNSIGNED_BYTE, self->bufs[0].data); glerr();
	self->mipmaped = 1;

	glGenerateMipmap(self->target); glerr();

	glTexParameteri(self->target, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(self->target, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(self->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(self->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glerr();

	glActiveTexture(GL_TEXTURE0);

	return 1;
}

int buffer_new(const char *name, int is_float, int dims)
{
	texture_t *texture = _g_tex_creating;
	GLuint targ = texture->target;
	glActiveTexture(GL_TEXTURE0 + ID_2D);

	int i = texture->bufs_size++;
	texture->bufs[i].name = strdup(name);

	glGenTextures(1, &texture->bufs[i].id); glerr();

	int wrap = texture->repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE;

	if(texture->depth_buffer)
	{
		texture->prev_id = 1;
		texture->draw_id = 1;
	}

	glBindTexture(targ, texture->bufs[i].id); glerr();
	glTexParameterf(targ, GL_TEXTURE_MAG_FILTER, texture->interpolate ? GL_LINEAR : GL_NEAREST);
	glTexParameterf(targ, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(targ, GL_TEXTURE_WRAP_S, wrap);
	glTexParameteri(targ, GL_TEXTURE_WRAP_T, wrap);
	if(texture->mipmaped)
	{
		glTexParameteri(targ, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(targ, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	}

	if(dims == 4)
	{
		texture->bufs[i].format   = GL_RGBA;
		texture->bufs[i].internal = is_float ? GL_RGBA16F : GL_RGBA;
	}
	else if(dims == 3)
	{
		texture->bufs[i].format   = GL_RGB;
		texture->bufs[i].internal = is_float ? GL_RGB16F : GL_RGB;
	}
	else if(dims == 2)
	{
		texture->bufs[i].format   = GL_RG;
		texture->bufs[i].internal = is_float ? GL_RG16F : GL_RG;
	}
	else
	{
		if(i > 0) perror("Depth component must be added first\n");
		texture->bufs[i].format = GL_DEPTH_COMPONENT;
		texture->bufs[i].internal = GL_DEPTH_COMPONENT;
		texture->depth_buffer = 1;
	}

	texture->bufs[i].dims = dims;

	texture_alloc_buffer(texture, i);

	if(texture->mipmaped)
	{
		glGenerateMipmap(texture->target); glerr();
	}

	glBindTexture(targ, 0);
	glActiveTexture(GL_TEXTURE0);
	texture->framebuffer_ready = 0;

	return i;
}

__thread texture_t *_g_tex_creating = NULL;

texture_t *_texture_new_2D_pre
(
	uint width,
	uint height,
	uint flags
)
{
	texture_t *self = calloc(1, sizeof *self);
	_g_tex_creating = self;

	self->target = GL_TEXTURE_2D;

	self->repeat = !!(flags & TEX_REPEAT);
	self->mipmaped = !!(flags & TEX_MIPMAP);
	self->interpolate = !!(flags & TEX_INTERPOLATE);

	self->width = width;
	self->height = height;

	return self;
}

texture_t *_texture_new(int ignore, ...)
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

}

texture_t *texture_new_3D
(
	uint width,
	uint height,
	uint depth,
	int dims
)
{
	texture_t *self = calloc(1, sizeof *self);

	self->target = GL_TEXTURE_3D;

	self->bufs[0].dims = dims;
	if(dims == 4)
	{
		self->bufs[0].format	= GL_RGBA;
		self->bufs[0].internal = GL_RGBA16F;
	}
	else if(dims == 3)
	{
		self->bufs[0].format	= GL_RGB;
		self->bufs[0].internal = GL_RGB16F;
	}
	else if(dims == 2)
	{
		self->bufs[0].format	= GL_RG;
		self->bufs[0].internal = GL_RG16F;
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


	loader_push(g_candle->loader, (loader_cb)texture_new_3D_loader, self, NULL);

	return self;
}

void texture_set_xyz(texture_t *self, int x, int y, int z,
		GLubyte r, GLubyte g, GLubyte b, GLubyte a)
{
	int i = (x + (y + z * self->height) * self->width) * 4;
	self->bufs[0].data[i + 0] = r;
	self->bufs[0].data[i + 1] = g;
	self->bufs[0].data[i + 2] = b;
	self->bufs[0].data[i + 3] = a;
}

void texture_set_xy(texture_t *self, int x, int y,
		GLubyte r, GLubyte g, GLubyte b, GLubyte a)
{
	int i = (x + y * self->width) * 4;
	self->bufs[0].data[i + 0] = r;
	self->bufs[0].data[i + 1] = g;
	self->bufs[0].data[i + 2] = b;
	self->bufs[0].data[i + 3] = a;
}

texture_t *texture_load(texture_t *self, const char *filename)
{
	texture_t temp = {.target = GL_TEXTURE_2D};
	int result = 0;
	result = LoadTGA(&temp, filename);
	if(!result)
	{
		printf("Could not find texture file: %s\n", filename);
		return NULL;
	}

	*self = temp;
	strncpy(self->name, filename, sizeof(self->name));
	self->filename = strdup(filename);
	self->draw_id = 0;
	self->prev_id = 0;
	self->bufs_size = 1;
	self->bufs[0].name = strdup("color");

	loader_push(g_candle->loader, (loader_cb)texture_from_file_loader, self, NULL);

    return self;
}
texture_t *texture_from_file(const char *filename)
{
	texture_t temp = {.target = GL_TEXTURE_2D};
	int result = 0;

	result = LoadTGA(&temp, filename);

	if(!result)
	{
		printf("Could not find texture file: %s\n", filename);
		return NULL;
	}

	texture_t *self = calloc(1, sizeof *self);
	*self = temp;
	strncpy(self->name, filename, sizeof(self->name));
	self->filename = strdup(filename);
	self->draw_id = 0;
	self->prev_id = 0;
	self->bufs_size = 1;
	self->bufs[0].name = strdup("color");

	loader_push(g_candle->loader, (loader_cb)texture_from_file_loader, self, NULL);

    return self;
}

static int texture_2D_frame_buffer(texture_t *self)
{
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	if(!self->frame_buffer[0])
	{
		glGenFramebuffers(1, self->frame_buffer);
	}
	GLuint targ = self->target;

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, self->frame_buffer[0]);

	int i;

	for(i = self->depth_buffer; i < self->bufs_size; i++)
	{
		if(!self->bufs[i].ready)
		{
			glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,
					GL_COLOR_ATTACHMENT0 + i - self->depth_buffer, targ,
					self->bufs[i].id, 0);
			self->bufs[i].ready = 1;
		}
	}

	/* glDrawBuffers(self->bufs_size - self->depth_buffer, attachments); */

	/* GLuint status = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER); */
	/* if(status != GL_FRAMEBUFFER_COMPLETE) */
	/* { */
		/* printf("Failed to create framebuffer!\n"); */
		/* exit(1); */
	/* } */

	/* glBindTexture(targ, 0); */
	/* glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); */
	/* glerr(); */

	self->framebuffer_ready = 1;
	return 1;
}

int texture_target_sub(texture_t *self, int width, int height,
		texture_t *depth, int fb) 
{
	if(!self->bufs[self->depth_buffer].id) return 0;
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

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, self->frame_buffer[fb]); glerr();

	if(self->last_depth != depth)
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

int texture_target(texture_t *self, texture_t *depth, int fb)
{
	return texture_target_sub(self, self->width, self->height, depth, fb);
}

void texture_draw_id(texture_t *self, int tex)
{
	self->draw_id = tex;
}

void texture_bind(texture_t *self, int tex)
{
	tex = tex >= 0 ? tex : self->draw_id;

	if(!self->bufs[tex].id) return;

	glBindTexture(self->target, self->bufs[tex].id); glerr();
}

void texture_destroy(texture_t *self)
{
	int i;

	for(i = 0; i < self->bufs_size; i++)
	{
		if(self->bufs[i].data) free(self->bufs[i].data);
		if(self->bufs[i].id) glDeleteTextures(1, &self->bufs[i].id);
	}
	if(self->frame_buffer[1]) /* CUBEMAP */
	{
		glDeleteFramebuffers(6, &self->frame_buffer[0]);
	}
	else if(self->frame_buffer[0]) /* TEX2D */
	{
		glDeleteFramebuffers(1, &self->frame_buffer[0]);
	}


	/* TODO: free gl buffers and tex */
	free(self);
}

int texture_2D_resize(texture_t *self, int width, int height)
{
	if(self->width == width && self->height == height) return 0;
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glActiveTexture(GL_TEXTURE0 + ID_2D);
	self->width = width;
	self->height = height;

	int i;
	for(i = 0; i < self->bufs_size; i++)
	{
		/* uint Bpp = self->bufs[i].dims; */

		/* if(self->bufs[i].dims == -1) */ 
		/* { */
			/* Bpp = 1 * sizeof(float); */
		/* } */
		/* uint imageSize = Bpp * self->width * self->height; */
		/* self->bufs[i].data = realloc(self->bufs[i].data, imageSize); */

		texture_alloc_buffer(self, i);

		/* glBindTexture(targ, 0); */
		/* self->framebuffer_ready = 0; */
	}

	if(self->mipmaped)
	{
		glGenerateMipmap(self->target); glerr();
	}

	glActiveTexture(GL_TEXTURE0);

	return 1;
}

static int texture_cubemap_frame_buffer(texture_t *self)
{
	int f;
	glActiveTexture(GL_TEXTURE0 + ID_CUBE);
	/* glBindTexture(self->target, self->texId[0]); */

	if(!self->frame_buffer[0])
	{
		glGenFramebuffers(6, self->frame_buffer);
	}
	for(f = 0; f < 6; f++)
	{
		int i;
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

static int texture_cubemap_loader(texture_t *self)
{
	int i;
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
					GL_DEPTH_COMPONENT32, self->width, self->height, 0,
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
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA16F,
					self->width, self->height, 0, GL_RGBA, GL_FLOAT, NULL);
			glerr();
		}
	}

	glBindTexture(self->target, 0); glerr();
	/* texture_cubemap_frame_buffer(self); */

	glActiveTexture(GL_TEXTURE0);

	return 1;
}

texture_t *texture_cubemap
(
	uint width,
	uint height,
	uint depth_buffer
)
{
	texture_t *self = calloc(1, sizeof *self);
	self->id = g_tex_num++;
	self->target = GL_TEXTURE_CUBE_MAP;

	self->width = width;
	self->height = height;
	self->depth_buffer = depth_buffer;
	self->draw_id = 0;
	self->prev_id = 0;
	self->mipmaped = 0;

	self->bufs[0].dims = -1;
	self->bufs[1].dims = 4;
	self->bufs[0].name = strdup("depth");
	self->bufs[1].name = strdup("color");
	self->bufs_size = 2;

	loader_push(g_candle->loader, (loader_cb)texture_cubemap_loader, self, NULL);

	return self;
}

typedef struct
{
	GLubyte Header[12];
} TGAHeader;

typedef struct
{
	GLubyte			header[6];
	uint	temp;
	uint	format;
} TGA;

int LoadTGA(texture_t *self, const char * filename)
{
	TGAHeader tgaheader;
	TGA tga;
	GLubyte *colorbuffer = NULL;

	FILE * fTGA;
	fTGA = fopen(filename, "rb");
	int compressed;
	GLubyte uTGAcompare[12] = {0,0,2, 0,0,0,0,0,0,0,0,0};	// Uncompressed TGA Header
	GLubyte cTGAcompare[12] = {0,0,10,0,0,0,0,0,0,0,0,0};	// Compressed TGA Header

	if(fTGA == NULL)
	{
		return 0;
	}

	if(fread(&tgaheader, sizeof(TGAHeader), 1, fTGA) == 0)
	{
		printf("Could not read file header\n");
		goto fail;
	}

	if(memcmp(uTGAcompare, &tgaheader, sizeof(tgaheader)) == 0)
	{
		compressed = 0;
	}
	else if(memcmp(cTGAcompare, &tgaheader, sizeof(tgaheader)) == 0)
	{
		compressed = 1;
	}
	else
	{
		printf("TGA file be type 2 or type 10 \n");
		goto fail;
	}

	if(fread(tga.header, sizeof(tga.header), 1, fTGA) == 0)
	{
		printf("Could not read info header\n");
		goto fail;
	}

	self->width  = tga.header[1] * 256 + tga.header[0];
	self->height = tga.header[3] * 256 + tga.header[2];
	int bpp	 = tga.header[4];

	if((self->width <= 0) || (self->height <= 0) ||
			((bpp != 24) && (bpp != 32)))
	{
		printf("Invalid texture information\n");
		goto fail;
	}

	if(bpp == 24)
	{
		self->bufs[0].format	= GL_RGB;
		self->bufs[0].internal = GL_RGB16F;
	}
	else
	{
		self->bufs[0].format	= GL_RGBA;
		self->bufs[0].internal = GL_RGBA16F;
	}

	uint bytesPerPixel = bpp / 8;
	uint imageSize;
	imageSize		= (bytesPerPixel * self->width * self->height);
	self->bufs[0].data = malloc(imageSize);

	if(self->bufs[0].data == NULL)
	{
		goto fail;
	}

	uint pixelcount	= self->height * self->width;
	uint currentpixel	= 0;
	uint currentbyte	= 0;
	colorbuffer = malloc(bytesPerPixel);

	if(compressed)
	{
		do
		{
			GLubyte chunkheader = 0;

			if(fread(&chunkheader, sizeof(GLubyte), 1, fTGA) == 0)
			{
				printf("Could not read RLE header\n");
				goto fail;
			}

			if(chunkheader < 128)
			{
				chunkheader++;
				for(short counter = 0; counter < chunkheader; counter++)
				{
					if(fread(colorbuffer, 1, bytesPerPixel, fTGA) != bytesPerPixel)
					{
						printf("Could not read image data\n");
						goto fail;
					}
					self->bufs[0].data[currentbyte		] = colorbuffer[2];
					self->bufs[0].data[currentbyte + 1	] = colorbuffer[1];
					self->bufs[0].data[currentbyte + 2	] = colorbuffer[0];

					if(bytesPerPixel == 4)
					{
						self->bufs[0].data[currentbyte + 3] = colorbuffer[3];
					}

					currentbyte += bytesPerPixel;
					currentpixel++;

					if(currentpixel > pixelcount)
					{
						printf("Too many pixels read\n");
						goto fail;
					}
				}
			}
			else
			{
				chunkheader -= 127;
				if(fread(colorbuffer, 1, bytesPerPixel, fTGA) != bytesPerPixel)
				{	
					printf("Could not read from file\n");
					goto fail;
				}

				for(short counter = 0; counter < chunkheader; counter++)
				{
					self->bufs[0].data[currentbyte		] = colorbuffer[2];
					self->bufs[0].data[currentbyte + 1	] = colorbuffer[1];
					self->bufs[0].data[currentbyte + 2	] = colorbuffer[0];

					if(bytesPerPixel == 4)
					{
						self->bufs[0].data[currentbyte + 3] = colorbuffer[3];
					}

					currentbyte += bytesPerPixel;
					currentpixel++;

					if(currentpixel > pixelcount)
					{
						printf("Too many pixels read\n");
						goto fail;
					}
				}
			}
		}
		while(currentpixel < pixelcount);
	}
	else
	{
		if(fread(self->bufs[0].data, 1, imageSize, fTGA) != imageSize)	// Attempt to read image data
		{
			printf("Could not read image data\n");		// Display Error
			goto fail;
		}

		uint cswap;
		// Byte Swapping Optimized By Steve Thomas
		for(cswap = 0; cswap < imageSize; cswap += bytesPerPixel)
		{
			uint tmp = self->bufs[0].data[cswap];
			self->bufs[0].data[cswap] = self->bufs[0].data[cswap+2] ;
			self->bufs[0].data[cswap+2] = tmp;
		}

	}

	fclose(fTGA);

	if(colorbuffer != NULL)
	{
		free(colorbuffer);
	}


	return 1;
fail:

	if(fTGA != NULL)
	{
		fclose(fTGA);
	}	

	if(colorbuffer != NULL)
	{
		free(colorbuffer);
	}

	if(self->bufs[0].data != NULL)
	{
		free(self->bufs[0].data);
	}

	return 0;
}
