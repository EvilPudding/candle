#ifndef MESH_GL_H
#define MESH_GL_H

#include <utils/khash.h>
#include <utils/shader.h>

typedef struct drawable_t drawable_t;

typedef struct
{
	/* VERTEX DATA */
	vecN_t *pos;	/* 0 */
	vec3_t *nor;	/* 1 */
	vec2_t *tex;	/* 2 */
	vec3_t *tan;	/* 3 */
	vec2_t *id;		/* 4 */
	vec3_t *col;	/* 5 */

	uvec4_t *bid;	/* 6 */
	vec4_t  *wei;	/* 7 */

	unsigned int *ind;

	int vert_num;
	int vert_alloc;
	int vert_num_gl;

	int ind_num;
	int ind_alloc;
	int ind_num_gl;

	int updating;
	GLuint vbo[24];

	int update_id_gl;
	int update_id_ram;

	mesh_t *mesh;
} varray_t;

struct conf_vars
{
	mesh_t *mesh;
	vs_t *vs;
	int xray;
	int padding;
};

typedef struct
{
	varray_t *varray;

	mat4_t *inst;	/* 8 */
	uvec4_t *props;	/* 12 */
#ifdef MESH4
	float *angle4;	/* 13 */
#endif
	drawable_t **comps;

	int inst_num;
	int inst_alloc;
	int gl_inst_num;
	/* ----------- */

	GLuint vao;
	GLuint vbo[24];

	struct conf_vars vars;
} draw_conf_t;

typedef struct drawable_t
{
	int32_t instance_id;
	/* int groups_num; */

	uint8_t updates; /* updates cover vbos */
	mesh_t *mesh;
	int32_t xray;
	int32_t mat;
	mat4_t transform;
#ifdef MESH4
	float angle4;
#endif
	entity_t entity;
	vs_t *vs;

	int32_t grp;
	int32_t box;
	draw_conf_t *conf;
	void *userptr;
} drawable_t;

KHASH_MAP_INIT_INT(config, draw_conf_t*)

typedef khash_t(config) draw_box_t;
typedef struct
{
	int32_t (*filter)(drawable_t *);
	draw_box_t *boxes[8];
} draw_group_t;

void drawable_init(drawable_t *self, uint32_t group, void *usrptr);

void drawable_update(drawable_t *self);

void drawable_set_group(drawable_t *self, uint32_t group);
void drawable_set_mesh(drawable_t *self, mesh_t *mesh);
void drawable_set_mat(drawable_t *self, int32_t mat);
void drawable_set_vs(drawable_t *self, vs_t *vs);
void drawable_set_xray(drawable_t *self, int32_t xray);
void drawable_set_entity(drawable_t *self, entity_t entity);

int32_t drawable_draw(drawable_t *self);

void drawable_set_transform(drawable_t *self, mat4_t transform);

#ifdef MESH4
void drawable_set_angle4(drawable_t *self, float angle4);
#endif

int32_t drawable_model_changed(drawable_t *self);

void draw_group(uint32_t ref, int32_t filter);
void draw_filter(uint32_t ref, int32_t(*filter)(drawable_t *drawable));

#endif /* !MESH_GL_H */
