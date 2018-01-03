#ifndef AMBIENT_H
#define AMBIENT_H

#include "../glutil.h"
#include <ecm.h>
#include "../texture.h"
#include "../material.h"

typedef struct c_ambient_t
{
	c_t super;

	float map_size;
} c_ambient_t;

extern unsigned long ct_ambient;

DEF_CASTER(ct_ambient, c_ambient, c_ambient_t)

c_ambient_t *c_ambient_new(int map_size);
void c_ambient_destroy(c_ambient_t *self);
void c_ambient_register(ecm_t *ecm);

#endif /* !AMBIENT_H */
