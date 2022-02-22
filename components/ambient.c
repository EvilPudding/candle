#include "ambient.h"
#include "spatial.h"
#include "node.h"
#include "../candle.h"
#include <stdlib.h>

void ct_ambient(ct_t *self)
{
	ct_init(self, "ambient", sizeof(c_ambient_t));
	ct_add_dependency(self, ct_node);
}

c_ambient_t *c_ambient_new(int map_size)
{
	c_ambient_t *self = component_new(ct_ambient);

	self->map_size = map_size;

	/* if(!g_shader) g_shader = fs_new("ambient"); */

	/* entity_add_component(c_entity(self), */
			/* (c_t*)c_probe_new(self->map_size, ref("visible"), g_shader)); */

	return self;
}

void c_ambient_destroy(c_ambient_t *self)
{
}
