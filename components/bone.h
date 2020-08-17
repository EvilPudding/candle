#ifndef BONE_H
#define BONE_H

#include "../ecs/ecm.h"
#include "../utils/mafs.h"
#include "../utils/drawable.h"

typedef struct
{
	c_t super; /* extends c_t */
	drawable_t draw;
	drawable_t joint;
	entity_t skin;
	mat4_t offset;
} c_bone_t;

DEF_CASTER(ct_bone, c_bone, c_bone_t)

c_bone_t *c_bone_new(entity_t skin, mat4_t offset);

#endif /* !BONE_H */
