#ifndef DECAL_H
#define DECAL_H

#include "../utils/glutil.h"
#include "../ecs/ecm.h"
#include "../utils/texture.h"
#include "../utils/material.h"
#include "../utils/shader.h"

typedef struct c_decal_t
{
	c_t super;
	int visible;
	mat_t *mat;
} c_decal_t;

void ct_decal(ct_t *self);
DEF_CASTER(ct_decal, c_decal, c_decal_t)

c_decal_t *c_decal_new(mat_t *mat, int visible, int selectable);
void c_decal_destroy(c_decal_t *self);

#endif /* !DECAL_H */
