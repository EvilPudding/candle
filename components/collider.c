#include "../ext.h"
#include "collider.h"


static void c_collider_init(c_collider_t *self)
{
	self->cb = NULL;
}

c_collider_t *c_collider_new(collider_cb cb)
{
	c_collider_t *self = component_new("collider");

	self->cb = cb;

	return self;
}

REG()
{
	ct_new("collider", sizeof(c_collider_t),
			(init_cb)c_collider_init, 0);
}
