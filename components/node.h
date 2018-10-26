#ifndef NODE_H
#define NODE_H

#include <utils/glutil.h>
#include <ecs/ecm.h>
#include <components/spacial.h>

typedef struct
{
	c_t super; /* extends c_t */

	mat4_t model;
	vec4_t rot;
#ifdef MESH4
	float angle4;
#endif
	int32_t cached;
	int32_t ghost_inheritance;
	entity_t unpack_inheritance;

	entity_t *children;
	uint64_t children_size;

	entity_t parent;
	int32_t inherit_scale;

	int32_t ghost;
	int32_t has_shadows;
	int32_t visible;
	int32_t unpacked;
} c_node_t;

DEF_CASTER("node", c_node, c_node_t)

c_node_t *c_node_new(void);
entity_t c_node_get_by_name(c_node_t *self, uint hash);
void c_node_add(c_node_t *self, int32_t num, ...);
void c_node_update_model(c_node_t *self);
void c_node_unparent(c_node_t *self, int32_t inherit_transform);
void c_node_remove(c_node_t *self, entity_t child);
vec3_t c_node_global_to_local(c_node_t *self, vec3_t vec);
vec3_t c_node_local_to_global(c_node_t *self, vec3_t vec);
vec3_t c_node_dir_to_local(c_node_t *self, vec3_t vec);
vec3_t c_node_dir_to_global(c_node_t *self, vec3_t vec);
void c_node_pack(c_node_t *self, int32_t packed);

#endif /* !NODE_H */
