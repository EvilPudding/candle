#include <utils/glutil.h>
#include "entity.h"
#include <stdarg.h>
#include <ecs/ecm.h>
#include <candle.h>
#include <components/name.h>
#include <components/destroyed.h>

/* static void entity_check_missing_dependencies(entity_t self); */

struct entity_creation {
	entity_t entities[32];
	uint32_t creating_num;
};
static SDL_SpinLock _g_tls_lock;
static SDL_TLSID _g_creating_entity;
extern SDL_sem *sem;

static struct entity_creation *g_creating_entity_init(void)
{
	struct entity_creation *value = calloc(1, sizeof(*value));
	SDL_AtomicLock(&_g_tls_lock);
	if (!_g_creating_entity) {
		_g_creating_entity = SDL_TLSCreate();
	}
	SDL_AtomicUnlock(&_g_tls_lock);

	SDL_TLSSet(_g_creating_entity, value, 0);

	return value;
}

static struct entity_creation *g_creating_entity_get(void)
{
	struct entity_creation *value = SDL_TLSGet(_g_creating_entity);
	if (!value)
	{
		value = g_creating_entity_init();
	}
    return value;
}

void entity_new_post()
{
	struct entity_creation *ec = g_creating_entity_get();
	entity_t self = ec->entities[--ec->creating_num];

	entity_signal_same(self, sig("entity_created"), NULL, NULL);
}

entity_t entity_new_pre(void)
{
	entity_t self = ecm_new_entity();

	struct entity_creation *ec = g_creating_entity_get();
	ec->entities[ec->creating_num++] = self;

	return self;
}

int filter_listener(listener_t *self)
{
	return CONTINUE;
	/* unsigned long thr = SDL_ThreadID(); */
	/* int is_render_thread = thr == candle->render_id; */
	/* if(is_render_thread) */
	/* { */
	/* 	return self->flags & RENDER_THREAD; */
	/* } */
	/* return !(self->flags & RENDER_THREAD); */
}

void entity_destroy(entity_t self)
{
	if(!entity_exists(self)) return;

	if(!g_ecm->safe)
	{
		if(!c_destroyed(&self))
		{
			entity_add_component(self, c_destroyed_new());
			g_ecm->dirty = 1;
		}
	}
	else
	{
		entity_info_t *info;
		unsigned int pos = entity_pos(self);
		unsigned int uid = entity_uid(self);
		ct_t *ct;

		ecm_foreach_ct(ct, {
			c_t *c;
			uint32_t key;
			khiter_t k = kh_get(c, ct->cs, entity_uid(self));
			if(k == kh_end(ct->cs)) continue;
			c = kh_value(ct->cs, k);


			if(ct->destroy) ct->destroy(c);
			c->entity = entity_null;

			kh_del(c, ct->cs, k);
			
			key = ent_comp_ref(ct->id, uid);
			k = kh_get(c, g_ecm->cs, key);
			kh_del(c, g_ecm->cs, k);
			free(c);
		});
		info = &g_ecm->entities_info[pos];
		info->next_free = g_ecm->entities_info[0].next_free;
		info->uid = 0;
		g_ecm->entities_info[0].next_free = pos;
	}
}

int listener_signal(listener_t *self, entity_t ent, void *data, void *output)
{
	khiter_t k;
	ct_t *ct = ecm_get(self->target);

	for(k = kh_begin(ct->cs); k != kh_end(ct->cs); ++k)
	{
		c_t *c;

		if(!kh_exist(ct->cs, k)) continue;

		c = kh_value(ct->cs, k);
		if(!c_entity(c)) continue;
		if((self->flags & ENTITY) && c_entity(c) != ent) continue;

		if(self->cb(c, data, output) == STOP) return STOP;
	}
	return CONTINUE;
}

int listener_signal_same(listener_t *self, entity_t ent, void *data, void *output)
{
	c_t *c = ct_get(ecm_get(self->target), &ent);
	if(c)
	{
		if(c_entity(c) != ent)  return CONTINUE;
		return self->cb(c, data, output);
	}
	return CONTINUE;
}

int component_signal(c_t *comp, ct_t *ct, uint32_t signal, void *data, void *output)
{
	listener_t *listener = ct_get_listener(ct, signal);
	if(listener)
	{
		listener->cb(comp, data, output);
	}
	return CONTINUE;
}

int entity_signal_same(entity_t self, uint32_t signal, void *data, void *output)
{
	int i;
	signal_t *sig = ecm_get_signal(signal);
	for(i = vector_count(sig->listener_types) - 1; i >= 0; i--)
	{
		listener_t *lis = vector_get(sig->listener_types, i);
		int res = listener_signal_same(lis, self, data, output);
		if(res == STOP) return STOP;
	}
	return CONTINUE;
}

int entity_signal(entity_t self, uint32_t signal, void *data, void *output)
{
	int i;
	signal_t *sig = ecm_get_signal(signal);
	for(i = vector_count(sig->listener_types) - 1; i >= 0; i--)
	{
		listener_t *lis = vector_get(sig->listener_types, i);
		if(listener_signal(lis, self, data, output) == STOP) return STOP;
	}
	return CONTINUE;
}


entity_t _entity_get_current()
{
	struct entity_creation *ec = g_creating_entity_get();
	return ec->entities[ec->creating_num - 1];
}

void _entity_add_post(entity_t self, c_t *comp)
{
	struct entity_creation *ec = g_creating_entity_get();
	ec->creating_num--;

	if (comp)
	{
		ct_t *ct = ecm_get(comp->comp_type);
		component_signal(comp, ct, ref("entity_created"), NULL, NULL);
	}
}

void _entity_add_pre(entity_t entity)
{
	struct entity_creation *ec = g_creating_entity_get();
	ec->entities[ec->creating_num++] = entity;
}

/* void _entity_add_component(entity_t self, c_t *comp, int on_creation) */
/* { */
/* 	ct_t *ct = ecm_get(comp->comp_type); */
/* 	ct_add(ct, comp); */
/* 	if(!on_creation) */
/* 	{ */
/* 		component_signal(comp, ct, entity_created, NULL); */
/* 	} */
/* } */

