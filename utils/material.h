#ifndef MATERIAL_H
#define MATERIAL_H

#include <vil/vil.h>
#include "texture.h"
#include "mafs.h"

typedef struct c_sauces_t c_sauces_t;
typedef struct shader_t shader_t;

typedef struct
{
	float blend;
	texture_t *texture;
	float scale;
	vec4_t color;
} prop_t;

typedef struct
{
	char name[256];

	prop_t albedo;
	prop_t roughness;
	prop_t metalness;
	prop_t transparency;
	prop_t normal;
	prop_t emissive;

	int id;
	struct vitype_t *vil;
} mat_t;

extern char g_mats_path[256];

mat_t *mat_new(const char *name);
mat_t *mat_from_file(const char *filename);
mat_t *mat_from_dir(const char *name, const char *dirname);
void mat_bind(mat_t *self, shader_t *shader);
void mat_set_normal(mat_t *self, prop_t normal);
void mat_set_albedo(mat_t *self, prop_t albedo);
void mat_destroy(mat_t *self);
int mat_menu(mat_t *self, void *ctx);

void materials_init_vil(void);
void materials_reg(void);

extern int g_mats_num;
extern mat_t *g_mats[255];

#endif /* !MATERIAL_H */
