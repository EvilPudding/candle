#ifndef RESAUCES_H
#define RESAUCES_H

#include <ecm.h>
#include <mesh.h>
#include <texture.h>
#include <material.h>

typedef struct c_sauces_t
{
	c_t super;

	material_t **materials;
	uint materials_size;

	texture_t **textures;
	uint textures_size;

	mesh_t **meshes;
	uint meshes_size;

} c_sauces_t;

DEF_CASTER(ct_sauces, c_sauces, c_sauces_t)

c_sauces_t *c_sauces_new(void);
void c_sauces_register(ecm_t *ecm);

material_t *c_sauces_material_get(c_sauces_t *self, const char *name);
int c_sauces_get_materials_at(c_sauces_t *self, const char *dir_name);
void c_sauces_material_reg(c_sauces_t *self, const char *name, material_t *material);

texture_t *c_sauces_texture_get(c_sauces_t *self, const char *name);
void c_sauces_texture_reg(c_sauces_t *self, const char *name, texture_t *texture);

mesh_t *c_sauces_mesh_get(c_sauces_t *self, const char *name);
void c_sauces_mesh_reg(c_sauces_t *self, const char *name, mesh_t *mesh);

#define sauces_mat_at(name) (c_sauces_get_materials_at(c_sauces(&candle->systems), name))
#define sauces_mat(name) (c_sauces_material_get(c_sauces(&candle->systems), name))
#define sauces_mesh(name) (c_sauces_mesh_get(c_sauces(&candle->systems), name))
#define sauces_tex(name) (c_sauces_texture_get(c_sauces(&candle->systems), name))

#endif /* !RESAUCES_H */
