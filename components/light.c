#include "light.h"
#include "probe.h"
#include "spacial.h"
#include "candle.h"
#include <stdlib.h>

DEC_CT(ct_light);
DEC_SIG(render_shadows);

static fs_t *g_depth_fs = NULL;

void c_light_init(c_light_t *self)
{
}

c_light_t *c_light_new(float intensity, vec4_t color, int shadow_size)
{
	c_light_t *self = component_new(ct_light);

	self->intensity = intensity;
	self->color = color;
	self->shadow_size = shadow_size;

	if(!g_depth_fs) g_depth_fs = fs_new("depth");

	entity_add_component(c_entity(self), (c_t*)c_probe_new(self->shadow_size));

	return self;
}

int c_light_created(c_light_t *self)
{
	c_probe_update_position(c_probe(self));

	return 1;
}


void c_light_destroy(c_light_t *self)
{
	free(self);
}

int c_light_render(c_light_t *self)
{
	c_probe_t *probe = c_probe(self);
	if(!probe) return 0;
	if(!g_depth_fs) return 0;

	fs_bind(g_depth_fs);

	c_probe_render(probe, render_shadows);
	return 1;
}

void c_light_register()
{
	ct_t *ct = ct_new("c_light", &ct_light,
			sizeof(c_light_t), (init_cb)c_light_init, 2, ct_spacial, ct_node);

	ct_listener(ct, ENTITY, entity_created, c_light_created);

	ct_listener(ct, WORLD, offscreen_render, c_light_render);

	signal_init(&render_shadows, 0);
}
