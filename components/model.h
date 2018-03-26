#ifndef MODEL_H
#define MODEL_H

#include <ecm.h>
#include <systems/renderer.h>

typedef struct
{
	int selection;
	mat_t *mat;
	int wireframe;
	int cull_front;
	int cull_back;
	int update_id;
	float offset;
	float smooth_angle;
} mat_layer_t;

typedef struct
{
	c_t super; /* extends c_t */

	mesh_t *mesh;

	mat_layer_t *layers;
	int layers_num;

	int visible;
	int cast_shadow;
	before_draw_cb before_draw;
	int decal;
	int sprite;
} c_model_t;


DEF_SIG(mesh_changed);

DEF_CASTER(ct_model, c_model, c_model_t)
extern int g_update_id;

c_model_t *c_model_new(mesh_t *mesh, mat_t *mat, int cast_shadow);
c_model_t *c_model_cull_face(c_model_t *self, int layer, int inverted);
c_model_t *c_model_wireframe(c_model_t *self, int layer, int wireframe);
c_model_t *c_model_paint(c_model_t *self, int layer, mat_t *mat);
int c_model_render_transparent(c_model_t *self);
int c_model_render_visible(c_model_t *self);
int c_model_render(c_model_t *self);
void c_model_register(void);
void c_model_set_mesh(c_model_t *self, mesh_t *mesh);

#endif /* !MODEL_H */
