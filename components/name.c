#include "name.h"
#include <string.h>


static void c_name_init(c_name_t *self)
{

	self->name[0] = '\0';
}

c_name_t *c_name_new(const char *name)
{
	c_name_t *self = component_new("name");

	strncpy(self->name, name, sizeof(self->name));

	return self;
}

REG()
{
	ct_new("name", sizeof(c_name_t), (init_cb)c_name_init, 0);
}


