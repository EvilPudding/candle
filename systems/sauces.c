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

void to_lower_case(char *str);

c_sauces_t *c_sauces_new()
{
	c_sauces_t *self = component_new("sauces");
	self->materials = kh_init(res);
	self->meshes = kh_init(res);
	self->textures = kh_init(res);

	int ret;
	khiter_t k = kh_put(res, self->materials, ref("_default"), &ret);
	resource_t *sauce = &kh_value(self->materials, k);
	sauce->data = mat_new("_default");

	return self;
}

mat_t *c_sauces_mat_get(c_sauces_t *self, const char *name)
{
	resource_t *sauce = c_sauces_get(self, self->materials, name);
	if(!sauce) return NULL;
	if(sauce->data) return sauce->data;
	mat_t *mat = mat_from_file(sauce->path);
	sauce->data = mat;
	return mat;
}

int load_mesh(mesh_t *mesh)
{
	char buffer[256];
	strcpy(buffer, mesh->name);
	printf("loading %s\n", mesh->name);
	mesh_load(mesh, buffer);
	return 1;
}

mesh_t *c_sauces_mesh_get(c_sauces_t *self, const char *name)
{
	resource_t *sauce = c_sauces_get(self, self->meshes, name);
	if(!sauce) return NULL;
	if(sauce->data) return sauce->data;
	mesh_t *mesh = mesh_new();
	sauce->data = mesh;
	strcpy(mesh->name, sauce->path);

	SDL_CreateThread((int(*)(void*))load_mesh, "load_mesh", mesh);
	/* load_mesh(mesh); */

	return mesh;
}

static inline mat4_t mat4_from_ai(const struct aiMatrix4x4 m)
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
	return r;
}

void to_lower_case(char *str)
{
	char c;
	while((c = *str))
	{
		if(c >= 'A' && c <= 'Z')
		{
			*str = c + 'a' - 'A';
		}
		str++;
	}
}

char *filter_sauce_name(char *path_name)
{
	char *slash = strrchr(path_name, '/')?:strrchr(path_name, '\\');
	if(slash) path_name = slash + 1;
	char *dot = strrchr(path_name, '.');
	if(dot) *dot = '\0';
	to_lower_case(path_name);
	return path_name;
}

void load_node(entity_t entity, const struct aiScene *scene,
		const struct aiNode *anode, float scale)
{
	int i;
	c_node_t *node = c_node(&entity);
	c_spacial_t *spacial = c_spacial(&entity);

	c_spacial_set_model(spacial, mat4_from_ai(anode->mTransformation));

	for(i = 0; i < anode->mNumMeshes; i++)
	{
		const struct aiMesh *mesh = scene->mMeshes[anode->mMeshes[i]];
		c_model_t *mc = c_model(&entity);
		if(!mc)
		{
			entity_add_component(entity, c_model_new(mesh_new(),
						sauces_mat("_default"), 1, 1));

			mc = c_model(&entity);
		}

		mesh_load_scene(mc->mesh, mesh);

		if(mesh->mMaterialIndex < scene->mNumMaterials)
		{
			char buffer[512] = "unnamed";
			const struct aiMaterial *mat =
				scene->mMaterials[mesh->mMaterialIndex];
			struct aiString name;

			struct aiString path;
			if(aiGetMaterialString(mat, AI_MATKEY_NAME, &name) ==
					aiReturn_SUCCESS)
			{
				strncpy(buffer, name.data, sizeof(buffer));
				to_lower_case(buffer);
			}
			mat_t *material = sauces_mat(buffer);
			if(!material)
			{
				material = mat_new(buffer);

				enum aiTextureMapping mapping;
				unsigned int uvi = 0;
				enum aiTextureOp op;
				float blend;
				enum aiTextureMapMode mode;
				unsigned int flags;

				if(aiGetMaterialTexture( mat, aiTextureType_DIFFUSE, 0, &path,
							&mapping, &uvi, &blend, &op, &mode, &flags) ==
						aiReturn_SUCCESS)
				{
					strncpy(buffer, path.data, sizeof(buffer));
					char *fname = filter_sauce_name(buffer);
					texture_t *texture = sauces_tex(fname);
					printf("%s\n", fname);
					if(texture)
					{
						material->albedo.texture = texture;
						material->albedo.texture_blend = 1.0f - blend;
						material->albedo.texture_scale = 1.0f;
					}
				}

				if(aiGetMaterialTexture( mat, aiTextureType_SHININESS, 0, &path,
							&mapping, &uvi, &blend, &op, &mode, &flags) ==
						aiReturn_SUCCESS)
				{
					strncpy(buffer, path.data, sizeof(buffer));
					char *fname = filter_sauce_name(buffer);
					texture_t *texture = sauces_tex(fname);
					printf("%s\n", fname);
					if(texture)
					{
						material->roughness.texture = texture;
						material->roughness.texture_blend = 1.0f - blend;
						material->roughness.texture_scale = 1.0f;
					}
				}

				vec4_t color;
				unsigned int max;
				if (AI_SUCCESS == aiGetMaterialFloatArray(mat,
							AI_MATKEY_COLOR_DIFFUSE, (ai_real*)&color, &max))
				{
					material->albedo.color = color;
				}

				int j; for(j = 0; j < mat->mNumProperties; j++) { printf("%s\n", mat->mProperties[j]->mKey.data); }
			}
			mc->layers[0].mat = material;
		}
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
	resource_t *sauce = c_sauces_get(self, self->meshes, name);

	entity_t result;
	const struct aiScene *scene = aiImportFile(sauce->path,
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
				entity_add_component(node, c_light_new(40.0f, color,
							256));
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

int load_tex(texture_t *texture)
{
	char buffer[256];
	strcpy(buffer, texture->name);
	texture_load(texture, buffer);
	return 1;
}

resource_t *c_sauces_get(c_sauces_t *self, khash_t(res) *hash, const char *name)
{
	char buffer[64];
	strncpy(buffer, name, sizeof(buffer));
	to_lower_case(buffer);
	khiter_t k = kh_get(res, hash, ref(buffer));
	if(k == kh_end(hash))
	{
		printf("no indexed sauce named %s\n", name);
		return NULL;
	}
	return &kh_value(hash, k);
}

texture_t *c_sauces_texture_get(c_sauces_t *self, const char *name)
{
	resource_t *sauce = c_sauces_get(self, self->textures, name);
	if(!sauce) return NULL;
	if(sauce->data) return sauce->data;

	texture_t *texture;

	texture = texture_new_2D(0, 0, TEX_INTERPOLATE);
	strcpy(texture->name, sauce->path);

	SDL_CreateThread((int(*)(void*))load_tex, "load_tex", texture);

	sauce->data = texture;
	return texture;
}

#define PNG 4215392736
#define JPG 492009405
#define TGA 519450434
#define OBJ 1199244993
#define DAE 1928915418
#define FBX 804230617
#define MAT 4225884277

int c_sauces_index_dir(c_sauces_t *self, const char *dir_name)
{
	DIR *dir = opendir(dir_name);
	if(dir == NULL) return 0;

	struct dirent *ent;
	while((ent = readdir(dir)) != NULL)
	{
		khash_t(res) *hash;
		char path[512];
		char name[64];

		if(ent->d_name[0] == '.') continue;

		strncpy(path, dir_name, sizeof(path));
		path_join(path, sizeof(path), ent->d_name);
		if(c_sauces_index_dir(self, path)) continue;

		strcpy(name, ent->d_name);
		to_lower_case(name);

		char *dot = strrchr(name, '.');
		if(!dot || dot[1] == '\0')
		{
			continue;
		}
		else
		{
			*dot = '\0';
		}

		uint ext = ref(dot + 1);
		switch(ext)
		{
			case TGA:
			case JPG:
			case PNG:
				hash = self->textures;
				break;
			case MAT:
				hash = self->materials;
				break;
			case OBJ:
			case FBX:
			case DAE:
				hash = self->meshes;
				break;
			default:
				printf("Unknown extension %s.%s %u\n", name, dot + 1, ref(dot+1));
				continue;
		}
		printf("%s\n", name);

		uint key = ref(name);
		khiter_t k = kh_get(res, hash, key);
		if(k != kh_end(hash))
		{
			printf("Name collision! %s"
					" Resources should be uniquely named.\n", ent->d_name);
			continue;
		}
		int ret;
		k = kh_put(res, hash, key, &ret);
		resource_t *sauce = &kh_value(hash, k);
		strncpy(sauce->path, path, sizeof(sauce->path));
		strncpy(sauce->name, name, sizeof(sauce->name));
		sauce->data = NULL;


	}
	closedir(dir);

	return 1;
}


int c_sauces_component_menu(c_sauces_t *self, void *ctx)
{
	if(nk_tree_push(ctx, NK_TREE_TAB, "Textures", NK_MINIMIZED))
	{
		khiter_t i;
		for(i = kh_begin(self->textures); i != kh_end(self->textures); ++i)
		{
			if(!kh_exist(self->textures, i)) continue;
			resource_t *sauce = &kh_value(self->textures, i);
			if(sauce->data)
			{
				if(nk_button_label(ctx, sauce->name))
				{
					c_editmode_open_texture(c_editmode(self), sauce->data);
				}
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
