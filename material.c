#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "material.h"
#include "file.h"
#include "shader.h"
#include <candle.h>
#include <dirent.h>

char g_mats_path[256] = "resauces/materials";

mat_t *mat_new(const char *name)
{
	mat_t *self = calloc(1, sizeof *self);
	mat_set_normal(self, (prop_t){.color=vec4(0.5, 0.5, 1.0, 0.0)});

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

		if(!strcmp(prop, "diffuse")) prp = &self->diffuse;
		else if(!strcmp(prop, "normal")) prp = &self->normal;
		else if(!strcmp(prop, "specular")) prp = &self->specular;
		else if(!strcmp(prop, "transparency")) prp = &self->transparency;
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
					char buffer[256];
					strncpy(buffer, self->name, sizeof(buffer));
					path_join(buffer, sizeof(buffer), arg);

					prp->texture = sauces_tex(buffer);
					if(!prp->texture)
					{
						prp->texture = sauces_tex(arg);
					}
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
	char buffer[265];
	char global_path[265];

	strncpy(self->name, path_relative(filename, g_mats_path),
			sizeof(self->name));

	strncpy(global_path, g_mats_path, sizeof(global_path));
	path_join(global_path, sizeof(global_path), self->name);

	strncpy(buffer, g_mats_path, sizeof(buffer));

	char *file_name = self->name + (strrchr(self->name, '/') - self->name);
	if(!file_name) file_name = self->name;

	path_join(buffer, sizeof(buffer), self->name);
	if(is_dir(global_path))
	{
		path_join(buffer, sizeof(buffer), file_name);
	}

	strncat(buffer, ".mat", sizeof(buffer));

	FILE *fp = fopen(buffer, "r");

	if(fp)
	{
		mat_parse(self, fp);
		fclose(fp);
	}
	else
	{
		printf("File does not exist: '%s'\n", buffer);
		mat_destroy(self);
		return 0;
	}

	c_sauces_mat_reg(c_sauces(&candle->systems), self->name, self);
	return self;
}

void mat_destroy(mat_t *self)
{
	/* TODO: free textures */
	free(self);
}

mat_t *mat_from_dir(const char *name, const char *dirname)
{
	/* mat_t *self = mat_new(name); */
	/* candle_mat_reg(candle, name, self); */
	/* texture_set_path(dirname); */

	/* printf("importing mat %s from %s\n", name, dirname); */

	/* char buffer[256]; */

	/* texture_t *diffuse = texture_from_file("diffuse"); */
	/* self->diffuse.texture_blend = (self->diffuse.texture = diffuse) != NULL; */
	/* self->diffuse.texture_scale = 1.0; */ 

	/* sprintf(buffer, "%s_diffuse", name); */
	/* candle_texture_reg(candle, buffer, diffuse); */



	/* texture_t *specular = texture_from_file("specular"); */
	/* self->specular.texture_blend = (self->specular.texture = specular) != NULL; */
	/* self->specular.texture_scale = 1.0; */ 

	/* sprintf(buffer, "%s_specular", name); */
	/* candle_texture_reg(candle, buffer, specular); */


	/* texture_t *normal = texture_from_file("normal"); */
	/* self->normal.texture_blend = (self->normal.texture = normal) != NULL; */
	/* self->normal.texture_scale = 1.0; */ 

	/* sprintf(buffer, "%s_normal", name); */
	/* candle_texture_reg(candle, buffer, normal); */


	/* texture_t *transparency = texture_from_file("transparency"); */
	/* self->transparency.texture_blend = (self->transparency.texture = transparency) != NULL; */
	/* self->transparency.texture_scale = 1.0; */ 

	/* sprintf(buffer, "%s_transparency", name); */
	/* candle_texture_reg(candle, buffer, transparency); */


	/* return self; */
	return NULL;
}

void mat_bind_prop(u_prop_t *uniforms, prop_t *prop, int num)
{
	glUniform1f(uniforms->texture_blend, prop->texture_blend); glerr();
	glUniform1f(uniforms->texture_scale, prop->texture_scale); glerr();

	glUniform1i(uniforms->texture, num); glerr();
	glActiveTexture(GL_TEXTURE0 + num); glerr();

	if(prop->texture_blend)
	{
		texture_bind(prop->texture, -1);
	}
	else
	{
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	glUniform4f(uniforms->color, prop->color.r, prop->color.g, prop->color.b,
			prop->color.a); glerr();
}

void mat_set_specular(mat_t *self, prop_t specular)
{
	self->specular = specular;
}

void mat_set_transparency(mat_t *self, prop_t transparency)
{
	self->transparency = transparency;
}

void mat_set_normal(mat_t *self, prop_t normal)
{
	self->normal = normal;
}

void mat_set_diffuse(mat_t *self, prop_t diffuse)
{
	self->diffuse = diffuse;
}

void mat_set_ambient_light(mat_t *self, vec4_t light)
{
	self->ambient_light = light;
}

void mat_bind(mat_t *self, shader_t *shader)
{
	mat_bind_prop(&shader->u_diffuse, &self->diffuse, shader->bound_textures++);
	mat_bind_prop(&shader->u_specular, &self->specular, shader->bound_textures++);
	mat_bind_prop(&shader->u_normal, &self->normal, shader->bound_textures++);
	mat_bind_prop(&shader->u_transparency, &self->transparency, shader->bound_textures++);
	/* mat_bind_prop(&shader->u_position, &self->position, 5); */
	/* mat_bind_prop(&shader->u_position, &self->position, 6); */

	glUniform4f(shader->u_ambient_light, self->ambient_light.r,
			self->ambient_light.g, self->ambient_light.b, self->ambient_light.a);
	glerr();

}
