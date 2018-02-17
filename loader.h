#ifndef LOADER_H
#define LOADER_H

#include "glutil.h"
#include "ecm.h"

typedef int(*loader_cb)(void*);

#define LOADER_STACK_SIZE 512

typedef struct
{
	void *usrptr;
	loader_cb cb;
	SDL_sem *semaphore;
	c_t ct;
} load_t;

typedef struct
{
	SDL_Thread *thread;
	SDL_threadID threadId;
	SDL_GLContext *context;
	SDL_sem *semaphore;
	SDL_atomic_t done;
	load_t stack[LOADER_STACK_SIZE];
	int last;
	int first;
} loader_t;

loader_t *loader_new(void);
void loader_push(loader_t *self, loader_cb cb, void *usrptr, c_t *c);
void loader_push_wait(loader_t *self, loader_cb cb, void *usrptr, c_t *c);
int loader_update(loader_t *self);
void loader_wait(loader_t *self);
void loader_destroy(loader_t *self);

#endif /* !LOADER_H */
