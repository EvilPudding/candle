#ifndef MATERIAL_H
#define MATERIAL_H

#include "texture.h"
#include "mafs.h"

typedef struct candle_t candle_t;
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
	prop_t reflection;
	prop_t normal;
	prop_t transparency;
	/* prop_t position; */
	/* prop_t position; */

	vec4_t ambient_light;
} material_t;

material_t *material_new(const char *name);
material_t *material_from_file(const char *filename, candle_t *candle);
material_t *material_from_dir(const char *name, const char *dirname,
		candle_t *candle);
void material_bind(material_t *self, shader_t *shader);
void material_set_diffuse(material_t *self, prop_t diffuse);
void material_set_normal(material_t *self, prop_t normal);
void material_set_reflection(material_t *self, prop_t reflection);
void material_set_specular(material_t *self, prop_t specular);
void material_set_transparency(material_t *self, prop_t transparency);
/* void material_set_reflection(material_t *self, prop_t reflection); */
void material_set_ambient_light(material_t *self, vec4_t light);
void material_destroy(material_t *self);

#endif /* !MATERIAL_H */
