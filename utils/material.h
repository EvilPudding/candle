#ifndef MATERIAL_H
#define MATERIAL_H

#include "texture.h"
#include "mafs.h"

typedef struct c_sauces_t c_sauces_t;
typedef struct shader_t shader_t;

typedef struct
{
	float texture_blend;
	texture_t *texture;
	float texture_scale;
	vec4_t color;
} prop_t;

typedef struct
{
	char name[256];

	prop_t diffuse;
	prop_t specular;
	prop_t transparency;
	prop_t normal;
	prop_t emissive;

} mat_t;

extern char g_mats_path[256];

mat_t *mat_new(const char *name);
mat_t *mat_from_file(const char *filename);
mat_t *mat_from_dir(const char *name, const char *dirname);
void mat_bind(mat_t *self, shader_t *shader);
void mat_set_diffuse(mat_t *self, prop_t diffuse);
void mat_set_normal(mat_t *self, prop_t normal);
void mat_set_specular(mat_t *self, prop_t specular);
void mat_set_transparency(mat_t *self, prop_t transparency);
void mat_destroy(mat_t *self);
void mat_menu(mat_t *self, void *ctx);

#endif /* !MATERIAL_H */
