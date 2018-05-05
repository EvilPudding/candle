#define _GNU_SOURCE
#include <search.h>

#include <string.h>
#include "ecm.h"
#include <stdarg.h>
#include <SDL2/SDL.h>

#include "candle.h"
#include "components/destroyed.h"


static int steps = 0;
static SDL_sem *sem1 = NULL;
static SDL_sem *sem2 = NULL;
static SDL_sem *sem;

listener_t *ct_get_listener(ct_t *self, uint signal)
{
	signal_t *sig = ecm_get_signal(signal);
	if(!self) return NULL;
	uint i;
	for(i = 0; i < vector_count(sig->listeners); i++)
	{
		listener_t *listener = vector_get(sig->listeners, i);
		if(listener->comp_type == self->id)
		{
			return listener;
		}
	}

	return NULL;
}

void *_component_new(uint comp_type)
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

	ecm_new_entity(); // entity_null

	sem = SDL_CreateSemaphore(1);

	if(!sem1)
	{
		sem1 = SDL_CreateSemaphore(0);
		sem2 = SDL_CreateSemaphore(0);
	}

}


void ecm_add_reg(c_reg_cb reg)
{
	printf("ADD REG\n");
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

signal_t *ecm_get_signal(uint signal)
{
	khiter_t k = kh_get(sig, g_ecm->signals, signal);
	if(k == kh_end(g_ecm->signals))
	{
		int ret;
		k = kh_put(sig, g_ecm->signals, signal, &ret);
		signal_t *sig = &kh_value(g_ecm->signals, k);
		*sig = (signal_t){
			.listeners = vector_new(sizeof(listener_t), 0, NULL,
					(vector_compare_cb)listeners_compare)
		};
	}
	return &kh_value(g_ecm->signals, k);
}

void _ct_listener(ct_t *self, int flags, uint signal, signal_cb cb)
{
	signal_t *sig = ecm_get_signal(signal);
	if(ct_get_listener(self, signal)) exit(1);

	listener_t lis = {.signal = signal, .cb = (signal_cb)cb,
		.flags = flags & 0xFF00, .priority = flags & 0x00FF,
		.comp_type = self->id};

	vector_add(sig->listeners, &lis);

}

void _signal_init(uint id, uint size)
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
		.listeners = vector_new(sizeof(listener_t), 0, NULL,
				(vector_compare_cb)listeners_compare)};
}

void ecm_destroy_all()
{
	uint i;
	int *iter;
	for(i = 0, iter = g_ecm->entities_busy;
			i < g_ecm->entities_busy_size; i++, iter++)
	{
		if(*iter)
		{
			entity_destroy(i);
		}
	}
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

ct_t *_ct_new(const char *name, uint hash, uint size, init_cb init,
		destroy_cb destroy, int depend_size, ...)
{
	va_list depends;

	va_start(depends, depend_size);
	uint j;
	for(j = 0; j < depend_size; j++)
	{
		if(va_arg(depends, uint) == IDENT_NULL) return NULL;
	}
	va_end(depends);

	int ret;
	khiter_t k = kh_put(ct, g_ecm->cts, hash, &ret);

	ct_t *ct = &kh_value(g_ecm->cts, k);
	printf("%s %u\n", name, hash);

	*ct = (ct_t) {
		.id = hash,
		.init = init,
		.destroy = destroy,
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

	if(!ecm_get(hash)) exit(1);

	return ct;
}

void ecm_clean2(int force)
{
	if(force)
	{
		int p, i;
		ct_t *ct;
		ecm_foreach_ct(ct,
		{
			if(ct->destroy)
			{
				for(p = 0; p < ct->pages_size; p++)
				{
					struct comp_page *page = &ct->pages[p];
					for(i = 0; i < page->components_size; i++)
					{
						c_t *c = ct_get_at(ct, p, i);
						ct->destroy(c);
					}

				}
			}
		});
		goto end;
	}

	ct_t *dest = ecm_get(ref("destroyed"));

	while(dest->pages[0].components_size)
	{

		c_destroyed_t *dc = (c_destroyed_t*)ct_get_at(dest, 0, 0);
		entity_t ent = c_entity(dc);
		if(!ent)
		{
			printf("bug\n");
			exit(1);
		}

		ct_t *ct;
		ecm_foreach_ct(ct,
		{
			c_t *c = ct_get(ct, &ent); if(!c) continue;
			if(ct->destroy)
			{
				ct->destroy(c);
			}
			c->entity = entity_null;

			uint offset = ct->offsets[ent].offset;
			uint page = ct->offsets[ent].page;

			struct comp_page *last = &ct->pages[ct->pages_size - 1];
			struct comp_page *curr = &ct->pages[page];
			if(ct->pages_size <= page) continue;

			uint last_offset = last->components_size - 1;
			if(last != curr && offset != last_offset)
			{
				memcpy(&curr->components[offset],
						&last->components[last_offset], ct->size);
			
				entity_t ent2 = ct_get_at(ct, page, offset)->entity;

				if(ent2)
				{
					ct->offsets[ent2].offset = offset;
					ct->offsets[ent2].page = page;
				}
			}
			

			ct->offsets[ent].offset = -1;
			last->components_size--;
			if(last->components_size == 0)
			{
				free(last->components);
				ct->pages_size--;
			}
		});
	}
end:

	g_ecm->dirty = 0;
	steps = 0;

	SDL_SemPost(sem2);
}

void ecm_clean(int force)
{
	if(!g_ecm->dirty && !force) return;
	if(steps == 2)
	{
		ecm_clean2(force);
	}
	else if(steps == 0)
	{
		steps++;
		SDL_SemWait(sem1);
		ecm_clean2(force);
	}
	else
	{
		steps++;
		SDL_SemPost(sem1);
		SDL_SemWait(sem2);
	}
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
		int j, new_size = entity + 1;
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

