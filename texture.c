#include "texture.h"
#include "file.h"
#include <candle.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

int LoadTGA(texture_t *self, const char * filename);
static int texture_cubemap_frame_buffer(texture_t *self);
static void texture_frame_buffer(texture_t *self);
static int texture_2D_frame_buffer(texture_t *self);

char g_texture_paths[][256] = {"", "resauces/textures", "resauces/materials"};
int g_texture_paths_size = 3;
int g_tex_num = 0;

static void texture_update_gl_loader(texture_t *self)
{

	if(self->target == GL_TEXTURE_2D)
	{
		glActiveTexture(GL_TEXTURE15);
		glBindTexture(self->target, self->texId[COLOR_TEX]);

		glTexSubImage2D(self->target, 0, 0, 0, self->width, self->height,
				self->format, GL_UNSIGNED_BYTE, self->imageData);
		glerr();
	}
	if(self->target == GL_TEXTURE_3D)
	{
		glActiveTexture(GL_TEXTURE15);
		glBindTexture(self->target, self->texId[COLOR_TEX]);

		glTexSubImage3D(self->target, 0, 0, 0, 0,
				self->width, self->height, self->depth,
				self->format, GL_UNSIGNED_BYTE, self->imageData);
		glerr();
	}
	glBindTexture(self->target, 0); glerr();
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
	texture_bind(self, COLOR_TEX);

	glGetTexImage(self->target, 9, self->format, GL_UNSIGNED_BYTE,
			self->imageData);
	int value = *(char*)self->imageData;
	self->brightness = ((float)value) / 256.0f;
}

static int texture_from_file_loader(texture_t *self)
{
	glActiveTexture(GL_TEXTURE15);

	glGenTextures(1, &self->texId[COLOR_TEX]); glerr();
	glBindTexture(self->target, self->texId[COLOR_TEX]); glerr();
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

	glTexImage2D(self->target, 0, self->format, self->width, self->height, 0, self->format,
			GL_UNSIGNED_BYTE, self->imageData); glerr();
	self->mipmaped = 1;

	glGenerateMipmap(self->target); glerr();

	glTexParameteri(self->target, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(self->target, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(self->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(self->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glerr();

	glActiveTexture(GL_TEXTURE0);
	self->ready = 1;

	return 1;
}

static void texture_new_2D_loader(texture_t *self)
{
	glActiveTexture(GL_TEXTURE15);

	if(self->depth_buffer)
	{
		glGenTextures(2, self->texId); glerr();
	}
	else
	{
		glGenTextures(1, &self->texId[COLOR_TEX]); glerr();
	}

	int wrap = self->repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE;
	/* if(self->color_buffer) */
	{
		glBindTexture(self->target, self->texId[COLOR_TEX]); glerr();
		glTexParameterf(self->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(self->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(self->target, GL_TEXTURE_WRAP_S, wrap);
		glTexParameteri(self->target, GL_TEXTURE_WRAP_T, wrap);

		if(self->mipmaped)
		{
			glTexParameteri(self->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(self->target, GL_TEXTURE_MIN_FILTER,
					GL_LINEAR_MIPMAP_LINEAR); glerr();
		}

		glTexImage2D(self->target, 0, self->internal, self->width, self->height,
				0, self->format, GL_UNSIGNED_BYTE, self->imageData); glerr();

		if(self->mipmaped)
		{
			glGenerateMipmap(self->target); glerr();
		}
	}

	if(self->depth_buffer)
	{
		glBindTexture(self->target, self->texId[DEPTH_TEX]); glerr();
		glTexParameterf(self->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(self->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(self->target, GL_TEXTURE_WRAP_S, wrap);
		glTexParameteri(self->target, GL_TEXTURE_WRAP_T, wrap);

		glTexImage2D(self->target, 0, GL_DEPTH_COMPONENT, self->width,
				self->height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL); glerr();
	}

	/* texture_2D_frame_buffer(self); */

	glBindTexture(self->target, 0); glerr();

	self->ready = 1;
}

int texture_add_buffer(texture_t *self, const char *name, int is_float,
		int dims)
{
	GLuint targ = self->target;
	glActiveTexture(GL_TEXTURE15);

	int i = self->color_buffers_size++;
	self->texNames[COLOR_TEX + i] = strdup(name);

	self->attachments[COLOR_TEX + i] = GL_COLOR_ATTACHMENT0 + i;

	glGenTextures(1, &self->texId[COLOR_TEX + i]); glerr();

	glBindTexture(targ, self->texId[COLOR_TEX + i]); glerr();
	glTexParameterf(targ, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(targ, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	GLuint internal;
	GLuint format;
	if(dims == 4)
	{
		format   = GL_RGBA;
		internal = is_float ? GL_RGBA16F : GL_RGBA;
	}
	else if(dims == 3)
	{
		format   = GL_RGB;
		internal = is_float ? GL_RGB16F : GL_RGB;
	}
	else
	{
		format   = GL_RG;
		internal = is_float ? GL_RG16F : GL_RG;
	}

	glTexImage2D(targ, 0, internal, self->width, self->height, 0, format,
			GL_UNSIGNED_BYTE, NULL); glerr();

	if(self->mipmaped)
	{
		glGenerateMipmap(targ); glerr();
		glTexParameteri(targ, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(targ, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(targ, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(targ, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	}


	glBindTexture(targ, 0);
	self->framebuffer_ready = 0;
	return i;
}

texture_t *texture_new_2D
(
	uint width,
	uint height,
	uint dims,
	uint depth_buffer,
	uint repeat,
	uint is_float,
	uint mipmaped
)
{
	texture_t *self = calloc(1, sizeof *self);
	self->id = g_tex_num++;

	self->target = GL_TEXTURE_2D;

	self->repeat = repeat;
	self->bpp = dims * 8;
	if(dims == 4)
	{
		self->format   = GL_RGBA;
		self->internal = is_float ? GL_RGBA16F : GL_RGBA;
	}
	else if(dims == 3)
	{
		self->format   = GL_RGB;
		self->internal = is_float ? GL_RGB16F : GL_RGB;
	}
	else
	{
		self->format   = GL_RG;
		self->internal = is_float ? GL_RG16F : GL_RG;
	}
	self->width = width;
	self->height = height;
	self->depth_buffer = 1;
	/* self->depth_buffer = depth_buffer; */

	if(self->depth_buffer)
	{
		self->texNames[DEPTH_TEX] = strdup("depth");
	}
	self->texNames[COLOR_TEX] = strdup("diffuse");

	self->color_buffers_size = 1;
	self->attachments[0] = GL_DEPTH_ATTACHMENT;
	self->attachments[1] = GL_COLOR_ATTACHMENT0;

	self->draw_id = COLOR_TEX;
	self->prev_id = COLOR_TEX;

	self->mipmaped = mipmaped;
	uint imageSize = (self->bpp / 8) * width * height;

	self->imageData = calloc(imageSize, 1);
	
	loader_push(g_candle->loader, (loader_cb)texture_new_2D_loader, self, NULL);

	return self;
}

static void texture_new_3D_loader(texture_t *self)
{
	glActiveTexture(GL_TEXTURE15);

	glGenTextures(1, &self->texId[COLOR_TEX]); glerr();
	glBindTexture(self->target, self->texId[COLOR_TEX]); glerr();

	glTexParameterf(self->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(self->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(self->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(self->target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(self->target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
	glerr();

    /* glPixelStorei(GL_UNPACK_ALIGNMENT, 1); glerr(); */

	glTexImage3D(self->target, 0, self->format,
			self->width, self->height, self->depth,
			0, self->format, GL_UNSIGNED_BYTE, self->imageData); glerr();

	glBindTexture(self->target, 0); glerr();

	self->ready = 1;
}

texture_t *texture_new_3D
(
	uint width,
	uint height,
	uint depth,
	uint dims
)
{
	texture_t *self = calloc(1, sizeof *self);
	self->id = g_tex_num++;

	self->target = GL_TEXTURE_3D;

	self->bpp = dims * 8;
	if(dims == 4)
	{
		self->format	= GL_RGBA;
		self->internal = GL_RGBA16F;
	}
	else if(dims == 3)
	{
		self->format	= GL_RGB;
		self->internal = GL_RGB16F;
	}
	else
	{
		self->format	= GL_RG;
		self->internal = GL_RG16F;
	}
	self->width = width;
	self->height = height;
	self->depth = depth;
	self->draw_id = COLOR_TEX;
	self->prev_id = COLOR_TEX;

	self->imageData	= calloc(dims * width * height * depth, 1);

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
	self->imageData[i + 0] = r;
	self->imageData[i + 1] = g;
	self->imageData[i + 2] = b;
	self->imageData[i + 3] = a;
}

void texture_set_xy(texture_t *self, int x, int y,
		GLubyte r, GLubyte g, GLubyte b, GLubyte a)
{
	int i = (x + y * self->width) * 4;
	self->imageData[i + 0] = r;
	self->imageData[i + 1] = g;
	self->imageData[i + 2] = b;
	self->imageData[i + 3] = a;
}

texture_t *texture_from_file(const char *filename)
{
	char buffer[256];
	texture_t temp = {.target = GL_TEXTURE_2D};
	int result = 0;
	int i;
	for(i = 0; i < g_texture_paths_size; i++)
	{
		strncpy(buffer, g_texture_paths[i], sizeof(buffer));
		path_join(buffer, sizeof(buffer), filename);
		strncat(buffer, ".tga", sizeof(buffer));

		result = LoadTGA(&temp, buffer);
		if(result) break;
	}
	if(!result)
	{
		printf("Could not find texture file: %s\n", filename);
		return NULL;
	}

	texture_t *self = calloc(1, sizeof *self);
	*self = temp;
	self->id = g_tex_num++;
	strncpy(self->name, filename, sizeof(self->name));
	self->filename = strdup(buffer);
	self->draw_id = COLOR_TEX;
	self->prev_id = COLOR_TEX;
	self->color_buffers_size = 1;
	self->texNames[COLOR_TEX] = strdup("color");

	loader_push(g_candle->loader, (loader_cb)texture_from_file_loader, self, NULL);

    return self;
}

void texture_draw_id(texture_t *self, int tex)
{
	self->draw_id = tex;
}

void texture_bind(texture_t *self, int tex)
{
	if(!self->ready) return;

	tex = tex >= 0 ? tex : self->draw_id;

	glBindTexture(self->target, self->texId[tex]); glerr();
}

static void texture_frame_buffer(texture_t *self)
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

int texture_target_sub(texture_t *self, int width, int height, int id) 
{
	if(!self->ready) return 0;

	if(!self->framebuffer_ready) texture_frame_buffer(self);

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, self->frame_buffer[id]); glerr();

	/* glDrawBuffers(self->color_buffers_size, self->attachments + COLOR_TEX); */

	glViewport(0, 0, width, height);
	return 1;
}

int texture_target(texture_t *self, int id)
{
	return texture_target_sub(self, self->width, self->height, id);
}

void texture_destroy(texture_t *self)
{
	int i;

	if(self->imageData) free(self->imageData);

	for(i = 0; i < (sizeof(self->texId)/sizeof(*self->texId)); i++)
	{
		if(self->texId[i]) glDeleteTextures(1, &self->texId[i]);
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
	glActiveTexture(GL_TEXTURE15);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	self->width = width;
	self->height = height;

	int i;
	for(i = 0; i < self->color_buffers_size; i++)
	{
		uint imageSize = (self->bpp / 8) * self->width * self->height;
		self->imageData = realloc(self->imageData, imageSize);

		glBindTexture(self->target, self->texId[COLOR_TEX + i]); glerr();
		glTexImage2D(self->target, 0, self->internal, self->width, self->height,
				0, self->format, GL_UNSIGNED_BYTE, self->imageData); glerr();

		if(self->mipmaped)
		{
			glGenerateMipmap(self->target); glerr();
		}

		/* glBindTexture(targ, 0); */
		/* self->framebuffer_ready = 0; */
	}
	if(self->depth_buffer)
	{
		glBindTexture(self->target, self->texId[DEPTH_TEX]); glerr();

		glTexImage2D(self->target, 0, GL_DEPTH_COMPONENT, self->width,
				self->height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL); glerr();
	}

	return 1;
}

static int texture_2D_frame_buffer(texture_t *self)
{
	if(!self->frame_buffer[0])
	{
		glGenFramebuffers(1, self->frame_buffer);
	}
	GLuint targ = self->target;

	/* glBindTexture(targ, self->texId[COLOR_TEX]); */

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, self->frame_buffer[0]);

	int i;

	for(i = 0; i < self->color_buffers_size + 1; i++)
	{
		if(!self->attachments_ready[i])
		{
			self->attachments_ready[i] = 1;
			glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, self->attachments[i],
					targ, self->texId[i], 0);
		}
	}

	glDrawBuffers(self->color_buffers_size, self->attachments + COLOR_TEX);

	GLuint status = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
	if(status != GL_FRAMEBUFFER_COMPLETE)
	{
		printf("Failed to create framebuffer!\n");
		exit(1);
	}

	glBindTexture(targ, 0);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glerr();


	self->framebuffer_ready = 1;
	return 1;
}

static int texture_cubemap_frame_buffer(texture_t *self)
{
	int f;
	/* glActiveTexture(GL_TEXTURE16); */
	/* glBindTexture(self->target, self->texId[COLOR_TEX]); */

	if(!self->frame_buffer[0])
	{
		glGenFramebuffers(6, self->frame_buffer);
	}
	for(f = 0; f < 6; f++)
	{
		int i;
		GLuint targ = GL_TEXTURE_CUBE_MAP_POSITIVE_X + f;

		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, self->frame_buffer[f]);

		for(i = 0; i < self->color_buffers_size + 1; i++)
		{
			glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, self->attachments[i],
					targ, self->texId[i], 0);
		}
	}
	glDrawBuffers(self->color_buffers_size, self->attachments + COLOR_TEX);

	GLuint status = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
	if(status != GL_FRAMEBUFFER_COMPLETE)
	{
		printf("Failed to create framebuffer!\n");
		exit(1);
	}

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glerr();

	self->framebuffer_ready = 1;
	return 1;
}

static int texture_cubemap_loader(texture_t *self)
{
	int i;
	glActiveTexture(GL_TEXTURE15);

	if(self->depth_buffer)
	{
		glGenTextures(2, self->texId); glerr();
	}
	else
	{
		glGenTextures(1, self->texId + COLOR_TEX); glerr();
	}

	if(self->depth_buffer)
	{
		glBindTexture(self->target, self->texId[DEPTH_TEX]); glerr();
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
		glBindTexture(self->target, self->texId[COLOR_TEX]); glerr();
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

	/* texture_cubemap_frame_buffer(self); */

	self->ready = 1;
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
	self->depth_buffer = 1;
	self->depth_buffer = depth_buffer;
	self->draw_id = COLOR_TEX;
	self->prev_id = COLOR_TEX;
	self->mipmaped = 0;

	self->color_buffers_size = 1;
	self->attachments[0] = GL_DEPTH_ATTACHMENT;
	self->attachments[1] = GL_COLOR_ATTACHMENT0;

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
	self->bpp	 = tga.header[4];

	if((self->width <= 0) || (self->height <= 0) || ((self->bpp != 24) && (self->bpp !=32)))
	{
		printf("Invalid texture information\n");
		goto fail;
	}

	if(self->bpp == 24)
	{
		self->format	= GL_RGB;
	}
	else
	{
		self->format	= GL_RGBA;
	}

	uint bytesPerPixel = self->bpp / 8;
	uint imageSize;
	imageSize		= (bytesPerPixel * self->width * self->height);
	self->imageData = malloc(imageSize);

	if(self->imageData == NULL)
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
					self->imageData[currentbyte		] = colorbuffer[2];
					self->imageData[currentbyte + 1	] = colorbuffer[1];
					self->imageData[currentbyte + 2	] = colorbuffer[0];

					if(bytesPerPixel == 4)
					{
						self->imageData[currentbyte + 3] = colorbuffer[3];
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
					self->imageData[currentbyte		] = colorbuffer[2];
					self->imageData[currentbyte + 1	] = colorbuffer[1];
					self->imageData[currentbyte + 2	] = colorbuffer[0];

					if(bytesPerPixel == 4)
					{
						self->imageData[currentbyte + 3] = colorbuffer[3];
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
		if(fread(self->imageData, 1, imageSize, fTGA) != imageSize)	// Attempt to read image data
		{
			printf("Could not read image data\n");		// Display Error
			goto fail;
		}

		uint cswap;
		// Byte Swapping Optimized By Steve Thomas
		for(cswap = 0; cswap < imageSize; cswap += bytesPerPixel)
		{
			uint tmp = self->imageData[cswap];
			self->imageData[cswap] = self->imageData[cswap+2] ;
			self->imageData[cswap+2] = tmp;
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

	if(self->imageData != NULL)
	{
		free(self->imageData);
	}

	return 0;
}
