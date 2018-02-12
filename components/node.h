#ifndef NODE_H
#define NODE_H

#include "../glutil.h"
#include <ecm.h>

typedef struct
{
	c_t super; /* extends c_t */

	mat4_t model;
	int cached;

	entity_t *children;
	ulong children_size;

	entity_t parent;
} c_node_t;

DEF_CASTER(ct_node, c_node, c_node_t)

c_node_t *c_node_new(void);
entity_t c_node_get_by_name(c_node_t *self, const char *name);
void c_node_add(c_node_t *self, int num, ...);
void c_node_update_model(c_node_t *self);
vec3_t c_node_global_to_local(c_node_t *self, vec3_t vec);
void c_node_register(ecm_t *ecm);

#endif /* !NODE_H */
