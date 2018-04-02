#include <candle.h>
#include <systems/editmode.h>
#include "destroyed.h"

c_destroyed_t *c_destroyed_new()
{
	return component_new("destroyed");
}

REG()
{
	ct_new("destroyed", sizeof(c_destroyed_t), NULL, NULL, 0);
}

