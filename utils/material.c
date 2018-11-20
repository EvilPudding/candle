#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "material.h"
#include <vil/vil.h>
#include "file.h"
#include "shader.h"
#include <candle.h>
#include <systems/editmode.h>
#include <dirent.h>
#include <systems/render_device.h>

int g_mats_num;
mat_t *g_mats[255];
static vil_t g_mat_ctx;


mat_t *mat_new(const char *name)
{
	mat_t *self = calloc(1, sizeof *self);
	mat_set_normal(self, (prop_t){.color=vec4(0.5, 0.5, 1.0, 0.0)});
	self->albedo.color = vec4(0.5, 0.5, 0.5, 1.0);
	self->vil = vil_get(&g_mat_ctx, ref("material"));

	self->albedo.scale = 1;
	self->roughness.scale = 1;
	self->normal.scale = 1;
	self->metalness.scale = 1;
	self->transparency.scale = 1;
	self->emissive.scale = 1;

	strncpy(self->name, name, sizeof(self->name));

	self->id = g_mats_num++;
	g_mats[self->id] = self;

	world_changed();
	return self;
}

vec4_t color_from_hex(const char *hex_str)
{
	vec4_t self;
	uint64_t hex = strtol(hex_str + 1, NULL, 16);
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
		uint32_t key = ref(prop);

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
					prp->texture = sauces(arg);
				}
				prp->scale = 1.0;

				char *scale = strtok(NULL, ",");
				if(scale)
				{
					if(sscanf(scale, "%f", &prp->scale) == -1)
					{
						printf("Error in texture format.\n");
					}
				}
				prp->blend = 1;
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
	world_changed();

	return self;
}

void mat_destroy(mat_t *self)
{
	/* TODO: free textures */
	free(self);
}

void mat_bind_prop(u_prop_t *uniforms, prop_t *prop, int *num)
{
	/* glUniform1f(uniforms->blend, prop->blend); glerr(); */
	/* glUniform1f(uniforms->scale, prop->scale); glerr(); */
	/* glUniform4f(uniforms->color, prop->color.r, prop->color.g, prop->color.b, */
	/* 		prop->color.a); glerr(); */

	/* if(prop->blend && ((int)uniforms->texture) != -1 && prop->texture) */
	/* { */
	/* 	glUniform1i(uniforms->texture, (*num)); glerr(); */
	/* 	glActiveTexture(GL_TEXTURE0 + (*num)); glerr(); */
	/* 	bind(prop->texture, 0); */
	/* 	(*num)++; */
	/* } */

}

void mat_set_transparency(mat_t *self, prop_t transparency)
{
	self->transparency = transparency;
	world_changed();
}

void mat_set_normal(mat_t *self, prop_t normal)
{
	self->normal = normal;
	world_changed();
}

void mat_set_albedo(mat_t *self, prop_t albedo)
{
	self->albedo = albedo;
	world_changed();
}

static void color_gui(vicall_t *call, struct nk_colorf *color, void *ctx) 
{
	nk_layout_row_dynamic(ctx, 29, 1);
	if (nk_combo_begin_color(ctx, nk_rgb_cf(*color), nk_vec2(200,400))) {
		nk_layout_row_dynamic(ctx, 120, 1);
		*color = nk_color_picker(ctx, *color, NK_RGBA);

		nk_combo_end(ctx);
	}
}

static void number_gui(vicall_t *call, float *num, void *ctx) 
{
	/* if(*num < 29) *num = 29; */
	/* nk_layout_row_begin(ctx, NK_DYNAMIC, *num, 2); */
	nk_layout_row_begin(ctx, NK_DYNAMIC, 29, 2);
	nk_layout_row_push(ctx, 0.20);
	nk_label(ctx, vicall_name(call), NK_TEXT_LEFT);
	nk_layout_row_push(ctx, 0.80);
	*num = nk_propertyf(ctx, "#", 0, *num, 1, 0.01, 0.01);
	nk_layout_row_end(ctx);
	/* nk_slider_int(ctx, 29, num, 200, 1); */
}

int mat_prop_menu(mat_t *self, const char *name, prop_t *prop, void *ctx)
{

	int changes = 0;
	uint32_t id = murmur_hash(&prop, 8, 0);
	if(nk_tree_push_id(ctx, NK_TREE_NODE, name, NK_MINIMIZED, id))
	{
		if(prop->texture)
		{
			float blend = prop->blend;
			nk_property_float(ctx, "#blend:", 0, &blend, 1, 0.1, 0.05);
			if(blend != prop->blend)
			{
				changes = 1;
				prop->blend = blend;
			}
			if (nk_button_label(ctx, prop->texture->name))
			{
				c_editmode_open_texture(c_editmode(&SYS), prop->texture);
			}
		}
		/* nk_layout_row_dynamic(ctx, 180, 1); */
		vec4_t vcolor = prop->color;
		union { struct nk_colorf *nk; vec4_t *v; } color = { .v = &vcolor };
		/* *color.nk = nk_color_picker(ctx, *color.nk, NK_RGBA); */

		if (nk_combo_begin_color(ctx, nk_rgb_cf(*color.nk), nk_vec2(200,400))) {
			enum color_mode {COL_RGB, COL_HSV};
			static int col_mode = COL_RGB;
			nk_layout_row_dynamic(ctx, 120, 1);
			(*color.nk) = nk_color_picker(ctx, (*color.nk), NK_RGBA);

			nk_layout_row_dynamic(ctx, 25, 2);
			col_mode = nk_option_label(ctx, "RGB", col_mode == COL_RGB) ? COL_RGB : col_mode;
			col_mode = nk_option_label(ctx, "HSV", col_mode == COL_HSV) ? COL_HSV : col_mode;

			nk_layout_row_dynamic(ctx, 25, 1);
			if (col_mode == COL_RGB) {
				color.nk->r = nk_propertyf(ctx, "#R:", 0, color.nk->r, 1.0f, 0.01f,0.005f);
				color.nk->g = nk_propertyf(ctx, "#G:", 0, color.nk->g, 1.0f, 0.01f,0.005f);
				color.nk->b = nk_propertyf(ctx, "#B:", 0, color.nk->b, 1.0f, 0.01f,0.005f);
				color.nk->a = nk_propertyf(ctx, "#A:", 0, color.nk->a, 1.0f, 0.01f,0.005f);
			} else {
				float hsva[4];
				nk_colorf_hsva_fv(hsva, (*color.nk));
				hsva[0] = nk_propertyf(ctx, "#H:", 0, hsva[0], 1.0f, 0.01f,0.05f);
				hsva[1] = nk_propertyf(ctx, "#S:", 0, hsva[1], 1.0f, 0.01f,0.05f);
				hsva[2] = nk_propertyf(ctx, "#V:", 0, hsva[2], 1.0f, 0.01f,0.05f);
				hsva[3] = nk_propertyf(ctx, "#A:", 0, hsva[3], 1.0f, 0.01f,0.05f);
				(*color.nk) = nk_hsva_colorfv(hsva);
			}
			nk_combo_end(ctx);
			if(memcmp(&vcolor, &prop->color, sizeof(vec4_t)))
			{
				prop->color = vcolor;
				changes = 1;
			}
		}
		nk_tree_pop(ctx);
	}
	if(changes)
	{
		world_changed();
	}
	return changes;
}

int mat_menu(mat_t *self, void *ctx)
{
	int changes = 0;
	changes |= mat_prop_menu(self, "albedo", &self->albedo, ctx); 
	changes |= mat_prop_menu(self, "roughness", &self->roughness, ctx); 
	changes |= mat_prop_menu(self, "normal", &self->normal, ctx); 
	changes |= mat_prop_menu(self, "metalness", &self->metalness, ctx); 
	changes |= mat_prop_menu(self, "transparency", &self->transparency, ctx); 
	changes |= mat_prop_menu(self, "emissive", &self->emissive, ctx); 
	if(nk_button_label(ctx, "vil"))
	{
		c_editmode(&SYS)->open_vil = self->vil;
	}
	/* return CONTINUE; */
	return changes;
}

void mat_bind(mat_t *self, shader_t *shader)
{
	/* TODO */
	/* mat_bind_prop(&shader->u_albedo, &self->albedo, &shader->bound_textures); */
	/* mat_bind_prop(&shader->u_roughness, &self->roughness, &shader->bound_textures); */
	/* mat_bind_prop(&shader->u_metalness, &self->metalness, &shader->bound_textures); */
	/* mat_bind_prop(&shader->u_transparency, &self->transparency, &shader->bound_textures); */
	/* mat_bind_prop(&shader->u_normal, &self->normal, &shader->bound_textures); */
	/* mat_bind_prop(&shader->u_emissive, &self->emissive, &shader->bound_textures); */

	glerr();

}

void *mat_loader(const char *path, const char *name, uint32_t ext)
{
	return mat_from_file(path);
}

void materials_reg()
{
	sauces_loader(ref("mat"), mat_loader);
	sauces_register("_default.mat", NULL, mat_new("_default.mat"));

}

void materials_init_vil()
{
	vicall_t *r, *g, *b, *a;
	vil_t *ctx = &g_mat_ctx;

	vil_context_init(ctx);

	vitype_t *mat = vil_add_type(ctx, "material",
			NULL, sizeof(uint32_t));

	vitype_t *tnum = vil_add_type(ctx, "number",
			(vitype_gui_cb)number_gui, sizeof(float));

	vitype_t *v2 = vil_add_type(ctx, "vec2", NULL, sizeof(vec2_t));
		vitype_add(v2, tnum, "x", vec2(40, 10), V_ALL);
		vitype_add(v2, tnum, "y", vec2(40, 260), V_ALL);

	vitype_t *v3 = vil_add_type(ctx, "vec3", NULL, sizeof(vec3_t));
		vitype_add(v3, tnum, "x", vec2(40, 10), V_ALL);
		vitype_add(v3, tnum, "y", vec2(40, 260), V_ALL);
		vitype_add(v3, tnum, "z", vec2(40, 360), V_ALL);


	vitype_t *tcol = vil_add_type(ctx, "_color_picker",
			(vitype_gui_cb)color_gui, sizeof(vec4_t));

	vitype_t *rgba = vil_add_type(ctx, "rgba", NULL, sizeof(vec4_t));
		vitype_add(rgba, tcol, "color", vec2(100, 10), V_ALL);
		r = vitype_add(rgba, tnum, "r", vec2(100, 10), V_ALL);
		g = vitype_add(rgba, tnum, "g", vec2(200, 10), V_ALL);
		b = vitype_add(rgba, tnum, "b", vec2(300, 10), V_ALL);
		a = vitype_add(rgba, tnum, "a", vec2(300, 10), V_ALL);
		vicall_color(r, vec4(1, 0, 0, 1));
		vicall_color(g, vec4(0, 1, 0, 1));
		vicall_color(b, vec4(0, 0, 1, 1));
		vicall_color(a, vec4(1, 1, 1, 1));

	float y = 0.0f;
	float inc = 100.0f;

	vitype_add(mat, v2, "uv-space", vec2(0, y += inc), V_IN);
	vitype_add(mat, v2, "screen-space", vec2(0, y += inc), V_IN);
	vitype_add(mat, v3, "world-space", vec2(0, y += inc), V_IN);
	vitype_add(mat, v3, "view-space", vec2(0, y += inc), V_IN);
}
