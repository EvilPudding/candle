#ifndef MESH_GL_H
#define MESH_GL_H

#include <ecs/ecm.h>
#include <utils/glutil.h>

typedef struct c_mesh_gl_t c_mesh_gl_t;

typedef struct skin_t skin_t;

typedef struct
{
	/* VERTEX DATA */
	vec2_t *tex;
	vec3_t *nor;
	vec3_t *tan;
	vecN_t *pos;
	vec2_t *id;
	vec3_t *col;

	uvec4_t *bid;
	vec4_t  *wei;

	mat4_t *inst;

	int vert_num; int vert_alloc;
	int inst_num; int inst_alloc;
	/* ----------- */

	unsigned int *ind;
	int ind_num; int ind_alloc;

	GLuint vao;
	GLuint vbo[24];
	int vbo_num;
	int gl_ind_num;
	int gl_vert_num;
	int gl_inst_num;

	int ready;

	int layer_id;

	int updated;
	int update_id;

	skin_t *skin;
	mesh_t *mesh;
	mat_t *mat;

	int moved;
} glg_t;

typedef struct c_mesh_gl_t
{
	c_t super; /* extends c_t */

	glg_t *glo;
	int instance_id;
	/* int groups_num; */

	int ram_update_id;
} c_mesh_gl_t;


DEF_CASTER("mesh_gl", c_mesh_gl, c_mesh_gl_t)

c_mesh_gl_t *c_mesh_gl_new(void);

int c_mesh_gl_draw(c_mesh_gl_t *self, mat4_t *transform, int transparent);
int c_mesh_gl_draw_ent(c_mesh_gl_t *self, entity_t ent,
		mat4_t *transform, int transparent);

int c_mesh_gl_updated(c_mesh_gl_t *self);
void c_mesh_gl_update(c_mesh_gl_t *self);

#endif /* !MESH_GL_H */
