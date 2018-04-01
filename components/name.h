#ifndef NAME_H
#define NAME_H

#include <ecm.h>

typedef struct
{
	c_t super; /* extends c_t */
	char name[32];
} c_name_t;

DEF_CASTER("c_name", c_name, c_name_t)

c_name_t *c_name_new(const char *name);

#endif /* !NAME_H */
