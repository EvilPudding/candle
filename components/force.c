#include "../ext.h"
#include "force.h"
#include "velocity.h"
#include "spacial.h"

DEC_CT(ct_force);

void c_force_init(c_force_t *self)
{
	self->force = vec3(0.0, 0.0, 0.0);
	self->active = 1;
}

c_force_t *c_force_new(float x, float y, float z, int active)
{
	c_force_t *self = component_new(ct_force);

	self->active = active;
	self->force = vec3(x, y, z);

	return self;
}

void c_force_register()
{
	ct_new("c_force", &ct_force, sizeof(c_force_t), (init_cb)c_force_init, 0);
}

