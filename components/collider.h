#ifndef COLLIDER_H
#define COLLIDER_H

#include <ecm.h>
#include <systems/physics.h>

typedef struct
{
	c_t super; /* extends c_t */

	collider_cb cb;
} c_collider_t;

DEF_CASTER(ct_collider, c_collider, c_collider_t)

c_collider_t *c_collider_new(collider_cb cb);
void c_collider_register(void);

#endif /* !COLLIDER_H */
