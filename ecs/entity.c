#include "../utils/glutil.h"
#include "entity.h"
#include <stdarg.h>
#include "../ecs/ecm.h"
#include "../candle.h"
#include "../components/name.h"
#include "../components/destroyed.h"
#ifdef THREADED
#include "../third_party/tinycthread/source/tinycthread.h"
#endif

/* static void entity_check_missing_dependencies(entity_t self); */

struct entity_creation {
	entity_t entities[32];
	uint32_t creating_num;
};
#ifdef THREADED
static tss_t _g_creating_entity;
#else
struct entity_creation _g_creating_entity;
#endif

void entity_creation_init()
{
#ifdef THREADED
	if (tss_create(&_g_creating_entity, (tss_dtor_t)free) != thrd_success)
	{
		assert(false);
	}
#endif
}

static struct entity_creation *g_creating_entity_get(void)
{
#ifdef THREADED
	struct entity_creation *value = tss_get(_g_creating_entity);
	if (!value)
	{
		value = calloc(1, sizeof(*value));
		tss_set(_g_creating_entity, value);
	}
    return value;
#else
    return &_g_creating_entity;
#endif
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

int listener_signal(const listener_t *self, entity_t ent, void *data, void *output)
{
	khiter_t k;
	ct_t *ct = ecm_get(self->target);
	assert(ct->ready);

	for(k = kh_begin(ct->cs); k != kh_end(ct->cs); ++k)
	{
		/* double start; */
		c_t *c;
		int ret;

		if(!kh_exist(ct->cs, k)) continue;

		c = kh_value(ct->cs, k);
		if(!c_entity(c)) continue;
		if((self->flags & ENTITY) && c_entity(c) != ent) continue;

		/* if (self->signal == ref("world_update")) */
			/* start = glfwGetTime(); */

		ret = self->cb(c, data, output);

		/* if (self->signal == ref("world_update")) */
			/* printf("%s %f\n", ct->name, glfwGetTime() - start); */

		if(ret == STOP) return STOP;
	}
	return CONTINUE;
}

int listener_signal_same(listener_t *self, entity_t ent, void *data, void *output)
{
	ct_t *ct = ecm_get(self->target);
	c_t *c = ct_get(ct, &ent);
	if(c)
	{
		/* double start; */
		int ret;
		if(c_entity(c) != ent)  return CONTINUE;

		/* if (self->signal == ref("world_update")) */
			/* start = glfwGetTime(); */

		ret = self->cb(c, data, output);

		/* if (self->signal == ref("world_update")) */
			/* printf("%s %f\n", ct->name, glfwGetTime() - start); */

		return ret;
	}
	return CONTINUE;
}

int component_signal(c_t *comp, ct_t *ct, uint32_t signal, void *data, void *output)
{
	listener_t *listener;

	listener = ct_get_listener(ct, signal);

	if(listener)
	{
		/* double start; */

		/* if (signal == ref("world_update")) */
			/* start = glfwGetTime(); */

		listener->cb(comp, data, output);

		/* if (signal == ref("world_update")) */
			/* printf("%s %f\n", ct->name, glfwGetTime() - start); */
	}
	return CONTINUE;
}

int entity_signal_same(entity_t self, uint32_t signal, void *data, void *output)
{
	int i;
	int response = CONTINUE;
	vector_t *listeners;

	if (self == entity_null)
	{
		return entity_signal(self, signal, data, output);
	}

	listeners = ecm_get_listeners(signal);

	for(i = vector_count(listeners) - 1; i >= 0; i--)
	{
		listener_t *lis;
		int res;

		vector_get_copy(listeners, i, &lis);

		res = listener_signal_same(lis, self, data, output);
		if(res == STOP)
		{
			response = STOP;
			break;
		}
	}
	return response;
}

int entity_signal(entity_t self, uint32_t signal, void *data, void *output)
{
	int i;
	int response = CONTINUE;
	vector_t *listeners;

	listeners = ecm_get_listeners(signal);


	for(i = vector_count(listeners) - 1; i >= 0; i--)
	{
		int ret;
		listener_t *lis;

		vector_get_copy(listeners, i, &lis);

		ret = listener_signal(lis, self, data, output);

		if(ret == STOP)
		{
			response = CONTINUE;
			break;
		}
	}

	return response;
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

