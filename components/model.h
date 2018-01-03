#ifndef MODEL_H
#define MODEL_H

#include <ecm.h>

typedef struct
{
	c_t super; /* extends c_t */

	mesh_t *mesh;
	material_t *mat;
	int visible;
	int cast_shadow;
	before_draw_cb before_draw;
} c_model_t;


extern unsigned long ct_model;

extern unsigned long mesh_changed;
extern unsigned long render_filter;

DEF_CASTER(ct_model, c_model, c_model_t)

c_model_t *c_model_new(mesh_t *mesh, material_t *mat, int cast_shadow);
void c_model_wireframe(c_model_t *self, int wireframe);
void c_model_cull_face(c_model_t *self, int cull_face);
int c_model_draw(c_model_t *self);
void c_model_register(ecm_t *ecm);

#endif /* !MODEL_H */
