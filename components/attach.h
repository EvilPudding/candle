#ifndef ATTACH_H
#define ATTACH_H

#include <ecm.h>

typedef struct
{
	c_t super; /* extends c_t */

	entity_t target;
	mat4_t offset;
} c_attach_t;

DEF_CASTER("attach", c_attach, c_attach_t)

c_attach_t *c_attach_new(entity_t target);

void c_attach_target(c_attach_t *self, entity_t target);

#endif /* !ATTACH_H */
