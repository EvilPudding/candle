#ifndef ECM_H
#define ECM_H

#include <assert.h>
#include <stdint.h>
#include <limits.h>
#include "utils/material.h"
#include "utils/texture.h"
#include "utils/mesh.h"
#include "entity.h"
#include "utils/vector.h"
#include "utils/khash.h"
#include "utils/macros.h"

typedef struct ecm_t ecm_t;

typedef struct c_t c_t;

typedef void(*init_cb)(c_t *self);
typedef void(*destroy_cb)(c_t *self);
typedef int32_t(*signal_cb)(c_t *self, void *data, void *output);
typedef void(*c_reg_cb)(void);

struct set_var
{
	char ident[64];
	float value;
};

#define IDENT_NULL UINT_MAX
#define DEF_CASTER(ct, cn, nc_t) \
	static inline nc_t *cn(void *entity)\
{ return (nc_t*)ct_get(ecm_get(ref(ct)), entity); }

#define CONSTR_BEFORE_REG 102
#define CONSTR_REG 103
#define CONSTR_AFTER_REG 104
/* #define REG() \ */
	/* static void _register(void); \ */
	/* __attribute__((constructor (CONSTR_REG))) static void add_reg(void) \ */
	/* { ecm_add_reg(_register); } \ */
	/* static void _register(void) */
#define REG() \
	__attribute__((constructor (CONSTR_REG))) static void _register(void)

#define ecm_foreach_c(vvar, code) { khint_t __i;		\
	for (__i = kh_begin(g_ecm->cs); __i != kh_end(g_ecm->cs); ++__i) {		\
		if (!kh_exist(g_ecm->cs,__i)) continue;						\
		(vvar) = &kh_val(g_ecm->cs,__i);								\
		code;												\
	} }

#define ecm_foreach_ct(vvar, code) { khint_t __i;		\
	for (__i = kh_begin(g_ecm->cts); __i != kh_end(g_ecm->cts); ++__i) {		\
		if (!kh_exist(g_ecm->cts,__i)) continue;						\
		(vvar) = &kh_val(g_ecm->cts,__i);								\
		code;												\
	} }

#define CONTINUE			0x0000
#define STOP				0x0001
#define RENEW				0x0010

#define WORLD				0x0000
#define ENTITY				0x0100

/* #define RENDER_THREAD 0x02 */

typedef struct
{
	uint32_t signal;
	signal_cb cb;
	int32_t flags;
	uint32_t target;
	int32_t priority;
} listener_t;

typedef struct
{
	uint32_t ct;
	int32_t is_interaction;
} dep_t;

struct comp_page
{
	char *components;
	uint32_t components_size;
};

KHASH_MAP_INIT_INT(c, c_t*)

typedef struct ct_t
{
	char name[32];

	init_cb init;
	destroy_cb destroy;

	uint32_t id;
	uint32_t size;

	khash_t(c) *cs;
	/* struct comp_page *pages; */
	/* uint32_t pages_size; */

	dep_t *depends;
	uint32_t depends_size;

	int32_t is_interaction;

} ct_t;

typedef struct
{
	uint32_t size;

	vector_t *listener_types;
	vector_t *listener_comps;

} signal_t;

KHASH_MAP_INIT_INT(sig, signal_t)
KHASH_MAP_INIT_INT(ct, ct_t)

typedef struct
{
	uint32_t next_free;
	uint32_t uid;
} entity_info_t;

typedef struct ecm_t
{
	entity_info_t *entities_info;
	uint32_t entities_info_size;
	uint32_t entity_uid_counter;

	khash_t(ct) *cts;
	khash_t(c) *cs;

	khash_t(sig) *signals;

	uint32_t global;

	int32_t regs_size;
	c_reg_cb *regs;

	int32_t dirty;
	int32_t safe;
} ecm_t; /* Entity Component System */

typedef struct c_t
{
	entity_t entity;
	uint32_t comp_type;
	uint32_t ghost;
} c_t;

#define c_entity(c) (((c_t*)c)->entity)

#define _type(a, b) __builtin_types_compatible_p(typeof(a), b)
#define _if(c, a, b) __builtin_choose_expr(c, a, b)

/* static inline c_t *ct_get_at(ct_t *self, uint32_t page, uint32_t i) */
/* { */
	/* return (c_t*)&(self->pages[page].components[i * self->size]); */
/* } */

signal_t *ecm_get_signal(uint32_t signal);
void _signal_init(uint32_t id, uint32_t size);
#define signal_init(id, size) \
	(_signal_init(id, size))

void _ct_listener(ct_t *self, int32_t flags, int32_t priority, uint32_t signal, signal_cb cb);
#define ct_listener(self, flags, priority, signal, cb) \
	(_ct_listener(self, flags, priority, signal, (signal_cb)cb))

listener_t *ct_get_listener(ct_t *self, uint32_t signal);

/* void ct_register_callback(ct_t *self, uint32_t callback, void *cb); */

c_t *ct_add(ct_t *self, entity_t entity);

extern ecm_t *g_ecm;
void ecm_init(void);
entity_t ecm_new_entity(void);
void ecm_add_reg(c_reg_cb reg);
void ecm_register_all(void);
void ecm_clean(int32_t force);
void ecm_destroy_all(void);

void ecm_add_entity(entity_t *entity);
/* uint32_t ecm_register_system(ecm_t *self, void *system); */

#define ct_new(name, size, init, destroy, ...) \
	_ct_new(name, ref(name), size, (init_cb)init, (destroy_cb)destroy, \
			__VA_ARGS__)
ct_t *_ct_new(const char *name, uint32_t hash, uint32_t size, init_cb init,
		destroy_cb destroy, int32_t depend_size, ...);

void ct_add_dependency(ct_t *dep, uint32_t target);
void ct_add_interaction(ct_t *dep, uint32_t target);


static inline ct_t *ecm_get(uint32_t comp_type)
{
	khiter_t k = kh_get(ct, g_ecm->cts, comp_type);
	if(k == kh_end(g_ecm->cts)) return NULL;
	return &kh_value(g_ecm->cts, k);
}
/* void ecm_generate_hashes(ecm_t *self); */

#define component_new_from_ref(comp_type) _component_new(comp_type)
#define component_new(comp_type) _component_new(ref(comp_type))
void *_component_new(uint32_t comp_type);

static inline uint32_t entity_exists(entity_t self)
{
	if(self == entity_null) return 0;
	entity_info_t info = g_ecm->entities_info[entity_pos(self)];
	return info.uid == entity_uid(self);
}

static inline c_t *ct_get_nth(ct_t *self, int32_t n)
{
	khiter_t k;
	for(k = kh_begin(self->cs); k != kh_end(self->cs); ++k)
	{
		if(!kh_exist(self->cs, k)) continue;
		if(n == 0)
		{
			return kh_value(self->cs, k);
		}
		else
		{
			n--;
		}
	}
	return NULL;
}

static inline c_t *ct_get(ct_t *self, entity_t *entity)
{
	if(!self) return NULL;
	if(!entity_exists(*entity)) return NULL;
	khiter_t k = kh_get(c, self->cs, entity_uid(*entity));
	if(k == kh_end(self->cs)) return NULL;
	return kh_value(self->cs, k);
}


/* TODO maybe this shouldn't be here */
static inline uvec2_t entity_to_uvec2(entity_t self)
{
	int32_t pos = entity_pos(self);

	union {
		uint32_t i;
		struct{
			uint8_t r, g, b, a;
		};
	} convert = {.i = pos};

	return uvec2(convert.r, convert.g);
}
static inline vec2_t entity_to_vec2(entity_t self)
{
	uvec2_t v = entity_to_uvec2(self);
	return vec2((float)v.x / 255.0f, (float)v.y / 255.0f);
}

#define SYS ((entity_t){0x0000000100000001ul})

static inline int emit(uint32_t sig, void *input, void *output)
{
	return entity_signal(entity_null, sig, input, output);
}

/* builtin signals */

#endif /* !ECM_H */
