#ifndef DECAL_H
#define DECAL_H

#include <utils/glutil.h>
#include <ecm.h>
#include <utils/texture.h>
#include <utils/material.h>
#include <utils/shader.h>

typedef struct c_decal_t
{
	c_t super;
	int visible;
	mat_t *mat;
} c_decal_t;

DEF_CASTER("decal", c_decal, c_decal_t)

c_decal_t *c_decal_new(mat_t *mat);
void c_decal_destroy(c_decal_t *self);

#endif /* !DECAL_H */
