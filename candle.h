#ifndef CREST_H
#define CREST_H

#include <glutil.h>

#include <components/camera.h>
#include <components/freelook.h>
#include <components/freemove.h>
#include <components/rigid_body.h>
#include <components/aabb.h>
#include <components/spacial.h>
#include <components/velocity.h>
#include <components/force.h>
#include <components/name.h>
#include <components/node.h>
#include <components/model.h>
#include <components/mesh_gl.h>
#include <components/probe.h>
#include <components/light.h>
#include <components/ambient.h>
#include <systems/renderer.h>
#include <systems/physics.h>
#include <systems/window.h>

#include <loader.h>

#include <texture.h>

#include <keyboard.h>
#include <mouse.h>

extern unsigned long world_update;
extern unsigned long world_draw;

void candle_register(ecm_t *ecm);

typedef entity_t(*template_cb)(entity_t root, FILE *fd, candle_t *candle);

typedef struct
{
	char key[32];
	template_cb cb;
} template_t;

typedef struct
{
	material_t **materials;
	unsigned long materials_size;

	texture_t **textures;
	unsigned long textures_size;

	mesh_t **meshes;
	unsigned long meshes_size;
} resources_t;

typedef struct candle_t
{
	entity_t systems;
	loader_t *loader;
	ecm_t *ecm;

	char *firstDir;

	int exit;
	int last_update;
	int pressing;
	void *nkctx;

	template_t *templates;
	unsigned long templates_size;

	resources_t resources;
} candle_t;

candle_t *candle_new(int comps_size, ...);
void candle_init(candle_t *self);
void candle_reset_dir(candle_t *self);

material_t *candle_material_get(candle_t *self, const char *name);
void candle_material_reg(candle_t *self, const char *name, material_t *material);
mesh_t *candle_mesh_get(candle_t *self, const char *name);
void candle_mesh_reg(candle_t *self, const char *name, mesh_t *mesh);
texture_t *candle_texture_get(candle_t *self, const char *name);
void candle_texture_reg(candle_t *self, const char *name, texture_t *texture);
void candle_register_template(candle_t *self, const char *key, template_cb cb);
int candle_import(candle_t *self, entity_t root, const char *map_name);
int candle_get_materials_at(candle_t *self, const char *dir_name);
int candle_import_dir(candle_t *self, entity_t root, const char *dir_name);

extern candle_t *candle;

#endif /* !CREST_H */
