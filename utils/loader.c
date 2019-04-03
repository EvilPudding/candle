#include <utils/glutil.h>
#include "loader.h"
#include <stdlib.h>
#include <stdio.h>

/* SDL_threadID glid; */
loader_t *loader_new()
{
	loader_t *self = calloc(1, sizeof *self);
	self->threadId = SDL_ThreadID();
	self->semaphore = SDL_CreateSemaphore(1);
	return self;
}

void _loader_push_wait(loader_t *self, loader_cb cb, void *usrptr, c_t *c)
{
	if(SDL_ThreadID() == self->threadId)
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
		SDL_SemWait(self->semaphore);
			load_t *load = &self->stack[self->last];
			load->usrptr = usrptr;
			load->cb = cb;
			if(c) load->ct = *c;
			self->last = (self->last + 1) % LOADER_STACK_SIZE;
		SDL_SemPost(self->semaphore);

		load->semaphore = SDL_CreateSemaphore(0);

		SDL_SemWait(load->semaphore);

		SDL_DestroySemaphore(load->semaphore);
	}
}


void _loader_push(loader_t *self, loader_cb cb, void *usrptr, c_t *c)
{
	int same_thread = SDL_ThreadID() == self->threadId;
	if(!same_thread) SDL_SemWait(self->semaphore);
		load_t *load = &self->stack[self->last];
		load->usrptr = usrptr;
		load->cb = cb;
		if(c) load->ct = *c;
		load->semaphore = NULL;
		self->last = (self->last + 1) % LOADER_STACK_SIZE;
	if(!same_thread) SDL_SemPost(self->semaphore);
}

int loader_update(loader_t *self)
{
	SDL_SemWait(self->semaphore);
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

		if(load->semaphore)
		{
			SDL_SemPost(load->semaphore);
		}

		load->cb = NULL;
		load->usrptr = NULL;
		load->semaphore = NULL;

		self->first = (self->first + 1) % LOADER_STACK_SIZE;
	}
	SDL_SemPost(self->semaphore);
	return 1;
}

void loader_destroy(loader_t *self)
{
	SDL_DestroySemaphore(self->semaphore);  
	
	free(self);
}

void loader_wait(loader_t *self)
{
	while(self->last != self->first)
	{
		/* printf("%d %d\n", self->last, self->first); */
		SDL_Delay(1);
	}
}
