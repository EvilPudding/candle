#ifndef FORCE_H
#define FORCE_H

#include <ecm.h>
#include "../glutil.h"

typedef struct
{
	c_t super; /* extends c_t */

	int active;
	vec3_t force;
} c_force_t;

DEF_CASTER("force", c_force, c_force_t)

c_force_t *c_force_new(float x, float y, float z, int active);

#endif /* !FORCE_H */
