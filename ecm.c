#include "ext.h"
#include "ecm.h"
#include <stdarg.h>

#include "candle.h"

DEC_SIG(entity_created);

listener_t *ct_get_listener(ct_t *self, uint signal)
{
	if(signal == IDENT_NULL) return NULL;
	if(!self) return NULL;
	uint i;
	for(i = 0; i < self->listeners_size; i++)
	{
		listener_t *listener = &self->listeners[i];
		if(listener->signal == signal)
		{
			return listener;
		}
	}

	return NULL;
}

c_t component_new(int comp_type)
{
	c_t self = {.comp_type = comp_type, .entity = {.id = -1}};
	return self;
}

ecm_t *ecm_new()
{
	ecm_t *self = calloc(1, sizeof *self);
	self->none = (entity_t){.id = -1, .ecm = self};

	self->global = IDENT_NULL;

	ecm_register(self, "C_T", &self->global, sizeof(c_t), NULL, 0);

	ecm_register_signal(self, &entity_created, 0);

	self->common = entity_new(self, 0);

	return self;
}

void ct_register_listener(ct_t *self, int flags, uint signal,
	signal_cb cb)
{
	if(!self) return;
	if(signal == IDENT_NULL) return;
	if(ct_get_listener(self, signal)) return;

	uint i = self->listeners_size++;
	self->listeners = realloc(self->listeners, sizeof(*self->listeners) *
			self->listeners_size);

	self->listeners[i] = (listener_t){.signal = signal, .cb = (signal_cb)cb,
		.flags = flags};

	signal_t *sig = &self->ecm->signals[signal];
	i = sig->cts_size++;
	sig->cts = realloc(sig->cts, sizeof(*sig->cts) * sig->cts_size);
	sig->cts[i] = self->id;
}

void ecm_register_signal(ecm_t *self, uint *target, uint size)
{
	if(!self) return;
	if(*target != IDENT_NULL) return;
	uint i = self->signals_size++;
	self->signals = realloc(self->signals, sizeof(*self->signals) *
			self->signals_size);

	self->signals[i] = (signal_t){.size = size};

	*target = i;
}

entity_t ecm_new_entity(ecm_t *self)
{
	uint i;
	int *iter;

	for(i = 0, iter = self->entities_busy;
			i < self->entities_busy_size && *iter; i++, iter++);

	/* printf("%lu %lu\n", self->entities_busy_size, i); */
	if(iter == NULL || i == self->entities_busy_size)
	{
		i = self->entities_busy_size++;
		self->entities_busy = realloc(self->entities_busy,
				sizeof(*self->entities_busy) * self->entities_busy_size);
		iter = &self->entities_busy[i];
	}

	*iter = 1;

	return (entity_t){.id = i, .ecm = self};
}

/* uint ecm_register_system(ecm_t *self, void *system) */
/* { */
/* 	uint i = self->systems_size++; */
/* 	self->systems = realloc(self->systems, sizeof(*self->systems) * i); */
/* 	self->systems[i] = system; */
/* 	return i; */
/* } */

void ct_add_interaction(ct_t *ct, ct_t *dep)
{
	if(!ct) return;
	if(!dep) return;

	dep->is_interaction = 1;
	int i = ct->depends_size++;
	ct->depends = realloc(ct->depends, sizeof(*ct->depends) * ct->depends_size);
	ct->depends[i].ct = dep->id;
	ct->depends[i].is_interaction = 1;
}

void ct_add_dependency(ct_t *ct, ct_t *dep)
{
	if(!ct) return;
	if(!dep) return;
	int i = ct->depends_size++;
	ct->depends = realloc(ct->depends, sizeof(*ct->depends) * ct->depends_size);
	ct->depends[i].ct = dep->id;
	ct->depends[i].is_interaction = 0;
}

ct_t *ecm_register(ecm_t *self, const char *name, uint *target, uint size,
		init_cb init, int depend_size, ...)
{
	if(*target != IDENT_NULL) return ecm_get(self, *target);
	va_list depends;

	va_start(depends, depend_size);
	uint j;
	for(j = 0; j < depend_size; j++)
	{
		if(va_arg(depends, uint) == IDENT_NULL) return NULL;
	}
	va_end(depends);

	uint i = self->cts_size++;

	if(target) *target = i;

	self->cts = realloc(self->cts,
			sizeof(*self->cts) *
			self->cts_size);

	ct_t *ct = &self->cts[i];

	*ct = (ct_t)
	{
		.id = i,
		.ecm = self,
		.init = init,
		.size = size,
		.depends_size = depend_size,
		.is_interaction = 0
	};
	strncpy(ct->name, name, sizeof(ct->name));

	if(depend_size)
	{

		ct->depends = malloc(sizeof(*ct->depends) * depend_size),

		va_start(depends, depend_size);
		for(j = 0; j < depend_size; j++)
		{
			ct->depends[j].ct = va_arg(depends, uint);
			ct->depends[j].is_interaction = 0;
		}
		va_end(depends);
	}

	return ct;
}

void ct_add(ct_t *self, c_t *comp)
{
	if(!self) return;
	if(!comp) return;
	uint offset, i = self->components_size++;
	entity_t entity = c_entity(comp);
	if(entity.id >= self->offsets_size)
	{
		uint j, new_size = entity.id + 1;
		self->offsets = realloc(self->offsets, sizeof(*self->offsets) *
				new_size);
		for(j = self->offsets_size; j < new_size - 1; j++)
		{
			self->offsets[j] = -1;
		}

		self->offsets_size = entity.id + 1;
	}

	self->offsets[entity.id] = offset = i * self->size;

	self->components = realloc(self->components, self->size * self->components_size);

	memcpy(&(self->components[offset]), comp, self->size);
}

