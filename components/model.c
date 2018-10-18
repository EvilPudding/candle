#include "model.h"
#include "node.h"
#include "spacial.h"
#include "light.h"
#include <utils/nk.h>
#include <utils/drawable.h>
#include <candle.h>
#include <systems/renderer.h>
#include <systems/editmode.h>
#include <systems/lua.h>

static mat_t *g_missing_mat = NULL;

static int paint_3d(mesh_t *mesh, vertex_t *vert);
static int paint_2d(mesh_t *mesh, vertex_t *vert);
int c_model_menu(c_model_t *self, void *ctx);
int g_update_id = 0;
vs_t *g_model_vs = NULL;

struct edit_tool g_edit_tools[32];
int g_edit_tools_count = 0;

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

static mesh_t *tool_circle_edit(mesh_t *state, struct conf_circle *conf)
{
	state = mesh_clone(state);
	mesh_circle(state, conf->radius, conf->segments, VEC3(0.0, 1.0, 0.0));
	mesh_select(state, SEL_EDITING, MESH_EDGE, -1);
	mesh_for_each_selected(state, MESH_VERT, (iter_cb)paint_2d);
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
		mesh_for_each_selected(state, MESH_VERT, (iter_cb)paint_3d);
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
		mesh_for_each_selected(state, MESH_VERT, (iter_cb)paint_3d);
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
	n = vec3_norm(n);
    float p = sqrtf(n.z * 8.0f + 8.0f);
	if(p == 0) return vec2(0);
	/* printf("%f ", p); vec3_print(n); */
    return vec2_add_number(vec2_div_number(n.xy, p), 0.5f);
}

static int paint_2d(mesh_t *mesh, vertex_t *vert)
{
	vec3_t norm = vec3_get_unit(XYZ(vert->pos));
	vec2_t n = encode_normal(norm);
	vert->color = vec4(vert->pos.y > 0, n.y, n.x, 1.0f);
	/* vert->color = vec4(_vec2(n), vert->pos.y > 0, 1.0f); */
	return 1;
}

static int paint_3d(mesh_t *mesh, vertex_t *vert)
{
	vec3_t norm = vec3_get_unit(XYZ(vert->pos));
	vec2_t n = encode_normal(norm);
	vert->color = vec4(vert->pos.w > 0, n.y, n.x, 1.0f);
	/* vert->color.xyz = vec3_add_number(vec3_scale(vert->pos, 0.5f), 0.5f); */
	/* vert->color.a = vert->pos.w; */
	return 1;
}


static mesh_t *tool_cube_edit(mesh_t *mesh, struct conf_cube *conf)
{
	mesh = mesh_clone(mesh);
	mesh_lock(mesh);
	mesh_cube(mesh, conf->size, 1);
	mesh_select(mesh, SEL_EDITING, MESH_EDGE, -1);
	mesh->has_texcoords = 0;

	mesh_for_each_selected(mesh, MESH_VERT, (iter_cb)paint_3d);
	mesh_unlock(mesh);
	return mesh;
}

struct int_int {int a, b;};

static float interpret_scale(mesh_t *self, float x,
		struct int_int *interpreters)
{
	c_lua_t *lua = c_lua(&SYS);

	c_lua_setvar(lua, "x", x);
	char *msg = NULL;
	double y = c_lua_eval(lua, interpreters->a, &msg);
	if (msg) exit(1);
	return (float)y;
}

static float interpret_offset(mesh_t *self, float x,
		struct int_int *interpreters)
{
	c_lua_t *lua = c_lua(&SYS);

	c_lua_setvar(lua, "r", x);
	char *msg = NULL;
	double y = c_lua_eval(lua, interpreters->b, &msg);
	if (msg) exit(1);
	return (float)y;
}


static mesh_t *tool_extrude_edit(
		mesh_t *last, struct conf_extrude *new,
		mesh_t *state, struct conf_extrude *old)
{

	c_lua_t *lua = c_lua(&SYS);
	char *msg = NULL;
	if(lua)
	{
		c_lua_setvar(lua, "r", 0);
		c_lua_setvar(lua, "x", 0);
		if(strcmp(new->scale_e, old->scale_e))
		{
			if(old->scale_f) c_lua_unref(lua, old->scale_f);

			new->scale_f = c_lua_loadexpr(lua, new->scale_e, &msg);
			if(msg)
			{
				puts(msg);
				free(msg);
				new->scale_f = 0;
				return state;
			}
			c_lua_eval(lua, new->scale_f, &msg);
			if (msg)
			{
				puts(msg);
				free(msg);
				new->scale_f = 0;
				return state;
			}
		}
		if(strcmp(new->offset_e, old->offset_e))
		{
			if(old->offset_f) c_lua_unref(lua, old->offset_f);

			new->offset_f = c_lua_loadexpr(lua, new->offset_e, &msg);

			if(msg)
			{
				free(msg);
				new->offset_f = 0;
				return state;
			}
			c_lua_eval(lua, new->offset_f, &msg);
			if(msg)
			{
				free(msg);
				new->offset_f = 0;
				return state;
			}
		}
	}
	state = mesh_clone(last);

	struct int_int args = {new->scale_f, new->offset_f};

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
		mesh_for_each_selected(state, MESH_VERT, (iter_cb)paint_2d);
		mesh_remove_lone_edges(state);
		mesh_triangulate(state);
	}
	mesh_unlock(state);

	return state;
}

static void c_model_init(c_model_t *self)
{
	if(!g_missing_mat)
	{
		g_missing_mat = mat_new("missing");
		g_missing_mat->albedo.color = vec4(0.0, 0.9, 1.0, 1.0);
	}

	self->visible = 1;
	/* self->layers = malloc(sizeof(*self->layers) * 16); */
	self->history = vector_new(sizeof(mesh_history_t), FIXED_ORDER, NULL, NULL);

	if(!g_model_vs)
	{
		g_model_vs = vs_new("model", 1, vertex_modifier_new(
			"	{\n"
			"#ifdef MESH4\n"
			"		float Y = cos(ANG4);\n"
			"		float W = sin(ANG4);\n"
			"		pos = vec4(vec3(P.x, P.y * Y + P.w * W, P.z), 1.0);\n"
			"#endif\n"

			"		poly_color = COL;\n"
			"		mat4 MV    = scene.camera.view * M;\n"
			"		vec3 vertex_normal    = normalize(MV * vec4( N, 0.0f)).xyz;\n"
			"		vec3 vertex_tangent   = normalize(MV * vec4(TG, 0.0f)).xyz;\n"
			"		vec3 vertex_bitangent = cross(vertex_tangent, vertex_normal);\n"

			"		poly_id = ID;\n"
			"		TM = mat3(vertex_tangent, vertex_bitangent, vertex_normal);\n"

			"		pos = (scene.camera.projection * MV) * pos;\n"
			"	}\n"
		));
	}
	self->vs = g_model_vs;

	drawable_init(&self->draw, "visible");
	drawable_set_entity(&self->draw, c_entity(self));
	drawable_set_vs(&self->draw, self->vs);
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

c_model_t *c_model_new(mesh_t *mesh, mat_t *mat, int cast_shadow, int visible)
{
	c_model_t *self = component_new("model");

	/* c_model_add_layer(self, mat, -1, 0); */
	/* self->mat = mat; */

	/* self->mesh = mesh; */
	self->cast_shadow = cast_shadow;
	self->visible = visible;

	c_model_set_mesh(self, mesh);
	c_model_set_mat(self, mat);

	return self;
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

void c_model_propagate_edit(c_model_t *self, int cmd_id)
{
	if(cmd_id >= vector_count(self->history)) return;
	int last_update_id = 0;
	if(self->mesh)
	{
		last_update_id = self->mesh->update_id;
	}

	/* int update_id = -1; */
	mesh_t *last = NULL;
	if(cmd_id > 0)
	{
		mesh_history_t *prev = vector_get(self->history, cmd_id - 1);
		last = prev->state;
	}
	if(last)
	{
		last->update_id++;
	}
	int i;
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
	mesh_history_t command = {NULL, target, type,
		calloc(1, tool->size), calloc(1, tool->size)};

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

c_model_t *c_model_cull_face(c_model_t *self, int inverted)
{
	if(inverted == 0)
	{
		self->mesh->cull_front = 1;
		self->mesh->cull_back = 0;
	}
	else if(inverted == 1)
	{
		self->mesh->cull_front = 1;
		self->mesh->cull_back = 0;
	}
	else if(inverted == 2)
	{
		self->mesh->cull_front = 0;
		self->mesh->cull_back = 0;
	}
	else
	{
		self->mesh->cull_front = 1;
		self->mesh->cull_back = 1;
	}
	/* g_update_id++; */
	drawable_model_changed(&self->draw);
	return self;
}

void c_model_set_vs(c_model_t *self, vs_t *vs)
{
	self->vs = vs;
	drawable_set_vs(&self->draw, vs);
}

void c_model_set_xray(c_model_t *self, int xray)
{
	self->xray = xray;
	drawable_set_xray(&self->draw, xray);
}

c_model_t *c_model_smooth(c_model_t *self, int smooth)
{
	/* self->layers[layer].smooth_angle = smooth; */
	self->mesh->smooth_angle = smooth;
	mesh_modified(self->mesh);
	drawable_model_changed(&self->draw);
	return self;
}

c_model_t *c_model_wireframe(c_model_t *self, int wireframe)
{
	/* self->layers[layer].wireframe = wireframe; */
	self->mesh->wireframe = wireframe;
	g_update_id++;
	drawable_model_changed(&self->draw);
	return self;
}

void c_model_set_mat(c_model_t *self, mat_t *mat)
{
	self->mat = mat;
	drawable_set_mat(&self->draw, mat->id);
}

void c_model_set_mesh(c_model_t *self, mesh_t *mesh)
{
	mesh_t *old_mesh = self->mesh;
	self->mesh = mesh;
	if(old_mesh) mesh_destroy(old_mesh);

	drawable_set_mesh(&self->draw, mesh);
}

int c_model_created(c_model_t *self)
{
	if(self->mesh) g_update_id++;
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
				entity_t ent = entity_new(c_model_new(NULL, mat_new("k"), 1, 1));
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

	new_value = nk_check_label(ctx, "Visible", self->visible);
	if(new_value != self->visible)
	{
		self->visible = new_value;
		changes = 1;
	}
	new_value = nk_check_label(ctx, "Cast shadow", self->cast_shadow);
	if(new_value != self->cast_shadow)
	{
		self->cast_shadow = new_value;
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

		mesh_t *mesh = self->mesh;

		new_value = nk_check_label(ctx, "Wireframe",
				mesh->wireframe);

		if(new_value != mesh->wireframe)
		{
			mesh->wireframe = new_value;
			changes = 1;
		}

		float smooth = mesh->smooth_angle;
		nk_property_float(ctx, "#smooth:", 0.0f, &smooth,
				1.0f, 0.05f, 0.05f);
		if(smooth != mesh->smooth_angle)
		{
			mesh->smooth_angle = smooth;
			/* self->mesh->update_id++; */
			mesh_modified(mesh);
			mesh_update(mesh);
		}

		new_value = nk_check_label(ctx, "Cull front",
				mesh->cull_front);

		if(new_value != mesh->cull_front)
		{
			mesh->cull_front = new_value;
			changes = 1;
		}

		new_value = nk_check_label(ctx, "Cull back",
				mesh->cull_back);

		if(new_value != mesh->cull_back)
		{
			mesh->cull_back = new_value;
			changes = 1;
		}
	}

	if(self->mat && self->mat->name[0] != '_')
	{
		mat_menu(self->mat, ctx);
	}
	if(changes)
	{
		g_update_id++;
		drawable_model_changed(&self->draw);
	}

	/* nk_tree_pop(ctx); */


	return CONTINUE;
}

static int c_model_position_changed(c_model_t *self)
{
	c_node_t *node = c_node(self);
	c_node_update_model(node);
	drawable_set_transform(&self->draw, node->model);

#ifdef MESH4
	drawable_set_angle4(&self->draw, node->angle4
#endif

	return CONTINUE;
}

void add_tool(char *name, tool_gui_cb gui, tool_edit_cb edit, size_t size,
		void *defaults, int require_sys)
{
	struct edit_tool tool = {gui, edit, size};
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
	g_update_id++;
	drawable_set_mesh(&self->draw, NULL);
}

REG()
{
	signal_init(sig("model_changed"), sizeof(mesh_t));

	/* TODO destroyer */
	ct_t *ct = ct_new("model", sizeof(c_model_t),
			c_model_init, c_model_destroy, 1, ref("node"));

	ct_listener(ct, ENTITY, sig("entity_created"), c_model_created);

	ct_listener(ct, WORLD, sig("component_menu"), c_model_menu);
	ct_listener(ct, WORLD, sig("component_tool"), c_model_tool);

	ct_listener(ct, ENTITY, sig("node_changed"), c_model_position_changed);

	/* ct_listener(ct, WORLD, sig("render_shadows"), c_model_render_shadows); */

	/* ct_listener(ct, WORLD, sig("render_selectable"), c_model_render_selectable); */


	add_tool("circle", (tool_gui_cb)tool_circle_gui,
			(tool_edit_cb)tool_circle_edit,
		sizeof(struct conf_circle), &(struct conf_circle){2, 10}, 1);

	add_tool("sphere", (tool_gui_cb)tool_sphere_gui,
			(tool_edit_cb)tool_sphere_edit, sizeof(struct conf_sphere), NULL, 1);

	add_tool("cube", (tool_gui_cb)tool_cube_gui, (tool_edit_cb)tool_cube_edit,
		sizeof(struct conf_cube),
		&(struct conf_cube){1.0f}, 1);

	add_tool("torus", (tool_gui_cb)tool_torus_gui, (tool_edit_cb)tool_torus_edit,
		sizeof(struct conf_torus),
		&(struct conf_torus){1.0f, 0.2f, 30, 20}, 1);

	add_tool("disk", (tool_gui_cb)tool_disk_gui, (tool_edit_cb)tool_disk_edit,
		sizeof(struct conf_disk),
		&(struct conf_disk){2.0f, 0.2f, 10}, 1);

	add_tool("icosphere", (tool_gui_cb)tool_icosphere_gui,
			(tool_edit_cb)tool_icosphere_edit, sizeof(struct conf_ico),
			&(struct conf_ico){1.0f, 0}, 1);

	add_tool("extrude", (tool_gui_cb)tool_extrude_gui,
		(tool_edit_cb)tool_extrude_edit, sizeof(struct conf_extrude),
		&(struct conf_extrude){VEC3(0.0f, 2.0f, 0.0f), 1.0f, 1}, 0);

	add_tool("spherize", (tool_gui_cb)tool_spherize_gui,
		(tool_edit_cb)tool_spherize_edit, sizeof(struct conf_spherize),
		&(struct conf_spherize){1, 1}, 0);

	add_tool("subdivide", (tool_gui_cb)tool_subdivide_gui,
		(tool_edit_cb)tool_subdivide_edit, sizeof(struct conf_subdivide),
		&(struct conf_subdivide){1}, 0);
}
