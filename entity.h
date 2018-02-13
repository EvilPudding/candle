#ifndef ENTITY_H
#define ENTITY_H

typedef struct ecm_t ecm_t;
typedef struct c_t c_t;
typedef struct ct_t ct_t;

typedef struct entity_t
{
	long id;
	ecm_t *ecm;
} entity_t;

typedef int(*filter_cb)(c_t *self, c_t *accepted, void *data);

entity_t entity_new(ecm_t *ecm, int comp_num, ...);
int entity_signal_same(entity_t self, unsigned int signal, void *data);
int entity_signal(entity_t self, unsigned int signal, void *data);
int component_signal(c_t *comp, ct_t *ct, unsigned int signal, void *data);

void entity_filter(entity_t self, unsigned int signal, void *data,
		filter_cb cb, c_t *c_caller, void *cb_data);

void _entity_add_component(entity_t self, c_t *comp, int on_creation);
#define entity_add_component(self, comp) _entity_add_component(self, ((c_t*)(comp)), 0)

int entity_equal(entity_t self, entity_t other);

void entity_destroy(entity_t self);
static inline int entity_is_null(entity_t self) { return self.id == -1; }
static inline entity_t entity_null(void) { return (entity_t){.id = -1}; }


#endif /* !ENTITY_H */
