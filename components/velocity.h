#ifndef VELOCITY_H
#define VELOCITY_H

#include <ecm.h>
#include "../glutil.h"

typedef struct
{
	c_t super; /* extends c_t */

	vec3_t pre_movement_pos;
	vec3_t pre_collision_pos;
	vec3_t computed_pos;

	vec3_t normal;
	vec3_t velocity;
} c_velocity_t;

extern unsigned long ct_velocity;

DEF_CASTER(ct_velocity, c_velocity, c_velocity_t)

c_velocity_t *c_velocity_new(float x, float y, float z);
void c_velocity_init(c_velocity_t *self);
void c_velocity_set_vel(c_velocity_t *self, float x, float y, float z);
void c_velocity_register(ecm_t *ecm);

#endif /* !VELOCITY_H */
