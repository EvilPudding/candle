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

DEF_CASTER("sprite", c_sprite, c_sprite_t)
extern vs_t *g_sprite_vs;
extern mesh_t *g_sprite_mesh;

vs_t *sprite_vs(void);

c_sprite_t *c_sprite_new(mat_t *mat, int cast_shadow);

#endif /* !SPRITE_H */
