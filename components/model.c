#include "model.h"
#include "mesh_gl.h"
#include "node.h"
#include "spacial.h"
#include "light.h"
#include <utils/nk.h>
#include <candle.h>
#include <systems/renderer.h>
#include <utils/shader.h>
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

static int tool_torus_gui(void *ctx, struct conf_torus *conf)
{
	nk_property_float(ctx, "#radius:", -10000, &conf->radius1, 10000, 0.1, 0.05);
	nk_property_int(ctx, "#segments:", 1, &conf->segments1, 1000, 1, 1);
	nk_property_float(ctx, "#inner radius:", -10000, &conf->radius2, 10000, 0.1, 0.05);
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
	nk_property_float(ctx, "#w:", -10000, &conf->offset.w, 10000, 0.1, 0.05);
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
		mesh_extrude_edges(state, new->steps, new->offset.xyz, new->scale,
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
	self->layers = malloc(sizeof(*self->layers) * 16);

	if(!g_model_vs)
	{
		g_model_vs = vs_new("model", 1, vertex_modifier_new(
			"	{\n"
			"#ifdef MESH4\n"
			"		float Y = cos(angle4);\n"
			"		float W = sin(angle4);\n"
			"		pos = vec4(vec3(P.x, P.y * Y + P.w * W, P.z), 1.0);\n"
			"#endif\n"
			"		vertex_position = (camera.view * M * pos).xyz;\n"

			"		poly_color = COL;\n"
			"		mat4 MV    = camera.view * M;\n"
			"		vec3 vertex_normal    = normalize(MV * vec4( N, 0.0f)).xyz;\n"
			"		vec3 vertex_tangent   = normalize(MV * vec4(TG, 0.0f)).xyz;\n"
			"		vec3 vertex_bitangent = cross(vertex_tangent, vertex_normal);\n"

			"		object_id = id;\n"
			"		poly_id = ID;\n"
			"		TM = mat3(vertex_tangent, vertex_bitangent, vertex_normal);\n"

			"		pos = (camera.projection * MV) * pos;\n"
			"	}\n"
		));
	}
}

void c_model_add_layer(c_model_t *self, mat_t *mat, int selection, float offset)
{
	int i = self->layers_num++;
	self->layers[i].mat = mat;
	self->layers[i].selection = selection;
	self->layers[i].cull_front = 0;
	self->layers[i].cull_back = 1;
	self->layers[i].wireframe = 0;
	self->layers[i].offset = 0;
	self->layers[i].smooth_angle = 0.2f;
	entity_signal_same(c_entity(self), sig("mesh_changed"), NULL);
}

c_model_t *c_model_new(mesh_t *mesh, mat_t *mat, int cast_shadow, int visible)
{
	c_model_t *self = component_new("model");

	c_model_add_layer(self, mat, -1, 0);

	self->mesh = mesh;
	self->cast_shadow = cast_shadow;
	self->visible = visible;
	self->history = vector_new(sizeof(mesh_history_t), FIXED_ORDER, NULL, NULL);

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

	entity_signal_same(c_entity(self), sig("mesh_changed"), NULL);
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

c_model_t *c_model_paint(c_model_t *self, int layer, mat_t *mat)
{
	self->layers[layer].mat = mat;
	/* c_mesh_gl_t *gl = c_mesh_gl(self); */
	/* gl->groups[layer].mat = mat; */
	return self;
}

c_model_t *c_model_cull_face(c_model_t *self, int layer, int inverted)
{
	if(inverted == 0)
	{
		self->layers[layer].cull_front = 1;
		self->layers[layer].cull_back = 0;
	}
	else if(inverted == 1)
	{
		self->layers[layer].cull_front = 1;
		self->layers[layer].cull_back = 0;
	}
	else if(inverted == 2)
	{
		self->layers[layer].cull_front = 0;
		self->layers[layer].cull_back = 0;
	}
	else
	{
		self->layers[layer].cull_front = 1;
		self->layers[layer].cull_back = 1;
	}
	g_update_id++;
	return self;
}

c_model_t *c_model_smooth(c_model_t *self, int layer, int smooth)
{
	self->layers[layer].smooth_angle = smooth;
	mesh_modified(self->mesh);
	return self;
}

c_model_t *c_model_wireframe(c_model_t *self, int layer, int wireframe)
{
	self->layers[layer].wireframe = wireframe;
	g_update_id++;
	return self;
}

void c_model_set_mesh(c_model_t *self, mesh_t *mesh)
{
	mesh_t *old_mesh = self->mesh;
	self->mesh = mesh;
	entity_signal_same(c_entity(self), sig("mesh_changed"), NULL);
	if(old_mesh) mesh_destroy(old_mesh);
}

int c_model_created(c_model_t *self)
{

	if(self->mesh)
	{
		g_update_id++;
		entity_signal_same(c_entity(self), sig("mesh_changed"), NULL);
	}
	return CONTINUE;
}

/* #include "components/name.h" */
int c_model_render_shadows(c_model_t *self)
{
	if(self->cast_shadow) c_model_render_visible(self);
	return CONTINUE;
}

int c_model_render_visible(c_model_t *self)
{
	if(!self->visible) return CONTINUE;

	return c_model_render(self, vs_bind(g_model_vs), 0);
}

int c_model_render_selectable(c_model_t *self)
{
	if(!self->visible) return CONTINUE;
	c_model_render(self, vs_bind(g_model_vs), 2);
	return CONTINUE;
}

int c_model_render_transparent(c_model_t *self)
{
	if(!self->visible) return CONTINUE;

	return c_model_render(self, vs_bind(g_model_vs), 1);
}

int c_model_render(c_model_t *self, shader_t *shader, int flags)
{
	return c_model_render_at(self, shader, c_node(self), flags);
}

int c_model_render_at(c_model_t *self, shader_t *shader, c_node_t *node,
		int flags)
{
	if(!self->mesh || !shader) return STOP;
	if(node)
	{
		c_node_update_model(node);

		if(self->scale_dist > 0.0f)
		{
			vec3_t pos = mat4_mul_vec4(node->model, vec4(0,0,0,1)).xyz;
			float dist = vec3_dist(pos, c_renderer(&SYS)->bound_camera_pos);
			mat4_t model = mat4_scale_aniso(node->model, vec3(dist * self->scale_dist));
#ifdef MESH4
			shader_update(shader, &model, node->angle4);
#else
			shader_update(shader, &model);
#endif
		}
		else
		{
#ifdef MESH4
			shader_update(shader, &node->model, node->angle4);
#else
			shader_update(shader, &node->model);
#endif
		}
	}
	int depth_was_enabled = glIsEnabled(GL_DEPTH_TEST);
	int additive_was_enabled = glIsEnabled(GL_BLEND);

	if(self->xray)
	{
		glDepthRange(0, 0.01);
	}
	c_mesh_gl_draw(c_mesh_gl(self), flags);
	if(self->xray)
	{
		if(!additive_was_enabled)
		{
			/* glDisable(GL_BLEND); */
		}
		if(depth_was_enabled)
		{
			glEnable(GL_DEPTH_TEST);
		}
	}
	glDepthRange(0.0, 1.00);

	return CONTINUE;
}

int c_model_menu(c_model_t *self, void *ctx)
{
	int i;
	int new_value;

	new_value = nk_check_label(ctx, "Visible", self->visible);
	if(new_value != self->visible)
	{
		self->visible = new_value;
		g_update_id++;
	}
	new_value = nk_check_label(ctx, "Cast shadow", self->cast_shadow);
	if(new_value != self->cast_shadow)
	{
		self->cast_shadow = new_value;
		g_update_id++;
	}

	/* if(nk_tree_push(ctx, NK_TREE_TAB, "Edit", NK_MAXIMIZED)) */
	/* { */
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

	for(i = 0; i < self->layers_num; i++)
	{
		char buffer[32];
		snprintf(buffer, sizeof(buffer), "Layer %d", i);
		mat_layer_t *layer = &self->layers[i];
		if(nk_tree_push_id(ctx, NK_TREE_NODE, buffer, NK_MINIMIZED, i))
		{
			new_value = nk_check_label(ctx, "Wireframe",
					layer->wireframe);

			if(new_value != layer->wireframe)
			{
				layer->wireframe = new_value;
				g_update_id++;
			}

			float smooth = layer->smooth_angle;
			nk_property_float(ctx, "#smooth:", 0.0f, &smooth,
					1.0f, 0.05f, 0.05f);
			if(smooth != layer->smooth_angle)
			{
				layer->smooth_angle = smooth;
				/* self->mesh->update_id++; */
				mesh_modified(self->mesh);
				mesh_update(self->mesh);
			}

			new_value = nk_check_label(ctx, "Cull front",
					layer->cull_front);

			if(new_value != layer->cull_front)
			{
				layer->cull_front = new_value;
				g_update_id++;
			}

			new_value = nk_check_label(ctx, "Cull back",
					layer->cull_back);

			if(new_value != layer->cull_back)
			{
				layer->cull_back = new_value;
				g_update_id++;
			}

			if(layer->mat && layer->mat->name[0] != '_')
			{
				mat_menu(layer->mat, ctx);
			}

			nk_tree_pop(ctx);
		}
	}


	return CONTINUE;
}

int c_model_scene_changed(c_model_t *self, entity_t *entity)
{
	if(self->visible)
	{
		g_update_id++;
	}
	return CONTINUE;
}

void add_tool(char *name, tool_gui_cb gui, tool_edit_cb edit, size_t size,
		void *defaults)
{
	struct edit_tool tool = {gui, edit, size};
	strcpy(tool.name, name);
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
}


REG()
{
	signal_init(sig("mesh_changed"), sizeof(mesh_t));

	/* TODO destroyer */
	ct_t *ct = ct_new("model", sizeof(c_model_t),
			c_model_init, c_model_destroy, 1, ref("node"));

	ct_listener(ct, ENTITY, sig("entity_created"), c_model_created);

	ct_listener(ct, WORLD, sig("component_menu"), c_model_menu);

	ct_listener(ct, ENTITY, sig("spacial_changed"), c_model_scene_changed);

	ct_listener(ct, WORLD, sig("render_visible"), c_model_render_visible);

	ct_listener(ct, WORLD, sig("render_transparent"), c_model_render_transparent);

	ct_listener(ct, WORLD, sig("render_shadows"), c_model_render_shadows);

	ct_listener(ct, WORLD, sig("render_selectable"), c_model_render_selectable);

	add_tool("circle", (tool_gui_cb)tool_circle_gui,
			(tool_edit_cb)tool_circle_edit,
		sizeof(struct conf_circle), &(struct conf_circle){2, 10});

	add_tool("sphere", (tool_gui_cb)tool_sphere_gui,
			(tool_edit_cb)tool_sphere_edit, sizeof(struct conf_sphere), NULL);

	add_tool("cube", (tool_gui_cb)tool_cube_gui, (tool_edit_cb)tool_cube_edit,
		sizeof(struct conf_cube),
		&(struct conf_cube){1.0f});

	add_tool("torus", (tool_gui_cb)tool_torus_gui, (tool_edit_cb)tool_torus_edit,
		sizeof(struct conf_torus),
		&(struct conf_torus){2.0f, 0.2f, 10, 4});

	add_tool("icosphere", (tool_gui_cb)tool_icosphere_gui,
			(tool_edit_cb)tool_icosphere_edit, sizeof(struct conf_ico),
			&(struct conf_ico){1.0f, 0});

	add_tool("extrude", (tool_gui_cb)tool_extrude_gui,
		(tool_edit_cb)tool_extrude_edit, sizeof(struct conf_extrude),
		&(struct conf_extrude){vec4(0.0f, 2.0f, 0.0f, 0.0f), 1.0f, 1});

	add_tool("spherize", (tool_gui_cb)tool_spherize_gui,
		(tool_edit_cb)tool_spherize_edit, sizeof(struct conf_spherize),
		&(struct conf_spherize){1, 1});

	add_tool("subdivide", (tool_gui_cb)tool_subdivide_gui,
		(tool_edit_cb)tool_subdivide_edit, sizeof(struct conf_subdivide),
		&(struct conf_subdivide){1});
}
