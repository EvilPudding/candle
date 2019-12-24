#ifndef RESAUCES_H
#define RESAUCES_H

#include <ecs/ecm.h>

typedef void*(*sauces_loader_cb)(const char *path, const char *name, uint32_t ext);

struct load_signal
{
	const char *filename;
	float scale;
};

typedef struct
{
	uint32_t name;
	uint32_t ext;
} resource_handle_t;

typedef struct
{
	char name[64];
	char path[256];
	void *data;
} resource_t;

KHASH_MAP_INIT_INT(res, resource_t*)
KHASH_MAP_INIT_INT(loa, sauces_loader_cb)

typedef struct c_sauces
{
	c_t super;

	khash_t(loa) *loaders;
	khash_t(res) *sauces;
	khash_t(res) *generic;
} c_sauces_t;

void ct_sauces(ct_t *self);
DEF_CASTER(ct_sauces, c_sauces, c_sauces_t)

c_sauces_t *c_sauces_new(void);
void c_sauces_register(c_sauces_t *self, const char *name, const char *path,
		void *data);

void c_sauces_loader(c_sauces_t *self, uint32_t ref, sauces_loader_cb loader);
void *c_sauces_get(c_sauces_t *self, const char *name);
int c_sauces_index_dir(c_sauces_t *self, const char *dir_name);
resource_t *c_sauces_get_sauce(c_sauces_t *self, const resource_handle_t *handle);
void *c_sauces_get_data(c_sauces_t *self, resource_handle_t *handle);
resource_handle_t sauce_handle(const char *name);

#define sauces_loader(ref, loader) (c_sauces_loader(c_sauces(&SYS), ref, loader))
#define sauces_register(name, path, data) (c_sauces_register(c_sauces(&SYS), \
			name, path, data))
#define sauces(name) (c_sauces_get(c_sauces(&SYS), name))

#endif /* !RESAUCES_H */
