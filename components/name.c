#include "name.h"
#include <string.h>

c_name_t *c_name_new(const char *name)
{
	c_name_t *self = component_new("name");

	strncpy(self->name, name, sizeof(self->name));
	self->hash = ref(name);

	return self;
}

REG()
{
	ct_new("name", sizeof(c_name_t), NULL, NULL, 0);
}


