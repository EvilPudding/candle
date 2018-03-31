#ifndef SPRITE_H
#define SPRITE_H

#include <ecm.h>
#include <systems/renderer.h>

typedef struct
{
	c_t super; /* extends c_t */
	mat_t *mat;
	int visible;
	int cast_shadow;
} c_sprite_t;

DEF_CASTER(ct_sprite, c_sprite, c_sprite_t)
extern int g_update_id;

c_sprite_t *c_sprite_new(mat_t *mat, int cast_shadow);

#endif /* !SPRITE_H */
