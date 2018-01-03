#include "../ext.h"
#include "model.h"
#include "mesh_gl.h"
#include "node.h"
#include "spacial.h"
#include "../systems/renderer.h"
#include "shader.h"

unsigned long ct_model;

unsigned long mesh_changed;

unsigned long render_filter;

static void c_model_init(c_model_t *self)
{
	self->super = component_new(ct_model);

	self->cast_shadow = 0;
	self->visible = 1;
	self->mat = NULL;
	self->mesh = NULL;
	self->before_draw = NULL;
}

c_model_t *c_model_new(mesh_t *mesh, material_t *mat, int cast_shadow)
{
	c_model_t *self = malloc(sizeof *self);
	c_model_init(self);

	self->mat = mat;
	self->mesh = mesh;
	self->cast_shadow = cast_shadow;

	return self;
}

void c_model_cull_face(c_model_t *self, int inverted)
{
	c_mesh_gl_t *gl = c_mesh_gl(c_entity(self));
	switch(inverted)
	{
		case 0: gl->cull_face = GL_BACK; break;
		case 1: gl->cull_face = GL_FRONT; break;
		case 2: gl->cull_face = GL_FRONT_AND_BACK; break;
		case 3: gl->cull_face = GL_NONE; break;
	}
}

void c_model_wireframe(c_model_t *self, int wireframe)
{
	c_mesh_gl_t *gl = c_mesh_gl(c_entity(self));
	gl->wireframe = wireframe;
}

int c_model_draw(c_model_t *self)
{
	return c_mesh_gl_draw(c_mesh_gl(c_entity(self)));
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

static int c_model_render_filter(c_model_t *self, int *transparent)
{
	if(self->mesh && self->mat)
	{
		return (self->mat->transparency.color.a != 0.0f) == *transparent;
	}
	else return 0;
}

void c_model_register(ecm_t *ecm)
{
	mesh_changed = ecm_register_signal(ecm, sizeof(mesh_t));
	render_filter = ecm_register_signal(ecm, sizeof(shader_t));

	ct_t *ct = ecm_register(ecm, &ct_model, sizeof(c_model_t),
			(init_cb)c_model_init, 2, ct_spacial, ct_node);
	ct_register_listener(ct, SAME_ENTITY, entity_created,
			(signal_cb)c_model_created);

	ct_register_listener(ct, WORLD, render_filter,
			(signal_cb)c_model_render_filter);


}
