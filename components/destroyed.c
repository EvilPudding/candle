#include <candle.h>
#include <systems/editmode.h>
#include "destroyed.h"

static SDL_sem *sem = NULL;
static SDL_sem *sem2 = NULL;
c_destroyed_t *c_destroyed_new()
{
	if(!sem)
	{
		sem = SDL_CreateSemaphore(0);
		sem2 = SDL_CreateSemaphore(0);
	}

	return component_new("c_destroyed");
}

void c_destroyed_destroy(c_destroyed_t *self)
{
	entity_t ent = c_entity(self);
	ct_t *ct;
	ecm_foreach_ct(ct,
	{
		c_t *c = ct_get(ct, &ent);
		if(!c) continue;
		c->entity = entity_null;
		ct->offsets[ent].offset = -1;
	});

	SDL_SemPost(sem2);
}

int c_destroyed_thr(c_destroyed_t *self)
{
	if(self->steps == 2) return 1;
	if(self->steps == 0)
	{
		self->steps++;
		SDL_SemWait(sem);
		c_destroyed_destroy(self);
	}
	else
	{
		self->steps++;
		SDL_SemPost(sem);
		SDL_SemWait(sem2);
	}
	return 1;
}

REG()
{
	ct_t *ct = ct_new("c_destroyed", sizeof(c_destroyed_t), NULL, 0);

	ct_listener(ct, WORLD, sig("after_update"), c_destroyed_thr);
	ct_listener(ct, WORLD, sig("after_draw"), c_destroyed_thr);
}

