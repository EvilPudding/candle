#ifndef ECM_H
#define ECM_H

#include <assert.h>
#include <limits.h>
#include "entity.h"
#include "../utils/vector.h"
#include "../utils/khash.h"
#include "../utils/macros.h"
#include "../utils/mafs.h"

struct ecm;

struct c;

typedef void(*init_cb)(struct c *self);
typedef void(*destroy_cb)(struct c *self);
typedef int32_t(*signal_cb)(struct c *self, void *data, void *output);
typedef void(*c_reg_cb)(void);

struct set_var
{
	char ident[64];
	float value;
};

#define IDENT_NULL UINT_MAX
#define DEF_CASTER(ct, cn, nc_t) \
	static nc_t *cn(void *entity)\
{ return (nc_t*)ct_get(ecm_get(ct), entity); }

#define ecm_foreach_c(vvar, code) { khint_t __i;		\
	for (__i = kh_begin(g_ecm->cs); __i != kh_end(g_ecm->cs); ++__i) {		\
		if (!kh_exist(g_ecm->cs,__i)) continue;						\
		(vvar) = &kh_val(g_ecm->cs,__i);								\
		code;												\
	} }

#define ecm_foreach_ct(vvar, code) { khint_t __i;		\
	for (__i = kh_begin(g_ecm->cts); __i != kh_end(g_ecm->cts); ++__i) {		\
		if (!kh_exist(g_ecm->cts,__i)) continue;						\
		(vvar) = kh_val(g_ecm->cts,__i);								\
		code;												\
	} }

#define CONTINUE			0x0000
#define STOP				0x0001
#define RENEW				0x0010

#define WORLD				0x0000
#define ENTITY				0x0100

/* #define RENDER_THREAD 0x02 */

typedef void(*ct_id_t)(struct ct *self);

typedef struct
{
	int32_t priority;
	uint32_t signal;
	signal_cb cb;
	int32_t flags;
	ct_id_t target;
} listener_t;

typedef struct
{
	ct_id_t ct;
	int32_t is_interaction;
} dep_t;

struct comp_page
{
	char *components;
	uint32_t components_size;
};

typedef struct c
{
	entity_t entity;
	ct_id_t comp_type;
	uint32_t ghost;
} c_t;

KHASH_MAP_INIT_INT(c, c_t*)

typedef struct ct
{
	char name[32];

	init_cb init;
	destroy_cb destroy;

	ct_id_t id;
	uint32_t size;

	khash_t(c) *cs;
	/* struct comp_page *pages; */
	/* uint32_t pages_size; */

	dep_t *depends;
	uint32_t depends_size;

	bool_t is_interaction;
	bool_t ready;
} ct_t;

typedef struct vector_t signal_t;

KHASH_MAP_INIT_INT(sig, vector_t*)
KHASH_MAP_INIT_INT64(ct, ct_t*)

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
	void *mtx;
} ecm_t; /* Entity Component System */

#define c_entity(c) (((c_t*)c)->entity)

#define _type(a, b) __builtin_types_compatible_p(typeof(a), b)
#define _if(c, a, b) __builtin_choose_expr(c, a, b)

/* static inline c_t *ct_get_at(ct_t *self, uint32_t page, uint32_t i) */
/* { */
	/* return (c_t*)&(self->pages[page].components[i * self->size]); */
/* } */

vector_t *ecm_get_listeners(uint32_t signal);

void _ct_add_listener(ct_t *self, int32_t flags, int32_t priority, uint32_t signal, signal_cb cb);
#define ct_add_listener(self, flags, priority, signal, cb) \
	(_ct_add_listener(self, flags, priority, signal, (signal_cb)cb))

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

void ct_init(ct_t *self, const char *name, uint32_t size);
void ct_add_dependency(ct_t *dep, ct_id_t target);
void ct_add_interaction(ct_t *dep, ct_id_t target);
void ct_set_init(ct_t *dep, init_cb cb);
void ct_set_destroy(ct_t *dep, destroy_cb cb);


ct_t *ecm_get(ct_id_t id);
/* void ecm_generate_hashes(ecm_t *self); */

void *component_new(void(*ct_reg)(ct_t*));

static uint32_t entity_exists(entity_t self)
{
	entity_info_t info;
	if(self == entity_null) return 0;
	info = g_ecm->entities_info[entity_pos(self)];
	return info.uid == entity_uid(self);
}

static c_t *ct_get_nth(ct_t *self, int32_t n)
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

static c_t *ct_get(ct_t *self, entity_t *entity)
{
	khiter_t k;
	if(!self) return NULL;
	if(!entity_exists(*entity)) return NULL;
	k = kh_get(c, self->cs, entity_uid(*entity));
	if(k == kh_end(self->cs)) return NULL;
	return kh_value(self->cs, k);
}


/* TODO maybe this shouldn't be here */
static uvec2_t entity_to_uvec2(entity_t self)
{
	int32_t pos = entity_pos(self);

	union {
		uint32_t i;
		struct{
			uint8_t r, g, b, a;
		} _convert;
	} convert;
	convert.i = pos;

	return uvec2(convert._convert.r, convert._convert.g);
}
static vec2_t entity_to_vec2(entity_t self)
{
	uvec2_t v = entity_to_uvec2(self);
	return vec2((float)v.x / 255.0f, (float)v.y / 255.0f);
}

extern entity_t SYS;

static int emit(uint32_t sig, void *input, void *output)
{
	return entity_signal(entity_null, sig, input, output);
}

/* builtin signals */

#endif /* !ECM_H */
