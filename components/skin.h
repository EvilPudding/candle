#ifndef SKIN_H
#define SKIN_H

#include "../ecs/ecm.h"
#include "../utils/mafs.h"

typedef struct
{
	entity_t bones[30];
	mat4_t transforms[30];
	uint32_t bones_num;
	uint32_t update_id;
} skin_t;

typedef struct
{
	c_t super; /* extends c_t */

	skin_t info;
	uint32_t modified;
} c_skin_t;

void ct_skin(ct_t *self);
DEF_CASTER(ct_skin, c_skin, c_skin_t)

c_skin_t *c_skin_new(void);
void c_skin_changed(c_skin_t *self);

#endif /* !SKIN_H */
