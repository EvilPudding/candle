#ifndef COMPONENT_H
#define COMPONENT_H

#include "material.h"
#include "texture.h"
#include "mesh.h"
#include "entity.h"

typedef struct ecm_t ecm_t;

typedef struct c_t c_t;

typedef void(*init_cb)(c_t *self);
typedef int(*signal_cb)(c_t *self, void *data);
typedef void(*c_reg_cb)(ecm_t *ecm);

/* TODO: find appropriate place */
typedef int(*before_draw_cb)(c_t *self);

#define WORLD 0x00
#define SAME_ENTITY 0x01

typedef struct
{
	unsigned long signal;
	signal_cb cb;
	int flags;
} listener_t;

typedef struct
{
	ecm_t *ecm;

	init_cb init;

	unsigned long id;
	unsigned long size;

	unsigned long *offsets;
	unsigned long offsets_size;

	char *components;
	unsigned long components_size;

	unsigned long *depends;
	unsigned long depends_size;

	listener_t *listeners;
	unsigned long listeners_size;

	/* void *system_info; */
	/* unsigned long system_info_size; */
} ct_t;

typedef struct
{
	unsigned long size;

	unsigned long *cts;
	unsigned long cts_size;

} signal_t;

typedef struct ecm_t
{
	int *entities_busy;
	unsigned long entities_busy_size;

	ct_t *cts;
	unsigned long cts_size;

	signal_t *signals;
	unsigned long signals_size;

	unsigned long global;

	entity_t none;
	entity_t common;
} ecm_t; /* Entity Component System */

typedef struct c_t
{
	unsigned long comp_type;
	entity_t entity;
} c_t;

#define c_entity(c) (((c_t*)c)->entity)
#define c_ecm(c) (((c_t*)c)->entity.ecm)

#define _type(a, b) __builtin_types_compatible_p(typeof(a), b)
#define _if(c, a, b) __builtin_choose_expr(c, a, b)

#define DEF_CASTER(ct, cn, nc_t) static inline nc_t *cn(const entity_t entity)\
{ return (nc_t*)ct_get(ecm_get(entity.ecm, ct), entity); } \

static inline c_t *ct_get_at(ct_t *self, unsigned long i)
{
	return (c_t*)&(self->components[i * self->size]);
}

static inline c_t *ct_get(ct_t *self, entity_t entity)
{
	if(entity.id >= self->offsets_size) return NULL;
	unsigned long offset = self->offsets[entity.id];
	if(offset == -1) return NULL;
	return (c_t*)&(self->components[offset]);
}

void ct_register_listener(ct_t *self, int flags,
		unsigned long signal, signal_cb cb);

listener_t *ct_get_listener(ct_t *self, unsigned long signal);

/* void ct_register_callback(ct_t *self, unsigned long callback, void *cb); */

void ct_add(ct_t *self, c_t *comp);

ecm_t *ecm_new(void);
entity_t ecm_new_entity(ecm_t *ecm);
unsigned long ecm_register_signal(ecm_t *ecm, unsigned long size);

void ecm_add_entity(ecm_t *self, entity_t *entity);
/* unsigned long ecm_register_system(ecm_t *self, void *system); */

ct_t *ecm_register(ecm_t *self, unsigned long *target,
		unsigned long size, init_cb init, int depend_size, ...);

void ct_add_dependency(ct_t *ct, unsigned long target);

static inline ct_t *ecm_get(ecm_t *self, unsigned long comp_type) {
	return &self->cts[comp_type]; }

c_t component_new(int comp_type);

/* builtin signals */
extern unsigned long entity_created;

#endif /* !COMPONENT_H */
