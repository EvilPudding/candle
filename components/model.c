#include "model.h"
#include "node.h"
#include "spatial.h"
#include "light.h"
#include <utils/nk.h>
#include <utils/drawable.h>
#include <candle.h>
#include <systems/editmode.h>

static mat_t *g_missing_mat = NULL;

static int c_model_position_changed(c_model_t *self);
static int paint_3d(mesh_t *mesh, vertex_t *vert);
static int paint_2d(mesh_t *mesh, vertex_t *vert);
int c_model_menu(c_model_t *self, void *ctx);
static vs_t *g_model_vs;
static vs_t *g_widget_vs;

struct edit_tool g_edit_tools[32];
int g_edit_tools_count = 0;

vs_t *model_vs()
{
	if(!g_model_vs)
	{
		g_model_vs = vs_new("model", false, 1, vertex_modifier_new(
			"	{\n"
			"#ifdef MESH4\n"
			"		float Y = cos(ANG4);\n"
			"		float W = sin(ANG4);\n"
			"		pos = vec4(vec3(P.x, P.y * Y + P.w * W, P.z), 1.0);\n"
			"#endif\n"

			"		mat4 MV    = camera(view) * M;\n"
			"		$vertex_normal    = (MV * vec4( N, 0.0f)).xyz;\n"
			"		$vertex_tangent   = (MV * vec4(TG, 0.0f)).xyz;\n"
			"		pos = M * pos;\n"
			"		$vertex_world_position = pos.xyz;\n"
			"		pos = camera(view) * pos;\n"
			"		$vertex_position = pos.xyz;\n"

			"		pos = camera(projection) * pos;\n"
			"	}\n"
		));
	}

	return g_model_vs;
}

vs_t *widget_vs()
{
	if(!g_widget_vs)
	{
		g_widget_vs = vs_new("widget", false, 1, vertex_modifier_new(
			"	{\n"
			"#ifdef MESH4\n"
			"		float Y = cos(ANG4);\n"
			"		float W = sin(ANG4);\n"
			"		pos = vec4(vec3(P.x, P.y * Y + P.w * W, P.z), 1.0);\n"
			"#endif\n"

			"		mat4 MV    = camera(view) * M;\n"
			"		$vertex_normal    = (MV * vec4( N, 0.0f)).xyz;\n"
			"		$vertex_tangent   = (MV * vec4(TG, 0.0f)).xyz;\n"
			"		pos.xyz *= length($obj_pos - camera(pos));\n"
			"		pos = M * pos;\n"
			"		$vertex_world_position = pos.xyz;\n"
			"		pos = camera(view) * pos;\n"
			"		$vertex_position = pos.xyz;\n"

			"		pos = camera(projection) * pos;\n"
			"	}\n"
		));
	}

	return g_widget_vs;
}

static int tool_circle_gui(void *ctx, struct conf_circle *conf)
{
	nk_property_float(ctx, "#radius:", -10000, &conf->radius, 10000, 0.1, 0.05);
	nk_property_int(ctx, "#segments:", 1, &conf->segments, 1000, 1, 1);
	return 0;
}

static int tool_sphere_gui(void *ctx, void *conf)
{
	return 0;
}

static int tool_subdivide_gui(void *ctx, struct conf_subdivide *conf)
{
	nk_property_int(ctx, "#steps:", 0, &conf->steps, 4, 1, 1);
	return 0;
}

static int tool_icosphere_gui(void *ctx, struct conf_ico *conf)
{
	nk_property_float(ctx, "#radius:", -10000, &conf->radius, 10000, 0.1, 0.05);
	nk_property_int(ctx, "#subdivisions:", 0, &conf->subdivisions, 4, 1, 1);

	return 0;
}

static int tool_disk_gui(void *ctx, struct conf_disk *conf)
{
	nk_property_float(ctx, "#radius:", -10000, &conf->radius1, 10000, 0.1, 0.05);
	nk_property_float(ctx, "#inner radius:", -10000, &conf->radius2, 10000, 0.1, 0.05);
	nk_property_int(ctx, "#segments:", 1, &conf->segments, 1000, 1, 1);
	return 0;
}

static int tool_torus_gui(void *ctx, struct conf_torus *conf)
{
	nk_property_float(ctx, "#radius:", -10000, &conf->radius1, 10000, 0.1, 0.05);
	nk_property_float(ctx, "#inner radius:", -10000, &conf->radius2, 10000, 0.1, 0.05);
	nk_property_int(ctx, "#segments:", 1, &conf->segments1, 1000, 1, 1);
	nk_property_int(ctx, "#inner segments:", 1, &conf->segments2, 1000, 1, 1);
	return 0;
}

static int tool_cube_gui(void *ctx, struct conf_cube *conf)
{
	nk_property_float(ctx, "#size:", -10000, &conf->size, 10000, 0.1, 0.05);
	return 0;
}

static int tool_spherize_gui(void *ctx, struct conf_spherize *conf)
{
	/* nk_property_float(ctx, "#scale:", -10000, &conf->scale, 10000, 0.1, 0.05); */
	nk_property_float(ctx, "#roundness:", 0, &conf->roundness, 1, 0.01, 0.01);
	return 0;
}
static int tool_extrude_gui(void *ctx, struct conf_extrude *conf)
{
	nk_layout_row_dynamic(ctx, 25, 1);

	nk_property_float(ctx, "#x:", -10000, &conf->offset.x, 10000, 0.1, 0.05);
	nk_property_float(ctx, "#y:", -10000, &conf->offset.y, 10000, 0.1, 0.05);
	nk_property_float(ctx, "#z:", -10000, &conf->offset.z, 10000, 0.1, 0.05);
#ifdef MESH4
	nk_property_float(ctx, "#w:", -10000, &conf->offset.w, 10000, 0.1, 0.05);
#endif
	nk_property_float(ctx, "#scale:", -10000, &conf->scale, 10000, 0.1, 0.05);
	nk_property_int(ctx, "#steps:", 1, &conf->steps, 1000, 1, 1);
	/* 0.1+0.9*math.pow(x*2-1,2) */
	nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD,
			conf->scale_e, sizeof(conf->scale_e), nk_filter_ascii);

	nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD,
			conf->offset_e, sizeof(conf->offset_e), nk_filter_ascii);

	return 0;
}
static int tool_deform_gui(void *ctx, struct conf_deform *conf)
{
	nk_layout_row_dynamic(ctx, 25, 1);

	conf->normal = nk_check_label(ctx, "normal", conf->normal);
	if(!conf->normal)
	{
		nk_property_float(ctx, "#x:", -10000, &conf->direction.x, 10000, 0.1, 0.05);
		nk_property_float(ctx, "#y:", -10000, &conf->direction.y, 10000, 0.1, 0.05);
		nk_property_float(ctx, "#z:", -10000, &conf->direction.z, 10000, 0.1, 0.05);
#ifdef MESH4
		nk_property_float(ctx, "#w:", -10000, &conf->direction.w, 10000, 0.1, 0.05);
#endif
	}
	nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD,
			conf->deform_e, sizeof(conf->deform_e), nk_filter_ascii);

	return 0;
}

static mesh_t *tool_circle_edit(mesh_t *state, struct conf_circle *conf)
{
	state = mesh_clone(state);
	mesh_circle(state, conf->radius, conf->segments, VEC3(0.0, 1.0, 0.0));
	mesh_select(state, SEL_EDITING, MESH_VERT, -1);
	mesh_for_each_selected(state, MESH_VERT, (iter_cb)paint_2d, NULL);
	return state;
}

static int tool_sphere_edit(void)
{
	return 0;
}

static mesh_t *tool_spherize_edit(mesh_t *last,
		struct conf_spherize *conf)
{
	mesh_t *state = mesh_clone(last);
	mesh_select(state, SEL_EDITING, MESH_FACE, -1);
	mesh_spherize(state, conf->roundness);
	return state;
}

static mesh_t *tool_subdivide_edit(
		mesh_t *last, struct conf_subdivide *new,
		mesh_t *state, struct conf_subdivide *old)
{
	if(state && new->steps > old->steps)
	{
		mesh_select(state, SEL_EDITING, MESH_FACE, -1);
		mesh_subdivide(state, new->steps - old->steps);
		mesh_select(state, SEL_EDITING, MESH_FACE, -1);
	}
	else
	{
		state = mesh_clone(last);

		mesh_lock(state);
		if(new->steps)
		{
			mesh_select(state, SEL_EDITING, MESH_FACE, -1);
			mesh_subdivide(state, new->steps);
		}
		mesh_unlock(state);
	}
	return state;
}

static mesh_t *tool_icosphere_edit(
		mesh_t *last, struct conf_ico *new,
		mesh_t *state, struct conf_ico *old)
{
	if(state && (new->subdivisions > old->subdivisions && new->radius == old->radius))
	{
		mesh_lock(state);
		mesh_select(state, SEL_EDITING, MESH_FACE, -1);
		mesh_subdivide(state, new->subdivisions - old->subdivisions);
		mesh_spherize(state, 1);
		mesh_select(state, SEL_EDITING, MESH_FACE, -1);
		mesh_for_each_selected(state, MESH_VERT, (iter_cb)paint_3d, NULL);
		mesh_unlock(state);
	}
	else
	{
		state = mesh_clone(last);

		mesh_lock(state);
		mesh_ico(state, new->radius / 2);
		if(new->subdivisions)
		{
			mesh_select(state, SEL_EDITING, MESH_FACE, -1);
			mesh_subdivide(state, new->subdivisions);
		}
		mesh_spherize(state, 1);
		mesh_select(state, SEL_EDITING, MESH_FACE, -1);
		mesh_for_each_selected(state, MESH_VERT, (iter_cb)paint_3d, NULL);
		mesh_unlock(state);
	}
	return state;
}

static mesh_t *tool_disk_edit(mesh_t *mesh, struct conf_disk *conf)
{
	mesh = mesh_clone(mesh);
	mesh_lock(mesh);
	mesh_disk(mesh, conf->radius1, conf->radius2, conf->segments,
			VEC3(0.0, 1.0, 0.0));
	mesh_select(mesh, SEL_EDITING, MESH_FACE, -1);
	mesh->has_texcoords = 0;

	mesh_unlock(mesh);
	return mesh;
}

static mesh_t *tool_torus_edit(mesh_t *mesh, struct conf_torus *conf)
{
	/* mesh = mesh_clone(mesh); */
	mesh = mesh_torus(conf->radius1, conf->radius2, conf->segments1, conf->segments2);
	return mesh;
}

static vec2_t encode_normal(vec3_t n)
{
	float p;
	n = vec3_norm(n);
    p = sqrtf(n.z * 8.0f + 8.0f);
	if(p == 0) return vec2(0.0f, 0.0f);
	/* printf("%f ", p); vec3_print(n); */
    return vec2_add_number(vec2_div_number(vec3_xy(n), p), 0.5f);
}

static int paint_2d(mesh_t *mesh, vertex_t *vert)
{
	vec3_t norm = vec3_get_unit(vecN_xyz(vert->pos));
	vec2_t n = encode_normal(norm);
	vert->color = vec4(vert->pos.y > 0, n.y, n.x, 1.0f);
	/* vert->color = vec4(_vec2(n), vert->pos.y > 0, 1.0f); */
	return 1;
}

static int paint_3d(mesh_t *mesh, vertex_t *vert)
{
	vec3_t norm = vec3_get_unit(vecN_xyz(vert->pos));
	vec2_t n = encode_normal(norm);

	vert->color = vec4(vert->pos.z > 0, n.y, n.x, 1.0f);
	/* vert->color.xyz = vec3_add_number(vec3_scale(vert->pos, 0.5f), 0.5f); */
	/* vert->color.a = vert->pos.w; */
	return 1;
}


static mesh_t *tool_cube_edit(mesh_t *mesh, struct conf_cube *conf)
{
	mesh = mesh_clone(mesh);
	mesh_lock(mesh);
	mesh_cube(mesh, conf->size, 1);
	mesh_select(mesh, SEL_EDITING, MESH_VERT, -1);
	mesh->has_texcoords = 0;

	mesh_for_each_selected(mesh, MESH_VERT, (iter_cb)paint_3d, NULL);
	mesh_unlock(mesh);
	return mesh;
}

struct int_int {int a, b;};

static float interpret_scale(mesh_t *self, float x,
		struct int_int *interpreters)
{
	float y = 0.f/0.f;
	struct set_var var = {"x"};
	var.value = x;
	emit(ref("expr_var"), &var, NULL);
	emit(ref("expr_eval"), &interpreters->a, &y);

	if(y != y) return 0.0f;

	return y;
}

static float interpret_offset(mesh_t *self, float r,
		struct int_int *interpreters)
{
	float y = 0.f/0.f;
	struct set_var var = {"r"};
	var.value = r;

	emit(ref("expr_var"), &var, NULL);
	emit(ref("expr_eval"), &interpreters->b, &y);

	if (y != y) return 0.0f;

	return y;
}


static mesh_t *tool_extrude_edit(
		mesh_t *last, struct conf_extrude *new,
		mesh_t *state, struct conf_extrude *old)
{
	float result;
	struct int_int args;
	struct set_var var_r = {"r"};
	struct set_var var_x = {"x"};
	var_r.value = 0.0f;
	var_x.value = 0.0f;
	emit(ref("expr_var"), &var_r, NULL);
	emit(ref("expr_var"), &var_x, NULL);
	if(strcmp(new->scale_e, old->scale_e))
	{
		if(old->scale_f) emit(ref("expr_del"), &old->scale_f, NULL);

		emit(ref("expr_load"), new->scale_e, &new->scale_f);
		if(new->scale_f == 0) return state;

		result = 0.f/0.f;
		emit(ref("expr_eval"), &new->scale_f, &result);
		if(result != result)
		{
			new->scale_f = 0;
			return state;
		}
	}
	if(strcmp(new->offset_e, old->offset_e))
	{
		if(old->offset_f) emit(ref("expr_del"), &old->offset_f, NULL);

		emit(ref("expr_load"), new->offset_e, &new->offset_f);
		if(new->offset_f == -1) return state;

		result = 0.f/0.f;
		emit(ref("expr_eval"), &new->offset_f, &result);
		if(result != result)
		{
			new->offset_f = 0;
			return state;
		}
	}
	state = mesh_clone(last);
	args.a = new->scale_f;
	args.b = new->offset_f;

	mesh_lock(state);

	if(vector_count(state->faces))
	{
#ifdef MESH4
		if(!state->triangulated)
		{
			mesh_select(state, SEL_EDITING, MESH_FACE, -1);
			mesh_triangulate(state);
		}
		mesh_extrude_faces(state, new->steps, new->offset, new->scale,
				new->scale_f ? (modifier_cb)interpret_scale : NULL,
				new->offset_f ? (modifier_cb)interpret_offset : NULL, &args);
		mesh_for_each_selected(state, MESH_VERT, (iter_cb)paint_3d);
		mesh_remove_lone_faces(state);
#endif
	}
	else
	{
		mesh_extrude_edges(state, new->steps, new->offset, new->scale,
				new->scale_f ? (modifier_cb)interpret_scale : NULL,
				new->offset_f ? (modifier_cb)interpret_offset : NULL, &args);
		mesh_for_each_selected(state, MESH_VERT, (iter_cb)paint_2d, NULL);
		mesh_remove_lone_edges(state);
		mesh_triangulate(state);
	}
	mesh_unlock(state);

	return state;
}

static int deform(mesh_t *mesh, vertex_t *vert, struct conf_deform *conf)
{
	float result = 0.f/0.f;
	struct set_var var_x = {"x"};
	struct set_var var_y = {"y"};
	struct set_var var_z = {"z"};
#ifdef MESH4
	struct set_var_w = {"w"};
#endif
	vecN_t direction = /*conf->normal ? vert->n :*/ conf->direction;
	var_x.value = vert->pos.x;
	var_y.value = vert->pos.y;
	var_z.value = vert->pos.z;
#ifdef MESH4
	var_w.value = vert->pos.w;
#endif

	emit(ref("expr_var"), &var_x, NULL);
	emit(ref("expr_var"), &var_y, NULL);
	emit(ref("expr_var"), &var_z, NULL);
#ifdef MESH4
	emit(ref("expr_var"), &var_w, NULL);
#endif
	emit(ref("expr_eval"), &conf->deform_s, &result);

	if(vert->pos.y > 0.0f)
	{
		vecN_(print)(vert->pos);
	}
	if(result != result) return 1;

	vert->pos = vecN_(add)(vert->pos, vecN_(scale)(direction, result));

	return 1;
}

static mesh_t *tool_deform_edit(
		mesh_t *last, struct conf_deform *new,
		mesh_t *state, struct conf_deform *old) /* (struct conf_deform){0, VEC3(0, 1, 0), "1"} */
{
	float result;
	struct set_var var_x = {"x"};
	struct set_var var_y = {"y"};
	struct set_var var_z = {"z"};
#ifdef MESH4
	struct set_var_w = {"w"};
#endif
	var_x.value = 0.0f;
	var_y.value = 0.0f;
	var_z.value = 0.0f;
#ifdef MESH4
	var_w.value = 0.0f;
#endif


	emit(ref("expr_var"), &var_x, NULL);
	emit(ref("expr_var"), &var_y, NULL);
	emit(ref("expr_var"), &var_z, NULL);
#ifdef MESH4
	emit(ref("expr_var"), &var_w, NULL);
#endif
	if(strcmp(new->deform_e, old->deform_e))
	{
		if(old->deform_s) emit(ref("expr_del"), &old->deform_s, NULL);

		emit(ref("expr_load"), new->deform_e, &new->deform_s);
		if(new->deform_s == 0) return state;

		result = 0.f/0.f;
		emit(ref("expr_eval"), &new->deform_s, &result);
		if(result != result)
		{
			new->deform_s = 0;
			return state;
		}
	}
	state = mesh_clone(last);

	if(new->deform_s)
	{
		mesh_select(state, SEL_EDITING, MESH_VERT, -1);
		mesh_lock(state);

		mesh_for_each_selected(state, MESH_VERT, (iter_cb)deform, new);

		mesh_unlock(state);
	}

	return state;
}

static void c_model_init(c_model_t *self)
{
	if(!g_missing_mat)
	{
		g_missing_mat = mat_new("missing", "default");
		mat4f(g_missing_mat, ref("albedo.color"), vec4(0.0, 0.9, 1.0, 1.0));
	}

	self->visible_group = ref("visible");
	self->shadow_group = ref("shadow");
	self->transparent_group = ref("transparent");
	self->selectable_group = ref("selectable");
	self->visible = 1;
	/* self->layers = malloc(sizeof(*self->layers) * 16); */
	self->history = vector_new(sizeof(mesh_history_t), FIXED_ORDER, NULL, NULL);

}

/* void c_model_add_layer(c_model_t *self, mat_t *mat, int selection, float offset) */
/* { */
/* 	int i = self->layers_num++; */
/* 	self->layers[i].mat = mat; */
/* 	self->layers[i].selection = selection; */
/* 	self->layers[i].cull_front = 0; */
/* 	self->layers[i].cull_back = 1; */
/* 	self->layers[i].wireframe = 0; */
/* 	self->layers[i].offset = 0; */
/* 	self->layers[i].smooth_angle = 0.2f; */
/* 	entity_signal_same(c_entity(self), sig("mesh_changed"), NULL, NULL); */
/* } */

void c_model_init_drawables(c_model_t *self)
{
	drawable_init(&self->draw, self->visible_group);
	drawable_add_group(&self->draw, ref("selectable"));
	drawable_set_entity(&self->draw, c_entity(self));
	drawable_set_vs(&self->draw, model_vs());
}

void c_model_run_command(c_model_t *self, mesh_t *last, mesh_history_t *cmd)
{
	struct edit_tool *tool = &g_edit_tools[cmd->type];
	mesh_t *result = tool->edit(last,		cmd->conf_new,
								cmd->state, cmd->conf_old);
	if(result != cmd->state)
	{
		mesh_destroy(cmd->state);
	}
	memcpy(cmd->conf_old, cmd->conf_new, tool->size);
	cmd->state = result;
}

/* FIXME this function should not be needed */
/* update id change should be enough for mesh to update */
void c_model_dirty(c_model_t *self)
{
	drawable_set_mesh(&self->draw, NULL);
	drawable_set_mesh(&self->draw, self->mesh);
}

void c_model_propagate_edit(c_model_t *self, int cmd_id)
{
	int32_t i;
	int32_t last_update_id;
	mesh_t *last;

	if(cmd_id >= vector_count(self->history)) return;
	last_update_id = 0;
	if(self->mesh)
	{
		last_update_id = self->mesh->update_id;
	}

	/* int update_id = -1; */
	if(self->base_mesh == self->mesh)
	{
		self->base_mesh = mesh_clone(self->base_mesh);
	}
	last = self->base_mesh;
	if(cmd_id > 0)
	{
		mesh_history_t *prev = vector_get(self->history, cmd_id - 1);
		last = prev->state;
	}
	if(last)
	{
		last->update_id++;
	}
	for(i = cmd_id; i < vector_count(self->history); i++)
	{
		mesh_history_t *cmd = vector_get(self->history, i);
		if(i > cmd_id)
		{
			mesh_destroy(cmd->state);
			cmd->state = NULL;
		}
		c_model_run_command(self, last, cmd);
		last = cmd->state;
	}
	if(!self->mesh && last) self->mesh = mesh_new();

	mesh_lock(self->mesh);
	mesh_assign(self->mesh, last);
	self->mesh->update_id = last_update_id + 1;
	mesh_unlock(self->mesh);
	c_model_dirty(self);

	entity_signal_same(c_entity(self), sig("model_changed"), NULL, NULL);
}

void c_model_remove_edit(c_model_t *self, int cmd_id)
{
	mesh_history_t *cmd = vector_get(self->history, cmd_id);
	if(cmd->state) mesh_destroy(cmd->state);
	vector_remove(self->history, cmd_id);

	c_model_propagate_edit(self, cmd_id);

}


void c_model_edit(c_model_t *self, mesh_edit_t type, geom_t target)
{
	struct edit_tool *tool = &g_edit_tools[type];
	mesh_history_t command = {0};
	command.target = target;
	command.type = type;
	command.conf_new = calloc(1, tool->size);
	command.conf_old = calloc(1, tool->size);

	if(tool->defaults)
	{
		memcpy(command.conf_new, tool->defaults, tool->size);
	}

	vector_add(self->history, &command); 
	c_model_propagate_edit(self, vector_count(self->history) - 1);
}

/* c_model_t *c_model_paint(c_model_t *self, int layer, mat_t *mat) */
/* { */
/* 	self->layers[layer].mat = mat; */
/* 	/1* drawable_t *gl = self->draw; *1/ */
/* 	/1* gl->groups[layer].mat = mat; *1/ */
/* 	return self; */
/* } */

c_model_t *c_model_cull_invert(c_model_t *self)
{
	self->mesh->cull = (~self->mesh->cull) & 0x3;
	drawable_model_changed(&self->draw);
	return self;
}

c_model_t *c_model_cull_face(c_model_t *self, bool_t cull_front, bool_t cull_back)
{
	self->mesh->cull = cull_front | (cull_back << 1);
	drawable_model_changed(&self->draw);
	return self;
}

void c_model_set_vs(c_model_t *self, vs_t *vs)
{
	drawable_set_vs(&self->draw, vs);
}

void c_model_set_xray(c_model_t *self, bool_t xray)
{
	self->xray = xray;
	drawable_set_xray(&self->draw, xray);
}

c_model_t *c_model_smooth(c_model_t *self, float smooth)
{
	/* self->layers[layer].smooth_angle = smooth; */
	self->mesh->smooth_angle = smooth;
	mesh_modified(self->mesh);
	drawable_model_changed(&self->draw);
	return self;
}

c_model_t *c_model_wireframe(c_model_t *self, bool_t wireframe)
{
	/* self->layers[layer].wireframe = wireframe; */
	self->mesh->wireframe = wireframe;
	drawable_model_changed(&self->draw);
	return self;
}
/* static int visible_type_filter(drawable_t *drawable) */
/* { */
/* } */


void c_model_update_mat(c_model_t *self)
{
	if (self->mat)
	{
		int transp = mat_is_transparent(self->mat);
		if(transp)
		{
			drawable_remove_group(&self->draw, self->visible_group);
			drawable_add_group(&self->draw, self->transparent_group);
		}
		else
		{
			drawable_remove_group(&self->draw, self->transparent_group);
			drawable_add_group(&self->draw, self->visible_group);
		}
		if(self->selectable_group)
		{
			drawable_add_group(&self->draw, self->selectable_group);
		}
		if(self->shadow_group && self->cast_shadow)
		{
			drawable_add_group(&self->draw, self->shadow_group);
		}
		self->mat_update_id = self->mat->update_id;
	}

	drawable_set_mat(&self->draw, self->mat);
}

void c_model_set_groups(c_model_t *self, uint32_t visible, uint32_t shadow,
		uint32_t transp, uint32_t selectable)
{
	drawable_remove_group(&self->draw, self->selectable_group);
	drawable_remove_group(&self->draw, self->transparent_group);
	drawable_remove_group(&self->draw, self->visible_group);
	drawable_remove_group(&self->draw, self->shadow_group);

	self->visible_group = visible;
	self->transparent_group = transp;
	self->selectable_group = selectable;
	self->shadow_group = shadow;

	c_model_update_mat(self);
}

void c_model_set_mat(c_model_t *self, mat_t *mat)
{
	if(!mat)
	{
		mat = g_mats[0];
	}
	if(self->mat != mat)
	{
		self->mat = mat;
		c_model_update_mat(self);
	}

}

void c_model_set_cast_shadow(c_model_t *self, bool_t cast_shadow)
{
	if(self->cast_shadow == cast_shadow) return;

	self->cast_shadow = cast_shadow;
	if(!self->cast_shadow)
	{
		drawable_remove_group(&self->draw, self->shadow_group);
	}
	else
	{
		drawable_add_group(&self->draw, self->shadow_group);
	}

}

void c_model_set_visible(c_model_t *self, bool_t visible)
{
	self->visible = visible;
	drawable_set_mesh(&self->draw, visible ? self->mesh : NULL);
}

void c_model_set_mesh(c_model_t *self, mesh_t *mesh)
{
	/* FIXME this is broken when the mesh is destroyed */
	if(mesh != self->mesh)
	{
		mesh_t *old_mesh = self->mesh;
		self->mesh = mesh;
		self->base_mesh = mesh;

		if(self->visible)
		{
			drawable_set_mesh(&self->draw, mesh);
		}
		if(old_mesh) mesh_destroy(old_mesh);
	}
}

int c_model_created(c_model_t *self)
{
	drawable_model_changed(&self->draw);
	return CONTINUE;
}


int c_model_tool(c_model_t *self, void *ctx)
{
	int i;
	int sys = c_entity(self) == SYS;
	for(i = 0; i < g_edit_tools_count; i++)
	{
		struct edit_tool *tool = &g_edit_tools[i];
		if(!sys && !tool->require_sys)
		{
			if(nk_button_label(ctx, tool->name))
			{
				c_model_edit(self, i, MESH_FACE);
				return STOP;
			}
		}
		else if(sys && tool->require_sys)
		{
			if(nk_button_label(ctx, tool->name))
			{
				entity_t ent = entity_new(c_model_new(NULL, mat_new("k", "default"), 1, 1));
				c_editmode_select(c_editmode(&SYS), ent);
				c_model_edit(c_model(&ent), i, MESH_FACE);
				return STOP;
			}
		}
	}
	return CONTINUE;
}

int c_model_menu(c_model_t *self, void *ctx)
{
	int i;
	int new_value;
	int changes = 0;
	float smooth;
	int cull_back;
	int cull_front;
	mesh_t *mesh;

	new_value = nk_check_label(ctx, "Visible", self->visible);
	if(new_value != self->visible)
	{
		c_model_set_visible(self, new_value);
		changes = 1;
	}
	new_value = nk_check_label(ctx, "Cast shadow", self->cast_shadow);
	if(new_value != self->cast_shadow)
	{
		c_model_set_cast_shadow(self, new_value);
		changes = 1;
	}

	/* if(nk_tree_push(ctx, NK_TREE_TAB, "Edit", NK_MAXIMIZED)) */
	/* { */
	if(self->mesh)
	{
		for(i = 0; i < vector_count(self->history); i++)
		{
			mesh_history_t *cmd = vector_get(self->history, i);
			struct edit_tool *tool = &g_edit_tools[cmd->type];
			if(nk_tree_push_id(ctx, NK_TREE_TAB, tool->name, NK_MAXIMIZED, i))
			{
				tool->gui(ctx, cmd->conf_new);

				if(nk_button_label(ctx, "remove"))
				{
					c_model_remove_edit(self, i);
					nk_tree_pop(ctx);
					break;
				}

				if(memcmp(cmd->conf_old, cmd->conf_new, tool->size))
				{
					c_model_propagate_edit(self, i);
				}
				nk_tree_pop(ctx);
			}
		}
		/* nk_tree_pop(ctx); */
	/* } */

		mesh = self->mesh;

		new_value = nk_check_label(ctx, "Wireframe",
				mesh->wireframe);

		if(new_value != mesh->wireframe)
		{
			mesh->wireframe = new_value;
			changes = 1;
		}

		new_value = nk_check_label(ctx, "Receive Shadows",
				mesh->receive_shadows);

		if(new_value != mesh->receive_shadows)
		{
			mesh->receive_shadows = new_value;
			changes = 1;
		}

		smooth = mesh->smooth_angle;
		nk_property_float(ctx, "#smooth:", 0.0f, &smooth,
				1.0f, 0.05f, 0.05f);
		if(smooth != mesh->smooth_angle)
		{
			mesh->smooth_angle = smooth;
			/* self->mesh->update_id++; */
			mesh_modified(mesh);
			mesh_update(mesh);
		}

		cull_front = mesh->cull & 0x1;
		cull_back = mesh->cull >> 0x1;

		new_value = nk_check_label(ctx, "Cull front", cull_front);

		if(new_value != cull_front)
		{
			mesh->cull = cull_back | new_value;
			changes = 1;
		}

		new_value = nk_check_label(ctx, "Cull back", cull_back);

		if(new_value != cull_back)
		{
			mesh->cull = cull_front | (new_value << 1);
			changes = 1;
		}
	}

	if(self->mat && self->mat->name[0] != '_')
	{
		changes |= mat_menu(self->mat, ctx);
	}
	if(changes)
	{
		drawable_model_changed(&self->draw);
	}

	/* nk_tree_pop(ctx); */


	return CONTINUE;
}

static int c_model_position_changed(c_model_t *self)
{
	/* mat4_t model = node->model; */
	/* if(self->scale_dist > 0.0f) */
	/* { */
	/* 	vec3_t pos = mat4_mul_vec4(model, vec4(0,0,0,1)).xyz; */
	/* 	float dist = vec3_dist(pos, c_renderer(&SYS)->bound_camera_pos); */
	/* 	mat4_t model = mat4_scale_aniso(model, vec3(dist * self->scale_dist)); */
	/* } */
	self->modified = 1;
	return CONTINUE;
}

vs_t *sprite_vs(void);
static int c_model_pre_draw(c_model_t *self)
{
	c_node_t *node = c_node(self);
	if(!node) return CONTINUE;
	if (c_node_update_model(node))
	{
		if(entity_exists(node->unpack_inheritance) && node->unpack_inheritance != SYS)
		{
			drawable_set_entity(&self->draw, node->unpack_inheritance);
		}
		else
		{
			drawable_set_entity(&self->draw, entity_null);
		}
	}
	if (self->mat && self->mat_update_id != self->mat->update_id)
	{
		c_model_update_mat(self);
	}

	if(self->modified)
	{
		self->modified = 0;

		drawable_set_transform(&self->draw, node->model);

#ifdef MESH4
		drawable_set_angle4(&self->draw, node->angle4);
#endif
	}
	return CONTINUE;
}


void add_tool(char *name, tool_gui_cb gui, tool_edit_cb edit, size_t size,
		void *defaults, int require_sys)
{
	struct edit_tool tool = {0};
	tool.gui = gui;
	tool.edit = edit;
	tool.size = size;

	strcpy(tool.name, name);
	tool.ref = ref(name);
	tool.require_sys = require_sys;
	if(defaults)
	{
		tool.defaults = calloc(1, size);
		memcpy(tool.defaults, defaults, size);
	}
	g_edit_tools[g_edit_tools_count++] = tool;
}

static void c_model_destroy(c_model_t *self)
{
	int i;
	drawable_set_mesh(&self->draw, NULL);
	for(i = 0; i < vector_count(self->history); i++)
	{
		mesh_history_t *cmd = vector_get(self->history, i);
		if(cmd->state)
		{
			mesh_destroy(cmd->state);
			cmd->state = NULL;
		}
	}
	if(self->mesh) mesh_destroy(self->mesh);
}

void ct_model(ct_t *self)
{
	struct conf_circle circle_opts = {2, 10};
	struct conf_cube cube_opts = {1.0f};
	struct conf_torus torus_opts = {1.0f, 0.2f, 30, 20};
	struct conf_disk disk_opts = {2.0f, 0.2f, 10};
	struct conf_ico ico_opts = {1.0f, 0};
	struct conf_extrude extrude_opts = {VEC3i(0.0f, 2.0f, 0.0f), 1.0f, 1};
	struct conf_spherize spherize_opts = {1, 1};
	struct conf_subdivide subdivide_opts = {1};
	struct conf_deform deform_opts = {0, VEC3i(0, 1, 0), "1"};

	ct_init(self, "model", sizeof(c_model_t));
	ct_set_init(self, (init_cb)c_model_init);
	ct_set_destroy(self, (destroy_cb)c_model_destroy);
	ct_add_dependency(self, ct_node);

	ct_add_listener(self, ENTITY, 0, ref("entity_created"), c_model_created);

	ct_add_listener(self, WORLD, 0, ref("component_menu"), c_model_menu);
	ct_add_listener(self, WORLD, 0, ref("component_tool"), c_model_tool);
	ct_add_listener(self, WORLD, 100, ref("world_pre_draw"), c_model_pre_draw);

	ct_add_listener(self, ENTITY, 0, ref("node_changed"), c_model_position_changed);

	add_tool("circle", (tool_gui_cb)tool_circle_gui,
			(tool_edit_cb)tool_circle_edit,
		sizeof(struct conf_circle), &circle_opts, 1);

	add_tool("sphere", (tool_gui_cb)tool_sphere_gui,
			(tool_edit_cb)tool_sphere_edit, sizeof(struct conf_sphere), NULL, 1);

	add_tool("cube", (tool_gui_cb)tool_cube_gui, (tool_edit_cb)tool_cube_edit,
		sizeof(struct conf_cube),
		&cube_opts, 1);

	add_tool("torus", (tool_gui_cb)tool_torus_gui, (tool_edit_cb)tool_torus_edit,
		sizeof(struct conf_torus),
		&torus_opts, 1);

	add_tool("disk", (tool_gui_cb)tool_disk_gui, (tool_edit_cb)tool_disk_edit,
		sizeof(struct conf_disk),
		&disk_opts, 1);

	add_tool("icosphere", (tool_gui_cb)tool_icosphere_gui,
			(tool_edit_cb)tool_icosphere_edit, sizeof(struct conf_ico),
			&ico_opts, 1);

	add_tool("extrude", (tool_gui_cb)tool_extrude_gui,
		(tool_edit_cb)tool_extrude_edit, sizeof(struct conf_extrude),
		&extrude_opts, 0);

	add_tool("spherize", (tool_gui_cb)tool_spherize_gui,
		(tool_edit_cb)tool_spherize_edit, sizeof(struct conf_spherize),
		&spherize_opts, 0);

	add_tool("subdivide", (tool_gui_cb)tool_subdivide_gui,
		(tool_edit_cb)tool_subdivide_edit, sizeof(struct conf_subdivide),
		&subdivide_opts, 0);

	add_tool("deform", (tool_gui_cb)tool_deform_gui,
		(tool_edit_cb)tool_deform_edit, sizeof(struct conf_deform),
		&deform_opts, 0);
}

c_model_t *c_model_new(mesh_t *mesh, mat_t *mat, bool_t cast_shadow, bool_t visible)
{
	c_model_t *self = component_new(ct_model);
	if(c_entity(self) == SYS)
	{
		self->super.ghost = 1;
		c_spatial(self)->super.ghost = 1;
	}

	c_model_init_drawables(self);

	c_model_set_mat(self, mat);

	c_model_set_cast_shadow(self, cast_shadow);
	c_model_set_mesh(self, mesh);
	c_model_set_visible(self, visible);
	c_model_position_changed(self);

	return self;
}

