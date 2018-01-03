#ifndef NAME_H
#define NAME_H

#include <ecm.h>

typedef struct
{
	c_t super; /* extends c_t */
	char name[32];
} c_name_t;

extern unsigned long ct_name;

DEF_CASTER(ct_name, c_name, c_name_t)

c_name_t *c_name_new(const char *name);
void c_name_register(ecm_t *ecm);

#endif /* !NAME_H */
