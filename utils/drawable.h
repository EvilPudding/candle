#ifndef MESH_GL_H
#define MESH_GL_H

#include "khash.h"
#include "shader.h"

/* TODO maybe skin_t should not be defined on a component header */
#include "../components/skin.h"
#ifdef SHORT_IND
typedef uint16_t vertid_t;
#define IDTYPE GL_UNSIGNED_SHORT
#else
typedef uint32_t vertid_t;
#define IDTYPE GL_UNSIGNED_INT
#endif

#include "mesh.h"
#include "material.h"

typedef void(*draw_cb)(void *usrptr, shader_t *shader);

typedef struct
{
	/* VERTEX DATA */
	vecN_t *pos; /* 0 */
	vec3_t *nor; /* 1 */
	vec2_t *tex; /* 2 */
	vec3_t *tan; /* 3 */
	vec2_t *id;  /* 4 */
	vec3_t *col; /* 5 */

	vec4_t *bid; /* 6 */
	vec4_t *wei; /* 7 */

	vertid_t *ind;

	int32_t vert_num;
	int32_t vert_alloc;
	int32_t vert_num_gl;

	int32_t ind_num;
	int32_t ind_alloc;
	int32_t ind_num_gl;

	bool_t updating;
	uint32_t vbo[9];

	int32_t update_id_gl;
	int32_t update_id_ram;

	mesh_t *mesh;
} varray_t;

typedef struct
{
	mat4_t bones[32];
} gl_skin_t;

struct conf_vars
{
	mesh_t *mesh;
	skin_t *skin;
	texture_t *custom_texture;
	vs_t *vs;
	draw_cb draw_callback;
	void *usrptr;
	int32_t xray;
	int32_t mat_type;
};
struct draw_conf;

struct draw_bind
{
	uint32_t grp;
	int32_t instance_id;
	struct draw_conf *conf;
	uint32_t updates;
};

typedef struct draw_conf
{
	varray_t *varray;

	mat4_t *inst;	/* 8 */
	vec4_t *props;	/* 12 */
#ifdef MESH4
	float *angle4;	/* 13 */
#endif
	struct draw_bind **comps;

	int inst_num;
	int inst_alloc;
	int gl_inst_num;
	/* ----------- */

	uint32_t vao;
	uint32_t vbo[5];
	uint32_t skin;

	struct conf_vars vars;
	void *mtx;
	int32_t trans_updates;
	int32_t props_updates;
	int32_t last_update_frame;
	uint32_t last_skin_update;
} draw_conf_t;

typedef struct drawable
{
	/* int groups_num; */

	mesh_t *mesh;
	skin_t *skin;
	texture_t *custom_texture;
	int32_t xray;
	mat_t *mat;
	uint32_t matid;
	mat4_t transform;
#ifdef MESH4
	float angle4;
#endif
	entity_t entity;
	draw_cb draw_callback;
	vs_t *vs;

	struct draw_bind bind[8];
	uint32_t bind_num;

	void *usrptr;
} drawable_t;

KHASH_MAP_INIT_INT(config, draw_conf_t*)

typedef struct
{
	khash_t(config) *configs;
	uint32_t update_id;
} draw_group_t;

void drawable_init(drawable_t *self, uint32_t group);

void drawable_update(drawable_t *self);

void drawable_add_group(drawable_t *self, uint32_t group);
void drawable_remove_group(drawable_t *self, uint32_t group);
void drawable_set_group(drawable_t *self, uint32_t group);
void drawable_set_mesh(drawable_t *self, mesh_t *mesh);
void drawable_set_skin(drawable_t *self, skin_t *skin);
void drawable_set_mat(drawable_t *self, mat_t *mat);
void drawable_set_matid(drawable_t *self, uint32_t matid);
void drawable_set_vs(drawable_t *self, vs_t *vs);
void drawable_set_xray(drawable_t *self, int32_t xray);
void drawable_set_entity(drawable_t *self, entity_t entity);
void drawable_set_callback(drawable_t *self, draw_cb cb, void *usrptr);
void drawable_set_texture(drawable_t *self, texture_t *texture);

int32_t drawable_draw(drawable_t *self);

void drawable_set_transform(drawable_t *self, mat4_t transform);

#ifdef MESH4
void drawable_set_angle4(drawable_t *self, float angle4);
#endif

void drawable_model_changed(drawable_t *self);

void draw_group(uint32_t ref);

void draw_groups_init(void);

#endif /* !MESH_GL_H */
