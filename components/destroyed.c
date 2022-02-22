#include "../candle.h"
#include "../systems/editmode.h"
#include "destroyed.h"

void ct_destroyed(ct_t *self)
{
	ct_init(self, "destroyed", sizeof(c_destroyed_t));
}

c_destroyed_t *c_destroyed_new()
{
	return component_new(ct_destroyed);
}
