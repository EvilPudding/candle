#ifndef MODEL_H
#define MODEL_H

#include <utils/macros.h>
#include <ecs/ecm.h>
#include "node.h"
#include <utils/shader.h>
#include <utils/drawable.h>

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
struct conf_deform
{
	int normal;
	vecN_t direction;

	char deform_e[255];
	int deform_s;
};
struct conf_spherize
{
	float scale;
	float roundness;
};
struct conf_ico
{
	float radius;
	int subdivisions;
};
struct conf_torus
{
	float radius1;
	float radius2;
	int segments1;
	int segments2;
};
struct conf_disk
{
	float radius1;
	float radius2;
	int segments;
};
struct conf_subdivide
{
	int steps;
};
struct conf_cube
{
	float size;
};
struct conf_extrude
{
	vecN_t offset;
	float scale;
	int steps;
	char scale_e[255];
	char offset_e[255];
	int scale_f;
	int offset_f;
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
	uint32_t ref;
	bool_t require_sys;
};
extern struct edit_tool g_edit_tools[];

/* typedef struct */
/* { */
/* 	int selection; */
/* 	mat_t *mat; */
/* 	int wireframe; */
/* 	int cull_front; */
/* 	int cull_back; */
/* 	float offset; */
/* 	float smooth_angle; */
/* } mat_layer_t; */

typedef enum
{
	MESH_CIRCLE,
	MESH_SPHERE,
	MESH_CUBE,
	MESH_TORUS,
	MESH_DISK,
	MESH_ICOSPHERE,
	MESH_EXTRUDE,
	MESH_SPHERIZE,
	MESH_SUBDIVIDE,
	MESH_DEFORM
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
	mesh_t *base_mesh;

	/* mat_layer_t *layers; */
	mat_t *mat;

	/* int layers_num; */
	float scale_dist;

	bool_t visible;
	bool_t cast_shadow;
	uint32_t visible_group;
	uint32_t shadow_group;
	uint32_t transparent_group;
	uint32_t selectable_group;
	bool_t xray;

	drawable_t draw;
	/* drawable_t select; */
	bool_t modified;
} c_model_t;

DEF_CASTER("model", c_model, c_model_t)
vs_t *model_vs(void);

void c_model_edit(c_model_t *self, mesh_edit_t type, geom_t target);
/* void c_model_add_layer(c_model_t *self, mat_t *mat, int selection, float offset); */
c_model_t *c_model_new(mesh_t *mesh, mat_t *mat, bool_t cast_shadow, bool_t visible);
c_model_t *c_model_cull_face(c_model_t *self, bool_t cull_front, bool_t cull_back);
c_model_t *c_model_wireframe(c_model_t *self, bool_t wireframe);
c_model_t *c_model_smooth(c_model_t *self, bool_t smooth);
c_model_t *c_model_paint(c_model_t *self, mat_t *mat);
void c_model_set_xray(c_model_t *self, bool_t xray);
void c_model_set_visible(c_model_t *self, bool_t visible);
void c_model_set_cast_shadow(c_model_t *self, bool_t cast_shadow);
void c_model_set_vs(c_model_t *self, vs_t *vs);
void c_model_set_mat(c_model_t *self, mat_t *mat);
void c_model_set_mesh(c_model_t *self, mesh_t *mesh);
void c_model_set_groups(c_model_t *self, uint32_t visible, uint32_t shadow,
		uint32_t transp, uint32_t selectable);

/* int c_model_render_transparent(c_model_t *self); */
/* int c_model_render_visible(c_model_t *self); */
/* int c_model_render(c_model_t *self, int transp); */
/* int c_model_render_at(c_model_t *self, c_node_t *node, int transp); */
void c_model_set_mesh(c_model_t *self, mesh_t *mesh);
void c_model_dirty(c_model_t *self);

#endif /* !MODEL_H */
