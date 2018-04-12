#ifndef NODE_H
#define NODE_H

#include <utils/glutil.h>
#include <ecm.h>
#include <components/spacial.h>

typedef struct
{
	c_t super; /* extends c_t */

	mat4_t model;
	mat4_t rot;
	int cached;
	int ghost;

	entity_t *children;
	ulong children_size;

	entity_t parent;
	int inherit_scale;
} c_node_t;

DEF_CASTER("node", c_node, c_node_t)

c_node_t *c_node_new(void);
entity_t c_node_get_by_name(c_node_t *self, uint hash);
void c_node_add(c_node_t *self, int num, ...);
void c_node_update_model(c_node_t *self);
vec3_t c_node_global_to_local(c_node_t *self, vec3_t vec);
vec3_t c_node_local_to_global(c_node_t *self, vec3_t vec);
vec3_t c_node_dir_to_local(c_node_t *self, vec3_t vec);
vec3_t c_node_dir_to_global(c_node_t *self, vec3_t vec);

#endif /* !NODE_H */
