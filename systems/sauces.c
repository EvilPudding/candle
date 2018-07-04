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

#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/metadata.h>
#include <assimp/postprocess.h>

void to_lower_case(char *str);
static void load_node_children(entity_t entity, const struct aiScene *scene,
		const struct aiNode *anode, int depth);
static void load_comp_children(
		entity_t entity,
		const struct aiScene *scene,
		const struct aiNode *anode,
		c_node_t *root,
		float scale
);

c_sauces_t *c_sauces_new()
{
	c_sauces_t *self = component_new("sauces");
	self->materials = kh_init(res);
	self->meshes = kh_init(res);
	self->textures = kh_init(res);

	c_sauces_register_mat(self, mat_new("_default"));

	return self;
}

void c_sauces_register_mat(c_sauces_t *self, mat_t *mat)
{
	int ret;
	khiter_t k = kh_put(res, self->materials, ref(mat->name), &ret);
	resource_t *sauce = &kh_value(self->materials, k);
	strncpy(sauce->name, mat->name, sizeof(sauce->name));
	sauce->data = mat;
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
	float *v = (float*)&m;
	r = *(mat4_t*)v;

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

void load_material(entity_t entity, const struct aiMesh *mesh,
		const struct aiScene *scene)
{
	if(mesh->mMaterialIndex >= scene->mNumMaterials) return;
	c_model_t *mc = c_model(&entity);
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
			if(texture)
			{
				material->roughness.texture = texture;
				material->roughness.texture_blend = 1.0f - blend;
				material->roughness.texture_scale = 1.0f;
			}
		}

		vec4_t color = vec4(0,0,0,1);
		if (AI_SUCCESS == aiGetMaterialColor(mat, AI_MATKEY_COLOR_DIFFUSE,
					(void*)&color))
		{
			material->albedo.color = color;
		}

		int j; for(j = 0; j < mat->mNumProperties; j++) { printf("%s\n", mat->mProperties[j]->mKey.data); }
		sauces_register_mat(material);
	}
	mc->layers[0].mat = material;
}

void load_comp(entity_t entity, const struct aiScene *scene,
		const struct aiNode *anode, c_node_t *root, float scale)
{
	int m;
	c_model_t *mc = c_model(&entity);
	if(!mc && anode->mNumMeshes)
	{
		entity_add_component(entity, c_model_new(mesh_new(),
					sauces_mat("_default"), 1, 1));
		mc = c_model(&entity);
	}

	int last_vertex = 0;
	for(m = 0; m < anode->mNumMeshes; m++)
	{
		const struct aiMesh *mesh = scene->mMeshes[anode->mMeshes[m]];

		mesh_load_scene(mc->mesh, mesh);

		if(mesh->mNumBones)
		{
			int b;
			c_skin_t *skin = c_skin(&entity);
			if(!skin)
			{
				entity_add_component(entity, c_skin_new());
				skin = c_skin(&entity);
			}
			c_skin_vert_prealloc(skin, vector_count(mc->mesh->verts));

			for(b = 0; b < mesh->mNumBones; b++)
			{
				int w;
				const struct aiBone* abone = mesh->mBones[b];
				entity_t bone = c_node_get_by_name(root, ref(abone->mName.data));
				if(!bone) continue;
				int bone_index = skin->bones_num;

				skin->bones[bone_index] = bone;
				mat4_t offset = mat4_from_ai(abone->mOffsetMatrix);
				skin->off[bone_index] = offset;
				skin->bones_num++;

				if(!c_bone(&bone))
				{
					entity_add_component(bone, c_bone_new());
				}
				for(w = 0; w < abone->mNumWeights; w++)
				{
					int i;
					const struct aiVertexWeight *vweight = &abone->mWeights[w];
					int real_id = (vweight->mVertexId + last_vertex);
					for(i = 0; i < 4; i++)
					{
						if(skin->wei[real_id]._[i] == 0.0f)
						{
							skin->wei[real_id]._[i] = vweight->mWeight;
							skin->bid[real_id]._[i] = bone_index;
							break;
						}
					}
					if(i == 4) printf("TOO MANY WEIGHTS\n");
				}
			}
		}
		last_vertex += mesh->mNumVertices;

		load_material(entity, mesh, scene);
	}

	load_comp_children(entity, scene, anode, root, scale);
}

static void load_comp_children(entity_t entity, const struct aiScene *scene,
		const struct aiNode *anode, c_node_t *root, float scale)
{
	c_node_t *node = c_node(&entity);
	int i;
	for(i = 0; i < anode->mNumChildren; i++)
	{
		const struct aiNode *cnode = anode->mChildren[i];
		const char *name = cnode->mName.data;

		if(!name[0])
		{
			load_comp_children(entity, scene, cnode, root, scale);
			continue;
		}
		entity_t n = c_node_get_by_name(node, ref(name));
		load_comp(n, scene, cnode, root, scale);
	}
}


void load_node(entity_t entity, const struct aiScene *scene,
		const struct aiNode *anode, int depth)
{
	int inherit_type = 0;
	c_spacial_t *spacial = c_spacial(&entity);

	const struct aiMetadata *meta = anode->mMetaData;
	if(meta)
	{
		int j;
		for(j = 0; j < meta->mNumProperties; j++)
		{
			const struct aiMetadataEntry *v = &meta->mValues[j];
			if(!strcmp(meta->mKeys[j].data, "InheritType"))
			{
				inherit_type = *(char*)v->mData;
			}
			continue;
			printf("%s: ", meta->mKeys[j].data);
			switch(v->mType)
			{
				case AI_BOOL: printf("%d\n", *(char*)v->mData); break;
				case AI_INT32: printf("%u\n", *(unsigned int*)v->mData); break;
				case AI_UINT64: printf("%lu\n", *(unsigned long*)v->mData); break;
				case AI_FLOAT: printf("%f\n", *(double*)v->mData); break;
				case AI_DOUBLE: printf("%lf\n", *(double*)v->mData); break;
				case AI_AISTRING: printf("'%s'\n", ((const struct aiString*)v->mData)->data); break;
				case AI_AIVECTOR3D: break;
				case AI_META_MAX: break;
				default: break;
			}
		}
	}

	if(inherit_type)
	{
		c_spacial_set_model(spacial, mat4_from_ai(anode->mParent->mTransformation));
	}
	else
	{
		c_spacial_set_model(spacial, mat4_from_ai(anode->mTransformation));
	}
	load_node_children(entity, scene, anode, depth);
}

static void load_node_children(entity_t entity, const struct aiScene *scene,
		const struct aiNode *anode, int depth)
{
	c_node_t *node = c_node(&entity);
	int i;
	for(i = 0; i < anode->mNumChildren; i++)
	{
		const struct aiNode *cnode = anode->mChildren[i];
		const char *name = cnode->mName.data;

		if(!name[0])
		{
			load_node_children(entity, scene, cnode, depth);
			continue;
		}
		entity_t n = entity_new(c_name_new(name), c_node_new());
		c_node_add(node, 1, n);
		load_node(n, scene, cnode, depth + 1);
	}
}

void load_timelines(entity_t entity, const struct aiScene *scene)
{
	c_node_t *root = c_node(&entity);
	int a;
	for(a = 0; a < scene->mNumAnimations; a++)
	{
		int n;
		const struct aiAnimation *anim = scene->mAnimations[a];

		for(n = 0; n < anim->mNumChannels; n++)
		{
			int k;
			const struct aiNodeAnim *nodeAnim = anim->mChannels[n];
			const char *name = nodeAnim->mNodeName.data;
			if(!name) continue;
			entity_t ent = c_node_get_by_name(root, ref(name));
			if(!ent) continue;

			c_timeline_t *tc = c_timeline(&ent);
			if(!tc)
			{
				entity_add_component(ent, c_timeline_new());
				tc = c_timeline(&ent);
				tc->duration = anim->mDuration;
				tc->ticks_per_sec = anim->mTicksPerSecond;
				if(!tc->ticks_per_sec) tc->ticks_per_sec = 30;
			}
			vector_alloc(tc->keys_pos, nodeAnim->mNumPositionKeys);
			vector_alloc(tc->keys_rot, nodeAnim->mNumRotationKeys);
			vector_alloc(tc->keys_scale, nodeAnim->mNumScalingKeys);

			for(k = 0; k < nodeAnim->mNumScalingKeys; k++)
			{
				const struct aiVectorKey *key = &nodeAnim->mScalingKeys[k];
				vec3_t vec = vec3(key->mValue.x, key->mValue.y, key->mValue.z);
				c_timeline_insert_scale(tc, vec, key->mTime);
			}
			for(k = 0; k < nodeAnim->mNumRotationKeys; k++)
			{
				const struct aiQuatKey *key = &nodeAnim->mRotationKeys[k];
				vec4_t quat = vec4(key->mValue.x, key->mValue.y, key->mValue.z,
						key->mValue.w);

				c_timeline_insert_rot(tc, quat, key->mTime);
			}
			for(k = 0; k < nodeAnim->mNumPositionKeys; k++)
			{
				const struct aiVectorKey *key = &nodeAnim->mPositionKeys[k];
				vec3_t vec = vec3(key->mValue.x, key->mValue.y, key->mValue.z);
				c_timeline_insert_pos(tc, vec, key->mTime);
			}

		}
		break;
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

	result = entity_new(c_name_new(name), c_node_new());
	/* c_model(&result)->mesh->transformation = */
	/* 	mat4_scale_aniso(c_model(&result)->mesh->transformation, vec3(scale)); */

	c_node_t *root = c_node(&result);
	load_node(result, scene, scene->mRootNode, 0);
	load_comp(result, scene, scene->mRootNode, root, scale);
	load_timelines(result, scene);

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
				entity_add_component(node, c_light_new(40.0f, color, 256));
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
#define PLY 1842197252
#define GLTF 1170648920
#define BIN 1516167604
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
			case PLY:
			case GLTF:
			case BIN:
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
