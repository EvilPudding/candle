#include "attach.h"
#include <candle.h>
#include <components/spacial.h>
#include <components/node.h>
#include <math.h>

c_attach_t *c_attach_new(entity_t target)
{
	c_attach_t *self = component_new("attach");

	self->target = target;
	self->offset = mat4();

	return self;
}

void c_attach_target(c_attach_t *self, entity_t target)
{
	self->target = target;
}

static int c_attach_update(c_attach_t *self, float *dt)
{
	if(!self->target) return CONTINUE;

	/* TODO: this should only be called when spacial_changed of target */

	c_node_t *nc = c_node(&self->target);

	c_node_update_model(nc);

	c_spacial_set_model(c_spacial(self), nc->model);

	/* entity_signal(c_entity(self), sig("spacial_changed"), &c_entity(self)); */

	return CONTINUE;
}

REG()
{
	ct_t *ct = ct_new("attach", sizeof(c_attach_t), NULL, NULL, 0);

	ct_listener(ct, WORLD, sig("world_update"), c_attach_update);
}

