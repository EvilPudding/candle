#ifndef ENTITY_H
#define ENTITY_H

#include <utils/macros.h>
struct c;
struct ct;

typedef uint64_t entity_t;

typedef int(*filter_cb)(struct c *self, struct c *accepted, void *data, void *output);
#define entity_null ((entity_t)0)

extern __thread entity_t _g_creating[32];
extern __thread int _g_creating_num;

#define entity_new(...) \
	(_entity_new_pre(),_entity_new_post((void*[]){NULL, __VA_ARGS__}))

void _entity_new_pre(void);
entity_t _entity_new_post(void *components[]);
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

static inline unsigned int entity_uid(entity_t self)
{
	struct { unsigned int pos, uid; } *separate = (void*)&self;
	return separate->uid;
}

static inline unsigned int entity_pos(entity_t self)
{
	struct { unsigned int pos, unique_id; } *separate = (void*)&self;
	return separate->pos;
}

#endif /* !ENTITY_H */
