#ifndef ENTITY_H
#define ENTITY_H

#include <utils/macros.h>
typedef struct c_t c_t;
typedef struct ct_t ct_t;

typedef unsigned long entity_t;

typedef int(*filter_cb)(c_t *self, c_t *accepted, void *data);
#define entity_null ((entity_t)0)

extern __thread entity_t _g_creating[32];
extern __thread int _g_creating_num;

#define entity_new(...) \
	(_entity_new_pre(),_entity_new(0, ##__VA_ARGS__))

void _entity_new_pre(void);
entity_t _entity_new(int comp_num, ...);
int entity_signal_same_TOPLEVEL(entity_t self, unsigned int signal, void *data);
int entity_signal_TOPLEVEL(entity_t self, unsigned int signal, void *data);
int component_signal_TOPLEVEL(c_t *comp, ct_t *ct, unsigned int signal, void *data);

int entity_signal_same(entity_t self, unsigned int signal, void *data);

int entity_signal(entity_t self, unsigned int signal, void *data);
int component_signal(c_t *comp, ct_t *ct, unsigned int signal, void *data);

void entity_filter(entity_t self, unsigned int signal, void *data,
		filter_cb cb, c_t *c_caller, void *cb_data);

void _entity_add_pre(entity_t self);
void _entity_add_post(entity_t self, c_t *comp);
#define entity_add_component(self, comp) \
	(_entity_add_pre(self), _entity_add_post(self, (c_t*)comp))

void entity_destroy(entity_t self);


#endif /* !ENTITY_H */
