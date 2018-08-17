#ifndef SKIN_H
#define SKIN_H

#include <ecs/ecm.h>
#include <utils/mafs.h>

typedef struct skin_t
{
	int bones_num;
	entity_t bones[30];
	mat4_t off[30];

	vec4_t  *wei;
	uvec4_t *bid;
	int vert_alloc;

} skin_t;

typedef struct
{
	c_t super; /* extends c_t */
	skin_t info;
} c_skin_t;

DEF_CASTER("skin", c_skin, c_skin_t)

c_skin_t *c_skin_new(void);
void skin_vert_prealloc(skin_t *self, int size);

#endif /* !SKIN_H */
