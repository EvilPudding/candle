#include <stdlib.h>
#include <stddef.h>
#include <utils/str.h>
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

mat_t *g_mats[255];
uint32_t g_mats_num;
static vil_t g_mat_ctx;
void mat_changed(vicall_t *call, slot_t slot, void *usrptr);

struct mat_vil_type {
	char *builtin_execute;
};

struct mat_type {
	char name[64];
	vifunc_t *mat;
	uint32_t ubo;
	uint32_t ubo_size;
	char *src;

	/* used for kernel generation */
	vicall_t *tmp_material_instance;
	uint32_t next_free[128];
	uint32_t free_position;
	char *code;
};

static struct mat_type g_mat_types[32];

static void init_type(uint32_t tid, const char *type_name);

static struct mat_type *get_type(const char *type_name)
{
	uint32_t tid;
	for (tid = 0; g_mat_types[tid].mat; tid++)
	{
		if (!strncmp(g_mat_types[tid].name, type_name,
		             sizeof(g_mat_types[tid].name) - 1))
		{
			break;
		}
	}
	if (!g_mat_types[tid].mat)
	{
		init_type(tid, type_name);
	}
	return &g_mat_types[tid];
}

uint32_t material_get_ubo(uint32_t mat_type)
{
	return g_mat_types[mat_type].ubo;
}

void _mat_changed(mat_t *mat)
{
	const slot_t slot = {0};
	mat_changed(mat->call, slot, mat);
}

void mat_assign_type(mat_t *self, struct mat_type *type)
{
	self->type = type - g_mat_types;

	self->id = type->free_position;
	type->free_position = type->next_free[type->free_position];

	if (self->id == 128)
		printf("Too many material instances for type %d\n", self->type);

	if (!self->call)
	{
		self->call = vicall_new(self->sandbox, type->mat, "material",
								vec2(100, 10), ~0, V_BOTH);
	}
	vicall_watch(self->call, mat_changed, self);
	loader_push(g_candle->loader, (loader_cb)_mat_changed, self, NULL);
}


mat_t *mat_new(const char *name, const char *type_name)
{
	char buffer[256];
	mat_t *self = calloc(1, sizeof *self);
	strncpy(self->name, name, sizeof(self->name) - 1);
	self->global_id = g_mats_num++;
	g_mats[self->global_id] = self;

	sprintf(buffer, "_material%d_sandbox", self->global_id);
	self->sandbox = vifunc_new(&g_mat_ctx, buffer, NULL, 0, false);

	if (type_name)
	{
		mat_assign_type(self, get_type(type_name));
	}

	return self;
}

vec4_t color_from_hex(const char *hex_str)
{
	vec4_t self;
	uint64_t hex = strtol(hex_str + 1, NULL, 16);
	self.w = (float)((hex >> 24) & 0XFF) / 255.0f;
	self.x = (float)((hex >> 16) & 0xFF) / 255.0f;
	self.y = (float)((hex >> 8) & 0xFF) / 255.0f;
	self.z = (float)((hex >> 0) & 0XFF) / 255.0f;
	if (self.w == 0.0f) self.w = 1.0f;

	return self;
}

mat_t *mat_from_file(const char *filename)
{
	mat_t *self;
	const char *file_name = strrchr(filename, '/');

	if (!file_name)
	{
		file_name = filename;
	}
	else
	{
		file_name += 1;
	}

	self = mat_new(file_name, NULL);
	printf("loading %s\n", filename);

	vifunc_load(self->sandbox, filename);

	self->call = vifunc_get(self->sandbox, "material");
	if (self->call)
	{
		mat_assign_type(self, self->call->type->usrptr);
	}
	else
	{
		printf("Material %s %d corrupt\n", file_name, self->sandbox->call_count);
	}

	return self;
}

void mat_destroy(mat_t *self)
{
	/* TODO: free textures */
	free(self);
}

typedef struct
{
	vec2_t coord;

	uint32_t tile;
	vec2_t size;
	vec4_t result;

	resource_handle_t source;
	texture_t *texture;
} _mat_sampler_t;

static void load_bot_tile(texture_t *tex, float *w, float *h)
{
	uint32_t mip = 0;
	*w = tex->width;
	*h = tex->height;
	while (*w > 128.0f || *h > 128.0f)
	{
		*w /= 2.0f;
		*h /= 2.0f;
		mip++;
	}
	if (mip > 0) mip -= 1;

	load_tile(tex, mip, 1, 1, 1);
}

static bool_t _texture_load(vicall_t *call, _mat_sampler_t *texid,
                            int argc, const char **argv) 
{
	char filename[256];

	if (argc < 1) return false;

	strcpy(filename, argv[0]);

	texid->texture = sauces(filename);
	if (!texid->texture)
	{
		printf("Failed to fetch texture '%s'\n", filename);
	}
	texid->tile = texid->texture->bufs[0].indir_n;
	texid->size = vec2(texid->texture->width, texid->texture->height);

	return true;
}

static void _texture_save(vicall_t *call, _mat_sampler_t *texid, FILE *fp) 
{
	fprintf(fp, "%s", texid->texture ? (char*)texid->texture->usrptr : "");
}

extern texture_t *g_cache;
static float _tex_previewer_gui(vicall_t *call, _mat_sampler_t *texid, void *ctx) 
{
	float call_h = 0;
	if (texid->texture)
	{
		struct nk_rect bounds;
		struct nk_image im;
		uint32_t tid;
		float w;
		float h;

		load_bot_tile(texid->texture, &w, &h);

		tid = texid->texture->bufs[0].indir_n;
		texid->tile = tid;
		texid->size = vec2(texid->texture->width, texid->texture->height);

		nk_layout_row_static(ctx, h, w, 1);
		nk_widget(&bounds, ctx);

		im = nk_image_id(g_cache->bufs[0].id);
		nk_draw_image_ext(nk_window_get_canvas(ctx), bounds, &im,
		                  nk_rgba(255, 255, 255, 255), 1, tid,
		                  texid->texture->width,
		                  texid->texture->height);
		call_h += nk_layout_widget_bounds(ctx).h;
	}

	nk_layout_row_dynamic(ctx, 20, 1);
	if (nk_button_label(ctx, texid->texture && (char*)texid->texture->usrptr
	                         ? (char*)texid->texture->usrptr : "pick texture "))
	{
		char *file = NULL;
		entity_signal(entity_null, ref("pick_file_load"), "png;tga;jpg", &file);
		if (file)
		{
			char *name = strrchr(file, '/');
			if (name)
			{
				name += 1;
			}
			else
			{
				name = file;
			}
			texid->texture = sauces(name);
			if (texid->texture)
			{
				texid->tile = texid->texture->bufs[0].indir_n;
				texid->size = vec2(texid->texture->width, texid->texture->height);
			}
			free(file);
		}
	}
	call_h += nk_layout_widget_bounds(ctx).h;

	return call_h;
}

static float _color_gui(vicall_t *call, struct nk_colorf *color, void *ctx) 
{
	nk_layout_row_dynamic(ctx, 20, 1);
	if (nk_combo_begin_color(ctx, nk_rgb_cf(*color), nk_vec2(200,400))) {
		nk_layout_row_dynamic(ctx, 120, 1);
		*color = nk_color_picker(ctx, *color, NK_RGBA);

		nk_combo_end(ctx);
	}
	return nk_layout_widget_bounds(ctx).h;
}

static void _code_save(vifunc_t *func, FILE *fp) 
{
	/* fprintf(fp, "%%{\n%s}%%", code->string); */
}

static bool_t _code_load(vifunc_t *func, FILE *fp) 
{
	struct mat_type *type;
	char buffer[1024];

	if (!func->usrptr)
	{
		return true;
	}
	type = func->usrptr;
	type->code = str_new(64);

	buffer[1024 - 1] = '\0';
	if (!fgets(buffer, 3, fp))
	{
		return true;
	}
	if (strncmp(buffer, "%{", 2))
	{
		rewind(fp);
		return true;
	}

	while (fgets(buffer, sizeof(buffer) - 1, fp))
	{
		char *end = strstr(buffer, "}%");
		if (end) *end = '\0';

		str_cat(&type->code, buffer);

		if (end) break;
	}

	return true;
}

static void _int_save(vicall_t *call, int *num, FILE *fp) 
{
	fprintf(fp, "%d", *num);
}

static bool_t _int_load(vicall_t *call, int *num, int argc, char **argv) 
{
	if (argc < 1)
	{
		return false;
	}
	*num = atoi(argv[0]);
	return true;
}

static float _vint_gui(vicall_t *call, int32_t *num, void *ctx) 
{
	/* if (*num < 20) *num = 20; */
	/* nk_layout_row_begin(ctx, NK_DYNAMIC, *num, 2); */
	nk_layout_row_begin(ctx, NK_DYNAMIC, 20, 2);
	nk_layout_row_push(ctx, 0.20);
	nk_label(ctx, vicall_name(call), NK_TEXT_LEFT);
	nk_layout_row_push(ctx, 0.80);
	*num = nk_propertyi(ctx, "#", 0, *num, 1000, 1, 0.01);
	nk_layout_row_end(ctx);
	/* nk_slider_int(ctx, 20, num, 200, 1); */
	return nk_layout_widget_bounds(ctx).h;
}

static void _number_save(vicall_t *call, float *num, FILE *fp) 
{
	fprintf(fp, "%f", *num);
}

static bool_t _number_load(vicall_t *call, float *num,
                           int argc, const char **argv) 
{
	if (argc < 1)
	{
		return false;
	}
	*num = atof(argv[0]);
	return true;
}

static float _number_gui(vicall_t *call, float *num, void *ctx) 
{
	/* if (*num < 20) *num = 20; */
	/* nk_layout_row_begin(ctx, NK_DYNAMIC, *num, 2); */
	nk_layout_row_begin(ctx, NK_DYNAMIC, 20, 2);
	nk_layout_row_push(ctx, 0.40);
	nk_label(ctx, vicall_name(call), NK_TEXT_LEFT);
	nk_layout_row_push(ctx, 0.60);
	*num = nk_propertyf(ctx, "#", -1000.0f, *num, 1000.0f, 0.01, 0.01);
	nk_layout_row_end(ctx);
	/* nk_slider_int(ctx, 20, num, 200, 1); */
	return nk_layout_widget_bounds(ctx).h;
}

int mat_menu(mat_t *self, void *ctx)
{
	struct mat_type *type;
	uint32_t new_tid;
	khiter_t k;
	bool_t ret = false;

	nk_layout_row_begin(ctx, NK_DYNAMIC, 20, 2);
	nk_layout_row_push(ctx, 0.70);
	type = &g_mat_types[self->type];
	new_tid = self->type;
	if (nk_combo_begin_label(ctx, type->name, nk_vec2(200.0f, 400.0f)))
	{
		nk_layout_row_dynamic(ctx, 20.0f, 1);
		for (k = kh_begin(g_mat_ctx.funcs); k != kh_end(g_mat_ctx.funcs); ++k)
		{
			vifunc_t *t;

			if(!kh_exist(g_mat_ctx.funcs, k)) continue;
			t = kh_value(g_mat_ctx.funcs, k);
			if (t->name[0] == '_' && vifunc_get(t, "pbr"))
			{
				if (!t->usrptr)
				{
					get_type(t->name + 1);
				}
				if (nk_combo_item_label(ctx, t->name + 1, NK_TEXT_LEFT))
				{
					new_tid = ((struct mat_type*)t->usrptr) - g_mat_types;
				}
			}
		}
		nk_combo_end(ctx);
	}

	if (new_tid != self->type)
	{
		char buffer[256];

		type->next_free[self->id] = type->free_position;
		type->free_position = self->id;

		if (self->call)
		{
			vicall_unwatch(self->call);
			self->call = NULL;
		}
		if (self->sandbox)
		{
			vifunc_destroy(self->sandbox);
			self->sandbox = NULL;
		}
		sprintf(buffer, "_material%d_%s", self->global_id,
		         g_mat_types[new_tid].name);
		self->sandbox = vifunc_new(&g_mat_ctx, buffer, NULL, 0, false);
		mat_assign_type(self, &g_mat_types[new_tid]);
		ret = true;
	}

	nk_layout_row_push(ctx, 0.30);
	if (nk_button_label(ctx, "edit"))
	{
		c_editmode(&SYS)->open_vil = type->mat;
	}
	nk_layout_row_end(ctx);

	vicall_gui(self->call, ctx, true, 0.0f);

	nk_layout_row_begin(ctx, NK_DYNAMIC, 20, 1);
	nk_layout_row_push(ctx, 1.0f);
	if (nk_button_label(ctx, "save"))
	{
		char *output = NULL;
		entity_signal(entity_null, ref("pick_file_save"), "vil", &output);
		if (output)
		{
			vifunc_save(self->call->parent, output);
			free(output);
		}
	}
	nk_layout_row_end(ctx);
	if (ret)
		self->update_id++;
	return ret;
}

void *mat_loader(const char *path, const char *name, uint32_t ext)
{
	return mat_from_file(path);
}

void materials_reg()
{
	sauces_loader(ref("mat"), mat_loader);
	sauces_register("_default.mat", NULL, mat_new("_default.mat", "default"));
	sauces_register("_parallax.mat", NULL, mat_new("_parallax.mat", "parallax"));
	sauces_register("_decal.mat", NULL, mat_new("_decal.mat", "decal"));
}

void material_foreach_input(vicall_t *call, slot_t slot, char **str)
{
	if (call->type->name[0] == '_' || call->type->name[0] == '$') return;
	if (call->type->id == ref("vec4"))
	{
		str_catf(str, "	vec4 %s;\n", call->name);
	}
	else if (call->type->id == ref("vec3"))
	{
		str_catf(str, "	vec3 %s;\n", call->name);
	}
	else if (call->type->id == ref("vec2"))
	{
		str_catf(str, "	vec2 %s;\n", call->name);
	}
	else if (call->type->id == ref("number"))
	{
		str_catf(str, "	float %s;\n", call->name);
	}
	else if (call->type->id == ref("integer"))
	{
		str_catf(str, "	int %s;\n", call->name);
	}
	else
	{
		str_catf(str, "	vil_%s_in_t %s;\n", call->type->name, call->name);
	}
}
void material_foreach_output(vicall_t *call, slot_t slot, char **str)
{
	if (call->type->name[0] == '_' || call->type->name[0] == '$') return;
	if (call->type->id == ref("vec4"))
	{
		str_catf(str, "	vec4 %s;\n", call->name);
	}
	else if (call->type->id == ref("vec3"))
	{
		str_catf(str, "	vec3 %s;\n", call->name);
	}
	else if (call->type->id == ref("vec2"))
	{
		str_catf(str, "	vec2 %s;\n", call->name);
	}
	else if (call->type->id == ref("number"))
	{
		str_catf(str, "	float %s;\n", call->name);
	}
	else if (call->type->id == ref("integer"))
	{
		str_catf(str, "	int %s;\n", call->name);
	}
	else
	{
		str_catf(str, "	vil_%s_out_t %s;\n", call->type->name, call->name);
	}
}

void material_generate_precall(vicall_t *call, slot_t slot, char **args)
{
	str_catf(args, "vil_%s_in_t %s_in;\n", call->type->name, call->name);
	str_catf(args, "vil_%s_out_t %s_out;\n", call->type->name, call->name);
}

void material_generate_assignments(vifunc_t *func, slot_t to, slot_t from, char **args)
{
	char buffer_to[256];
	char buffer_from[256];
	vifunc_slot_to_name(func, to, buffer_to, ".", "_in");
	vifunc_slot_to_name(func, from, buffer_from, ".", "_out");

	str_catf(args, "	%s = %s;\n", buffer_to, buffer_from);
}

void material_generate_assignments2(vifunc_t *func, slot_t to, slot_t from, char **args)
{
	char buffer_to[256];
	char buffer_from[256];
	vifunc_slot_to_name(func, to, buffer_to, ".", NULL);
	vifunc_slot_to_name(func, from, buffer_from, ".", NULL);

	str_catf(args, "	arg_in.%s = arg_out.%s;\n", buffer_to, buffer_from);
}


void material_generate_call(vicall_t *call, slot_t slot, char **args)
{
	if (!strcmp(call->type->name, "opaque")) return;
	if (!strcmp(call->type->name, "transparent")) return;
	str_catf(args, "	%s_func(%s_in, %s_out, tile_num, tile_array);\n",
	         call->type->name, call->name, call->name);
	/* str_catf(args, "%s_func(%s_in, %s_out);\n", */
	         /* call->type->name, call->name, call->name); */
}

void material_generate_call2(vicall_t *call, slot_t slot, char **args)
{
	if (!strcmp(call->type->name, "opaque")) return;
	if (!strcmp(call->type->name, "transparent")) return;
	str_catf(args, "	%s_func(arg_in.%s, arg_out.%s, tile_num, tile_array);\n",
	         call->type->name, call->name, call->name);
}


void material_generate_func(vifunc_t *func, char **str)
{
	if (func->name[0] == '_' || func->name[0] == '$') return;
	if (!strcmp(func->name, "opaque")) return;
	if (!strcmp(func->name, "transparent")) return;

	str_catf(str, "void %s_func(in vil_%s_in_t arg_in, out vil_%s_out_t arg_out,\n"
	                            "inout uint tile_num, inout uint tile_array[8]) {\n",
	/* str_catf(str, "void %s_func(in vil_%s_in_t arg_in, out vil_%s_out_t arg_out) {\n", */
			 func->name, func->name, func->name);

	if (func->id == ref("number"))
	{
		str_cat(str, "	arg_out = arg_in;\n");
	}
	else if (func->id == ref("rgba"))
	{
		str_cat(str, "	arg_out = vec4(arg_in.x, arg_in.y, arg_in.z, arg_in.w);\n");
	}
	else if (func->id == ref("vec4"))
	{
		str_cat(str, "	arg_out = vec4(arg_in.x, arg_in.y, arg_in.z, arg_in.w);\n");
	}
	else if (func->id == ref("vec3"))
	{
		str_cat(str, "	arg_out = vec3(arg_in.x, arg_in.y, arg_in.z);\n");
	}
	else if (func->id == ref("vec2"))
	{
		str_cat(str, "	arg_out = vec2(arg_in.x, arg_in.y);\n");
	}
	else if (func->id == ref("sin"))
	{
		str_cat(str, "	arg_out.result = sin(arg_in.x);\n");
	}
	else if (func->id == ref("range"))
	{
		str_cat(str, "	arg_out.result = ((arg_in.x - arg_in.min_in) "
		             " / (arg_in.max_in - arg_in.min_in))"
		             " * (arg_in.max_out - arg_in.min_out) + arg_in.min_out;\n");
	}
	else if (func->id == ref("mul"))
	{
		str_cat(str, "	arg_out.result = arg_in.x * arg_in.y;\n");
	}
	else if (   func->id == ref("mix_num")
	         || func->id == ref("mix_vec2")
	         || func->id == ref("mix_vec3")
	         || func->id == ref("mix_vec4") )
	{
		str_cat(str, "	arg_out.result = mix(arg_in.y, arg_in.x, arg_in.a);\n");
	}
	else if (func->id == ref("add"))
	{
		str_cat(str, "	arg_out.result = arg_in.x + arg_in.y;\n");
	}
	else if (func->id == ref("texture"))
	{
		str_cat(str,
"	uint tile_out;\n"
"#ifdef QUERY_PASS\n"
"	arg_out.result = textureSVT(uvec2(arg_in.size), uint(arg_in.tile), arg_in.coord, tile_out, 0.1);\n"
"	tile_array[tile_num++] = tile_out;\n"
"#else\n"
"	arg_out.result = textureSVT(uvec2(arg_in.size), uint(arg_in.tile), arg_in.coord, tile_out, 1.0);\n"
"#endif\n"
		);
	}
	else if (func->id == ref("flipbook"))
	{
		str_cat(str,
"	vec2 trans = 1.0 / vec2(arg_in.columns, arg_in.rows);\n"
"	int max_index = arg_in.columns * arg_in.rows;\n"
"	int time_index = int(floor(arg_in.frame)) % max_index;\n"
"	float time_index_x = float(time_index % int(arg_in.columns)) * trans.x;\n"
"	float time_index_y = float(arg_in.columns - time_index / int(arg_in.columns)) * trans.y;\n"
"	arg_out.result = fract(arg_in.coord) * trans + vec2(time_index_x, time_index_y);\n"
		);
	}
	else
	{
		vifunc_iterate_dependencies(func, ~0, ~0,
		                            (vil_link_cb)material_generate_assignments2,
		                            (vil_call_cb)material_generate_call2, str);
	}
	str_cat(str, "}\n");
}

void material_generate_struct(vifunc_t *func, char **args)
{
	char *inputs, *outputs;

	if (func->name[0] == '_' || func->name[0] == '$') return;
	if (   func->id == ref("vec4")
	    || func->id == ref("vec3")
	    || func->id == ref("vec2"))
	{
		str_catf(args, "#define vil_%s_in_t %s\n"
		               "#define vil_%s_out_t %s\n",
		               func->name, func->name, func->name, func->name);
		return;
	}
	if (func->id == ref("number"))
	{
		str_cat(args, "#define vil_number_in_t float\n"
		              "#define vil_number_out_t float\n");
		return;
	}
	if (func->id == ref("integer"))
	{
		str_cat(args, "#define vil_integer_in_t int\n"
		              "#define vil_integer_out_t int\n");
		return;
	}
	if (func->id == ref("rgba"))
	{
		str_cat(args, "#define vil_rgba_in_t vec4\n"
		              "#define vil_rgba_out_t vec4\n");
		return;
	}

	inputs = str_new(100);
	outputs = str_new(100);

	vifunc_foreach_call(func, false, true, false, true,
	                    (vil_call_cb)material_foreach_input, &inputs);
	vifunc_foreach_call(func, false, true, true, true,
	                    (vil_call_cb)material_foreach_output, &outputs);

	str_catf(args, "struct vil_%s_in_t {\n%s};\n", func->name,
	         str_len(inputs) ? inputs : "	int ignore;\n");
	str_catf(args, "struct vil_%s_out_t {\n%s};\n", func->name,
	         str_len(outputs) ? outputs : "	int ignore;\n");

	str_free(inputs);
	str_free(outputs);
}

struct transparent
{
	vec4_t absorb;
	vec3_t normal;
	float roughness;
	vec3_t emissive;
};

struct opaque
{
	vec4_t albedo;
	vec3_t normal;
	float roughness;
	vec3_t emissive;
	float metalness;
};

struct decal
{
	vec4_t albedo;
	vec3_t normal;
	float roughness;
	vec3_t emissive;
	float metalness;
};

struct parallax
{
	vec4_t albedo;
	vec3_t normal;
	float roughness;
	vec3_t emissive;
	float metalness;
	float height;
};

static void _generate_uniform(vicall_t *root, vicall_t *call, slot_t slot,
                        uint8_t *data, void *usrptr)
{
	char **args = usrptr;
	char buffer[256];
	vifunc_slot_to_name(root->parent, slot, buffer, "_", NULL);
	str_catf(args, "	vil_%s_in_t %s;\n", call->type->name, buffer);
}

static void _generate_uniform_assigment(vicall_t *root, vicall_t *call, slot_t slot,
                        uint8_t *data, void *usrptr)
{
	char **args = usrptr;
	char buffer[256];
	char buffer2[256];
	vifunc_slot_to_name(root->parent, slot, buffer, "_", NULL);
	vifunc_slot_to_name(root->type, slot_pop(slot), buffer2, ".", "_in");

	str_catf(args, "%s = costum_materials.info[matid].%s;\n", buffer2, buffer);
}

static void _generate_uniform_data(vicall_t *root, vicall_t *call, slot_t slot,
                        uint8_t *data, void *usrptr)
{
	char **output = usrptr;
	if (data)
	{
		memcpy(*output, data, call->type->builtin_size);
	}
	*output += call->type->builtin_size;
}

void mat_changed(vicall_t *call, slot_t slot, void *usrptr)
{
	struct mat_type *type;

	uint32_t unpadded = vicall_get_size(call);
	uint32_t size = ceilf(((float)unpadded) / 16) * 16;
	mat_t *self = usrptr;
	uint8_t *data = alloca(size);
	uint8_t *end = data;

	vicall_foreach_unlinked_input(call, _generate_uniform_data, &end);
	
	type = &g_mat_types[self->type];
	if (type->ubo)
	{
		glBindBuffer(GL_UNIFORM_BUFFER, type->ubo); glerr();
		glBufferSubData(GL_UNIFORM_BUFFER, size * self->id, unpadded, data); glerr();
	}
}

uint32_t mat_get_group(mat_t *self)
{
	return vifunc_get(self->call->type, "pbr")->type->id;
}

bool_t mat_is_transparent(mat_t *self)
{
	return mat_get_group(self) == ref("transparent");
}

void mat_type_inputs(vifunc_t *func)
{
	vicall_t *resolution = vifunc_get(func, "resolution");
	vicall_t *tex_space = vifunc_get(func, "tex_space");
	vicall_t *time = vifunc_get(func, "time");
	vicall_t *screen_space = vifunc_get(func, "screen_space");
	vicall_t *world_space = vifunc_get(func, "world_space");
	vicall_t *view_space = vifunc_get(func, "view_space");
	vicall_t *vertex_normal = vifunc_get(func, "vertex_normal");
	vicall_t *obj_pos = vifunc_get(func, "obj_pos");
	float y = 100.0f;
	float inc = 100.0f;
	if (!resolution)
	{
		vicall_new(func, vil_get(&g_mat_ctx, ref("vec2")),
		           "resolution", vec2(0, y += inc), ~0, V_OUT | V_LINKED);
	}
	if (!tex_space)
	{
		vicall_new(func, vil_get(&g_mat_ctx, ref("vec2")),
		           "tex_space", vec2(0, y += inc), ~0, V_OUT | V_LINKED);
	}
	if (!time)
	{
		vicall_new(func, vil_get(&g_mat_ctx, ref("number")),
		           "time", vec2(0, y += inc), ~0, V_OUT | V_LINKED);
	}
	if (!screen_space)
	{
		vicall_new(func, vil_get(&g_mat_ctx, ref("vec2")),
		           "screen_space", vec2(0, y += inc), ~0, V_OUT | V_LINKED);
	}
	if (!world_space)
	{
		vicall_new(func, vil_get(&g_mat_ctx, ref("vec3")),
		           "world_space", vec2(0, y += inc), ~0, V_OUT | V_LINKED);
	}
	if (!view_space)
	{
		vicall_new(func, vil_get(&g_mat_ctx, ref("vec3")),
		           "view_space", vec2(0, y += inc), ~0, V_OUT | V_LINKED);
	}
	if (!vertex_normal)
	{
		vicall_new(func, vil_get(&g_mat_ctx, ref("vec3")),
		           "vertex_normal", vec2(0, y += inc), ~0, V_OUT | V_LINKED);
	}
	if (!obj_pos)
	{
		vicall_new(func, vil_get(&g_mat_ctx, ref("vec3")),
		           "obj_pos", vec2(0, y += inc), ~0, V_OUT | V_LINKED);
	}
}

void mat_type_changed(vifunc_t *func, void *usrptr)
{
	uint32_t size, output_type, i, id;
	vicall_t *call = vifunc_get(func, "pbr");
	struct mat_type *type = usrptr;
	char *gbuffer;

	char *name = str_new(64);
	char *name2 = str_new(64);
	char *q_name = str_new(64);
	char *q_name2 = str_new(64);
	char *s_name = str_new(64);
	char *s_name2 = str_new(64);
	char *d_name = str_new(64);
	char *d_name2 = str_new(64);
	char *t_name = str_new(64);
	char *t_name2 = str_new(64);

	char *select = str_new(64);
	char *query = str_new(64);
	char *depth = str_new(64);
	char *transp = str_new(64);

	mat_type_inputs(func);

	size = ceilf(((float)vicall_get_size(type->tmp_material_instance)) / 16) * 16;
	if (!type->ubo)
	{
		glGenBuffers(1, &type->ubo); glerr();
	}
	glBindBuffer(GL_UNIFORM_BUFFER, type->ubo); glerr();
	if (type->ubo_size < size)
	{
		glBufferData(GL_UNIFORM_BUFFER, size * 128, NULL, GL_DYNAMIC_DRAW); glerr();
		type->ubo_size = size;
	}

	output_type = call->type->id;

	gbuffer = str_new(128);
	str_cat(&gbuffer, "#include \"common.glsl\"\n");
	vil_foreach_func(call->type->ctx, (vil_func_cb)material_generate_struct, &gbuffer);
	vil_foreach_func(call->type->ctx, (vil_func_cb)material_generate_func, &gbuffer);

	str_cat(&gbuffer, "struct costum_material_t {\n");
	vicall_foreach_unlinked_input(type->tmp_material_instance, _generate_uniform, &gbuffer);
	for (i = size; i < size; i += 4)
	{
		str_catf(&gbuffer, "	uint _padding_%d;\n", i + 4);
	}
	str_cat(&gbuffer, "};\n");

	str_cat(&gbuffer,
		"layout(std140) uniform cms_t {\n"
		"	costum_material_t info[128];\n"
		"} costum_materials;\n"
		"#ifdef QUERY_PASS\n"
		"layout (location = 0) out vec4 TILES0;\n"
		"layout (location = 1) out vec4 TILES1;\n"
		"layout (location = 2) out vec4 TILES2;\n"
		"layout (location = 3) out vec4 TILES3;\n"
	);
	str_cat(&gbuffer,
		"#elif defined(SELECT_PASS)\n"
		"layout (location = 0) out vec4 IDS;\n"
		"layout (location = 1) out vec2 GeomID;\n"
		"#elif defined(SHADOW_PASS)\n"
		"layout (location = 0) out vec4 Depth;\n"
	);
	str_cat(&gbuffer,
		"#elif defined(TRANSPARENCY_PASS)\n"
		"layout (location = 0) out vec4 FragColor;\n"
		"BUFFER { sampler2D color; } refr;\n"
		"#else\n"
		"layout (location = 0) out vec4 Alb;\n"
		"layout (location = 1) out vec4 NN;\n"
		"layout (location = 2) out vec4 MR;\n"
		"layout (location = 3) out vec3 Emi;\n");

	if (output_type == ref("decal"))
	{
		str_cat(&gbuffer,
			"BUFFER { sampler2D depth; } gbuffer;\n"
		);
	}

	str_cat(&gbuffer,
		"#endif\n"
		"void main(void) {\n"
		"	uint tile_array[8];\n"
		"	uint tile_num = 0u;\n"
	);

	vifunc_foreach_call(func, false, true, true, false,
	                    (vil_call_cb)material_generate_precall, &gbuffer);

	if (output_type == ref("transparent"))
	{
		str_cat(&gbuffer,
			"#ifdef QUERY_PASS\n"
			"	if (((int(gl_FragCoord.x) + int(gl_FragCoord.y)) & 1) == 0)\n"
			"		discard;\n"
			"#endif\n"
		);
	}

	if (output_type == ref("decal"))
	{
		str_cat(&gbuffer,
			"#if defined(QUERY_PASS)\n"
				"tex_space_in = texcoord;\n"
		        "world_space_in = vertex_world_position;\n"
		        "view_space_in = vertex_position;\n"
		);
		str_cat(&gbuffer,
			"#elif !defined(SELECT_PASS) && !defined(SHADOW_PASS) && !defined(TRANSPARENCY_PASS)\n"
			"	float depth = textureLod(gbuffer.depth, pixel_pos(), 0.0).r;\n"
			"	if (depth > gl_FragCoord.z) discard;\n"
			"	vec4 w_pos = (camera(model)*vec4(get_position(gbuffer.depth), 1.0));\n"
			"	vec3 m_pos = (inverse(model) * w_pos).xyz;\n"
			"	vec3 diff = abs(m_pos);\n"
			"	if (diff.x > 0.5 || diff.y > 0.5 || diff.z > 0.5) discard;\n"
			"	tex_space_in = m_pos.xy - 0.5;\n"
			"#endif\n"
		);
	}
	else
	{
		str_cat(&gbuffer,
				"tex_space_in = texcoord;\n"
		        "world_space_in = vertex_world_position;\n"
		        "view_space_in = vertex_position;\n"
		        );
	}

	str_cat(&gbuffer,
	        "resolution_in = screen_size;\n"
	        "#ifdef QUERY_PASS\n"
	        "time_in = scene.time + 10.0 * 0.01;\n"
	        "#else\n"
	        "time_in = scene.time;\n"
	        "#endif\n"
	        );

	vicall_foreach_unlinked_input(type->tmp_material_instance,
	                              _generate_uniform_assigment, &gbuffer);

	if (output_type == ref("parallax"))
	{
		str_cat(&gbuffer, "/* HEIGHT PRECOMPUTATION */\n");
		vicall_iterate_dependencies(call, ref("height"), ~0,
		                            (vil_link_cb)material_generate_assignments,
		                            (vil_call_cb)material_generate_call, &gbuffer);
		str_cat(&gbuffer, "/* --------------------- */\n");
		str_cat(&gbuffer,
				"float scl = ((1.0 - pbr_in.height) * .05);\n"
				"vec3 newpoint = (normalize(view_space_in) * scl);\n"
				"vec3 flatnorm = (newpoint * TM());\n"
				"flatnorm.y = -flatnorm.y;\n"
		        "tex_space_in += flatnorm.xy;\n"
		        "view_space_in += flatnorm;\n"
				"vec4 proj = camera(projection) * vec4(view_space_in, 1.0);\n"
				"gl_FragDepth = (proj.z / proj.w) * 0.5 + 0.5;\n"
		        );
	}

	/* RUN BODY */
	vicall_iterate_dependencies(call, ~0, ~0,
	                            (vil_link_cb)material_generate_assignments,
	                            (vil_call_cb)material_generate_call, &gbuffer);
	if (type->code)
	{
		str_cat(&gbuffer, type->code);
	}

	if (output_type != ref("transparent") && output_type != ref("decal"))
	{
		str_cat(&gbuffer,
			"#if defined(SELECT_PASS) || defined(SHADOW_PASS)\n"
			"	if (pbr_in.albedo.a < 0.7) discard;\n"
			"#endif\n");
	}
	str_cat(&gbuffer,
		"#ifdef QUERY_PASS\n"
		"	TILES0.r = float(tile_array[0] % 256u);\n"
		"	TILES0.g = float(tile_array[0] / 256u);\n"
		"	TILES0.b = float(tile_array[1] % 256u);\n"
		"	TILES0.a = float(tile_array[1] / 256u);\n"
		"	TILES1.r = float(tile_array[2] % 256u);\n"
		"	TILES1.g = float(tile_array[2] / 256u);\n"
		"	TILES1.b = float(tile_array[3] % 256u);\n"
		"	TILES1.a = float(tile_array[3] / 256u);\n"
	);
	str_cat(&gbuffer,
		"	TILES2.r = float(tile_array[4] % 256u);\n"
		"	TILES2.g = float(tile_array[4] / 256u);\n"
		"	TILES2.b = float(tile_array[5] % 256u);\n"
		"	TILES2.a = float(tile_array[5] / 256u);\n"
		"	TILES3.r = float(tile_array[6] % 256u);\n"
		"	TILES3.g = float(tile_array[6] / 256u);\n"
		"	TILES3.b = float(tile_array[7] % 256u);\n"
		"	TILES3.a = float(tile_array[7] / 256u);\n"
	);
	str_cat(&gbuffer,
		"	TILES0 /= 255.0;\n"
		"	TILES1 /= 255.0;\n"
		"	TILES2 /= 255.0;\n"
		"	TILES3 /= 255.0;\n"
		"#elif defined(SELECT_PASS)\n"
		"	IDS.xy = vec2(float(id.x) / 255.0, float(id.y) / 255.0);\n"
		"	IDS.zw = vec2(poly_id);\n"
		"#elif defined(SHADOW_PASS)\n"
		"	Depth = encode_float_rgba(gl_FragCoord.z);\n"
		"#elif defined(TRANSPARENCY_PASS)\n");
		if (output_type == ref("transparent"))
		{
			str_cat(&gbuffer,
				"	vec2 pp = pixel_pos();\n"
				"	vec4 final = vec4(1.0);\n"
				"	if (pbr_in.roughness > 0.0) {\n"
				"		vec3 nor = get_normal(pbr_in.normal);\n"
				"		vec2 coord = pp + nor.xy * (pbr_in.roughness * 0.03);\n"
				"		float mip = clamp(pbr_in.roughness * 3.0, 0.0, 3.0);\n"
				"		vec3 refracted = textureLod(refr.color, coord.xy, mip).rgb;\n"
				"		refracted = refracted * (1.0 - pbr_in.absorb);\n"
				"		final = vec4(mix(final.rgb, refracted, final.a), 1.0);\n"
				"	} else {\n"
				"	}\n"
				"	final.rgb += pbr_in.emissive;\n"
				"	FragColor = final;\n");
		}
		str_cat(&gbuffer, "#else\n");

	if (output_type == ref("decal"))
	{
		str_cat(&gbuffer,
			"	vec3 norm = ((camera(view) * model) * vec4(pbr_in.normal, 0.0)).xyz;\n");
	}
	else
	{
		str_cat(&gbuffer,
			"	vec3 norm = get_normal(pbr_in.normal);\n");
	}

	str_cat(&gbuffer,
		"	MR.g = pbr_in.roughness;\n"
		"	NN.rg = encode_normal(norm);\n");

	if (output_type == ref("transparent"))
	{
		str_cat(&gbuffer,
			"	MR.r = 1.f;\n");

	}
	else if (output_type == ref("decal"))
	{
		str_cat(&gbuffer,
			"	MR.r = pbr_in.metalness;\n"
			"	Alb = pbr_in.albedo.rgba;\n"
			"	Emi = pbr_in.emissive;\n"
			"	MR.a = pbr_in.albedo.a;\n"
			"	NN.a = 0.0;\n");
	}
	else
	{
		str_cat(&gbuffer,
			"	if (pbr_in.albedo.a < 0.7) discard;\n"
			"	MR.r = pbr_in.metalness;\n"
			"	Alb = vec4(pbr_in.albedo.rgb, receive_shadows ? 1.0 : 0.5);\n"
			"	Emi = pbr_in.emissive;\n");
	}
	str_cat(&gbuffer,
		"#endif\n"
		"}\n");

	/* if (output_type == ref("opaque")) */
	/* { */
	/* 	const char *line = gbuffer; */
	/* 	uint32_t line_num = 1u; */
	/* 	while (true) */
	/* 	{ */
	/* 		char *next_line = strchr(line, '\n'); */
	/* 		if (next_line) */
	/* 		{ */
	/* 			printf("%d	%.*s\n", line_num, (int)(next_line - line), line); */
	/* 			line = next_line+1; */
	/* 		} */
	/* 		else */
	/* 		{ */
	/* 			printf("%d	%s\n", line_num, line); */
	/* 			break; */
	/* 		} */
	/* 		line_num++; */
	/* 	} */
	/* } */

	c_render_device(&SYS)->shader = NULL;
	c_render_device(&SYS)->frag_bound = NULL;

	str_cat(&select, "#define SELECT_PASS\n");
	str_cat(&select, gbuffer);

	str_cat(&query, "#define QUERY_PASS\n");
	str_cat(&query, gbuffer);

	str_cat(&depth, "#define SHADOW_PASS\n");
	str_cat(&depth, gbuffer);

	str_cat(&transp, "#define TRANSPARENCY_PASS\n");
	str_cat(&transp, gbuffer);

	id = type - g_mat_types;
	str_catf(&d_name, "depth#%d", id);
	str_catf(&d_name2, "depth#%d.glsl", id);
	str_catf(&q_name, "query_mips#%d", id);
	str_catf(&q_name2, "query_mips#%d.glsl", id);
	str_catf(&s_name, "select#%d", id);
	str_catf(&s_name2, "select#%d.glsl", id);
	str_catf(&name, "gbuffer#%d", id);
	str_catf(&name2, "gbuffer#%d.glsl", id);
	str_catf(&t_name, "transparency#%d", id);
	str_catf(&t_name2, "transparency#%d.glsl", id);

	if (type->src)
	{
		if (strcmp(type->src, gbuffer))
		{
			fs_t *fs;
			shader_add_source(name2, (unsigned char*)gbuffer, str_len(gbuffer));
			shader_add_source(q_name2, (unsigned char*)query, str_len(query));
			shader_add_source(d_name2, (unsigned char*)depth, str_len(depth));
			shader_add_source(s_name2, (unsigned char*)select, str_len(select));
			shader_add_source(t_name2, (unsigned char*)transp, str_len(transp));

			str_free(type->src);
			type->src = gbuffer;
			fs = fs_new("gbuffer");
			fs_update_variation(fs, id);

			fs = fs_new("query_mips");
			fs_update_variation(fs, id);

			fs = fs_new("depth");
			fs_update_variation(fs, id);

			fs = fs_new("select");
			fs_update_variation(fs, id);

			fs = fs_new("transparency");
			fs_update_variation(fs, id);
		}
		else
		{
			str_free(gbuffer);
		}
		glerr();
	}
	else
	{
		fs_t *fs;

		shader_add_source(name2, (unsigned char*)gbuffer, str_len(gbuffer));
		shader_add_source(q_name2, (unsigned char*)query, str_len(query));
		shader_add_source(d_name2, (unsigned char*)depth, str_len(depth));
		shader_add_source(s_name2, (unsigned char*)select, str_len(select));
		shader_add_source(t_name2, (unsigned char*)transp, str_len(transp));

		type->src = gbuffer;
		fs = fs_new("gbuffer");
		fs_push_variation(fs, name);

		fs = fs_new("query_mips");
		fs_push_variation(fs, q_name);

		fs = fs_new("depth");
		fs_push_variation(fs, d_name);

		fs = fs_new("select");
		fs_push_variation(fs, s_name);

		fs = fs_new("transparency");
		fs_push_variation(fs, t_name);
	}

	str_free(query);
	str_free(q_name);
	str_free(q_name2);
	str_free(select);
	str_free(s_name);
	str_free(s_name2);
	str_free(depth);
	str_free(d_name);
	str_free(d_name2);
	str_free(t_name);
	str_free(t_name2);
	str_free(name);
	str_free(name2);
}

void _mat_type_changed(struct mat_type *type)
{
	mat_type_changed(type->mat, type);
}

static void init_type(uint32_t tid, const char *type_name)
{
	char buffer[256];
	struct mat_type *type;
	uint32_t i;
	vifunc_t *mat;
	vifunc_t *placeholder;

	if (!g_mat_ctx.funcs)
		materials_init_vil();

	type = &g_mat_types[tid];
	strncpy(type->name, type_name, sizeof(type->name) - 1);

	sprintf(buffer, "_%.*s", (int)(sizeof(type->name) - 1), type->name);

	type->mat = vil_get(&g_mat_ctx, ref(buffer));
	if (!type->mat)
	{
		type->mat = vifunc_new(&g_mat_ctx, buffer, NULL, 0, false);
	}
	for (i = 0; i < 128; i++)
	{
		type->next_free[i] = i + 1;
	}
	type->free_position = 0;

	mat = type->mat;
	mat->usrptr = type;
	if (!vifunc_get(mat, "pbr"))
	{
		sprintf(buffer, "%.*s.vil", (int)sizeof(type->name) - 1, type->name);
		if (!vifunc_load(mat, buffer))
		{
			printf("Type '%s' doesn't exist or is incomplete.\n", type_name);
			exit(1);
		}
	}
	vifunc_watch(mat, mat_type_changed, type);

	sprintf(buffer, "_placeholder_%.*s", (int)sizeof(type->name) - 1, type->name);

	placeholder = vifunc_new(&g_mat_ctx, buffer, NULL, 0, false);
	if (placeholder)
	{
		placeholder->usrptr = type;
		type->tmp_material_instance = vicall_new(placeholder, mat, type->name,
												 vec2(100, 10), ~0, V_BOTH);

		loader_push(g_candle->loader, (loader_cb)_mat_type_changed, type, NULL);
	}

}


void matcall_set_number(vicall_t *call, uint32_t ref, float value)
{ vicall_set_arg(call, ref, &value); }
void matcall_set_vec2(vicall_t *call, uint32_t ref, vec2_t value)
{ vicall_set_arg(call, ref, &value); }
void matcall_set_vec3(vicall_t *call, uint32_t ref, vec3_t value)
{ vicall_set_arg(call, ref, &value); }
void matcall_set_vec4(vicall_t *call, uint32_t ref, vec4_t value)
{ vicall_set_arg(call, ref, &value); }

void materials_init_vil()
{
	vicall_t *r, *g, *b, *a, *normal, *albedo, *roughn, *color;
	vil_t *ctx = &g_mat_ctx;
	vifunc_t *tint, *tnum, *v2, *v3, *v4, *tcol, *visin, *virange;
	vifunc_t *vimul, *vimix, *vimix2, *vimix3, *vimix4, *viadd, *tprev, *ttex;
	vifunc_t *tflip, *tprop, *opaque, *parallax, *transparent, *decal;
	vicall_t *mini, *maxi, *mino, *maxo;

	if (g_mat_ctx.funcs)
	    return;

	vil_init(ctx);

	ctx->builtin_load_func = (vifunc_load_cb)_code_load;
	ctx->builtin_save_func = (vifunc_save_cb)_code_save;

	tint = vifunc_new(ctx, "integer", (vifunc_gui_cb)_vint_gui,
			                    sizeof(int32_t), true);
	tint->builtin_load_call = (vicall_load_cb)_int_load;
	tint->builtin_save_call = (vicall_save_cb)_int_save;

	tnum = vifunc_new(ctx, "number", (vifunc_gui_cb)_number_gui,
			                    sizeof(float), true);
	tnum->builtin_load_call = (vicall_load_cb)_number_load;
	tnum->builtin_save_call = (vicall_save_cb)_number_save;

	v2 = vifunc_new(ctx, "vec2", NULL, sizeof(vec2_t), true);
		r = vicall_new(v2, tnum, "x", vec2(40, 10),  offsetof(vec2_t, x), V_BOTH);
		g = vicall_new(v2, tnum, "y", vec2(40, 260), offsetof(vec2_t, y), V_BOTH);
		vicall_color(r, vec4(1, 0, 0, 1));
		vicall_color(g, vec4(0, 1, 0, 1));

	v3 = vifunc_new(ctx, "vec3", NULL, sizeof(vec3_t), true);
		r = vicall_new(v3, tnum, "x", vec2(40, 10),  offsetof(vec3_t, x), V_BOTH);
		g = vicall_new(v3, tnum, "y", vec2(40, 260), offsetof(vec3_t, y), V_BOTH);
		b = vicall_new(v3, tnum, "z", vec2(40, 360), offsetof(vec3_t, z), V_BOTH);
		vicall_color(r, vec4(1, 0, 0, 1));
		vicall_color(g, vec4(0, 1, 0, 1));
		vicall_color(b, vec4(0, 0, 1, 1));

	tcol = vifunc_new(ctx, "_color_picker", (vifunc_gui_cb)_color_gui,
	                            0, false);

	v4 = vifunc_new(ctx, "vec4", NULL, sizeof(vec4_t), true);
		vicall_new(v4, tcol, "color", vec2(100, 10), 0, V_IN);
		r = vicall_new(v4, tnum, "x", vec2(40, 10),  offsetof(vec4_t, x), V_BOTH);
		g = vicall_new(v4, tnum, "y", vec2(40, 260), offsetof(vec4_t, y), V_BOTH);
		b = vicall_new(v4, tnum, "z", vec2(40, 360), offsetof(vec4_t, z), V_BOTH);
		a = vicall_new(v4, tnum, "w", vec2(40, 360), offsetof(vec4_t, w), V_BOTH);
		vicall_color(r, vec4(1, 0, 0, 1));
		vicall_color(g, vec4(0, 1, 0, 1));
		vicall_color(b, vec4(0, 0, 1, 1));
		vicall_color(a, vec4(1, 1, 1, 1));

	visin = vifunc_new(ctx, "sin", NULL, 0, false);
		vicall_new(visin, tnum, "x", vec2(40, 10),  ~0, V_IN);
		vicall_new(visin, tnum, "result", vec2(40, 360), ~0, V_OUT);

	virange = vifunc_new(ctx, "range", NULL, 0, false);
		vicall_new(virange, tnum, "x", vec2(40, 10),  ~0, V_IN);
		mini = vicall_new(virange, tnum, "min_in", vec2(40, 10),  ~0, V_IN);
		maxi = vicall_new(virange, tnum, "max_in", vec2(40, 10),  ~0, V_IN);
		mino = vicall_new(virange, tnum, "min_out", vec2(40, 10),  ~0, V_IN);
		maxo = vicall_new(virange, tnum, "max_out", vec2(40, 10),  ~0, V_IN);
		vicall_new(virange, tnum, "result", vec2(40, 360), ~0, V_OUT);
		matcall_set_number(mini, ~0,-1.0f);
		matcall_set_number(maxi, ~0, 1.0f);
		matcall_set_number(mino, ~0, 0.0f);
		matcall_set_number(maxo, ~0, 1.0f);

	vimul = vifunc_new(ctx, "mul", NULL, 0, false);
		vicall_new(vimul, tnum, "x", vec2(40, 10),  ~0, V_IN);
		r = vicall_new(vimul, tnum, "y", vec2(40, 10),  ~0, V_IN);
		vicall_new(vimul, tnum, "result", vec2(40, 360), ~0, V_OUT);
		matcall_set_number(r, ~0, 1.0f);

	vimix = vifunc_new(ctx, "mix_num", NULL, 0, false);
		vicall_new(vimix, tnum, "x", vec2(40, 10),  ~0, V_IN);
		vicall_new(vimix, tnum, "y", vec2(40, 10),  ~0, V_IN);
		vicall_new(vimix, tnum, "a", vec2(40, 10),  ~0, V_IN);
		vicall_new(vimix, tnum, "result", vec2(40, 360), ~0, V_OUT);

	vimix2 = vifunc_new(ctx, "mix_vec2", NULL, 0, false);
		vicall_new(vimix2, v2, "x", vec2(40, 10),  ~0, V_IN);
		vicall_new(vimix2, v2, "y", vec2(40, 10),  ~0, V_IN);
		vicall_new(vimix2, tnum, "a", vec2(40, 10),  ~0, V_IN);
		vicall_new(vimix2, v2, "result", vec2(40, 360), ~0, V_OUT);

	vimix3 = vifunc_new(ctx, "mix_vec3", NULL, 0, false);
		vicall_new(vimix3, v3, "x", vec2(40, 10),  ~0, V_IN);
		vicall_new(vimix3, v3, "y", vec2(40, 10),  ~0, V_IN);
		vicall_new(vimix3, tnum, "a", vec2(40, 10),  ~0, V_IN);
		vicall_new(vimix3, v3, "result", vec2(40, 360), ~0, V_OUT);

	vimix4 = vifunc_new(ctx, "mix_vec4", NULL, 0, false);
		vicall_new(vimix4, v4, "x", vec2(40, 10),  ~0, V_IN);
		vicall_new(vimix4, v4, "y", vec2(40, 10),  ~0, V_IN);
		vicall_new(vimix4, tnum, "a", vec2(40, 10),  ~0, V_IN);
		vicall_new(vimix4, v4, "result", vec2(40, 360), ~0, V_OUT);

	viadd = vifunc_new(ctx, "add", NULL, 0, false);
		vicall_new(viadd, tnum, "x", vec2(40, 10),  ~0, V_IN);
		vicall_new(viadd, tnum, "y", vec2(40, 10),  ~0, V_IN);
		vicall_new(viadd, tnum, "result", vec2(40, 360), ~0, V_OUT);

	tprev = vifunc_new(ctx, "_tex_previewer",
	                             (vifunc_gui_cb)_tex_previewer_gui, 0, false);

	ttex = vifunc_new(ctx, "texture", NULL,
	                            sizeof(_mat_sampler_t), false);
		vicall_new(ttex, tprev, "preview", vec2(100, 10), 0, V_IN);
		vicall_new(ttex, v2, "coord", vec2(100, 10), offsetof(_mat_sampler_t, coord), V_IN);
		vicall_new(ttex, tint, "tile", vec2(100, 10), offsetof(_mat_sampler_t, tile), V_IN | V_HID);
		vicall_new(ttex, v2, "size", vec2(100, 10), offsetof(_mat_sampler_t, size), V_IN | V_HID);
		vicall_new(ttex, v4, "result", vec2(300, 10), offsetof(_mat_sampler_t, result), V_OUT);
	ttex->builtin_load_call = (vicall_load_cb)_texture_load;
	ttex->builtin_save_call = (vicall_save_cb)_texture_save;

	tflip = vifunc_new(ctx, "flipbook", NULL, 0, false);
		vicall_new(tflip, v2, "coord", vec2(100, 10), ~0, V_IN);
		vicall_new(tflip, tint, "rows", vec2(100, 10), ~0, V_IN);
		vicall_new(tflip, tint, "columns", vec2(100, 10), ~0, V_IN);
		vicall_new(tflip, tnum, "frame", vec2(100, 10), ~0, V_IN);
		vicall_new(tflip, v2, "result", vec2(300, 10), ~0, V_OUT);

	tprop = vifunc_new(ctx, "property4", NULL, 0, false);
		vicall_new(tprop, ttex, "texture", vec2(100, 10), ~0, V_IN);
		color = vicall_new(tprop, v4, "color", vec2(200, 10), ~0, V_IN);
		vicall_new(tprop, vimix4, "mix", vec2(300, 10), ~0, V_BOTH);
		vicall_new(tprop, tnum, "blend", vec2(400, 10), ~0, V_IN);
		vifunc_link(tprop, ref("texture.result"), ref("mix.x"));
		vifunc_link(tprop, ref("color"), ref("mix.y"));
		vifunc_link(tprop, ref("blend"), ref("mix.a"));
		matcall_set_vec4(color, ~0, vec4(0.5f, 0.5f, 0.5f, 1.0f));

	tprop = vifunc_new(ctx, "property3", NULL, 0, false);
		vicall_new(tprop, ttex, "texture", vec2(100, 10), ~0, V_IN);
		color = vicall_new(tprop, v3, "color", vec2(200, 10), ~0, V_IN);
		vicall_new(tprop, vimix3, "mix", vec2(300, 10), ~0, V_BOTH);
		vicall_new(tprop, tnum, "blend", vec2(400, 10), ~0, V_IN);
		vifunc_link(tprop, ref("texture.result.x"), ref("mix.x.x"));
		vifunc_link(tprop, ref("texture.result.y"), ref("mix.x.y"));
		vifunc_link(tprop, ref("texture.result.z"), ref("mix.x.z"));
		vifunc_link(tprop, ref("color"), ref("mix.y"));
		vifunc_link(tprop, ref("blend"), ref("mix.a"));
		matcall_set_vec3(color, ~0, Z3);

	tprop = vifunc_new(ctx, "property2", NULL, 0, false);
		vicall_new(tprop, ttex, "texture", vec2(100, 10), ~0, V_IN);
		color = vicall_new(tprop, v2, "color", vec2(200, 10), ~0, V_IN);
		vicall_new(tprop, vimix2, "mix", vec2(300, 10), ~0, V_BOTH);
		vicall_new(tprop, tnum, "blend", vec2(400, 10), ~0, V_IN);
		vifunc_link(tprop, ref("texture.result.x"), ref("mix.x.x"));
		vifunc_link(tprop, ref("texture.result.y"), ref("mix.x.y"));
		vifunc_link(tprop, ref("color"), ref("mix.y"));
		vifunc_link(tprop, ref("blend"), ref("mix.a"));
		matcall_set_vec2(color, ~0, Z2);

	tprop = vifunc_new(ctx, "property1", NULL, 0, false);
		vicall_new(tprop, ttex, "texture", vec2(100, 10), ~0, V_IN);
		color = vicall_new(tprop, tnum, "value", vec2(200, 10), ~0, V_IN);
		vicall_new(tprop, vimix, "mix", vec2(300, 10), ~0, V_BOTH);
		vicall_new(tprop, tnum, "blend", vec2(400, 10), ~0, V_IN);
		vifunc_link(tprop, ref("texture.result.x"), ref("mix.x"));
		vifunc_link(tprop, ref("value"), ref("mix.y"));
		vifunc_link(tprop, ref("blend"), ref("mix.a"));
		matcall_set_number(color, ~0, 0.0f);

	opaque = vifunc_new(ctx, "opaque", NULL, sizeof(struct opaque), false);
		albedo = vicall_new(opaque, v4, "albedo", vec2(100, 10),
		                    offsetof(struct opaque, albedo), V_IN);
		normal = vicall_new(opaque, v3, "normal", vec2(200, 10),
		                    offsetof(struct opaque, normal), V_IN);
		roughn = vicall_new(opaque, tnum, "roughness", vec2(300, 10),
		                    offsetof(struct opaque, roughness), V_IN);
		vicall_new(opaque, tnum, "metalness", vec2(400, 10),
		           offsetof(struct opaque, metalness), V_IN);
		vicall_new(opaque, v3, "emissive", vec2(500, 10),
		           offsetof(struct opaque, emissive), V_IN);
		matcall_set_vec4(albedo, ~0, vec4(0.5f, 0.5f, 0.5f, 1.0f));
		matcall_set_vec3(normal, ~0, vec3(0.5f, 0.5f, 1.0f));
		matcall_set_number(roughn, ~0, 0.5f);

	parallax = vifunc_new(ctx, "parallax", NULL, sizeof(struct parallax), false);
		albedo = vicall_new(parallax, v4, "albedo", vec2(100, 10),
		                    offsetof(struct parallax, albedo), V_IN);
		normal = vicall_new(parallax, v3, "normal", vec2(200, 10),
		                    offsetof(struct parallax, normal), V_IN);
		roughn = vicall_new(parallax, tnum, "roughness", vec2(300, 10),
		                    offsetof(struct parallax, roughness), V_IN);
		vicall_new(parallax, tnum, "metalness", vec2(400, 10),
		           offsetof(struct parallax, metalness), V_IN);
		vicall_new(parallax, v3, "emissive", vec2(500, 10),
		           offsetof(struct parallax, emissive), V_IN);
		vicall_new(parallax, tnum, "height", vec2(600, 10),
		           offsetof(struct parallax, height), V_IN);
		matcall_set_vec4(albedo, ~0, vec4(0.5f, 0.5f, 0.5f, 1.0f));
		matcall_set_vec3(normal, ~0, vec3(0.5f, 0.5f, 1.0f));
		matcall_set_number(roughn, ~0, 0.5f);



	transparent = vifunc_new(ctx, "transparent", NULL, sizeof(struct transparent), false);
		vicall_new(transparent, v3, "absorb", vec2(200, 10),
		           offsetof(struct transparent, absorb), V_IN);
		vicall_new(transparent, v3, "normal", vec2(200, 10),
		           offsetof(struct transparent, normal), V_IN);
		vicall_new(transparent, tnum, "roughness", vec2(300, 10),
		           offsetof(struct transparent, roughness), V_IN);
		vicall_new(transparent, v3, "emissive", vec2(500, 10),
		           offsetof(struct transparent, emissive), V_IN);

	decal = vifunc_new(ctx, "decal", NULL, sizeof(struct decal), false);
		albedo = vicall_new(decal, v4, "albedo", vec2(100, 10),
		                    offsetof(struct decal, albedo), V_IN);
		normal = vicall_new(decal, v3, "normal", vec2(200, 10),
		                    offsetof(struct decal, normal), V_IN);
		roughn = vicall_new(decal, tnum, "roughness", vec2(300, 10),
		                    offsetof(struct decal, roughness), V_IN);
		vicall_new(decal, tnum, "metalness", vec2(400, 10),
		           offsetof(struct decal, metalness), V_IN);
		vicall_new(decal, v3, "emissive", vec2(500, 10),
		           offsetof(struct decal, emissive), V_IN);
		matcall_set_vec4(albedo, ~0, vec4(0.5f, 0.5f, 0.5f, 1.0f));
		matcall_set_vec3(normal, ~0, vec3(0.5f, 0.5f, 1.0f));
		matcall_set_number(roughn, ~0, 0.5f);

}

void mat1i(mat_t *self, uint32_t ref, int32_t value)
{
	vicall_set_arg(self->call, ref, &value);
}

void mat1f(mat_t *self, uint32_t ref, float value)
{
	vicall_set_arg(self->call, ref, &value);
}

void mat2f(mat_t *self, uint32_t ref, vec2_t value)
{
	vicall_set_arg(self->call, ref, &value);
}

void mat3f(mat_t *self, uint32_t ref, vec3_t value)
{
	vicall_set_arg(self->call, ref, &value);
}

void mat4f(mat_t *self, uint32_t ref, vec4_t value)
{
	vicall_set_arg(self->call, ref, &value);
}

void mat1s(mat_t *self, uint32_t ref, const char *value)
{
	char *string = str_new(64);
	str_cat(&string, value);
	vicall_set_arg(self->call, ref, &string);
}

void mat1t(mat_t *self, uint32_t ref, texture_t *value)
{
	_mat_sampler_t sampler;
	uint32_t i;

	for (i = 0; !value->bufs[0].indir_n; i++)
	{
		SDL_Delay(100);
		if (i == 16)
		{
			printf("giving up on %s\n", (char*)value->usrptr);
			break;
		}
	}
	sampler.texture = value;
	sampler.size.x = value->width;
	sampler.size.y = value->height;
	sampler.tile = value->bufs[0].indir_n;
	vicall_set_arg(self->call, ref, &sampler);
}
