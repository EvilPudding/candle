#define _GNU_SOURCE
#include <search.h>

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
	c_t self = {.comp_type = comp_type,
		.entity = _g_creating[_g_creating_num - 1]};
	return self;
}

ecm_t *g_ecm = NULL;
void ecm_init()
{
	ecm_t *self = calloc(1, sizeof *self);
	g_ecm = self;

	self->global = IDENT_NULL;

	ecm_register("C_T", &g_ecm->global, sizeof(c_t), NULL, 0);

	ecm_register_signal(&entity_created, 0);

	ecm_new_entity(); // entity_null

	g_ecm->common = entity_new();

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

	signal_t *sig = &g_ecm->signals[signal];
	i = sig->cts_size++;
	sig->cts = realloc(sig->cts, sizeof(*sig->cts) * sig->cts_size);
	sig->cts[i] = self->id;
}

void ecm_register_signal(uint *target, uint size)
{
	if(!g_ecm) return;
	if(*target != IDENT_NULL) return;
	uint i = g_ecm->signals_size++;
	g_ecm->signals = realloc(g_ecm->signals, sizeof(*g_ecm->signals) *
			g_ecm->signals_size);

	g_ecm->signals[i] = (signal_t){.size = size};

	*target = i;
}

entity_t ecm_new_entity()
{
	uint i;
	int *iter;

	for(i = 0, iter = g_ecm->entities_busy;
			i < g_ecm->entities_busy_size && *iter; i++, iter++);

	/* printf("%lu %lu\n", g_ecm->entities_busy_size, i); */
	if(iter == NULL || i == g_ecm->entities_busy_size)
	{
		i = g_ecm->entities_busy_size++;
		g_ecm->entities_busy = realloc(g_ecm->entities_busy,
				sizeof(*g_ecm->entities_busy) * g_ecm->entities_busy_size);
		iter = &g_ecm->entities_busy[i];
	}

	*iter = 1;

	return i;
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

/* void ecm_generate_hashes(ecm_t *self) */
/* { */
/* 	int i, j; */
/* 	for(i = 0; i < self->cts_size; i++) */
/* 	{ */
/* 		ct_t *ct = &self->cts[i]; */
/* 		ct->listeners_table = calloc(1, sizeof(*ct->listeners_table)); */
/* 		int res = hcreate_r(ct->listeners_size, ct->listeners_table); */
/* 		ENTRY item; */
/* 		for(j = 0; j < ct->listeners_size; j++) */
/* 		{ */
/* 			listener_t *listener = &ct->listeners[j]; */
/* 			item.data = listener; */
/* 			item.key = listener->signal; */

/* 			res = hsearch_r(item, ENTER, NULL, &ct->listeners_table); */
/* 		} */
/* 	} */
/* } */

ct_t *ecm_register(const char *name, uint *target, uint size,
		init_cb init, int depend_size, ...)
{
	if(*target != IDENT_NULL) return ecm_get(*target);
	va_list depends;

	va_start(depends, depend_size);
	uint j;
	for(j = 0; j < depend_size; j++)
	{
		if(va_arg(depends, uint) == IDENT_NULL) return NULL;
	}
	va_end(depends);

	uint i = g_ecm->cts_size++;

	if(target) *target = i;

	g_ecm->cts = realloc(g_ecm->cts,
			sizeof(*g_ecm->cts) *
			g_ecm->cts_size);

	ct_t *ct = &g_ecm->cts[i];

	*ct = (ct_t)
	{
		.id = i,
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
	if(entity >= self->offsets_size)
	{
		uint j, new_size = entity + 1;
		self->offsets = realloc(self->offsets, sizeof(*self->offsets) *
				new_size);
		for(j = self->offsets_size; j < new_size - 1; j++)
		{
			self->offsets[j] = -1;
		}

		self->offsets_size = entity + 1;
	}

	self->offsets[entity] = offset = i * self->size;

	self->components = realloc(self->components, self->size * self->components_size);

	memcpy(&(self->components[offset]), comp, self->size);
}

