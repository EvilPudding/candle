#define _GNU_SOURCE
#include <search.h>

#include <string.h>
#include "ecm.h"
#include <stdarg.h>
#include <SDL2/SDL.h>

#include "candle.h"
#include "components/destroyed.h"


static SDL_sem *sem1 = NULL;
static SDL_sem *sem;

listener_t *ct_get_listener(ct_t *self, uint32_t signal)
{
	signal_t *sig = ecm_get_signal(signal);
	if(!self) return NULL;
	uint32_t i;
	for(i = 0; i < vector_count(sig->listener_types); i++)
	{
		listener_t *listener = vector_get(sig->listener_types, i);
		if(listener->target == self->id)
		{
			return listener;
		}
	}

	return NULL;
}

void *_component_new(uint32_t comp_type)
{
	entity_t entity = _g_creating[_g_creating_num - 1];

	ct_t *ct = ecm_get(comp_type);

	/* printf("%ld << %s\n", entity, ct->name); */
	c_t *self = ct_add(ct, entity);

	int i;
	for(i = 0; i < ct->depends_size; i++)
	{
		ct_t *ct2 = ecm_get(ct->depends[i].ct);
		if(!ct_get(ct2, &entity))
		{
			/* printf("adding dependency %s\n", ct2->name); */
			_component_new(ct2->id);
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

	self->signals = kh_init(sig);
	self->cts = kh_init(ct);
	self->cs = kh_init(c);

	signal_init(sig("entity_created"), 0);

	/* create entity_null */
	{
		self->entities_info = malloc(sizeof(*g_ecm->entities_info));
		self->entities_info->uid = 0;
		self->entities_info->next_free = 1;
		self->entities_info_size = 1;
		self->entity_uid_counter = 1;
	}

	sem = SDL_CreateSemaphore(1);

	if(!sem1)
	{
		sem1 = SDL_CreateSemaphore(1);
	}

}


void ecm_add_reg(c_reg_cb reg)
{
	/* printf("ADD REG\n"); */
	g_ecm->regs[g_ecm->regs_size++] = reg;
}

void ecm_register_all()
{
	int j;
	for(j = 0; j < g_ecm->regs_size; j++)
	{
		g_ecm->regs[j]();
	}
}

int listeners_compare(listener_t *a, listener_t *b)
{
	return a->priority - b->priority;
}

signal_t *ecm_get_signal(uint32_t signal)
{
	khiter_t k = kh_get(sig, g_ecm->signals, signal);
	if(k == kh_end(g_ecm->signals))
	{
		int ret;
		k = kh_put(sig, g_ecm->signals, signal, &ret);
		signal_t *sig = &kh_value(g_ecm->signals, k);
		*sig = (signal_t){
			.listener_types = vector_new(sizeof(listener_t), 0, NULL,
					(vector_compare_cb)listeners_compare)
		};
	}
	return &kh_value(g_ecm->signals, k);
}

void _ct_listener(ct_t *self, int flags, uint32_t signal, signal_cb cb)
{
	signal_t *sig = ecm_get_signal(signal);
	if(ct_get_listener(self, signal)) exit(1);

	listener_t lis = {.signal = signal, .cb = (signal_cb)cb,
		.flags = flags & 0xFF00, .priority = flags & 0x00FF,
		.target = self->id};

	vector_add(sig->listener_types, &lis);

}

void _signal_init(uint32_t id, uint32_t size)
{
	signal_t *sig = ecm_get_signal(id);
	if(sig)
	{
		ecm_get_signal(id)->size = size;
		return;
	}

	int ret;
	khiter_t k = kh_put(sig, g_ecm->signals, id, &ret);
	kh_value(g_ecm->signals, k) = (signal_t){
		.size = size,
		.listener_types = vector_new(sizeof(listener_t), 0, NULL,
				(vector_compare_cb)listeners_compare),
		.listener_comps = vector_new(sizeof(listener_t), FIXED_INDEX, NULL,
				NULL)
	};
}

void ecm_destroy_all()
{
	uint32_t i;
	entity_info_t *iter;
	for(i = 0, iter = g_ecm->entities_info;
			i < g_ecm->entities_info_size; i++, iter++)
	{
		if(iter->uid)
		{
			entity_destroy(i);
		}
	}
}

entity_t ecm_new_entity()
{

	SDL_SemWait(sem);
	entity_info_t *info;

	unsigned int i = g_ecm->entities_info[0].next_free;

	if(i == g_ecm->entities_info_size)
	{
		g_ecm->entities_info_size++;
		g_ecm->entities_info = realloc(g_ecm->entities_info,
				sizeof(*g_ecm->entities_info) * g_ecm->entities_info_size);

		info = &g_ecm->entities_info[i];
		info->uid = 0;

		g_ecm->entities_info[0].next_free = g_ecm->entities_info_size;
	}
	else
	{
		info = &g_ecm->entities_info[i];
		g_ecm->entities_info[0].next_free = info->next_free;
	}

	info->uid = g_ecm->entity_uid_counter++;

	entity_t ent = info->uid;

	struct { unsigned int pos, uid; } *separate = (void*)&ent;
	separate->pos = i;
	separate->uid = info->uid;

	SDL_SemPost(sem);
	return ent;
}

void ct_add_interaction(ct_t *dep, uint32_t target)
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

void ct_add_dependency(ct_t *dep, uint32_t target)
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

/* struct comp_page *ct_add_page(ct_t *self) */
/* { */
/* 	int page_id = self->pages_size++; */
/* 	self->pages = realloc(self->pages, sizeof(*self->pages) * self->pages_size); */
/* 	struct comp_page *page = &self->pages[page_id]; */
	
/* 	page->components = malloc(self->size * PAGE_SIZE); */
/* 	page->components_size = 0; */
/* 	return page; */

/* } */

ct_t *_ct_new(const char *name, uint32_t hash, uint32_t size, init_cb init,
		destroy_cb destroy, int depend_size, ...)
{
	va_list depends;

	va_start(depends, depend_size);
	uint32_t j;
	for(j = 0; j < depend_size; j++)
	{
		if(va_arg(depends, uint32_t) == IDENT_NULL) return NULL;
	}
	va_end(depends);

	int ret;
	khiter_t k = kh_put(ct, g_ecm->cts, hash, &ret);

	ct_t *ct = &kh_value(g_ecm->cts, k);
	/* printf("%s %u\n", name, hash); */

	*ct = (ct_t) {
		.id = hash,
		.init = init,
		.destroy = destroy,
		.size = size,
		.depends_size = depend_size,
		.is_interaction = 0,
		.cs = kh_init(c)
		/* .pages_size = 0 */
	};
	strncpy(ct->name, name, sizeof(ct->name));

	if(depend_size)
	{

		ct->depends = malloc(sizeof(*ct->depends) * depend_size),

		va_start(depends, depend_size);
		for(j = 0; j < depend_size; j++)
		{
			ct->depends[j].ct = va_arg(depends, uint32_t);
			ct->depends[j].is_interaction = 0;
		}
		va_end(depends);
	}
	/* ct_add_page(ct); */

	if(!ecm_get(hash)) exit(1);

	return ct;
}

void ecm_clean2(int force)
{
	khiter_t k;
	if(force)
	{
		ct_t *ct;
		ecm_foreach_ct(ct,
		{
			if(ct->destroy)
			{
				for(k = kh_begin(ct->cs); k != kh_end(ct->cs); ++k)
				{
					if(!kh_exist(ct->cs, k)) continue;
					c_t *c = kh_value(ct->cs, k);
					ct->destroy(c);
				}
			}
		});
		goto end;
	}

	ct_t *dest = ecm_get(ref("destroyed"));

	for(k = kh_begin(dest->cs); k != kh_end(dest->cs); ++k)
	{
		if(!kh_exist(dest->cs, k)) continue;
		c_destroyed_t *dc = (c_destroyed_t*)kh_value(dest->cs, k);
		entity_t ent = c_entity(dc);
		entity_destroy(ent);
	}
end:

	g_ecm->dirty = 0;
	g_ecm->safe = 0;
}

void ecm_clean(int force)
{
	if(!g_ecm->dirty && !force) return;

	int rt = SDL_ThreadID() == g_candle->loader->threadId;

	if(g_ecm->safe && rt)
	{
		ecm_clean2(force);
		SDL_SemPost(sem1);
	}

	if(!rt)
	{
		SDL_AtomicIncRef((SDL_atomic_t*)&g_ecm->safe);
		SDL_SemWait(sem1);
	}
}

static SDL_mutex *mut = NULL;
c_t *ct_add(ct_t *self, entity_t entity)
{
	if(!mut) SDL_CreateMutex();
	SDL_LockMutex(mut);
	if(!self) return NULL;

	/* int page_id = self->pages_size - 1; */
	/* struct comp_page *page = &self->pages[page_id]; */
	/* if(page->components_size == PAGE_SIZE) */
	/* { */
	/* 	page = ct_add_page(self); */
	/* 	page_id++; */
	/* } */

	/* uint32_t offset; */

	int ret;
	unsigned int uid = entity_uid(entity);
	unsigned int key = ent_comp_ref(self->id, uid);
	khiter_t k = kh_put(c, g_ecm->cs, key, &ret);
	c_t **comp = &kh_value(g_ecm->cs, k);
	*comp = calloc(1, self->size);

	k = kh_put(c, self->cs, uid, &ret);
	kh_value(self->cs, k) = *comp;

	/* page->components_size++; */

	/* if(pos >= self->offsets_size) */
	/* { */
		/* self->offsets_size = pos + 1; */
	/* } */
	SDL_UnlockMutex(mut);

	/* c_t *comp = (c_t*)&page->components[offset]; */
	/* memset(comp, 0, self->size); */
	(*comp)->entity = entity;
	(*comp)->comp_type = self->id;
	return *comp;
}

