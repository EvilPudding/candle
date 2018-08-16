#include "decal.h"
#include "spacial.h"
#include "candle.h"
#include <components/mesh_gl.h>
#include <components/model.h>
#include <components/node.h>
#include <stdlib.h>


mesh_t *g_decal_mesh = NULL;

void c_decal_init(c_decal_t *self)
{

	if(!g_decal_mesh)
	{
		g_decal_mesh = mesh_new();
		mesh_cube(g_decal_mesh, 1, 1);
	}
	self->visible = 1;


} 

c_decal_t *c_decal_new(mat_t *mat)
{
	c_decal_t *self = component_new("decal");

	self->mat = mat;

	if(!c_mesh_gl(self))
	{
		entity_add_component(c_entity(self),
				c_model_new(g_decal_mesh, mat, 0, 1));
		c_model(self)->visible = 0;
		
	}

	return self;
}

int c_decal_render_bound(c_decal_t *self)
{
	c_model_cull_face(c_model(self), 0, 0);
	int res = c_model_render(c_model(self), vs_bind(g_model_vs), 0);

	return res;
}

int c_decal_render(c_decal_t *self)
{
	c_model_cull_face(c_model(self), 0, 1);
	c_renderer_invert_depth(c_renderer(&SYS), 1);
	int res = c_model_render(c_model(self), vs_bind(g_model_vs), 0);
	c_renderer_invert_depth(c_renderer(&SYS), 0);

	return res;
}

REG()
{
	ct_t *ct = ct_new("decal", sizeof(c_decal_t),
			c_decal_init, NULL, 1, ref("spacial"));

	ct_listener(ct, WORLD, sig("render_decals"), c_decal_render);
	ct_listener(ct, WORLD, sig("render_selectable"), c_decal_render_bound);
}


