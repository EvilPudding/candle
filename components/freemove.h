#ifndef FREEMOVE_H
#define FREEMOVE_H

#include <ecm.h>

typedef struct
{
	c_t super; /* extends c_t */

	int forward, backward, left, right;

	entity_t orientation;

	vec3_t up;

	int plane_movement;

	entity_t force_down;
} c_freemove_t;

DEF_CASTER(ct_freemove, c_freemove, c_freemove_t)

c_freemove_t *c_freemove_new(entity_t orientation, int plane_movement, entity_t force_down);
void c_freemove_register(ecm_t *ecm);

#endif /* !FREEMOVE_H */
