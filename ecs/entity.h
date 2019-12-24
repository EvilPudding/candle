#ifndef ENTITY_H
#define ENTITY_H

#include <utils/macros.h>
struct c;
struct ct;

typedef uint64_t entity_t;

typedef int(*filter_cb)(struct c *self, struct c *accepted, void *data, void *output);
#define entity_null ((entity_t)0)

#define entity_new(components) \
	entity_new_pre();components;entity_new_post()

entity_t entity_new_pre(void);
void entity_new_post(void);
int entity_signal_same_TOPLEVEL(entity_t self, unsigned int signal, void *data, void *output);
int entity_signal_TOPLEVEL(entity_t self, unsigned int signal, void *data, void *output);
int component_signal_TOPLEVEL(struct c *comp, struct ct *ct, unsigned int signal, void *data, void *output);

int entity_signal_same(entity_t self, unsigned int signal, void *data, void *output);

int entity_signal(entity_t self, unsigned int signal, void *data, void *output);
int component_signal(struct c *comp, struct ct *ct, unsigned int signal, void *data, void *output);

void entity_filter(entity_t self, unsigned int signal, void *data, void *output,
		filter_cb cb, struct c *c_caller, void *cb_data);

void _entity_add_pre(entity_t self);
void _entity_add_post(entity_t self, struct c *comp);
#define entity_add_component(self, comp) \
	(_entity_add_pre(self), _entity_add_post(self, (struct c*)comp))

void entity_destroy(entity_t self);

static unsigned int entity_uid(entity_t self)
{
	struct { unsigned int pos, uid; } *separate = (void*)&self;
	return separate->uid;
}

static unsigned int entity_pos(entity_t self)
{
	struct { unsigned int pos, unique_id; } *separate = (void*)&self;
	return separate->pos;
}

#endif /* !ENTITY_H */
