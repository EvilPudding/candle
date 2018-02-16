#ifndef MESH_GL_H
#define MESH_GL_H

#include <ecm.h>
#include <glutil.h>

typedef struct c_mesh_gl_t c_mesh_gl_t;

typedef struct
{
	/* VERTEX DATA */
	vec2_t *tex;
	vec3_t *nor;
	vec3_t *tan;
	vec3_t *bit;
#ifdef MESH4
	vec4_t *pos;
#else
	vec3_t *pos;
#endif
	vec4_t *col;

	int vert_num; int vert_alloc;
	/* ----------- */

	unsigned int *ind;
	int ind_num; int ind_alloc;

	GLuint vao;
	GLuint vbo[7];
	int gl_ind_num;
	int gl_vert_num;

	int ready;

	entity_t entity;
	int layer_id;

	int updated;
	int update_id;
} glg_t;

typedef struct c_mesh_gl_t
{
	c_t super; /* extends c_t */

	mesh_t *mesh;

	glg_t groups[8];
	int groups_num;


	int ram_update_id;
} c_mesh_gl_t;


DEF_CASTER(ct_mesh_gl, c_mesh_gl, c_mesh_gl_t)

c_mesh_gl_t *c_mesh_gl_new(void);

void c_mesh_gl_register(void);

int c_mesh_gl_draw(c_mesh_gl_t *self, shader_t *shader, int transparent);

int c_mesh_gl_updated(c_mesh_gl_t *self);
void c_mesh_gl_update(c_mesh_gl_t *self);

#endif /* !MESH_GL_H */
