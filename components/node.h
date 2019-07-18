#ifndef NODE_H
#define NODE_H

#include <ecs/ecm.h>
#include <components/spatial.h>

typedef int32_t(*node_cb)(entity_t entity, void *usrptr);

typedef struct
{
	c_t super; /* extends c_t */

	mat4_t model;
	vec4_t rot;
#ifdef MESH4
	float angle4;
#endif
	bool_t cached;
	int32_t ghost_inheritance;
	entity_t unpack_inheritance;

	entity_t *children;
	uint32_t children_size;

	entity_t parent;
	int32_t inherit_scale;

	bool_t ghost;
	bool_t has_shadows;
	bool_t visible;
	bool_t unpacked;
	bool_t inherit_transform;
} c_node_t;

DEF_CASTER("node", c_node, c_node_t)

c_node_t *c_node_new(void);
entity_t c_node_get_by_name(c_node_t *self, uint32_t hash);
void c_node_add(c_node_t *self, int32_t num, ...);
int32_t c_node_propagate(c_node_t *self, node_cb cb, void *usrptr);
bool_t c_node_update_model(c_node_t *self);
void c_node_unparent(c_node_t *self, int32_t inherit_transform);
void c_node_remove(c_node_t *self, entity_t child);
vec3_t c_node_pos_to_local(c_node_t *self, vec3_t vec);
vec3_t c_node_pos_to_global(c_node_t *self, vec3_t vec);
vec3_t c_node_dir_to_local(c_node_t *self, vec3_t vec);
vec3_t c_node_dir_to_global(c_node_t *self, vec3_t vec);
vec4_t c_node_rot_to_local(c_node_t *self, vec4_t vec);
vec4_t c_node_rot_to_global(c_node_t *self, vec4_t vec);
void c_node_disable_inherit_transform(c_node_t *self);
void c_node_pack(c_node_t *self, int32_t packed);
int c_node_changed(c_node_t *self);

#endif /* !NODE_H */
