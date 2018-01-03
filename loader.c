#include "loader.h"
#include <stdlib.h>
#include <stdio.h>

static int loader_thread(loader_t *self);

loader_t *loader_new(int async)
{
	loader_t *self = calloc(1, sizeof *self);

	if(async)
	{
		SDL_AtomicSet(&self->done, 0);

		self->semaphore = SDL_CreateSemaphore(0);

		self->context = SDL_GL_CreateContext(NULL);

		if(self->context == NULL)
		{
			printf( "OpenGL context could not be created! SDL Error: %s\n", SDL_GetError());
			exit(1);
		}

		self->thread = SDL_CreateThread((int(*)(void*))loader_thread, "loader_thread", self);
	}
	else
	{
		self->threadId = SDL_ThreadID();
	}
	return self;
}

void loader_push_wait(loader_t *self, loader_cb cb, void *usrptr, c_t *c)
{
	if(SDL_ThreadID() == self->threadId)
	{
		printf("Can't wait for load in the same thread as loader!\n");
		exit(1);
	}
	else
	{
		load_t *load = &self->stack[self->last];
		load->usrptr = usrptr;
		load->cb = cb;
		if(c) load->ct = *c;
		self->last = (self->last + 1) % LOADER_STACK_SIZE;

		load->semaphore = SDL_CreateSemaphore(0);

		SDL_SemWait(load->semaphore);

		SDL_DestroySemaphore(load->semaphore);
	}
}

void loader_push(loader_t *self, loader_cb cb, void *usrptr, c_t *c)
{
	load_t *load = &self->stack[self->last];
	load->usrptr = usrptr;
	load->cb = cb;
	if(c) load->ct = *c;
	load->semaphore = NULL;
	self->last = (self->last + 1) % LOADER_STACK_SIZE;

	if(self->semaphore) SDL_SemPost(self->semaphore);
}

int loader_update(loader_t *self)
{
	while(self->first != self->last)
	{

		load_t *load = &self->stack[self->first];

		if(load->usrptr)
		{
			load->cb(load->usrptr);
		}
		else
		{
			void *c = ct_get(ecm_get(load->ct.entity.ecm, load->ct.comp_type),
					load->ct.entity);
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
	return 1;
}

static int loader_thread(loader_t *self)
{
	self->threadId = SDL_ThreadID();

	while(!SDL_AtomicGet(&self->done))
	{
		loader_update(self);
		SDL_SemWait(self->semaphore);
	}
	return 1;
}

void loader_destroy(loader_t *self)
{
	if(self->thread)
	{
		SDL_AtomicSet(&self->done, 1);
		SDL_SemPost(self->semaphore);
		SDL_WaitThread(self->thread, NULL);

		SDL_GL_DeleteContext(self->context);  
		SDL_DestroySemaphore(self->semaphore);  
	}
	
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
