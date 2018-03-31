#include "../ext.h"
#include "model.h"
#include "mesh_gl.h"
#include "node.h"
#include "spacial.h"
#include "light.h"
#include <nk.h>
#include <candle.h>
#include <systems/renderer.h>
#include "shader.h"
#include <systems/editmode.h>

static mat_t *g_missing_mat = NULL;

int c_model_menu(c_model_t *self, void *ctx);
int g_update_id = 0;
vs_t *g_model_vs = NULL;

static void c_model_init(c_model_t *self)
{
	if(!g_missing_mat)
	{
		g_missing_mat = mat_new("missing");
		mat_set_diffuse(g_missing_mat, (prop_t){.color=vec4(0.0, 0.9, 1.0, 1.0)});

	}

	self->sprite = 0;
	self->visible = 1;
	self->layers = malloc(sizeof(*self->layers) * 16);

	if(!g_model_vs)
	{
		g_model_vs = vs_new("model", 1, vertex_modifier_new(
			"	{\n"
			"#ifdef MESH4\n"
			"		float Y = cos(camera.angle4);\n"
			"		float W = sin(camera.angle4);\n"
			"		pos = vec4(vec3(P.x, P.y * Y + P.w * W, P.z), 1.0);\n"
			"#endif\n"
			"		vertex_position = (camera.view * M * pos).xyz;\n"

			"		mat4 MV    = camera.view * M;\n"
			"		vertex_normal    = (MV * vec4( N, 0.0f)).xyz;\n"
			"		vertex_tangent   = (MV * vec4(TG, 0.0f)).xyz;\n"
			"		vertex_bitangent = (MV * vec4(BT, 0.0f)).xyz;\n"
			"		texcoord = vec2(-UV.y, UV.x);\n"

			"		object_id = id;\n"
			"		poly_id = ID;\n"
			"		TM = mat3(vertex_tangent, vertex_bitangent, vertex_normal);\n"

			"		pos = MVP * pos;\n"
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
	entity_signal_same(c_entity(self), sig("mesh_changed"), NULL);
}

c_model_t *c_model_new(mesh_t *mesh, mat_t *mat, int cast_shadow)
{
	c_model_t *self = component_new(ct_model);

	self->layers_num = 0;
	self->mesh = mesh;
	self->cast_shadow = cast_shadow;

	c_model_add_layer(self, mat, -1, 0);

	return self;
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
	return 1;
}

/* #include "components/name.h" */
int c_model_render_shadows(c_model_t *self)
{
	if(self->cast_shadow) c_model_render_visible(self);
	return 1;
}

int c_model_render_transparent(c_model_t *self)
{
	if(!self->mesh || !self->visible) return 1;
	if(self->before_draw) if(!self->before_draw((c_t*)self)) return 1;

	return c_model_render(self, 1);
}

int c_model_render(c_model_t *self, int transp)
{
	return c_model_render_at(self, c_node(self), transp);
}

int c_model_render_at(c_model_t *self, c_node_t *node, int transp)
{
	shader_t *shader = vs_bind(g_model_vs);
	if(!shader) return 0;
	if(node)
	{
		c_node_update_model(node);

		shader_update(shader, &node->model);
	}
	c_mesh_gl_draw(c_mesh_gl(self), transp);
	return 1;
}

int c_model_render_visible(c_model_t *self)
{
	if(!self->mesh || !self->visible) return 1;
	if(self->before_draw) if(!self->before_draw((c_t*)self)) return 1;

	return c_model_render(self, 0);
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

			nk_tree_pop(ctx);
		}
	}


	return 1;
}

int c_model_scene_changed(c_model_t *self, entity_t *entity)
{
	if(self->visible)
	{
		g_update_id++;
	}
	return 1;
}


DEC_CT(ct_model)
{
	signal_init(sig("mesh_changed"), sizeof(mesh_t));

	ct_t *ct = ct_new("c_model", &ct_model, sizeof(c_model_t),
			(init_cb)c_model_init, 2, ct_spacial, ct_node);

	ct_listener(ct, ENTITY, sig("entity_created"), c_model_created);

	ct_listener(ct, WORLD, sig("component_menu"), c_model_menu);

	ct_listener(ct, ENTITY, sig("spacial_changed"), c_model_scene_changed);

	ct_listener(ct, WORLD, sig("render_visible"), c_model_render_visible);

	ct_listener(ct, WORLD, sig("render_transparent"), c_model_render_transparent);

	ct_listener(ct, WORLD, sig("render_shadows"), c_model_render_shadows);
}
