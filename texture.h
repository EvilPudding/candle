#ifndef TEXTURE_H
#define TEXTURE_H

#include "glutil.h"

#define DEPTH_TEX 0
#define COLOR_TEX 1

#ifndef uint
#	define uint unsigned int
#endif

typedef struct
{
	char name[256];

	GLubyte	*imageData;
	GLuint type;
	GLuint bpp;
	GLuint gl_type;
	uint width;
	uint height;
	uint depth;
	uint depth_buffer;
	uint repeat;

	GLuint texId[16]; /* 0 is depth, 1 is color, >= 2 are color buffers */
	uint attachments[16];

	int color_buffers_size;

	float brightness;
	GLuint target;
	GLuint frame_buffer[6];
	char *filename;
	int ready;
	int draw_id;
} texture_t;

texture_t *texture_from_file(const char *filename);

texture_t *texture_new_2D
(
	uint width,
	uint height,
	uint bpp,
	uint gl_type,
	uint depth_buffer,
	uint repeat
);

texture_t *texture_new_3D
(
	uint width,
	uint height,
	uint depth,
	uint bpp
);

texture_t *texture_cubemap
(
	uint width,
	uint height,
	uint depth_buffer
);

void texture_bind(texture_t *self, int tex);
int texture_target(texture_t *self, int id);
int texture_target_sub(texture_t *self, int width, int height,
		int id);
void texture_destroy(texture_t *self);

void texture_set_xy(texture_t *self, int x, int y,
		GLubyte r, GLubyte g, GLubyte b, GLubyte a);
void texture_set_xyz(texture_t *self, int x, int y, int z,
		GLubyte r, GLubyte g, GLubyte b, GLubyte a);
void texture_update_gl(texture_t *self);

void texture_update_brightness(texture_t *self);
int texture_get_pixel(texture_t *self, int buffer, int x, int y);

void texture_draw_id(texture_t *self, int tex);

int texture_add_buffer(texture_t *self, int is_float, int alpha, int mipmaped);

#endif /* !TEXTURE_H */
