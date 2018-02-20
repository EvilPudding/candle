#include "light.h"
#include "probe.h"
#include "spacial.h"
#include "candle.h"
#include <stdlib.h>

DEC_CT(ct_light);
DEC_SIG(render_shadows);

static shader_t *g_shader;

void c_light_init(c_light_t *self)
{
}

c_light_t *c_light_new(float intensity, vec4_t color, int shadow_size)
{
	c_light_t *self = component_new(ct_light);

	self->intensity = intensity;
	self->color = color;
	self->shadow_size = shadow_size;

	if(!g_shader) g_shader = shader_new("depth");

	return self;
}

int c_light_created(c_light_t *self)
{
	entity_add_component(c_entity(self),
			(c_t*)c_probe_new(self->shadow_size, g_shader));
	return 1;
}

void c_light_destroy(c_light_t *self)
{
	free(self);
}

int c_light_render(c_light_t *self)
{
	c_probe_t *probe = c_probe(self);

	c_probe_render(probe, render_shadows, g_shader);
	return 1;
}

void c_light_register()
{
	ct_t *ct = ct_new("c_light", &ct_light,
			sizeof(c_light_t), (init_cb)c_light_init, 1, ct_spacial);

	ct_listener(ct, ENTITY, entity_created, c_light_created);

	ct_listener(ct, WORLD, offscreen_render, c_light_render);

	signal_init(&render_shadows, sizeof(shader_t*));
}
