#include "decal.h"
#include "spacial.h"
#include "candle.h"
#include <stdlib.h>

DEC_CT(ct_decal);

void c_decal_init(c_decal_t *self) { } 

mesh_t *g_decal_mesh = NULL;

c_decal_t *c_decal_new(mat_t *mat)
{
	c_decal_t *self = component_new(ct_decal);

	self->visible = 1;
	self->mat = mat;

	if(!g_decal_mesh)
	{
		g_decal_mesh = sauces_mesh("cube.ply");
	}

	if(!c_mesh_gl(self))
	{
		entity_add_component(c_entity(self),
				c_model_new(g_decal_mesh, mat, 0));
		c_model(self)->visible = 0;
	}

	return self;
}

int c_decal_render(c_decal_t *self, shader_t *shader)
{
	if(!self->visible) return 1;

	c_node_t *node = c_node(self);
	if(node)
	{
		c_node_update_model(node);
		shader_update(shader, &node->model);
	}

	c_mesh_gl_draw(c_mesh_gl(self), 0);
	return 1;
}

void c_decal_register()
{
	ct_t *ct = ct_new("c_decal", &ct_decal, sizeof(c_decal_t),
			(init_cb)c_decal_init, 1, ct_spacial);

	ct_listener(ct, WORLD, render_decals, c_decal_render);
}


