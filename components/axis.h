#ifndef AXIS_H
#define AXIS_H

#include <ecs/ecm.h>
#include <utils/mesh.h>

typedef struct
{
	c_t super; /* extends c_t */
	vec3_t dir;
	int type;
	mesh_t *dir_mesh;
	int visible;
} c_axis_t;

void ct_axis(ct_t *self);
DEF_CASTER(ct_axis, c_axis, c_axis_t)

c_axis_t *c_axis_new(int type, vecN_t dir);

#endif /* !AXIS_H */
