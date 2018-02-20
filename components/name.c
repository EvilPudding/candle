#include "name.h"
#include <string.h>

DEC_CT(ct_name);

static void c_name_init(c_name_t *self)
{

	self->name[0] = '\0';
}

c_name_t *c_name_new(const char *name)
{
	c_name_t *self = component_new(ct_name);

	strncpy(self->name, name, sizeof(self->name));

	return self;
}

void c_name_register()
{
	ct_new("c_name", &ct_name, sizeof(c_name_t), (init_cb)c_name_init, 0);
}


