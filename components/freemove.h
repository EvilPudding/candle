#ifndef FREEMOVE_H
#define FREEMOVE_H

#include <ecs/ecm.h>

typedef struct
{
	c_t super; /* extends c_t */

	int forward, backward, left, right;

	entity_t orientation;

	vec3_t up;

	int plane_movement;

	vec3_t vel;

	entity_t force_down;
} c_freemove_t;

DEF_CASTER(ct_freemove, c_freemove, c_freemove_t)

c_freemove_t *c_freemove_new(entity_t orientation, int plane_movement, entity_t force_down);

#endif /* !FREEMOVE_H */
