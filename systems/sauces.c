#include "sauces.h"
#include "../candle.h"
#include "editmode.h"
#include <utils/file.h>
#include <string.h>
#include <stdlib.h>
#include <third_party/tinydir/tinydir.h>

#include <components/model.h>
#include <components/timeline.h>
#include <components/bone.h>
#include <components/skin.h>
#include <components/light.h>
#include <components/node.h>
#include <components/name.h>

/* #include <assimp/cimport.h> */
/* #include <assimp/scene.h> */
/* #include <assimp/metadata.h> */
/* #include <assimp/postprocess.h> */

/* static void load_node_children(entity_t entity, const struct aiScene *scene, */
/* 		const struct aiNode *anode, int depth); */
/* static void load_comp_children( */
/* 		entity_t entity, */
/* 		const struct aiScene *scene, */
/* 		const struct aiNode *anode, */
/* 		c_node_t *root */
/* ); */
static sauces_loader_cb c_sauces_get_loader(c_sauces_t *self, uint32_t ext);

c_sauces_t *c_sauces_new()
{
	c_sauces_t *self = component_new(ct_sauces);
	self->sauces = kh_init(res);
	self->generic = kh_init(res);
	self->loaders = kh_init(loa);


	return self;
}

resource_t *c_sauces_get_sauce(c_sauces_t *self, const resource_handle_t *handle)
{
	if(handle->ext != ~0)
	{
		khiter_t k = kh_get(res, self->sauces, handle->name);
		if(k != kh_end(self->sauces))
		{
			return kh_value(self->sauces, k);
		}
	}
	else
	{
		khiter_t k = kh_get(res, self->generic, handle->name);
		if(k != kh_end(self->generic))
		{
			return kh_value(self->generic, k);
		}
	}

	return NULL;
}

static sauces_loader_cb c_sauces_get_loader(c_sauces_t *self, uint32_t ext)
{
	khiter_t k = kh_get(loa, self->loaders, ext);
	if(k == kh_end(self->loaders))
	{
		return NULL;
	}
	return kh_value(self->loaders, k);
}

void c_sauces_loader(c_sauces_t *self, uint32_t ref, sauces_loader_cb loader)
{
	int ret;
	khiter_t k = kh_get(loa, self->loaders, ref);

	if(k == kh_end(self->loaders))
	{
		k = kh_put(loa, self->loaders, ref, &ret);
	}
	kh_value(self->loaders, k) = loader;
}

void c_sauces_register(c_sauces_t *self, const char *name, const char *path, void *data)
{
	khiter_t k;
	uint32_t key;
	int ret;
	char buffer[64];
	char *dot;
	sauces_loader_cb cb = NULL;
	resource_t *sauce;

	strncpy(buffer, name, sizeof(buffer) - 1);
	dot = strrchr(buffer, '.');

	sauce = calloc(1, sizeof(*sauce));
	strncpy(sauce->name, name, sizeof(sauce->name) - 1);
	if(path)
	{
		strncpy(sauce->path, path, sizeof(sauce->path) - 1);
	}
	sauce->data = data;

	if(dot)
	{
		key = ref(buffer);
		k = kh_get(res, self->sauces, key);
		if(k != kh_end(self->sauces))
		{
			printf("Name collision! %s"
					" Resources should be uniquely named.\n", name);
			return;
		}
		/* printf("Registering name %s\n", name); */
		k = kh_put(res, self->sauces, key, &ret);
		kh_value(self->sauces, k) = sauce;

		*dot = '\0';

		cb = c_sauces_get_loader(self, ref(dot + 1));
	}


	if(cb)
	{
		key = ref(buffer);

		k = kh_put(res, self->generic, key, &ret);
		kh_value(self->generic, k) = sauce;
	}
}

void *c_sauces_get_data(c_sauces_t *self, resource_handle_t *handle)
{
	char *dot;
	sauces_loader_cb cb;
	resource_t *sauce = c_sauces_get_sauce(self, handle);

	if(!sauce) return NULL;
	if(sauce->data) return sauce->data;

	dot = strrchr(sauce->path, '.');
	if(!dot || dot[1] == '\0') return NULL;

	if (handle->ext == ~0) handle->ext = ref(dot + 1);
	
	cb = c_sauces_get_loader(self, handle->ext);
	if(!cb) return NULL;
	sauce->data = cb(sauce->path, sauce->name, handle->ext);
	return sauce->data;
}

resource_handle_t sauce_handle(const char *name)
{
	char *requested_dot;
	char buffer[64];
	resource_handle_t handle;

	strncpy(buffer, name, sizeof(buffer) - 1);
	to_lower_case(buffer);
	requested_dot = strrchr(buffer, '.');

	handle.name = ref(buffer);
	handle.ext = ~0;
	if (requested_dot)
	{
		*requested_dot = '\0';
		handle.ext = ref(requested_dot + 1);
	}
	return handle;
}

void *c_sauces_get(c_sauces_t *self, const char *name)
{
	resource_handle_t handle = sauce_handle(name);
	return c_sauces_get_data(self, &handle);
}

int c_sauces_index_dir(c_sauces_t *self, const char *dir_name)
{
	tinydir_dir dir;
	if (tinydir_open(&dir, dir_name) == -1) return false;

	while (dir.has_next)
	{
		tinydir_file file;
		char *dot;
		char path[512];
		char buffer[64];

		if (tinydir_readfile(&dir, &file) == -1)
		{
			perror("Error getting file");
			goto bail;
		}

		if(file.name[0] == '.') goto next;;
		strncpy(path, dir_name, sizeof(path) - 1);
		path_join(path, sizeof(path), file.name);

		if (file.is_dir)
		{
			c_sauces_index_dir(self, path);
			goto next;;
		}

		strcpy(buffer, file.name);
		to_lower_case(buffer);

		dot = strrchr(buffer, '.');
		if(!dot || dot[1] == '\0') goto next;;
		c_sauces_register(self, buffer, path, NULL);

next:
		if (tinydir_next(&dir) == -1)
		{
			perror("Error getting next file");
			goto bail;
		}
	}

bail:
	tinydir_close(&dir);
	return true;
}


int c_sauces_component_menu(c_sauces_t *self, void *ctx)
{
	/* if(nk_tree_push(ctx, NK_TREE_TAB, "Textures", NK_MINIMIZED)) */
	/* { */
	/* 	khiter_t i; */
	/* 	for(i = kh_begin(self->textures); i != kh_end(self->textures); ++i) */
	/* 	{ */
	/* 		if(!kh_exist(self->textures, i)) continue; */
	/* 		resource_t *sauce = &kh_value(self->textures, i); */
	/* 		if(sauce->data) */
	/* 		{ */
	/* 			if(nk_button_label(ctx, sauce->name)) */
	/* 			{ */
	/* 				c_editmode_open_texture(c_editmode(self), sauce->data); */
	/* 			} */
	/* 		} */
	/* 	} */
	/* 	nk_tree_pop(ctx); */
	/* } */
	return 1;
}


void ct_sauces(ct_t *self)
{
	ct_init(self, "sauces", sizeof(c_sauces_t));
	ct_listener(self, WORLD, 0, ref("component_menu"), c_sauces_component_menu);
}
