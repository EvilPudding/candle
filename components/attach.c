#include "attach.h"
#include <candle.h>
#include <components/spatial.h>
#include <components/node.h>
#include <math.h>

void c_attach_target(c_attach_t *self, entity_t target)
{
	self->target = target;
}

static int c_attach_update(c_attach_t *self, float *dt)
{
	c_node_t *nc;
	if(!self->target) return CONTINUE;

	/* TODO: this should only be called when spatial_changed of target */

	nc = c_node(&self->target);

	c_node_update_model(nc);

	c_spatial_set_model(c_spatial(self), nc->model);

	/* entity_signal(c_entity(self), sig("spatial_changed"), &c_entity(self)); */

	return CONTINUE;
}

void ct_attach(ct_t *self)
{
	ct_init(self, "attach", sizeof(c_attach_t));
	ct_add_dependency(self, ct_node);

	ct_add_listener(self, WORLD, 0, sig("world_update"), c_attach_update);
}

c_attach_t *c_attach_new(entity_t target)
{
	c_attach_t *self = component_new(ct_attach);

	self->target = target;
	self->offset = mat4();

	return self;
}

