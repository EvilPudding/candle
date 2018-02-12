#include "../ext.h"
#include "collider.h"

DEC_CT(ct_collider);

static void c_collider_init(c_collider_t *self)
{
	self->super = component_new(ct_collider);
	self->cb = NULL;
}

c_collider_t *c_collider_new(collider_cb cb)
{
	c_collider_t *self = malloc(sizeof *self);
	c_collider_init(self);

	self->cb = cb;

	return self;
}

void c_collider_register(ecm_t *ecm)
{
	ecm_register(ecm, &ct_collider, sizeof(c_collider_t), (init_cb)c_collider_init, 0);
}
