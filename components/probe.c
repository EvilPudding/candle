#include "probe.h"
#include "spacial.h"
#include "candle.h"
#include <stdlib.h>

unsigned long ct_probe;

void c_probe_init(c_probe_t *self)
{
	self->super = component_new(ct_probe);
}

c_probe_t *c_probe_new(int map_size, shader_t *shader)
{
	c_probe_t *self = malloc(sizeof *self);
	c_probe_init(self);
	self->shader = shader;

	self->map = texture_cubemap(map_size, map_size, 1);
	self->before_draw = NULL;

	self->last_update = 0;

	return self;
}

static int c_probe_update_position(c_probe_t *self)
{
	vec3_t pos = c_spacial(self->super.entity)->position;

	self->projection = mat4_perspective(M_PI / 2.0f, 1.0f, 0.5f, 100.5f); 

	self->views[0] = mat4_look_at(pos, vec3_add(pos, vec3(1.0, 0.0, 0.0)),
			vec3(0.0,-1.0,0.0));

	self->views[1] = mat4_look_at(pos, vec3_add(pos, vec3(-1.0, 0.0, 0.0)),
			vec3(0.0, -1.0, 0.0));

	self->views[2] = mat4_look_at(pos, vec3_add(pos, vec3(0.0, 1.0, 0.0)),
			vec3(0.0, 0.0, 1.0));

	self->views[3] = mat4_look_at(pos, vec3_add(pos, vec3(0.0, -1.0, 0.0)),
			vec3(0.0, 0.0, -1.0));

	self->views[4] = mat4_look_at(pos, vec3_add(pos, vec3(0.0, 0.0, 1.0)),
			vec3(0.0, -1.0, 0.0));

	self->views[5] = mat4_look_at(pos, vec3_add(pos, vec3(0.0, 0.0, -1.0)),
			vec3(0.0, -1.0, 0.0));

	self->last_update++;

	return 0;
}

void c_probe_destroy(c_probe_t *self)
{
	free(self);
}

void c_probe_register(ecm_t *ecm)
{
	ct_t *ct = ecm_register(ecm, &ct_probe, sizeof(c_probe_t),
			(init_cb)c_probe_init, 1, ct_spacial);

	ct_register_listener(ct, SAME_ENTITY, spacial_changed,
			(signal_cb)c_probe_update_position);
}


