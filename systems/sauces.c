#include "sauces.h"
#include "../candle.h"
#include "editmode.h"
#include <utils/file.h>
#include <string.h>
#include <stdlib.h>

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
static sauces_loader_cb c_sauces_get_loader(c_sauces_t *self, uint ext);

c_sauces_t *c_sauces_new()
{
	c_sauces_t *self = component_new("sauces");
	self->sauces = kh_init(res);
	self->generic = kh_init(res);
	self->loaders = kh_init(loa);


	return self;
}

resource_t *c_sauces_get_sauce(c_sauces_t *self, const char *name)
{
	char buffer[64];
	strncpy(buffer, name, sizeof(buffer));
	to_lower_case(buffer);
	char *dot = strrchr(buffer, '.');

	if(dot)
	{
		khiter_t k = kh_get(res, self->sauces, ref(buffer));
		if(k != kh_end(self->sauces))
		{
			return kh_value(self->sauces, k);
		}
		*dot = '\0';
	}

	{
		khiter_t k = kh_get(res, self->generic, ref(buffer));
		if(k != kh_end(self->sauces))
		{
			return kh_value(self->sauces, k);
		}

	}

	printf("no indexed sauce named %s\n", name);
	return NULL;
}

static sauces_loader_cb c_sauces_get_loader(c_sauces_t *self, uint ext)
{
	khiter_t k = kh_get(loa, self->loaders, ext);
	if(k == kh_end(self->loaders))
	{
		return NULL;
	}
	return kh_value(self->loaders, k);
}

void c_sauces_loader(c_sauces_t *self, uint ref, sauces_loader_cb loader)
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
	uint key;
	int ret;
	char buffer[64];
	strncpy(buffer, name, sizeof(buffer));
	char *dot = strrchr(buffer, '.');

	resource_t *sauce = calloc(1, sizeof(*sauce));
	strncpy(sauce->name, name, sizeof(sauce->name));
	if(path)
	{
		strncpy(sauce->path, path, sizeof(sauce->path));
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
		k = kh_put(res, self->sauces, key, &ret);
		kh_value(self->sauces, k) = sauce;

		*dot = '\0';
	}

	{
		key = ref(buffer);

		k = kh_put(res, self->generic, key, &ret);
		kh_value(self->generic, k) = sauce;
	}
}

void *c_sauces_get(c_sauces_t *self, const char *name)
{
	resource_t *sauce = c_sauces_get_sauce(self, name);
	if(!sauce) return NULL;
	if(sauce->data) return sauce->data;

	char *dot = strrchr(sauce->path, '.');
	if(!dot || dot[1] == '\0') return NULL;

	/* *dot = '\0'; */

	uint ext = ref(dot + 1);
	
	sauces_loader_cb cb = c_sauces_get_loader(self, ext);
	if(!cb) return NULL;
	sauce->data = cb(sauce->path, sauce->name, ext);
	return sauce->data;
}

/* #define PNG 4215392736 */
/* #define JPG 492009405 */
/* #define TGA 519450434 */

/* #define MAT 4225884277 */

int c_sauces_index_dir(c_sauces_t *self, const char *dir_name)
{
	DIR *dir = opendir(dir_name);
	if(dir == NULL) return 0;

	struct dirent *ent;
	while((ent = readdir(dir)) != NULL)
	{
		int ret;
		char path[512];
		char buffer[64];

		if(ent->d_name[0] == '.') continue;

		strncpy(path, dir_name, sizeof(path));
		path_join(path, sizeof(path), ent->d_name);
		if(c_sauces_index_dir(self, path)) continue;

		strcpy(buffer, ent->d_name);
		to_lower_case(buffer);

		char *dot = strrchr(buffer, '.');
		if(!dot || dot[1] == '\0') continue;
		/* *dot = '\0'; */

		if(!c_sauces_get_loader(self, ref(dot + 1))) continue;


		resource_t *sauce = calloc(1, sizeof(*sauce));
		strncpy(sauce->path, path, sizeof(sauce->path));
		strncpy(sauce->name, buffer, sizeof(sauce->name));

		c_sauces_register(self, buffer, path, NULL);

		/* if(dot) */
		/* { */
		/* 	key = ref(buffer); */
		/* 	printf("%s %s\n", buffer, path); */

		/* 	k = kh_get(res, self->sauces, key); */
		/* 	if(k != kh_end(self->sauces)) */
		/* 	{ */
		/* 		printf("Name collision! %s" */
		/* 				" Resources should be uniquely named.\n", ent->d_name); */
		/* 		continue; */
		/* 	} */
		/* 	k = kh_put(res, self->sauces, key, &ret); */
		/* 	kh_value(self->sauces, k) = sauce; */

		/* 	*dot = '\0'; */
		/* } */
		/* { */
		/* 	key = ref(buffer); */
		/* 	printf("%s %s\n", buffer, path); */

		/* 	k = kh_get(res, self->generic, key); */
		/* 	if(k == kh_end(self->generic)) */
		/* 	{ */
		/* 		k = kh_put(res, self->generic, key, &ret); */
		/* 		kh_value(self->generic, k) = sauce; */
		/* 	} */
		/* } */
	}
	closedir(dir);

	return 1;
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


REG()
{
	ct_t *ct = ct_new("sauces", sizeof(c_sauces_t), NULL, NULL, 0);

	ct_listener(ct, WORLD, sig("component_menu"), c_sauces_component_menu);
}
