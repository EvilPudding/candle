#include "decal.h"
#include "spatial.h"
#include "../candle.h"
#include "../utils/drawable.h"
#include "../components/model.h"
#include "../components/node.h"
#include <stdlib.h>


mesh_t *g_decal_mesh = NULL;

void c_decal_init(c_decal_t *self)
{

	if(!g_decal_mesh)
	{
		g_decal_mesh = mesh_new();
		mesh_cube(g_decal_mesh, -1, 1);
	}
	self->visible = 1;
} 

void ct_decal(ct_t *self)
{
	ct_init(self, "decal", sizeof(c_decal_t));
	ct_set_init(self, (init_cb)c_decal_init);
	ct_add_dependency(self, ct_node);
}

c_decal_t *c_decal_new(mat_t *mat, int visible, int selectable)
{
	c_model_t *mc;
	c_decal_t *self = component_new(ct_decal);

	self->mat = mat;

	entity_add_component(c_entity(self),
			c_model_new(g_decal_mesh, mat, 0, visible));

	mc = c_model(self);
	c_model_set_groups(mc, ref("decals"), 0, ref("transparent_decals"),
			selectable ? ref("selectable") : 0);

	return self;
}

