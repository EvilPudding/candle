#include <ecm.h>
#include <stdlib.h>
#include "velocity.h"
#include "spacial.h"

c_velocity_t *c_velocity_new(float x, float y, float z)
{
	c_velocity_t *self = component_new("c_velocity");

	self->velocity = vec3(x, y, z);

	return self;
}

void c_velocity_set_normal(c_velocity_t *self, vec3_t nor)
{
	self->normal = nor;
}

void c_velocity_set_vel(c_velocity_t *self, float x, float y, float z)
{
	self->velocity.x = x;
	self->velocity.y = y;
	self->velocity.z = z;
}

REG()
{
	ct_new("c_velocity", sizeof(c_velocity_t), NULL, 1, ref("c_spacial"));
}

