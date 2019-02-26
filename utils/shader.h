#ifndef SHADER_H
#define SHADER_H

#include "glutil.h"
#include "texture.h"
#include <ecs/ecm.h>

typedef struct
{
	uint32_t type;
	char *code;
} vertex_modifier_t;

typedef struct
{
	char *name;
	uint32_t index;
	vertex_modifier_t vmodifiers[32];
	vertex_modifier_t gmodifiers[32];
	uint32_t vmodifier_num;
	uint32_t gmodifier_num;
	GLuint vprogram;
	GLuint gprogram;
	uint32_t ready;
	char *vcode;
	char *gcode;
} vs_t;

typedef struct
{
	char *code;
	GLuint program;

	shader_t *combinations[32];

	char *filename;
	uint32_t ready;
} fs_t;

typedef struct
{
	GLuint texture_blend;
	GLuint texture_scale;
	GLuint texture;
	GLuint color;
} u_prop_t;

typedef struct light_t light_t;
typedef struct shader_t
{
	fs_t *fs;
	uint32_t index;
	uint32_t frame_bind;

	GLuint program;

	GLuint u_bones[30];

	uint32_t ready;
} shader_t;

shader_t *vs_bind(vs_t *vs);
void fs_bind(fs_t *fs);

vs_t *vs_new(const char *name, uint32_t num_modifiers, ...);
fs_t *fs_new(const char *filename);
vertex_modifier_t vertex_modifier_new(const char *code);
vertex_modifier_t geometry_modifier_new(const char *code);

shader_t *shader_new(fs_t *fs, vs_t *vs);
#ifdef MESH4
void shader_update(shader_t *self, float angle4);
#endif
void shader_bind(shader_t *self);
GLuint shader_uniform(shader_t *self, const char *uniform, const char *member);
GLuint _shader_uniform(shader_t *self, const char *uniform, const char *member);
void shader_destroy(shader_t *self);
void shader_add_source(const char *name, unsigned char data[],
		uint32_t len);
void shaders_reg(void);

/* TODO this should not be shared */
extern vs_t g_vs[32];
extern fs_t g_fs[32];

#endif /* !SHADER_H */
