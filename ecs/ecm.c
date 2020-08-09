#include <string.h>
#include "ecm.h"
#include <stdarg.h>

#include "candle.h"
#include "components/destroyed.h"

#include <ecs/entity.h>
#ifdef THREADED
#include <tinycthread.h>
#endif

#ifdef THREADED
static mtx_t entity_mtx;
static mtx_t clean_mtx;
static mtx_t cts_mtx;
static mtx_t signals_mtx;
#endif

entity_t SYS = 0x0000000100000001ul;

listener_t *ct_get_listener(ct_t *self, uint32_t signal)
{
	uint32_t i;
	vector_t *listeners;

	if(!self) return NULL;

	listeners = ecm_get_listeners(signal);

	for(i = 0; i < vector_count(listeners); i++)
	{
		listener_t *listener;
		vector_get_copy(listeners, i, &listener);
		if(listener->target == self->id)
		{
			return listener;
		}
	}

	return NULL;
}

void *component_new(ct_id_t ct_id)
{
	int i;
	c_t *self;
	entity_t entity = _entity_get_current();

	ct_t *ct = ecm_get(ct_id);

	self = ct_add(ct, entity);

	for(i = 0; i < ct->depends_size; i++)
	{
		ct_t *ct2 = ecm_get(ct->depends[i].ct);
		if(!ct_get(ct2, &entity))
		{
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

	self->signals = kh_init(sig);
	self->cts = kh_init(ct);
	self->cs = kh_init(c);


	/* create entity_null */
	{
		self->entities_info = malloc(sizeof(*g_ecm->entities_info));
		self->entities_info->uid = 0;
		self->entities_info->next_free = 1;
		self->entities_info_size = 1;
		self->entity_uid_counter = 1;
	}

#ifdef THREADED
	g_ecm->mtx = malloc(sizeof(mtx_t));
	mtx_init(g_ecm->mtx, mtx_recursive);

	mtx_init(&entity_mtx, mtx_recursive);

	mtx_init(&clean_mtx, mtx_plain);

	mtx_init(&cts_mtx, mtx_recursive);
	mtx_init(&signals_mtx, mtx_recursive);
#endif
	entity_creation_init();

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

int listeners_compare(listener_t **a, listener_t **b)
{
	return (*a)->priority - (*b)->priority;
}

vector_t *ecm_get_listeners(uint32_t signal)
{
	khiter_t k;
	vector_t *listeners;

#ifdef THREADED
	mtx_lock(&signals_mtx);
#endif
	k = kh_get(sig, g_ecm->signals, signal);
	if(k == kh_end(g_ecm->signals))
	{
		int ret;

		k = kh_put(sig, g_ecm->signals, signal, &ret);
		listeners = vector_new(sizeof(listener_t*), VECTOR_REENTRANT, NULL,
		                       (vector_compare_cb)listeners_compare);
		kh_value(g_ecm->signals, k) = listeners;
	}
	else
	{
		listeners = kh_value(g_ecm->signals, k);
	}
#ifdef THREADED
	mtx_unlock(&signals_mtx);
#endif
	return listeners;
}

void _ct_add_listener(ct_t *self, int32_t flags, int32_t priority, uint32_t signal,
                      signal_cb cb)
{
	listener_t *lis = malloc(sizeof(*lis));
	vector_t *listeners = ecm_get_listeners(signal);
	if(ct_get_listener(self, signal))
	{
		printf("Listener already exists\n");
		exit(1);
	}

	lis->signal = signal;
	lis->cb = cb;
	lis->flags = flags;
	lis->target = self->id;
	lis->priority = priority;
	
	vector_add(listeners, &lis);

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
	entity_t ent;
	entity_info_t *info;
	struct { uint32_t pos, uid; } *convert;
	uint32_t i;

#ifdef THREADED
	mtx_lock(&entity_mtx);
#endif

	i = g_ecm->entities_info[0].next_free;

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

	ent = info->uid;

	convert = (void*)&ent;
	convert->pos = i;
	convert->uid = info->uid;

#ifdef THREADED
	mtx_unlock(&entity_mtx);
#endif
	return ent;
}

void ct_add_interaction(ct_t *dep, ct_id_t target)
{
	int32_t i;
	ct_t *ct;

	ct = ecm_get(target);
	if(!ct) return;
	if(!dep) return;

	dep->is_interaction = 1;
	i = ct->depends_size++;
	ct->depends = realloc(ct->depends, sizeof(*ct->depends) * ct->depends_size);
	ct->depends[i].ct = dep->id;
	ct->depends[i].is_interaction = 1;
}

void ct_add_dependency(ct_t *self, ct_id_t target)
{
	int32_t i;
	if (!self) return;
	i = self->depends_size++;
	self->depends = realloc(self->depends, sizeof(*self->depends) * self->depends_size);
	self->depends[i].ct = target;
	self->depends[i].is_interaction = 0;
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

void ct_init(ct_t *self, const char *name, uint32_t size)
{
	self->size = size;
	strncpy(self->name, name, sizeof(self->name) - 1);
}

void ct_set_init(ct_t *self, init_cb init)
{
	self->init = init;
}

void ct_set_destroy(ct_t *self, destroy_cb destroy)
{
	self->destroy = destroy;
}

void ecm_clean2(int force)
{
	ct_t *dest;
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
					c_t *c;
					if(!kh_exist(ct->cs, k)) continue;
					c = kh_value(ct->cs, k);
					ct->destroy(c);

					kh_del(c, ct->cs, k);
				}
			}
		});
		goto end;
	}

	dest = ecm_get(ct_destroyed);

	for(k = kh_begin(dest->cs); k != kh_end(dest->cs); ++k)
	{
		c_destroyed_t *dc;
		entity_t ent;

		if(!kh_exist(dest->cs, k)) continue;
		dc = (c_destroyed_t*)kh_value(dest->cs, k);
		ent = c_entity(dc);
		entity_destroy(ent);
	}
end:

	g_ecm->dirty = 0;
	g_ecm->safe = 0;
}

void ecm_clean(int force)
{
	if(!g_ecm->dirty && !force) return;
#ifndef THREADED
	ecm_clean2(force);
#else

	if (is_render_thread())
	{
		if (g_ecm->safe)
		{
			ecm_clean2(force);
			mtx_unlock(&clean_mtx);
		}
	}
	else
	{
		mtx_lock(&entity_mtx);
		g_ecm->safe++;
		mtx_unlock(&entity_mtx);

		mtx_lock(&clean_mtx);
	}
#endif
}

c_t *ct_add(ct_t *self, entity_t entity)
{
	int32_t ret;
	uint32_t uid, key;
	khiter_t k;
	c_t **comp;

	if(!self) return NULL;

#ifdef THREADED
	mtx_lock(&cts_mtx);
#endif

	/* int page_id = self->pages_size - 1; */
	/* struct comp_page *page = &self->pages[page_id]; */
	/* if(page->components_size == PAGE_SIZE) */
	/* { */
	/* 	page = ct_add_page(self); */
	/* 	page_id++; */
	/* } */

	/* uint32_t offset; */

	uid = entity_uid(entity);
	key = ent_comp_ref(self->id, uid);
	k = kh_put(c, g_ecm->cs, key, &ret);
	comp = &kh_value(g_ecm->cs, k);
	*comp = calloc(1, self->size);

	k = kh_put(c, self->cs, uid, &ret);
	kh_value(self->cs, k) = *comp;

	/* page->components_size++; */

	/* if(pos >= self->offsets_size) */
	/* { */
		/* self->offsets_size = pos + 1; */
	/* } */
#ifdef THREADED
	mtx_unlock(&cts_mtx);
#endif

	/* c_t *comp = (c_t*)&page->components[offset]; */
	/* memset(comp, 0, self->size); */
	(*comp)->entity = entity;
	(*comp)->comp_type = self->id;
	return *comp;
}

ct_t *ecm_get(ct_id_t id)
{
	uint64_t comp_type = (uint64_t)id;
	khiter_t k;
	ct_t *ct = NULL;

#ifdef THREADED
	mtx_lock(g_ecm->mtx);
#endif
	k = kh_get(ct, g_ecm->cts, comp_type);

	if (k != kh_end(g_ecm->cts))
	{
		ct = kh_value(g_ecm->cts, k);
	}

	if (!ct)
	{
		int ret;

		k = kh_get(ct, g_ecm->cts, comp_type);
		if (k == kh_end(g_ecm->cts))
		{
			k = kh_put(ct, g_ecm->cts, comp_type, &ret);
			assert(k != kh_end(g_ecm->cts));
			ct = calloc(1, sizeof(*ct));
			kh_value(g_ecm->cts, k) = ct;
			memset(ct, 0, sizeof(*ct));
			ct->id = id;
			ct->cs = kh_init(c);
			id(ct);
			ct->ready = true;
			assert(ct->name[0]);
		}

	}
#ifdef THREADED
	mtx_unlock(g_ecm->mtx);
#endif
	return ct;
}

