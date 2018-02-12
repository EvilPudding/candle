#include "entity.h"
#include <stdarg.h>
#include <ecm.h>
#include <components/name.h>

static void entity_check_missing_dependencies(entity_t self);
static void *ct_create(ct_t *self);

entity_t entity_new(ecm_t *ecm, int comp_num, ...)
{
	int i;
	va_list comps;
	entity_t self = ecm_new_entity(ecm);

    va_start(comps, comp_num);
	for(i = 0; i < comp_num; i++)
	{
		c_t *c = va_arg(comps, c_t*);
		_entity_add_component(self, c, 1);
		free(c);
	}
	va_end(comps);

	entity_check_missing_dependencies(self);

	if(c_name(self) && !strcmp(c_name(self)->name, "grid"))
	{
		printf("new %s entity %ld\n", c_name(self)->name, self.id);
	}
	entity_signal_same(self, entity_created, NULL);
	return self;

}

static void *ct_create(ct_t *self)
{
	c_t *comp = calloc(self->size, 1);
	comp->comp_type = self->id;
	if(self->init) self->init(comp);
	return comp;
}

static void entity_check_missing_dependencies(entity_t self)
{
	ulong i, j;
	for(j = 0; j < self.ecm->cts_size; j++)
	{
		ct_t *ct = &self.ecm->cts[j];
		if(ct_get(ct, self))
		{
			for(i = 0; i < ct->depends_size; i++)
			{
				ct_t *ct2 = ecm_get(self.ecm, ct->depends[i]);
				if(!ct_get(ct2, self))
				{
					c_t *comp2 = ct_create(ct2);
					_entity_add_component(self, comp2, 1);
					free(comp2);
				}
			}
		}
	}
}


int component_signal(c_t *comp, ct_t *ct, ulong signal, void *data)
{
	listener_t *listener = ct_get_listener(ct, signal);
	if(listener)
	{
		return listener->cb(comp, data);
	}
	return 1;
}

int entity_signal_same(entity_t self, ulong signal, void *data)
{
	ulong i;

	signal_t *sig = &self.ecm->signals[signal];

	for(i = 0; i < sig->cts_size; i++)
	{
		ct_t *ct = ecm_get(self.ecm, sig->cts[i]);
		c_t *comp = ct_get(ct, self);
		if(comp)
		{
			int res = component_signal(comp, ct, signal, data);
			if(res == 0)
			{
				return 0;
			}
		}
	}
	return 1;
}

void entity_filter(entity_t self, ulong signal, void *data,
		filter_cb cb, c_t *c_caller, void *cb_data)
{
	ulong i, j;

	signal_t *sig = &self.ecm->signals[signal];

	for(i = 0; i < sig->cts_size; i++)
	{
		ulong ct_id = sig->cts[i];
		ct_t *ct = ecm_get(self.ecm, ct_id);
		listener_t *listener = ct_get_listener(ct, signal);
		if(listener)
		{
			for(j = 0; j < ct->components_size; j++)
			{
				c_t *c = ct_get_at(ct, j);
				if(listener->flags != SAME_ENTITY || c_entity(c).id == self.id)
				{
					if(listener->cb(c, data))
					{
						cb(c_caller, c, cb_data);
					}
				}
			}
		}
	}
}

int entity_signal(entity_t self, ulong signal, void *data)
{
	ulong i, j;

	signal_t *sig = &self.ecm->signals[signal];

	for(i = 0; i < sig->cts_size; i++)
	{
		ulong ct_id = sig->cts[i];
		ct_t *ct = ecm_get(self.ecm, ct_id);
		listener_t *listener = ct_get_listener(ct, signal);
		if(listener)
		{
			for(j = 0; j < ct->components_size; j++)
			{
				c_t *c = ct_get_at(ct, j);
				if(listener->flags != SAME_ENTITY || c_entity(c).id == self.id)
				{
					int res = listener->cb(c, data);
					if(res == 0)
					{
						return 0;
					}
				}
			}
		}
	}
	return 1;
}

void entity_destroy(entity_t self) 
{
}

void _entity_add_component(entity_t self, c_t *comp, int on_creation)
{
	comp->entity = self;
	ct_t *ct = ecm_get(self.ecm, comp->comp_type);
	ct_add(ct, comp);
	if(!on_creation)
	{
		component_signal(comp, ct, entity_created, NULL);
	}
}

