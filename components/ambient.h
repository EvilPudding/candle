#ifndef AMBIENT_H
#define AMBIENT_H

#include <utils/glutil.h>
#include <ecs/ecm.h>
#include <utils/texture.h>
#include <utils/material.h>

typedef struct c_ambient_t
{
	c_t super;

	float map_size;
} c_ambient_t;

DEF_CASTER("ambient", c_ambient, c_ambient_t)

c_ambient_t *c_ambient_new(int map_size);
void c_ambient_destroy(c_ambient_t *self);

#endif /* !AMBIENT_H */
