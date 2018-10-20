#include "texture.h"
#include "file.h"
#include <candle.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ASSERT(x)
#include <utils/stb_image.h>

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
	glerr();
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

struct tpair {
	texture_t *tex;
	int i;
};
static int alloc_buffer_gl(struct tpair *data)
{
	texture_t *self = data->tex;
	int i = data->i;

	if(!self->bufs[i].id)
	{
		glGenTextures(1, &self->bufs[i].id); glerr();
	}
	uint wrap = self->repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE;

	glActiveTexture(GL_TEXTURE0 + ID_2D);
	glBindTexture(self->target, self->bufs[i].id); glerr();
	glTexImage2D(self->target, 0, self->bufs[i].internal,
			self->width, self->height,
			0, self->bufs[i].format,
			(self->bufs[i].dims == -1) ? GL_FLOAT : GL_UNSIGNED_BYTE, NULL); glerr();

	glTexParameteri(self->target, GL_TEXTURE_WRAP_S, wrap);
	glTexParameteri(self->target, GL_TEXTURE_WRAP_T, wrap);
	glerr();

	if(self->mipmaped)
	{
		glTexParameteri(self->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(self->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glGenerateMipmap(self->target); glerr();
	}
	else
	{
		glTexParameteri(self->target, GL_TEXTURE_MAG_FILTER, self->interpolate ?
				GL_LINEAR : GL_NEAREST);
		glTexParameteri(self->target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glerr();
	}

	if(!self->bufs[i].handle)
	{
		self->bufs[i].handle = glGetTextureHandleARB(self->bufs[i].id); glerr();
		glMakeTextureHandleResidentARB(self->bufs[i].handle); glerr();
	}
	glBindTexture(self->target, 0); glerr();

	self->bufs[i].ready = 0;
	glActiveTexture(GL_TEXTURE0);

	return 1;
}

void texture_alloc_buffer(texture_t *self, int i)
{
	self->last_depth = NULL;
	self->framebuffer_ready = 0;
	loader_push_wait(g_candle->loader, (loader_cb)alloc_buffer_gl,
			&((struct tpair){self, i}), NULL);
}

unsigned int texture_handle(texture_t *self, int tex)
{
	tex = tex >= 0 ? tex : self->draw_id;

	return self->bufs[tex].handle;
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
	if(!self->bufs[0].handle)
	{
		self->bufs[0].handle = glGetTextureHandleARB(self->bufs[0].id); glerr();
		glMakeTextureHandleResidentARB(self->bufs[0].handle); glerr();
	}

	glActiveTexture(GL_TEXTURE0);

	return 1;
}
/* static int texture_from_file_loader(texture_t *self) */
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

int buffer_new(const char *name, int is_float, int dims)
{
	texture_t *texture = _g_tex_creating;

	int i = texture->bufs_size++;
	texture->bufs[i].name = strdup(name);

	if(texture->depth_buffer)
	{
		texture->prev_id = 1;
		texture->draw_id = 1;
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
	else if(dims == 1)
	{
		texture->bufs[i].format   = GL_RED;
		texture->bufs[i].internal = is_float ? GL_R16F : GL_RED;
	}
	else if(dims == -1)
	{
		if(i > 0) perror("Depth component must be added first\n");
		texture->bufs[i].format = GL_DEPTH_COMPONENT;
		texture->bufs[i].internal = GL_DEPTH_COMPONENT;
		texture->depth_buffer = 1;
	}

	texture->bufs[i].dims = dims;

	texture_alloc_buffer(texture, i);

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

	glerr();
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

int texture_load_from_memory(texture_t *self, void *buffer, int len)
{
	texture_t temp = {.target = GL_TEXTURE_2D};

	temp.bufs[0].dims = 0;
	temp.bufs[0].data = stbi_load_from_memory(buffer, len, (int*)&temp.width,
			(int*)&temp.height, &temp.bufs[0].dims, 0);

	if(!temp.bufs[0].data)
	{
		printf("Could not load from memory!\n");
		return 0;
	}
	*self = temp;

	switch(self->bufs[0].dims)
	{
		case 1:	self->bufs[0].format	= GL_RED;
				self->bufs[0].internal = GL_RED;
				break;
		case 2:	self->bufs[0].format	= GL_RG;
				self->bufs[0].internal = GL_RG;
				break;
		case 3:	self->bufs[0].format	= GL_RGB;
				self->bufs[0].internal = GL_RGB;
				break;
		case 4: self->bufs[0].format	= GL_RGBA;
				self->bufs[0].internal = GL_RGBA;
				break;
	}


	strncpy(self->name, "unnamed", sizeof(self->name));
	self->filename = "unnamed";
	self->draw_id = 0;
	self->prev_id = 0;
	self->bufs_size = 1;
	self->bufs[0].name = strdup("color");

	loader_push(g_candle->loader, (loader_cb)texture_from_file_loader, self, NULL);
	return 1;
}

int texture_load(texture_t *self, const char *filename)
{
	texture_t temp = {.target = GL_TEXTURE_2D};

	temp.bufs[0].dims = 0;
	temp.bufs[0].data = stbi_load(filename, (int*)&temp.width,
			(int*)&temp.height, &temp.bufs[0].dims, 0);

	if(!temp.bufs[0].data)
	{
		printf("Could not find texture file: %s\n", filename);
		return 0;
	}
	*self = temp;

	switch(self->bufs[0].dims)
	{
		case 1:	self->bufs[0].format	= GL_RED;
				self->bufs[0].internal = GL_RED;
				break;
		case 2:	self->bufs[0].format	= GL_RG;
				self->bufs[0].internal = GL_RG;
				break;
		case 3:	self->bufs[0].format	= GL_RGB;
				self->bufs[0].internal = GL_RGB;
				break;
		case 4: self->bufs[0].format	= GL_RGBA;
				self->bufs[0].internal = GL_RGBA;
				break;
	}


	strncpy(self->name, filename, sizeof(self->name));
	self->filename = strdup(filename);
	self->draw_id = 0;
	self->prev_id = 0;
	self->bufs_size = 1;
	self->bufs[0].name = strdup("color");

	loader_push(g_candle->loader, (loader_cb)texture_from_file_loader, self, NULL);
	return 1;
}

texture_t *texture_from_memory(void *buffer, int len)
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

	self->framebuffer_ready = 1;
	return 1;
}

int texture_target_sub(texture_t *self, int width, int height,
		texture_t *depth, int fb) 
{
	if(!self->bufs[self->depth_buffer /* this is the color buffer 0 */ ].id)
	{
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

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, self->frame_buffer[fb]); glerr();

	if(self->last_depth != depth && self->target == GL_TEXTURE_2D)
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
	/* printf("This shouldn't be used anymore\n"); exit(1); */
	tex = tex >= 0 ? tex : self->draw_id;

	if(!self->bufs[tex].id) return;

	glBindTexture(self->target, self->bufs[tex].id); glerr();
}

void texture_destroy(texture_t *self)
{
	int i;

	for(i = 0; i < self->bufs_size; i++)
	{
		if(self->bufs[i].data) stbi_image_free(self->bufs[i].data);
		if(self->bufs[i].id) glDeleteTextures(1, &self->bufs[i].id);
		if(self->bufs[i].handle) glMakeTextureHandleNonResidentARB(self->bufs[i].handle);
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

		if(self->bufs[i].id)
		{
			glMakeTextureHandleNonResidentARB(self->bufs[i].handle); glerr();
			glDeleteTextures(1, &self->bufs[i].id);
			self->bufs[i].handle = 0;
			self->bufs[i].id = 0;
		}

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
	if(!self->bufs[1].handle)
	{
		self->bufs[1].handle = glGetTextureHandleARB(self->bufs[1].id); glerr();
		glMakeTextureHandleResidentARB(self->bufs[1].handle); glerr();
	}


	glBindTexture(self->target, 0); glerr();
	/* texture_cubemap_frame_buffer(self); */

	glActiveTexture(GL_TEXTURE0);

	glerr();
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

int load_tex(texture_t *texture)
{
	char buffer[256];
	strcpy(buffer, texture->name);
	texture_load(texture, buffer);
	return 1;
}

void *tex_loader(const char *path, const char *name, uint ext)
{
	texture_t *texture = texture_new_2D(0, 0, TEX_INTERPOLATE);
	strcpy(texture->name, path);

	SDL_CreateThread((int(*)(void*))load_tex, "load_tex", texture);

	return texture;
}

void textures_reg()
{
	sauces_loader(ref("png"), tex_loader);
	sauces_loader(ref("tga"), tex_loader);
	sauces_loader(ref("jpg"), tex_loader);
}
