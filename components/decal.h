#ifndef DECAL_H
#define DECAL_H

#include "../glutil.h"
#include <ecm.h>
#include "../texture.h"
#include "../material.h"
#include <shader.h>

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
