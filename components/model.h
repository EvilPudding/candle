#ifndef MODEL_H
#define MODEL_H

#include <ecm.h>

typedef struct
{
	int selection;
	material_t *mat;
	int wireframe;
	int cull_face;
	int update_id;
} mat_layer_t;

typedef struct
{
	c_t super; /* extends c_t */

	mesh_t *mesh;

	mat_layer_t layers[16];
	int layers_num;

	int visible;
	int cast_shadow;
	before_draw_cb before_draw;
} c_model_t;


extern unsigned long ct_model;

extern unsigned long mesh_changed;
extern unsigned long render_model;

DEF_CASTER(ct_model, c_model, c_model_t)

c_model_t *c_model_new(mesh_t *mesh, int cast_shadow);
c_model_t *c_model_cull_face(c_model_t *self, int layer, int inverted);
c_model_t *c_model_wireframe(c_model_t *self, int layer, int wireframe);
c_model_t *c_model_paint(c_model_t *self, int layer, material_t *mat);
int c_model_render(c_model_t *self, int transparent, shader_t *shader);
void c_model_register(ecm_t *ecm);
void c_model_set_mesh(c_model_t *self, mesh_t *mesh);

#endif /* !MODEL_H */
