#define _GNU_SOURCE
#include <search.h>

#include <string.h>
#include "ext.h"
#include "ecm.h"
#include <stdarg.h>
#include <SDL2/SDL.h>

#include "candle.h"

DEC_SIG(entity_created);
SDL_sem *sem;

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

void *component_new(int comp_type)
{
	entity_t entity = _g_creating[_g_creating_num - 1];

	ct_t *ct = &g_ecm->cts[comp_type];

	/* printf("%ld << %s\n", entity, ct->name); */
	c_t *self = ct_add(ct, entity);

	int i;
	for(i = 0; i < ct->depends_size; i++)
	{
		ct_t *ct2 = ecm_get(ct->depends[i].ct);
		if(!ct_get(ct2, &entity))
		{
			/* printf("adding dependency %s\n", ct2->name); */
			component_new(ct2->id);
		}
	}
	if(ct->init) ct->init(self);

	return self;
}

ecm_t *g_ecm = NULL;
void ecm_init()
{
	ecm_t *self = calloc(1, sizeof *self);
	g_ecm = self;

	/* self->global = IDENT_NULL; */

	/* ecm_register("C_T", &g_ecm->global, sizeof(c_t), NULL, 0); */

	self->regs_size = 0;
	self->regs = malloc(sizeof(*self->regs) * 256);

	signal_init(&entity_created, 0);

	ecm_new_entity(); // entity_null

	sem = SDL_CreateSemaphore(1);
}


void ecm_add_reg(c_reg_cb reg)
{
	g_ecm->regs[g_ecm->regs_size++] = reg;
}

void ecm_register_all()
{
	int i, j;
	for(i = 0; i < 4; i++)
	{
		for(j = 0; j < g_ecm->regs_size; j++)
		{
			g_ecm->regs[j]();
		}
	}
}

void _ct_listener(ct_t *self, int flags, uint signal,
	signal_cb cb)
{
	if(!self) return;
	if(signal == IDENT_NULL) return;
	if(ct_get_listener(self, signal)) return;

	uint i = self->listeners_size++;
	self->listeners = realloc(self->listeners, sizeof(*self->listeners) *
			self->listeners_size);

	listener_t lis = {.signal = signal, .cb = (signal_cb)cb,
		.flags = flags, .comp_type = self->id};
	self->listeners[i] = lis;

	signal_t *sig = &g_ecm->signals[signal];

	i = sig->cts_size++;
	sig->cts = realloc(sig->cts, sizeof(*sig->cts) * sig->cts_size);
	sig->cts[i] = self->id;

	i = sig->listeners_size++;
	sig->listeners = realloc(sig->listeners, sizeof(*sig->listeners) *
			sig->listeners_size);
	sig->listeners[i] = lis;
}

void signal_init(uint *target, uint size)
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

	SDL_SemWait(sem);
	for(i = 0, iter = g_ecm->entities_busy;
			i < g_ecm->entities_busy_size && *iter; i++, iter++);

	if(iter == NULL || i == g_ecm->entities_busy_size)
	{
		i = g_ecm->entities_busy_size++;
		g_ecm->entities_busy = realloc(g_ecm->entities_busy,
				sizeof(*g_ecm->entities_busy) * g_ecm->entities_busy_size);
		iter = &g_ecm->entities_busy[i];
	}

	*iter = 1;
	SDL_SemPost(sem);
	/* printf(">>>>>>>> %u\n", i); */

	return i;
}

void ct_add_interaction(ct_t *dep, uint target)
{
	if(target == IDENT_NULL) return;
	ct_t * ct = ecm_get(target);
	if(!ct) return;
	if(!dep) return;

	dep->is_interaction = 1;
	int i = ct->depends_size++;
	ct->depends = realloc(ct->depends, sizeof(*ct->depends) * ct->depends_size);
	ct->depends[i].ct = dep->id;
	ct->depends[i].is_interaction = 1;
}

void ct_add_dependency(ct_t *dep, uint target)
{
	if(target == IDENT_NULL) return;
	ct_t * ct = ecm_get(target);
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

struct comp_page *ct_add_page(ct_t *self)
{
	int page_id = self->pages_size++;
	self->pages = realloc(self->pages, sizeof(*self->pages) * self->pages_size);
	struct comp_page *page = &self->pages[page_id];
	
	page->components = malloc(self->size * PAGE_SIZE);
	page->components_size = 0;
	return page;

}

ct_t *ct_new(const char *name, uint *target, uint size,
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
		.is_interaction = 0,
		.pages_size = 0
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
	ct_add_page(ct);

	return ct;
}

static SDL_mutex *mut = NULL;
c_t *ct_add(ct_t *self, entity_t entity)
{
	if(!mut) SDL_CreateMutex();
	SDL_LockMutex(mut);
	if(!self) return NULL;

	int page_id = self->pages_size - 1;
	struct comp_page *page = &self->pages[page_id];
	if(page->components_size == PAGE_SIZE)
	{
		page = ct_add_page(self);
		page_id++;
	}

	uint offset;


	if(entity >= self->offsets_size)
	{
		/* printf("increasing offsets %d -> %d\n", self->offsets_size, (int)entity + 1); */
		uint j, new_size = entity + 1;
		self->offsets = realloc(self->offsets, sizeof(*self->offsets) *
				new_size);
		for(j = self->offsets_size; j < new_size - 1; j++)
		{
			self->offsets[j].offset = -1;
		}

	}
	/* printf("c_size %d %s\n", (int)self->size, self->name); */

	self->offsets[entity].offset = offset = page->components_size * self->size;
	self->offsets[entity].page = page_id;


	page->components_size++;

	if(entity >= self->offsets_size)
	{
		self->offsets_size = entity + 1;
	}
	SDL_UnlockMutex(mut);
	c_t *comp = (c_t*)&page->components[offset];
	memset(comp, 0, self->size);
	comp->entity = entity;
	comp->comp_type = self->id;
	return comp;
}

