#include "ambient.h"
#include "probe.h"
#include "spacial.h"
#include "candle.h"
#include <stdlib.h>

unsigned long ct_ambient;

static shader_t *g_shader;

void c_ambient_init(c_ambient_t *self)
{
	self->super = component_new(ct_ambient);
}

c_ambient_t *c_ambient_new(int map_size)
{
	c_ambient_t *self = malloc(sizeof *self);
	c_ambient_init(self);

	self->map_size = map_size;

	if(!g_shader) g_shader = shader_new("ambient");

	return self;
}

int c_ambient_created(c_ambient_t *self)
{
	entity_add_component(c_entity(self),
			(c_t*)c_probe_new(self->map_size, g_shader));
	return 1;
}

void c_ambient_destroy(c_ambient_t *self)
{
	free(self);
}

void c_ambient_register(ecm_t *ecm)
{
	ct_t *ct = ecm_register(ecm, &ct_ambient, sizeof(c_ambient_t),
			(init_cb)c_ambient_init, 1, ct_spacial);

	ct_register_listener(ct, SAME_ENTITY, entity_created,
			(signal_cb)c_ambient_created);
}


