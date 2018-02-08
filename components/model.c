#include "../ext.h"
#include "model.h"
#include "mesh_gl.h"
#include "node.h"
#include "spacial.h"
#include "../systems/renderer.h"
#include "shader.h"

unsigned long ct_model;

unsigned long mesh_changed;

unsigned long render_model;
static material_t *g_missing_mat = NULL;

static void c_model_init(c_model_t *self)
{
	if(!g_missing_mat)
	{
		g_missing_mat = material_new("missing");
		material_set_diffuse(g_missing_mat, (prop_t){.color=vec4(0.0, 0.9, 1.0, 1.0)});

	}
	self->super = component_new(ct_model);

	self->cast_shadow = 0;
	self->visible = 1;
	self->mesh = NULL;
	self->before_draw = NULL;
}

void c_model_add_layer(c_model_t *self, int selection)
{
	int i = self->layers_num++;
	self->layers[i].mat = NULL;
	self->layers[i].selection = selection;
	self->layers[i].cull_face = 0;
	self->layers[i].wireframe = 0;
	self->layers[i].update_id = 0;
}

c_model_t *c_model_new(mesh_t *mesh, int cast_shadow)
{
	c_model_t *self = malloc(sizeof *self);
	c_model_init(self);

	self->layers_num = 0;
	self->mesh = mesh;
	self->cast_shadow = cast_shadow;

	c_model_add_layer(self, -1);

	return self;
}

c_model_t *c_model_paint(c_model_t *self, int layer, material_t *mat)
{
	self->layers[layer].mat = mat;
	/* c_mesh_gl_t *gl = c_mesh_gl(c_entity(self)); */
	/* gl->groups[layer].mat = mat; */
	self->layers[layer].update_id++;
	return self;
}

c_model_t *c_model_cull_face(c_model_t *self, int layer, int inverted)
{
	self->layers[layer].cull_face = inverted;
	self->layers[layer].update_id++;
	return self;
}

c_model_t *c_model_wireframe(c_model_t *self, int layer, int wireframe)
{
	self->layers[layer].wireframe = wireframe;
	self->layers[layer].update_id++;
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
		entity_signal_same(c_entity(self), mesh_changed, NULL);
	}
	return 1;
}

/* #include "components/name.h" */
int c_model_render(c_model_t *self, int transparent, shader_t *shader)
{
	if(!self->mesh || !self->visible) return 0;
	if(self->before_draw) if(!self->before_draw((c_t*)self)) return 0;

	c_node_t *node = c_node(c_entity(self));
	if(node)
	{
		c_node_update_model(node);
		shader_update(shader, &node->model);
	}
	shader_bind_mesh(shader, self->mesh);
	/* if(c_name(c_entity(model))) printf("%s\n", c_name(c_entity(model))->name); */

	return c_mesh_gl_draw(c_mesh_gl(c_entity(self)), shader, transparent);
}

void c_model_register(ecm_t *ecm)
{
	mesh_changed = ecm_register_signal(ecm, sizeof(mesh_t));
	render_model = ecm_register_signal(ecm, sizeof(shader_t));

	ct_t *ct = ecm_register(ecm, &ct_model, sizeof(c_model_t),
			(init_cb)c_model_init, 2, ct_spacial, ct_node);
	ct_register_listener(ct, SAME_ENTITY, entity_created,
			(signal_cb)c_model_created);


}
