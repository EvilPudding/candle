#include "name.h"
#include <string.h>

DEC_CT(ct_name);

static void c_name_init(c_name_t *self)
{
	self->super = component_new(ct_name);

	self->name[0] = '\0';
}

c_name_t *c_name_new(const char *name)
{
	c_name_t *self = malloc(sizeof *self);
	c_name_init(self);

	strncpy(self->name, name, sizeof(self->name));

	return self;
}

void c_name_register()
{
	ecm_register("Name", &ct_name, sizeof(c_name_t), (init_cb)c_name_init, 0);
}


