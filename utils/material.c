#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "material.h"
#include "file.h"
#include "shader.h"
#include <candle.h>
#include <systems/editmode.h>
#include <dirent.h>

mat_t *mat_new(const char *name)
{
	mat_t *self = calloc(1, sizeof *self);
	mat_set_normal(self, (prop_t){.color=vec4(0.5, 0.5, 1.0, 0.0)});
	self->albedo.color = vec4(0.5, 0.5, 0.5, 1.0);

	strncpy(self->name, name, sizeof(self->name));

	return self;
}

vec4_t color_from_hex(const char *hex_str)
{
	vec4_t self;
	ulong hex = strtol(hex_str + 1, NULL, 16);
	self.a = (float)((hex >> 24) & 0XFF) / 255.0f;
	self.r = (float)((hex >> 16) & 0xFF) / 255.0f;
	self.g = (float)((hex >> 8) & 0xFF) / 255.0f;
	self.b = (float)((hex >> 0) & 0XFF) / 255.0f;
	if(self.a == 0.0f) self.a = 1.0f;

	return self;
}

void mat_parse(mat_t *self, FILE *fd)
{
	char prop[64];
	char arg[256];
	char c;
	while(!feof(fd))
	{
		if(fscanf(fd, "%s ", prop) <= 0) continue;
		prop_t *prp;
		uint key = ref(prop);

		if(key == ref("diffuse"))
		{
			prp = &self->albedo;
		}
		else if(key == ref("albedo"))
		{
			prp = &self->albedo;
		}
		else if(key == ref("normal"))
		{
			prp = &self->normal;
		}
		else if(key == ref("roughness"))
		{
			prp = &self->roughness;
		}
		else if(key == ref("metalness"))
		{
			prp = &self->metalness;
		}
		else if(key == ref("transparency"))
		{
			prp = &self->transparency;
		}
		else if(key == ref("emissive"))
		{
			prp = &self->emissive;
		}
		else return;

		do
		{
			if(fscanf(fd, "%s", arg) <= 0) break;

			if(arg[0] == '#')
			{
				prp->color = color_from_hex(arg);
			}
			else
			{
				char *file = strtok(arg, "*");
				if(file)
				{
					prp->texture = sauces_tex(arg);
				}
				prp->texture_scale = 1.0;

				char *scale = strtok(NULL, ",");
				if(scale)
				{
					if(sscanf(scale, "%f", &prp->texture_scale) == -1)
					{
						printf("Error in texture format.\n");
					}
				}
				prp->texture_blend = 1;
			}
			c = getc(fd);
		}
		while(c != '\n');
	}
}

mat_t *mat_from_file(const char *filename)
{
	mat_t *self = mat_new(filename);

	char *file_name = self->name + (strrchr(self->name, '/') - self->name);
	if(!file_name) file_name = self->name;

	FILE *fp = fopen(filename, "r");

	if(fp)
	{
		mat_parse(self, fp);
		fclose(fp);
	}
	else
	{
		printf("File does not exist: '%s'\n", filename);
		mat_destroy(self);
		return 0;
	}

	return self;
}

void mat_destroy(mat_t *self)
{
	/* TODO: free textures */
	free(self);
}

void mat_bind_prop(u_prop_t *uniforms, prop_t *prop, int *num)
{
	glUniform1f(uniforms->texture_blend, prop->texture_blend); glerr();
	glUniform1f(uniforms->texture_scale, prop->texture_scale); glerr();
	glUniform4f(uniforms->color, prop->color.r, prop->color.g, prop->color.b,
			prop->color.a); glerr();

	if(prop->texture_blend && ((int)uniforms->texture) != -1)
	{
		glUniform1i(uniforms->texture, (*num)); glerr();
		glActiveTexture(GL_TEXTURE0 + (*num)); glerr();
		texture_bind(prop->texture, 0);
		(*num)++;
	}

}

void mat_set_transparency(mat_t *self, prop_t transparency)
{
	self->transparency = transparency;
}

void mat_set_normal(mat_t *self, prop_t normal)
{
	self->normal = normal;
}

void mat_set_albedo(mat_t *self, prop_t albedo)
{
	self->albedo = albedo;
}

void mat_prop_menu(mat_t *self, const char *name, prop_t *prop, void *ctx)
{
	uint id = murmur_hash(&prop, 8, 0);
	if(nk_tree_push_id(ctx, NK_TREE_NODE, name, NK_MINIMIZED, id))
	{
		nk_property_float(ctx, "blend:", 0, &prop->texture_blend, 1, 0.1, 0.05);
		nk_layout_row_dynamic(ctx, 180, 1);
		union { struct nk_colorf *nk; vec4_t *v; } color = { .v = &prop->color };
		*color.nk = nk_color_picker(ctx, *color.nk, NK_RGBA);
		nk_tree_pop(ctx);
	}
}

void mat_menu(mat_t *self, void *ctx)
{
	mat_prop_menu(self, "albedo", &self->albedo, ctx); 
	mat_prop_menu(self, "roughness", &self->roughness, ctx); 
	mat_prop_menu(self, "metalness", &self->metalness, ctx); 
	mat_prop_menu(self, "transparency", &self->transparency, ctx); 
	mat_prop_menu(self, "normal", &self->normal, ctx); 
	mat_prop_menu(self, "emissive", &self->emissive, ctx); 
}

void mat_bind(mat_t *self, shader_t *shader)
{
	mat_bind_prop(&shader->u_albedo, &self->albedo, &shader->bound_textures);
	mat_bind_prop(&shader->u_roughness, &self->roughness, &shader->bound_textures);
	mat_bind_prop(&shader->u_metalness, &self->metalness, &shader->bound_textures);
	mat_bind_prop(&shader->u_transparency, &self->transparency, &shader->bound_textures);
	mat_bind_prop(&shader->u_normal, &self->normal, &shader->bound_textures);
	mat_bind_prop(&shader->u_emissive, &self->emissive, &shader->bound_textures);
	/* mat_bind_prop(&shader->u_position, &self->position, 5); */
	/* mat_bind_prop(&shader->u_position, &self->position, 6); */

	glerr();

}
