#ifndef ECM_H
#define ECM_H

#include <limits.h>
#include "material.h"
#include "texture.h"
#include "mesh.h"
#include "entity.h"
#include "vector.h"

#define _GNU_SOURCE
#include <search.h>

typedef struct ecm_t ecm_t;

typedef struct c_t c_t;

typedef void(*init_cb)(c_t *self);
typedef int(*signal_cb)(c_t *self, void *data);
typedef void(*c_reg_cb)(void);

/* TODO: find appropriate place */
typedef int(*before_draw_cb)(c_t *self);

#define WORLD 0x00
#define ENTITY 0x01
/* #define RENDER_THREAD 0x02 */

typedef struct
{
	uint signal;
	signal_cb cb;
	int flags;
	uint comp_type;
} listener_t;

typedef struct
{
	uint ct;
	int is_interaction;
} dep_t;

#define PAGE_SIZE 32

struct comp_page
{
	char *components;
	uint components_size;
};

typedef struct ct_t
{
	char name[32];

	init_cb init;

	uint id;
	uint size;

	struct {
		uint page;
		uint offset;
	} *offsets;
	uint offsets_size;

	struct comp_page *pages;
	uint pages_size;

	dep_t *depends;
	uint depends_size;

	listener_t *listeners;
	uint listeners_size;

	int is_interaction;

	/* struct hsearch_data *listeners_table; */

	/* void *system_info; */
	/* uint system_info_size; */
} ct_t;

typedef struct
{
	uint size;

	uint *cts;
	uint cts_size;
	listener_t *listeners;
	uint listeners_size;

	
} signal_t;

typedef struct ecm_t
{
	int *entities_busy;
	uint entities_busy_size;

	ct_t *cts;
	uint cts_size;

	signal_t *signals;
	uint signals_size;

	uint global;

} ecm_t; /* Entity Component System */

typedef struct c_t
{
	entity_t entity;
	uint comp_type;
} c_t;

#define c_entity(c) (((c_t*)c)->entity)

#define _type(a, b) __builtin_types_compatible_p(typeof(a), b)
#define _if(c, a, b) __builtin_choose_expr(c, a, b)

#define IDENT_NULL UINT_MAX

#define DEC_CT(var) uint var = IDENT_NULL
#define DEC_SIG(var) uint var = IDENT_NULL
#define DEF_SIG(var) extern uint var

#define DEF_CASTER(ct, cn, nc_t) extern uint ct; \
	static inline nc_t *cn(const void *entity)\
{ return ct==IDENT_NULL?NULL:(nc_t*)ct_get(ecm_get(ct), *(entity_t*)entity); } \

static inline c_t *ct_get_at(ct_t *self, uint page, uint i)
{
	return (c_t*)&(self->pages[page].components[i * self->size]);
}

static inline c_t *ct_get(ct_t *self, entity_t entity)
{
	if(entity >= self->offsets_size) return NULL;
	uint offset = self->offsets[entity].offset;
	uint page = self->offsets[entity].page;
	if(offset == -1) return NULL;
	return (c_t*)&(self->pages[page].components[offset]);
}

void _ct_listener(ct_t *self, int flags,
		uint signal, signal_cb cb);
#define ct_listener(self, flags, signal, cb) \
	(_ct_listener(self, flags, signal, (signal_cb)cb))

listener_t *ct_get_listener(ct_t *self, uint signal);

/* void ct_register_callback(ct_t *self, uint callback, void *cb); */

c_t *ct_add(ct_t *self, entity_t entity);

extern ecm_t *g_ecm;
void ecm_init(void);
entity_t ecm_new_entity(void);
void signal_init(uint *target, uint size);

void ecm_add_entity(entity_t *entity);
/* uint ecm_register_system(ecm_t *self, void *system); */

ct_t *ct_new(const char *name, uint *target, uint size,
		init_cb init, int depend_size, ...);

void ct_add_dependency(ct_t *ct, ct_t *dep);
void ct_add_interaction(ct_t *ct, ct_t *dep);

static inline ct_t *ecm_get(uint comp_type) {
	return &g_ecm->cts[comp_type]; }
/* void ecm_generate_hashes(ecm_t *self); */

void *component_new(int comp_type);

/* builtin signals */
extern uint entity_created;

#endif /* !ECM_H */
