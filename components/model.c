#include "../ext.h"
#include "model.h"
#include "mesh_gl.h"
#include "node.h"
#include "spacial.h"
#include "light.h"
#include <nk.h>
#include <systems/renderer.h>
#include "shader.h"
#include <systems/editmode.h>

DEC_CT(ct_model);

DEC_SIG(mesh_changed);

static material_t *g_missing_mat = NULL;

int c_model_menu(c_model_t *self, void *ctx);
int g_update_id = 0;

static void c_model_init(c_model_t *self)
{
	if(!g_missing_mat)
	{
		g_missing_mat = material_new("missing");
		material_set_diffuse(g_missing_mat, (prop_t){.color=vec4(0.0, 0.9, 1.0, 1.0)});

	}

	self->cast_shadow = 0;
	self->visible = 1;
	self->mesh = NULL;
	self->before_draw = NULL;
	self->layers = malloc(sizeof(*self->layers) * 16);
}

void c_model_add_layer(c_model_t *self, int selection, float offset)
{
	int i = self->layers_num++;
	self->layers[i].mat = NULL;
	self->layers[i].selection = selection;
	self->layers[i].cull_front = 0;
	self->layers[i].cull_back = 1;
	self->layers[i].wireframe = 0;
	self->layers[i].update_id = 0;
	self->layers[i].offset = 0;
}

c_model_t *c_model_new(mesh_t *mesh, int cast_shadow)
{
	c_model_t *self = component_new(ct_model);

	self->layers_num = 0;
	self->mesh = mesh;
	self->cast_shadow = cast_shadow;

	c_model_add_layer(self, -1, 0);

	return self;
}

c_model_t *c_model_paint(c_model_t *self, int layer, material_t *mat)
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
	entity_signal_same(c_entity(self), mesh_changed, NULL);
	if(old_mesh) mesh_destroy(old_mesh);
}

int c_model_created(c_model_t *self)
{
	if(self->mesh)
	{
		g_update_id++;
		entity_signal_same(c_entity(self), mesh_changed, NULL);
	}
	return 1;
}

/* #include "components/name.h" */
int c_model_render_shadows(c_model_t *self, shader_t *shader)
{
	if(self->cast_shadow) c_model_render_visible(self, shader);
	return 1;
}

int c_model_render_transparent(c_model_t *self, shader_t *shader)
{
	if(!self->mesh || !self->visible) return 1;
	if(self->before_draw) if(!self->before_draw((c_t*)self)) return 1;

	c_node_t *node = c_node(self);
	if(node)
	{
		c_node_update_model(node);
		shader_update(shader, &node->model);
	}

	c_mesh_gl_draw(c_mesh_gl(self), shader, 1);
	return 1;
}

int c_model_render_visible(c_model_t *self, shader_t *shader)
{
	if(!self->mesh || !self->visible) return 1;
	if(self->before_draw) if(!self->before_draw((c_t*)self)) return 1;

	c_node_t *node = c_node(self);
	if(node)
	{
		c_node_update_model(node);
		shader_update(shader, &node->model);
	}

	c_mesh_gl_draw(c_mesh_gl(self), shader, 0);
	return 1;
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


void c_model_register()
{
	ecm_register_signal(&mesh_changed, sizeof(mesh_t));

	ct_t *ct = ecm_register("Model", &ct_model, sizeof(c_model_t),
			(init_cb)c_model_init, 2, ct_spacial, ct_node);

	ct_register_listener(ct, SAME_ENTITY, entity_created,
			(signal_cb)c_model_created);

	ct_register_listener(ct, WORLD|RENDER_THREAD, component_menu,
			(signal_cb)c_model_menu);

	ct_register_listener(ct, SAME_ENTITY, spacial_changed,
			(signal_cb)c_model_scene_changed);

	ct_register_listener(ct, WORLD|RENDER_THREAD, render_visible,
			(signal_cb)c_model_render_visible);

	ct_register_listener(ct, WORLD|RENDER_THREAD, render_transparent,
			(signal_cb)c_model_render_transparent);

	ct_register_listener(ct, WORLD|RENDER_THREAD, render_shadows,
			(signal_cb)c_model_render_shadows);
}
