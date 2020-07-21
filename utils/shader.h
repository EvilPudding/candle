#ifndef SHADER_H
#define SHADER_H

#include "texture.h"
#include <ecs/ecm.h>
#include <utils/khash.h>

typedef struct
{
	uint32_t type;
	char *code;
} vertex_modifier_t;

typedef struct
{
	char name[32];
	uint32_t index;
	vertex_modifier_t vmodifiers[32];
	vertex_modifier_t gmodifiers[32];
	uint32_t vmodifier_num;
	uint32_t gmodifier_num;
	uint32_t vprogram;
	uint32_t gprogram;
	uint32_t ready;
	bool_t has_skin;
} vs_t;

struct shader;

typedef struct
{
	char *code;
	uint32_t program;

	struct shader *combinations[32];

	char filename[32];
	uint32_t ready;
} fs_variation_t;

typedef struct
{
	fs_variation_t variations[32];
	uint32_t variations_num;
	char filename[32];
} fs_t;

typedef struct
{
	uint32_t texture_blend;
	uint32_t texture_scale;
	uint32_t texture;
	uint32_t color;
} u_prop_t;

KHASH_MAP_INIT_INT(uniform, uint32_t)

typedef struct light_t light_t;
typedef struct shader
{
	fs_variation_t *fs;
	uint32_t index;
	uint32_t fs_variation;
	uint32_t frame_bind;

	uint32_t program;

	uint32_t u_bones[30];

	bool_t ready;

	bool_t has_skin;

	uint32_t scene_ubi;
	uint32_t renderer_ubi;
	uint32_t skin_ubi;
	uint32_t cms_ubi;
	khash_t(uniform) *uniforms;
} shader_t;

shader_t *vs_bind(vs_t *vs, uint32_t fs_variation);
void fs_bind(fs_t *fs);

vs_t *vs_new(const char *name, bool_t has_skin, uint32_t num_modifiers, ...);
fs_t *fs_new(const char *filename);
fs_t *fs_get(const char *filename);
void fs_push_variation(fs_t *self, const char *filename);
void fs_update_variation(fs_t *self, uint32_t fs_variation);
vertex_modifier_t vertex_modifier_new(const char *code);
vertex_modifier_t geometry_modifier_new(const char *code);

shader_t *shader_new(fs_t *fs, uint32_t fs_variation, vs_t *vs);
#ifdef MESH4
void shader_update(shader_t *self, float angle4);
#endif
void shader_bind(shader_t *self);
/* uint32_t shader_uniform(shader_t *self, const char *uniform, const char *member); */
/* uint32_t _shader_uniform(shader_t *self, const char *uniform, const char *member); */
void shader_destroy(shader_t *self);
void shader_add_source(const char *name, char data[], uint32_t len);
void shaders_reg(void);
uint32_t shader_cached_uniform(shader_t *self, uint32_t ref);

/* TODO this should not be shared */
extern vs_t g_vs[64];
extern fs_t g_fs[64];

#endif /* !SHADER_H */
