#include "light.h"
#include "probe.h"
#include "spacial.h"
#include "candle.h"
#include <stdlib.h>

DEC_CT(ct_light);

static shader_t *g_shader;

void c_light_init(c_light_t *self)
{
	self->super = component_new(ct_light);
}

c_light_t *c_light_new(float intensity, vec4_t color, int shadow_size)
{
	c_light_t *self = malloc(sizeof *self);
	c_light_init(self);

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

void c_light_register(ecm_t *ecm)
{
	ct_t *ct = ecm_register(ecm, &ct_light, sizeof(c_light_t),
			(init_cb)c_light_init, 1, ct_spacial);

	ct_register_listener(ct, SAME_ENTITY, entity_created,
			(signal_cb)c_light_created);
}


