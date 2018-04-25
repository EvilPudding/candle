#ifndef MODEL_H
#define MODEL_H

#include <ecm.h>
#include <systems/renderer.h>
#include "node.h"
#include <utils/shader.h>

struct conf_sphere
{
	int usegments;
	int vsegments;
};
struct conf_circle
{
	float radius;
	int segments;
};
struct conf_ico
{
	int subdivisions;
};
struct conf_cube
{
	int size;
	int inverted;
};
struct conf_extrude
{
	vec4_t offset;
	float scale;
	int steps;
};
typedef mesh_t*(*tool_edit_cb)(mesh_t *last, void *new,
		mesh_t *state, void *old);
typedef int(*tool_gui_cb)(void *ctx, void *conf);
struct edit_tool
{
	tool_gui_cb gui;
	tool_edit_cb edit;
	size_t size;
	void *defaults;
	char name[32];
};
extern struct edit_tool g_edit_tools[];

typedef struct
{
	int selection;
	mat_t *mat;
	int wireframe;
	int cull_front;
	int cull_back;
	float offset;
	float smooth_angle;
} mat_layer_t;

typedef enum
{
	MESH_CIRCLE,
	MESH_SPHERE,
	MESH_CUBE,
	MESH_ICOSPHERE,
	MESH_EXTRUDE
} mesh_edit_t;

typedef struct
{
	mesh_t *state;
	geom_t target;
	mesh_edit_t type;
	void *conf_new;
	void *conf_old;
} mesh_history_t;

typedef struct
{
	c_t super; /* extends c_t */

	vector_t *history;
	int history_count;
	mesh_t *mesh;

	mat_layer_t *layers;
	int layers_num;
	float scale_dist;

	int visible;
	int cast_shadow;
	before_draw_cb before_draw;
	int xray;


} c_model_t;

DEF_CASTER("model", c_model, c_model_t)
extern int g_update_id;
extern vs_t *g_model_vs;

void c_model_edit(c_model_t *self, mesh_edit_t type, geom_t target);
void c_model_add_layer(c_model_t *self, mat_t *mat, int selection, float offset);
c_model_t *c_model_new(mesh_t *mesh, mat_t *mat, int cast_shadow, int visible);
c_model_t *c_model_cull_face(c_model_t *self, int layer, int inverted);
c_model_t *c_model_wireframe(c_model_t *self, int layer, int wireframe);
c_model_t *c_model_smooth(c_model_t *self, int layer, int smooth);
c_model_t *c_model_paint(c_model_t *self, int layer, mat_t *mat);
int c_model_render_transparent(c_model_t *self);
int c_model_render_visible(c_model_t *self);
int c_model_render(c_model_t *self, int transp);
int c_model_render_at(c_model_t *self, c_node_t *node, int transp);
void c_model_set_mesh(c_model_t *self, mesh_t *mesh);

#endif /* !MODEL_H */
