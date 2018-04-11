#ifndef PHYSICS_H
#define PHYSICS_H

#include <utils/glutil.h>
#include <utils/material.h>
#include <utils/texture.h>
#include <utils/mesh.h>
#include <utils/shader.h>
#include <ecm.h>

typedef float(*collider_cb)(c_t *self, vec3_t pos);
typedef float(*velocity_cb)(c_t *self, vec3_t pos);

typedef struct c_physics_t
{
	c_t super;
	/* currently, physics has no options */
} c_physics_t;

DEF_CASTER("physics", c_physics, c_physics_t)

c_physics_t *c_physics_new(void);

#endif /* !PHYSICS_H */
