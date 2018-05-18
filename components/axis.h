#ifndef AXIS_H
#define AXIS_H

#include <ecm.h>
#include <systems/renderer.h>

typedef struct
{
	c_t super; /* extends c_t */
	vec3_t dir;
	int pressing;
	int type;
	mesh_t *dir_mesh;
} c_axis_t;

DEF_CASTER("axis", c_axis, c_axis_t)

c_axis_t *c_axis_new(int type, vec4_t dir);

#endif /* !AXIS_H */
