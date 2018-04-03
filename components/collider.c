#include "collider.h"


c_collider_t *c_collider_new(collider_cb cb)
{
	c_collider_t *self = component_new("collider");

	self->cb = cb;

	return self;
}

REG()
{
	ct_new("collider", sizeof(c_collider_t), NULL, NULL, 0);
}
