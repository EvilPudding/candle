#include "entity.h"
#include <stdarg.h>
#include <ecm.h>
#include <candle.h>
#include <components/name.h>
#include <components/destroyed.h>

/* static void entity_check_missing_dependencies(entity_t self); */

__thread entity_t _g_creating[32] = {0};
__thread int _g_creating_num = 0;

extern SDL_sem *sem;

void _entity_new_pre(void)
{
	_g_creating[_g_creating_num++] = ecm_new_entity();
}

int filter_listener(listener_t *self)
{
	return 1;
	/* unsigned long thr = SDL_ThreadID(); */
	/* int is_render_thread = thr == candle->render_id; */
	/* if(is_render_thread) */
	/* { */
	/* 	return self->flags & RENDER_THREAD; */
	/* } */
	/* return !(self->flags & RENDER_THREAD); */
}

entity_t _entity_new(int comp_num, ...)
{
	/* int i; */
	/* va_list comps; */
	entity_t self = _g_creating[--_g_creating_num];

    /* va_start(comps, comp_num); */
	/* for(i = 0; i < comp_num; i++) */
	/* { */
		/* c_t *c = va_arg(comps, c_t*); */
		/* _entity_add_component(self, c, 1); */
		/* free(c); */
	/* } */
	/* va_end(comps); */

	/* entity_check_missing_dependencies(self); */

	/* if(c_name(&self) && !strcmp(c_name(&self)->name, "grid")) */
	/* { */
		/* printf("new %s entity %ld\n", c_name(&self)->name, self); */
	/* } */
	entity_signal_same(self, sig("entity_created"), NULL);

	return self;
}

void entity_destroy(entity_t self)
{
	if(!c_destroyed(&self))
	{
		entity_add_component(self, c_destroyed_new());
	}
}

int listener_signal(listener_t *self, entity_t ent, void *data)
{
	int p, j;
	ct_t *ct = ecm_get(self->comp_type);
	for(p = 0; p < ct->pages_size; p++)
	{
		for(j = 0; j < ct->pages[p].components_size; j++)
		{
			c_t *c = ct_get_at(ct, p, j);
			if(!c_entity(c)) continue;
			if((self->flags & ENTITY) && c_entity(c) != ent)
				continue;

			if(self->cb(c, data) == 0) return 0;
		}
	}
	return 1;
}

int listener_signal_same(listener_t *self, entity_t ent, void *data)
{
	c_t *c = ct_get(ecm_get(self->comp_type), &ent);
	if(c)
	{
		return self->cb(c, data);
	}
	return 1;
}

int component_signal(c_t *comp, ct_t *ct, uint signal, void *data)
{
	listener_t *listener = ct_get_listener(ct, signal);
	if(listener)
	{
		listener->cb(comp, data);
	}
	return 1;
}

int entity_signal_same(entity_t self, uint signal, void *data)
{
	uint i;

	signal_t *sig = ecm_get_signal(signal);

	for(i = 0; i < sig->listeners_size; i++)
	{
		listener_t *lis = &sig->listeners[i];
		int res = listener_signal_same(lis, self, data);
		if(res == 0) return 0;
	}
	return 1;
}

/* void entity_filter(entity_t self, uint signal, void *data, */
/* 		filter_cb cb, c_t *c_caller, void *cb_data) */
/* { */
/* 	uint i, j, p; */

/* 	signal_t *sig = &g_ecm->signals[signal]; */

/* 	for(i = 0; i < sig->cts_size; i++) */
/* 	{ */
/* 		uint ct_id = sig->cts[i]; */
/* 		ct_t *ct = ecm_get(ct_id); */
/* 		listener_t *listener = ct_get_listener(ct, signal); */
/* 		if(listener) */
/* 		{ */
/* 			for(p = 0; p < ct->pages_size; p++) */
/* 			{ */
/* 				for(j = 0; j < ct->pages[p].components_size; j++) */
/* 				{ */
/* 					c_t *c = ct_get_at(ct, p, j); */

/* 					if((listener->flags & SAME_ENTITY) && c_entity(c) != self) */
/* 					if(listener->cb(c, data)) */
/* 					{ */
/* 						cb(c_caller, c, cb_data); */
/* 					} */
/* 				} */
/* 			} */
/* 		} */
/* 	} */
/* } */

int entity_signal(entity_t self, uint signal, void *data)
{
	uint i;

	signal_t *sig = ecm_get_signal(signal);
	if(!sig) exit(1);
	for(i = 0; i < sig->listeners_size; i++)
	{
		listener_t *lis = &sig->listeners[i];
		if(listener_signal(lis, self, data) == 0) return 0;
	}
	return 1;
}


void _entity_add_post(entity_t self, c_t *comp)
{
	--_g_creating_num;

	ct_t *ct = ecm_get(comp->comp_type);
	component_signal(comp, ct, sig("entity_created"), NULL);

}

void _entity_add_pre(entity_t entity)
{
	_g_creating[_g_creating_num++] = entity;
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

