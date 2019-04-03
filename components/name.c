#include "name.h"
#include <string.h>
#include <ctype.h>

c_name_t *c_name_new(const char *name)
{
	c_name_t *self = component_new("name");

	for(int i = 0; name[i]; i++)
	{
		self->name[i] = tolower(name[i]);
	}
	self->hash = ref(name);

	return self;
}

REG()
{
	ct_new("name", sizeof(c_name_t), NULL, NULL, 0);
}


