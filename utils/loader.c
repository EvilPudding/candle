#include <candle.h>
#include <utils/glutil.h>
#include "loader.h"
#include <stdlib.h>
#include <stdio.h>
#include <tinycthread.h>

loader_t *loader_new()
{
	loader_t *self = calloc(1, sizeof *self);
	return self;
}

void loader_start(loader_t *self)
{
#ifndef __EMSCRIPTEN__
	unsigned int i;
	self->mutex = malloc(sizeof(mtx_t));
	mtx_init(self->mutex, mtx_plain | mtx_recursive);
	for (i = 0; i < LOADER_STACK_SIZE; i++)
	{
		self->stack[i].cond = NULL;
	}
#endif
}

void _loader_push_wait(loader_t *self, loader_cb cb, void *usrptr, c_t *c)
{
#ifndef __EMSCRIPTEN__
	if (!self->mutex)
	{
		printf("Loader not ready for push_wait.\n");
	}
#endif
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
		mtx_t mtx;
		load_t *load;

		mtx_lock(self->mutex);
			load = &self->stack[self->last];
			load->usrptr = usrptr;
			load->cb = cb;
			if(c) load->ct = *c;
			self->last = (self->last + 1) % LOADER_STACK_SIZE;

			mtx_init(&mtx, mtx_plain);
			mtx_lock(&mtx);
			load->cond = malloc(sizeof(cnd_t));
			cnd_init(load->cond);

		mtx_unlock(self->mutex);

		cnd_wait(load->cond, &mtx);
		mtx_unlock(&mtx);
		mtx_destroy(&mtx);
	}
}


void _loader_push(loader_t *self, loader_cb cb, void *usrptr, c_t *c)
{
	load_t *load;

	mtx_lock(self->mutex);
		load = &self->stack[self->last];
		load->usrptr = usrptr;
		load->cb = cb;
		if(c) load->ct = *c;
		if (load->cond)
		{
			cnd_destroy(load->cond);
			load->cond = NULL;
		}
		self->last = (self->last + 1) % LOADER_STACK_SIZE;
	mtx_unlock(self->mutex);
}

int loader_update(loader_t *self)
{
	mtx_lock(self->mutex);
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
			cnd_signal(load->cond);
			cnd_destroy(load->cond);
			load->cond = NULL;
		}

		self->first = (self->first + 1) % LOADER_STACK_SIZE;
	}
	mtx_unlock(self->mutex);
	return 1;
}

void loader_destroy(loader_t *self)
{
	mtx_destroy(self->mutex);
	free(self->mutex);
	
	free(self);
}
