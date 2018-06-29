#ifndef RESAUCES_H
#define RESAUCES_H

#include <ecm.h>
#include <utils/mesh.h>
#include <utils/texture.h>
#include <utils/material.h>

typedef struct
{
	char name[64];
	char path[256];
	void *data;
} resource_t;
KHASH_MAP_INIT_INT(res, resource_t)

typedef struct c_sauces_t
{
	c_t super;

	khash_t(res) *meshes;
	khash_t(res) *textures;
	khash_t(res) *materials;

} c_sauces_t;

DEF_CASTER("sauces", c_sauces, c_sauces_t)

c_sauces_t *c_sauces_new(void);
void c_sauces_register(void);

resource_t *c_sauces_get(c_sauces_t *self, khash_t(res) *hash, const char *name);
int c_sauces_index_dir(c_sauces_t *self, const char *dir_name);
mat_t *c_sauces_mat_get(c_sauces_t *self, const char *name);
int c_sauces_get_mats_at(c_sauces_t *self, const char *dir_name);

texture_t *c_sauces_texture_get(c_sauces_t *self, const char *name);

mesh_t *c_sauces_mesh_get(c_sauces_t *self, const char *name);

entity_t c_sauces_model_get(c_sauces_t *self, const char *name, float scale);

#define sauces_mat_at(name) (c_sauces_get_mats_at(c_sauces(&SYS), name))
#define sauces_mat(name) (c_sauces_mat_get(c_sauces(&SYS), name))
#define sauces_mesh(name) (c_sauces_mesh_get(c_sauces(&SYS), name))
#define sauces_model(name, scale) (c_sauces_model_get(c_sauces(&SYS), name, scale))
#define sauces_tex(name) (c_sauces_texture_get(c_sauces(&SYS), name))

#endif /* !RESAUCES_H */
