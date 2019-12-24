#include "name.h"
#include <string.h>
#include <ctype.h>

void ct_name(ct_t *self)
{
	ct_init(self, "name", sizeof(c_name_t));
}

c_name_t *c_name_new(const char *name)
{
	int32_t i;
	c_name_t *self = component_new(ct_name);

	for(i = 0; name[i]; i++)
	{
		self->name[i] = tolower(name[i]);
	}
	self->hash = ref(name);

	return self;
}
