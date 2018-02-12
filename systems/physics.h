#ifndef PHYSICS_H
#define PHYSICS_H

#include "../glutil.h"
#include "../material.h"
#include "../texture.h"
#include "../mesh.h"
#include "../shader.h"
#include <ecm.h>

typedef float(*collider_cb)(c_t *self, vec3_t pos);
typedef float(*velocity_cb)(c_t *self, vec3_t pos);

typedef struct c_physics_t
{
	c_t super;
	/* currently, physics has no options */
} c_physics_t;

DEF_CASTER(ct_physics, c_physics, c_physics_t)

void c_physics_register(ecm_t *ecm);

DEF_SIG(collider_callback);

c_physics_t *c_physics_new(void);

#endif /* !PHYSICS_H */
