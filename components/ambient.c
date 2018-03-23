#include "ambient.h"
#include "probe.h"
#include "spacial.h"
#include "candle.h"
#include <stdlib.h>
#include <systems/renderer.h>


static fs_t *g_shader;

void c_ambient_init(c_ambient_t *self)
{
}

c_ambient_t *c_ambient_new(int map_size)
{
	c_ambient_t *self = component_new(ct_ambient);

	self->map_size = map_size;

	if(!g_shader) g_shader = fs_new("ambient");

	entity_add_component(c_entity(self),
			(c_t*)c_probe_new(self->map_size));

	return self;
}

void c_ambient_destroy(c_ambient_t *self)
{
	free(self);
}

int c_ambient_render(c_ambient_t *self)
{
	c_probe_t *probe = c_probe(self);
	if(!probe) return 0;
	if(!g_shader) return 0;

	fs_bind(g_shader);

	c_probe_render(probe, render_visible);

	return 1;
}

DEC_CT(ct_ambient)
{
	ct_t *ct = ct_new("c_ambient", &ct_ambient,
			sizeof(c_ambient_t), (init_cb)c_ambient_init, 1, ct_spacial);

	ct_listener(ct, WORLD, offscreen_render, c_ambient_render);
}


