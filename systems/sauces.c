#include "sauces.h"
#include "../candle.h"
#include "editmode.h"
#include <utils/file.h>
#include <string.h>
#include <stdlib.h>

#include <components/model.h>
#include <components/light.h>
#include <components/node.h>
#include <components/name.h>

#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

c_sauces_t *c_sauces_new()
{
	c_sauces_t *self = component_new("sauces");
	return self;
}

void c_sauces_mat_reg(c_sauces_t *self, const char *name, mat_t *mat)
{
	uint i = self->mats_size++;
	self->mats = realloc(self->mats,
			sizeof(*self->mats) * self->mats_size);
	self->mats[i] = mat;
	if(mat->name != name) strncpy(mat->name, name, sizeof(mat->name));
}

mat_t *c_sauces_mat_get(c_sauces_t *self, const char *name)
{
	mat_t *mat;
	uint i;
	for(i = 0; i < self->mats_size; i++)
	{
		mat = self->mats[i];
		if(!strcmp(mat->name, name)) return mat;
	}
	mat = mat_from_file(name);
	if(mat) c_sauces_mat_reg(self, name, mat);
	return mat;
}

void c_sauces_mesh_reg(c_sauces_t *self, const char *name, mesh_t *mesh)
{
	uint i = self->meshes_size++;
	self->meshes = realloc(self->meshes,
			sizeof(*self->meshes) * self->meshes_size);
	self->meshes[i] = mesh;
	strncpy(mesh->name, name, sizeof(mesh->name));
}

int load_mesh(mesh_t *mesh)
{
	char buffer[256];
	strcpy(buffer, mesh->name);
	mesh_load(mesh, buffer);
	return 1;
}

mesh_t *c_sauces_mesh_get(c_sauces_t *self, const char *name)
{
	mesh_t *mesh;
	uint i;
	for(i = 0; i < self->meshes_size; i++)
	{
		mesh = self->meshes[i];
		if(!strcmp(mesh->name, name)) return mesh;
	}
	mesh = mesh_new();
	strcpy(mesh->name, name);

	SDL_CreateThread((int(*)(void*))load_mesh, "load_mesh", mesh);
	/* load_mesh(mesh); */

	c_sauces_mesh_reg(self, name, mesh);
	return mesh;
}

static inline mat4_t mat4_from_ai(const struct aiMatrix4x4 m, float scale)
{
	mat4_t r;

	r._[0]._[0] = m.a1; r._[0]._[1] = m.a2;
	r._[0]._[2] = m.a3; r._[0]._[3] = m.a4;

	r._[1]._[0] = m.b1; r._[1]._[1] = m.b2;
	r._[1]._[2] = m.b3; r._[1]._[3] = m.b4;

	r._[2]._[0] = m.c1; r._[2]._[1] = m.c2;
	r._[2]._[2] = m.c3; r._[2]._[3] = m.c4;

	r._[3]._[0] = m.d1; r._[3]._[1] = m.d2;
	r._[3]._[2] = m.d3; r._[3]._[3] = m.d4;

	r = mat4_transpose(r);
	r = mat4_scale(r, scale);
	return r;
}

void load_node(entity_t entity, const struct aiScene *scene,
		const struct aiNode *anode, float scale)
{
	int i;
	c_node_t *node = c_node(&entity);
	c_spacial_t *spacial = c_spacial(&entity);

	c_spacial_set_model(spacial, mat4_from_ai(anode->mTransformation, scale));

	for(i = 0; i < anode->mNumMeshes; i++)
	{
		const struct aiMesh *mesh = scene->mMeshes[anode->mMeshes[i]];
		c_model_t *mc = c_model(&entity);
		if(!mc)
		{
			entity_add_component(entity, c_model_new(mesh_new(),
						mat_new("t"), 1, 1));

			mc = c_model(&entity);
		}

		mesh_load_scene(mc->mesh, mesh);
	}

	for(i = 0; i < anode->mNumChildren; i++)
	{
		const struct aiNode *cnode = anode->mChildren[i];
		const char *name = cnode->mName.data;
		entity_t n = c_node_get_by_name(node, ref(name));
		if(!n)
		{
			n = entity_new(c_name_new(name), c_node_new());

			/* mesh_t *space = sauces_mesh("cube.ply"); */
			/* entity_t m = entity_new(c_node_new(), */
			/* 		c_model_new(space, mat_new("cb"), 1)); */
			/* c_spacial_set_scale(c_spacial(&m), vec3(0.2)); */
			/* c_node_add(c_node(&n), 1, m); */
		}
		c_node_add(node, 1, n);
		load_node(n, scene, cnode, scale);
	}
}

entity_t c_sauces_model_get(c_sauces_t *self, const char *name, float scale)
{
	int i;
	char buffer[2048];
	snprintf(buffer, sizeof(buffer), "resauces/models/%s", name);

	entity_t result;
	const struct aiScene *scene = aiImportFile(buffer,
			/* aiProcess_CalcTangentSpace  		| */
			aiProcess_Triangulate			|
			/* aiProcess_GenSmoothNormals		| */
			aiProcess_JoinIdenticalVertices 	|
			aiProcess_SortByPType);
	if(!scene)
	{
		printf("failed to load %s\n", name);
		return entity_null;
	}

	result = entity_new(c_name_new(name),
			c_model_new(mesh_new(), mat_new("t"), 1, 1));
	c_model(&result)->mesh->transformation =
		mat4_scale_aniso(c_model(&result)->mesh->transformation, vec3(scale));

	load_node(result, scene, scene->mRootNode, scale);
	c_node_t *root = c_node(&result);

	for(i = 0; i < scene->mNumLights; i++)
	{
		const struct aiLight *light = scene->mLights[i];
		entity_t node = c_node_get_by_name(root, ref(light->mName.data));
		if(node)
		{
			c_light_t *lc = c_light(&node);
			if(!lc)
			{
				vec4_t color = vec4(
					light->mColorDiffuse.r,
					light->mColorDiffuse.g,
					light->mColorDiffuse.b,
					1.0f
				);
				entity_add_component(node, c_light_new(1.0f, 40.0f,
							color, 256));
			}

			/* load_light(lc, light); */
		}
		else
		{
			printf("%s not found for light\n", light->mName.data);
		}
	}

	aiReleaseImport(scene);


	return result;
}

texture_t *c_sauces_texture_get(c_sauces_t *self, const char *name)
{
	texture_t *texture;
	uint i;
	for(i = 0; i < self->textures_size; i++)
	{
		texture = self->textures[i];
		if(!strcmp(texture->name, name)) return texture;
	}
	texture = texture_from_file(name);
	c_sauces_texture_reg(self, name, texture);

	return texture;
}

void c_sauces_texture_reg(c_sauces_t *self, const char *name, texture_t *texture)
{
	if(!texture) return;
	uint i = self->textures_size++;
	self->textures = realloc(self->textures,
			sizeof(*self->textures) * self->textures_size);
	self->textures[i] = texture;
	strncpy(texture->name, name, sizeof(texture->name));
}

int c_sauces_get_mats_at(c_sauces_t *self, const char *dir_name)
{
	char dir_buffer[2048];
	strncpy(dir_buffer, g_mats_path, sizeof(dir_buffer));

	path_join(dir_buffer, sizeof(dir_buffer), path_relative(dir_name, g_mats_path));

	DIR *dir = opendir(dir_buffer);
	if(dir == NULL) return 0;

	struct dirent *ent;
	while((ent = readdir(dir)) != NULL)
	{
		char buffer[512];

		if(ent->d_name[0] == '.') continue;

		strncpy(buffer, dir_buffer, sizeof(buffer));
		path_join(buffer, sizeof(buffer), ent->d_name);

		char *ext = strrchr(ent->d_name, '.');
		if(ext && ext != ent->d_name && !strcmp(ext, ".mat"))
		{
			ext = strrchr(buffer, '.');
			*ext = '\0';
			mat_from_file(buffer);
			continue;
		}

		if(is_dir(buffer))
		{
			if(!mat_from_file(buffer))
			{
				c_sauces_get_mats_at(self, buffer);
			}
			continue;
		}

	}
	closedir(dir);

	return 1;
}

int c_sauces_component_menu(c_sauces_t *self, void *ctx)
{
	if(nk_tree_push(ctx, NK_TREE_TAB, "Textures", NK_MINIMIZED))
	{
		int i;
		for(i = 0; i < self->textures_size; i++)
		{
			if(nk_button_label(ctx, self->textures[i]->name))
			{
				c_editmode_open_texture(c_editmode(self), self->textures[i]);
			}
		}
		nk_tree_pop(ctx);
	}
	return 1;
}


REG()
{
	ct_t *ct = ct_new("sauces", sizeof(c_sauces_t), NULL, NULL, 0);

	ct_listener(ct, WORLD, sig("component_menu"), c_sauces_component_menu);
}
