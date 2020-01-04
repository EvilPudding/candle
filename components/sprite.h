#ifndef SPRITE_H
#define SPRITE_H

#include <ecs/ecm.h>
#include <utils/drawable.h>

typedef struct
{
	c_t super; /* extends c_t */
	mat_t *mat;
	int visible;
	int cast_shadow;
	int xray;
	drawable_t draw;
} c_sprite_t;

void ct_sprite(ct_t *self);
DEF_CASTER(ct_sprite, c_sprite, c_sprite_t)

vs_t *sprite_vs(void);
vs_t *fixed_sprite_vs(void);
mesh_t *sprite_mesh(void);

c_sprite_t *c_sprite_new(mat_t *mat, int cast_shadow);

#endif /* !SPRITE_H */
