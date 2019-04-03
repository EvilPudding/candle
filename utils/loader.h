#ifndef LOADER_H
#define LOADER_H

#include <ecs/ecm.h>

typedef int(*loader_cb)(void*);

#define LOADER_STACK_SIZE 512

typedef struct
{
	void *usrptr;
	loader_cb cb;
	void *semaphore;
	c_t ct;
} load_t;

typedef struct
{
	void *thread;
	uint64_t threadId;
	void *context;
	void *semaphore;
	int32_t done;
	load_t stack[LOADER_STACK_SIZE];
	int last;
	int first;
} loader_t;

loader_t *loader_new(void);
void loader_push(loader_t *self, loader_cb cb, void *usrptr, c_t *c);
#define loader_push(s, cb, u, c) _loader_push(s, cb, u, c);
void loader_push_async(loader_t *self, loader_cb cb, void *usrptr, c_t *c);
void _loader_push(loader_t *self, loader_cb cb, void *usrptr, c_t *c);
void _loader_push_wait(loader_t *self, loader_cb cb, void *usrptr, c_t *c);
void loader_push_wait(loader_t *self, loader_cb cb, void *usrptr, c_t *c);
#define loader_push_wait(s, cb, u, c) _loader_push_wait(s, cb, u, c);
int loader_update(loader_t *self);
void loader_wait(loader_t *self);
void loader_destroy(loader_t *self);

#endif /* !LOADER_H */
