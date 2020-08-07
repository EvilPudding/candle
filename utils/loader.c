#include <candle.h>
#include <utils/glutil.h>
#include "loader.h"
#include <stdlib.h>
#include <stdio.h>
#ifdef THREADED
#include <tinycthread.h>
#endif

loader_t *loader_new()
{
	loader_t *self = calloc(1, sizeof *self);
	return self;
}

void loader_start(loader_t *self)
{
	unsigned int i;
#ifdef THREADED
	self->mutex = malloc(sizeof(mtx_t));
	mtx_init(self->mutex, mtx_plain | mtx_recursive);
#endif
	for (i = 0; i < LOADER_STACK_SIZE; i++)
	{
		self->stack[i].cond = NULL;
	}
}

void _loader_push_wait(loader_t *self, loader_cb cb, void *usrptr, c_t *c)
{
	if (!self->mutex)
	{
		printf("Loader not ready for push_wait.\n");
	}
	if (!self->mutex || is_render_thread())
	{
		if(c)
		{
			cb(ct_get(ecm_get(c->comp_type), &c->entity));
		}
		else
		{
			cb(usrptr);
		}
	}
	else
	{
		load_t *load;
#ifdef THREADED
		mtx_t mtx;

		mtx_lock(self->mutex);
#endif
			load = &self->stack[self->last];
			load->usrptr = usrptr;
			load->cb = cb;
			if(c) load->ct = *c;
			self->last = (self->last + 1) % LOADER_STACK_SIZE;

#ifdef THREADED
			mtx_init(&mtx, mtx_plain);
			mtx_lock(&mtx);
			load->cond = malloc(sizeof(cnd_t));
			cnd_init(load->cond);

		mtx_unlock(self->mutex);
		cnd_wait(load->cond, &mtx);
		mtx_unlock(&mtx);
		mtx_destroy(&mtx);
#endif
	}
}


void _loader_push(loader_t *self, loader_cb cb, void *usrptr, c_t *c)
{
	load_t *load;

#ifdef THREADED
	mtx_lock(self->mutex);
#endif
		load = &self->stack[self->last];
		load->usrptr = usrptr;
		load->cb = cb;
		if(c) load->ct = *c;
		if (load->cond)
		{
#ifdef THREADED
			cnd_destroy(load->cond);
#endif
			load->cond = NULL;
		}
		self->last = (self->last + 1) % LOADER_STACK_SIZE;
#ifdef THREADED
	mtx_unlock(self->mutex);
#endif
}

int loader_update(loader_t *self)
{
#ifdef THREADED
	mtx_lock(self->mutex);
#endif
	while(self->first != self->last)
	{
		load_t *load = &self->stack[self->first];

		if(load->usrptr)
		{
			load->cb(load->usrptr);
		}
		else
		{
			void *c = ct_get(ecm_get(load->ct.comp_type), &load->ct.entity);
			load->cb(c);
		}

		load->cb = NULL;
		load->usrptr = NULL;
		if (load->cond)
		{
#ifdef THREADED
			cnd_signal(load->cond);
			cnd_destroy(load->cond);
#endif
			load->cond = NULL;
		}

		self->first = (self->first + 1) % LOADER_STACK_SIZE;
	}
#ifdef THREADED
	mtx_unlock(self->mutex);
#endif
	return 1;
}

void loader_destroy(loader_t *self)
{
#ifdef THREADED
	mtx_destroy(self->mutex);
#endif
	free(self->mutex);
	
	free(self);
}
