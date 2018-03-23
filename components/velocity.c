#include "velocity.h"
#include "spacial.h"

DEC_SIG(velocity_changed);

void c_velocity_init(c_velocity_t *self)
{

	self->normal = vec3(0.0, 0.0, 0.0);
	self->velocity = vec3(0.0, 0.0, 0.0);
}

c_velocity_t *c_velocity_new(float x, float y, float z)
{
	c_velocity_t *self = component_new(ct_velocity);

	self->velocity = vec3(x, y, z);

	return self;
}

DEC_CT(ct_velocity)
{
	ct_new("c_velocity", &ct_velocity, sizeof(c_velocity_t),
			(init_cb)c_velocity_init, 1, ct_spacial);
}

void c_velocity_set_vel(c_velocity_t *self, float x, float y, float z)
{
	self->velocity.x = x;
	self->velocity.y = y;
	self->velocity.z = z;
}
