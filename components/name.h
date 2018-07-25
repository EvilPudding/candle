#ifndef NAME_H
#define NAME_H

#include <ecs/ecm.h>

typedef struct
{
	c_t super; /* extends c_t */
	char name[64];
	unsigned int hash;
} c_name_t;

DEF_CASTER("name", c_name, c_name_t)

c_name_t *c_name_new(const char *name);

#endif /* !NAME_H */
