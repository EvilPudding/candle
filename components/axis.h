#ifndef AXIS_H
#define AXIS_H

#include <ecs/ecm.h>

typedef struct
{
	c_t super; /* extends c_t */
	vec3_t dir;
	int type;
	mesh_t *dir_mesh;
	int visible;
} c_axis_t;

DEF_CASTER("axis", c_axis, c_axis_t)

c_axis_t *c_axis_new(int type, vecN_t dir);

#endif /* !AXIS_H */
