#include "ambient.h"
#include "spatial.h"
#include "candle.h"
#include <stdlib.h>

void c_ambient_init(c_ambient_t *self)
{
}

c_ambient_t *c_ambient_new(int map_size)
{
	c_ambient_t *self = component_new("ambient");

	self->map_size = map_size;

	/* if(!g_shader) g_shader = fs_new("ambient"); */

	/* entity_add_component(c_entity(self), */
			/* (c_t*)c_probe_new(self->map_size, ref("visible"), g_shader)); */

	return self;
}

void c_ambient_destroy(c_ambient_t *self)
{
}

REG()
{
	/* ct_t *ct = */ ct_new("ambient", sizeof(c_ambient_t),
			(init_cb)c_ambient_init, NULL, 1, ref("spatial"));
}


