#include "sauces.h"
#include "../candle.h"
#include "editmode.h"
#include "file.h"

DEC_CT(ct_sauces);

void c_sauces_init(c_sauces_t *self)
{
	self->super = component_new(ct_sauces);
}

c_sauces_t *c_sauces_new()
{
	c_sauces_t *self = calloc(1, sizeof *self);
	c_sauces_init(self);
	return self;
}


void c_sauces_material_reg(c_sauces_t *self, const char *name, material_t *material)
{
	uint i = self->materials_size++;
	self->materials = realloc(self->materials,
			sizeof(*self->materials) * self->materials_size);
	self->materials[i] = material;
	if(material->name != name) strncpy(material->name, name, sizeof(material->name));
}

material_t *c_sauces_material_get(c_sauces_t *self, const char *name)
{
	material_t *material;
	uint i;
	for(i = 0; i < self->materials_size; i++)
	{
		material = self->materials[i];
		if(!strcmp(material->name, name)) return material;
	}
	material = material_from_file(name);
	if(material) c_sauces_material_reg(self, name, material);
	return material;
}

void c_sauces_mesh_reg(c_sauces_t *self, const char *name, mesh_t *mesh)
{
	uint i = self->meshes_size++;
	self->meshes = realloc(self->meshes,
			sizeof(*self->meshes) * self->meshes_size);
	self->meshes[i] = mesh;
	strncpy(mesh->name, name, sizeof(mesh->name));
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
	mesh = mesh_from_file(name);
	c_sauces_mesh_reg(self, name, mesh);
	return mesh;
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

int c_sauces_get_materials_at(c_sauces_t *self, const char *dir_name)
{
	char dir_buffer[2048];
	strncpy(dir_buffer, g_materials_path, sizeof(dir_buffer));

	path_join(dir_buffer, sizeof(dir_buffer), path_relative(dir_name, g_materials_path));

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
			material_from_file(buffer);
			continue;
		}

		if(is_dir(buffer))
		{
			if(!material_from_file(buffer))
			{
				c_sauces_get_materials_at(self, buffer);
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


void c_sauces_register()
{
	ct_t *ct = ecm_register("Resources", &ct_sauces,
			sizeof(c_sauces_t), (init_cb)c_sauces_init, 0);

	ct_register_listener(ct, WORLD, component_menu,
			(signal_cb)c_sauces_component_menu);
}
