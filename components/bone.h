#ifndef AABB_H
#define AABB_H

#include <ecm.h>
#include <glutil.h>


typedef struct
{
	c_t super; /* extends c_t */

	mat4_t offset;
	mat4_t final_transform;
	int bone_index;
	
} c_bone_t;


DEF_CASTER("bone", c_bone, c_bone_t)

c_bone_t *c_bone_new(int bone_index, mat4_t offset);

int c_bone_intersects(c_bone_t *self, c_bone_t *other);

#endif /* !AABB_H */
