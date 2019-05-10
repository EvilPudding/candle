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

static void init_type(uint32_t tid);

const char *g_type_names[] = {"default"};
struct mat_type {
	vifunc_t *mat;
	uint32_t ubo;
	uint32_t ubo_size;
	char *src;

	/* used for kernel generation */
	vicall_t *tmp_material_instance;
	uint32_t next_free[128];
	uint32_t free_position;
};

static struct mat_type g_mat_types[32];
/* static uint32_t g_mat_types_num; */

uint32_t material_get_ubo(uint32_t mat_type)
{
	return g_mat_types[mat_type].ubo;
}

void _mat_changed(mat_t *mat)
{
	mat_changed(mat->call, (slot_t){0}, mat);
}

mat_t *mat_new(const char *name)
{
	mat_t *self = calloc(1, sizeof *self);

	char buffer[256];
	snprintf(buffer, sizeof(buffer), "_material%d_sandbox", g_mats_num);

	strncpy(self->name, name, sizeof(self->name));

	struct mat_type *type = &g_mat_types[self->type];
	self->global_id = g_mats_num++;
	self->type = 0;
	if (!type->mat)
	{
		init_type(self->type);
	}
	self->id = type->free_position;
	type->free_position = type->next_free[type->free_position];

	if (self->id == 128)
		printf("Too many material instances for type %d\n", self->type);

	g_mats[self->global_id] = self;

	char *str = str_new(64);
	str_catf(&str, "material%d", self->global_id);
	vifunc_t *sandbox = vifunc_new(&g_mat_ctx, buffer, NULL, 0, false);
	if (sandbox)
	{
		self->call = vicall_new(sandbox, type->mat,
				str, vec2(100, 10), ~0, V_BOTH);
		vicall_watch(self->call, mat_changed, self);
		loader_push(g_candle->loader, (loader_cb)_mat_changed, self, NULL);
	}
	str_free(str);


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

mat_t *mat_from_file(const char *filename)
{
	mat_t *self = mat_new(filename);

	char *file_name = self->name + (strrchr(self->name, '/') - self->name);
	if(!file_name) file_name = self->name;

	/* if (!vifunc_load(mat, "material.vil")) */
	/* { */
	/* } */

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

extern texture_t *g_cache;
static float _tex_previewer_gui(vicall_t *call, _mat_sampler_t *texid, void *ctx) 
{
	float call_h = 0;
	if (texid->texture)
	{
		uint32_t mip = 0;
		float w = texid->texture->width;
		float h = texid->texture->height;
		while (w > 128.0f || h > 128.0f)
		{
			w /= 2.0f;
			h /= 2.0f;
			mip++;
		}
		if (mip > 0) mip -= 1;
		nk_layout_row_static(ctx, h, w, 1);

		load_tile(texid->texture, mip, 1, 1, 1);

		uint32_t tid = texid->texture->bufs[0].id;
		texid->tile = tid;
		texid->size = vec2(texid->texture->width, texid->texture->height);

		struct nk_rect bounds;
		nk_widget(&bounds, ctx);

		struct nk_image im = nk_image_id(g_cache->bufs[0].id);
		nk_draw_image_ext(nk_window_get_canvas(ctx), bounds, &im,
		                  nk_rgba(255, 255, 255, 255), 1, tid,
		                  texid->texture->width,
		                  texid->texture->height);
		call_h += nk_layout_widget_bounds(ctx).h;
	}

	nk_layout_row_dynamic(ctx, 20, 1);
	if (nk_button_label(ctx, texid->texture ? texid->texture->filename : "pick texture "))
	{
		char *file = NULL;
		entity_signal(entity_null, ref("pick_file_load"), "png;tga;jpg", &file);
		if(file)
		{
		}
		texid->texture = sauces("flipbook.tga");
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

static float _vint_gui(vicall_t *call, int32_t *num, void *ctx) 
{
	/* if(*num < 20) *num = 20; */
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

static bool_t _number_load(vicall_t *call, float *num, FILE *fp) 
{
	return fscanf(fp, "%f", num) >= 0;
}

static float _number_gui(vicall_t *call, float *num, void *ctx) 
{
	/* if(*num < 20) *num = 20; */
	/* nk_layout_row_begin(ctx, NK_DYNAMIC, *num, 2); */
	nk_layout_row_begin(ctx, NK_DYNAMIC, 20, 2);
	nk_layout_row_push(ctx, 0.20);
	nk_label(ctx, vicall_name(call), NK_TEXT_LEFT);
	nk_layout_row_push(ctx, 0.80);
	*num = nk_propertyf(ctx, "#", -1000.0f, *num, 1000.0f, 0.01, 0.01);
	nk_layout_row_end(ctx);
	/* nk_slider_int(ctx, 20, num, 200, 1); */
	return nk_layout_widget_bounds(ctx).h;
}

int mat_menu(mat_t *self, void *ctx)
{
	struct mat_type *type = &g_mat_types[self->type];
	uint32_t new_tid = nk_combo(ctx, (const char**)g_type_names, 1, self->type, 20,
	                             nk_vec2(200, 400));
	if (new_tid != self->type)
	{
		struct mat_type *new_type = &g_mat_types[new_tid];
		type->next_free[self->id] = type->free_position;
		type->free_position = self->id;
		self->type = new_tid;

		self->id = new_type->free_position;
		new_type->free_position = new_type->next_free[new_type->free_position];
		type = new_type;
	}

	if(nk_button_label(ctx, "vil"))
	{
		c_editmode(&SYS)->open_vil = type->mat;
	}
	vicall_gui(self->call, ctx);

	nk_layout_row_begin(ctx, NK_DYNAMIC, 20, 1);
	nk_layout_row_push(ctx, 1.0f);
	if(nk_button_label(ctx, "save"))
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
	return 0;
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

void material_foreach_input(vicall_t *call, slot_t slot, char **str)
{
	if (call->type->name[0] == '_') return;
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
	if (call->type->name[0] == '_') return;
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

void material_generate_func(vifunc_t *func, char **str)
{
	if (func->name[0] == '_') return;
	if (!strcmp(func->name, "opaque")) return;

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
	else if (func->id == ref("mul"))
	{
		str_cat(str, "	arg_out.result = arg_in.x * arg_in.y;\n");
	}
	else if (   func->id == ref("mix_num")
	         || func->id == ref("mix_vec2")
	         || func->id == ref("mix_vec3")
	         || func->id == ref("mix_vec4") )
	{
		str_cat(str, "	arg_out.result = mix(arg_in.x, arg_in.y, arg_in.a);\n");
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

	}
	str_cat(str, "}\n");
}

void material_generate_struct(vifunc_t *func, char **args)
{
	if (func->name[0] == '_') return;
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

	char *inputs = str_new(100);
	char *outputs = str_new(100);
	vifunc_foreach_call(func, false, true, false, true,
	                    (vil_call_cb)material_foreach_input, &inputs);
	vifunc_foreach_call(func, false, false, true, true,
	                    (vil_call_cb)material_foreach_output, &outputs);

	str_catf(args, "struct vil_%s_in_t {\n%s};\n", func->name,
	         str_len(inputs) ? inputs : "	int ignore;\n");
	str_catf(args, "struct vil_%s_out_t {\n%s};\n", func->name,
	         str_len(outputs) ? outputs : "	int ignore;\n");

	str_free(inputs);
	str_free(outputs);
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

	str_catf(args, "%s = %s;\n", buffer_to, buffer_from);
}

void material_generate_call(vicall_t *call, slot_t slot, char **args)
{
	if (!strcmp(call->type->name, "opaque")) return;
	str_catf(args, "%s_func(%s_in, %s_out, tile_num, tile_array);\n",
	         call->type->name, call->name, call->name);
	/* str_catf(args, "%s_func(%s_in, %s_out);\n", */
	         /* call->type->name, call->name, call->name); */
}

struct transparent
{
	vec4_t albedo;
	vec3_t normal;
	vec3_t emissive;
	float roughness;
	vec4_t absorve;
};

struct opaque
{
	vec4_t albedo;
	vec3_t normal;
	vec3_t emissive;
	float roughness;
	float metalness;
	vec3_t padding;
};

static void _generate_uniform(vicall_t *root, vicall_t *call, slot_t slot,
                        char *data, void *usrptr)
{
	char **args = usrptr;
	char buffer[256];
	vifunc_slot_to_name(root->parent, slot, buffer, "_", NULL);
	str_catf(args, "	vil_%s_in_t %s;\n", call->type->name, buffer);
}

static void _generate_uniform_assigment(vicall_t *root, vicall_t *call, slot_t slot,
                        char *data, void *usrptr)
{
	char **args = usrptr;
	char buffer[256];
	char buffer2[256];
	vifunc_slot_to_name(root->parent, slot, buffer, "_", NULL);
	vifunc_slot_to_name(root->type, slot_pop(slot), buffer2, ".", "_in");

	str_catf(args, "%s = costum_materials.info[matid].%s;\n", buffer2, buffer);
}

static void _generate_uniform_data(vicall_t *root, vicall_t *call, slot_t slot,
                        char *data, void *usrptr)
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
	uint32_t size = ceilf(((float)vicall_get_size(call)) / 16) * 16;

	mat_t *self = usrptr;
	char *data = alloca(size);
	char *end = data;
	vicall_foreach_unlinked_input(call, _generate_uniform_data, &end);
	
	struct mat_type *type = &g_mat_types[self->type];
	glBindBuffer(GL_UNIFORM_BUFFER, type->ubo); glerr();
	glBufferSubData(GL_UNIFORM_BUFFER, size * self->id, size, data); glerr();
}

uint32_t mat_get_group(mat_t *mat)
{
	return vifunc_get(mat->call->type, "result")->type->id;
}

void mat_type_changed(vicall_t *call, slot_t slot, void *usrptr)
{
	struct mat_type *type = usrptr;

	uint32_t size = ceilf(((float)vicall_get_size(type->tmp_material_instance)) / 16) * 16;
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

	char *gbuffer = str_new(128);
	str_cat(&gbuffer, "#include \"common.glsl\"\n");
	vil_foreach_func(call->type->ctx, (vil_func_cb)material_generate_struct, &gbuffer);
	vil_foreach_func(call->type->ctx, (vil_func_cb)material_generate_func, &gbuffer);

	str_cat(&gbuffer, "struct costum_material_t {\n");
	vicall_foreach_unlinked_input(type->tmp_material_instance, _generate_uniform, &gbuffer);
	for (uint32_t i = size; i < size; i += 4)
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
		"#elif defined(SHADOW_PASS)\n"
		"layout (location = 0) out vec4 Depth;\n"
		"#else\n");
	uint32_t group = call->type->id;

	if (group == ref("opaque") || group == ref("decal"))
	{
		str_cat(&gbuffer,
			"layout (location = 0) out vec4 Alb;\n"
			"layout (location = 1) out vec4 NMR;\n"
			"layout (location = 2) out vec3 Emi;\n"
		);
	}
	else if (group == ref("transparent"))
	{
		str_cat(&gbuffer,
			"layout (location = 0) out vec4 FragColor;\n"
			"BUFFER { sampler2D color; } refr;\n"
		);
	}
	if (group == ref("decal"))
	{
		str_cat(&gbuffer,
			"layout (location = 0) out vec4 FragColor;\n"
			"BUFFER { sampler2D depth; } gbuffer;\n"
		);
	}

	str_cat(&gbuffer,
		"#endif\n"
		"void main(void) {\n"
		"	uint tile_array[8];\n"
		"	uint tile_num = 0u;\n"
	);

	vifunc_foreach_call(call->parent, false, true, true, false,
	                    (vil_call_cb)material_generate_precall, &gbuffer);

	if (group == ref("transparent"))
	{
		str_cat(&gbuffer,
			"#ifdef QUERY_PASS\n"
			"	if (transparent && (((int(gl_FragCoord.x) + int(gl_FragCoord.y)) & 1) == 0))\n"
			"		discard;\n"
			"#endif\n"
		);
	}

	if (group == ref("decal"))
	{
		str_cat(&gbuffer,
			"	float depth = textureLod(gbuffer.depth, pixel_pos(), 0.0).r;\n"
			"	if(depth > gl_FragCoord.z) discard;\n"
			"	vec4 w_pos = (camera(model)*vec4(get_position(gbuffer.depth), 1.0));\n"
			"	vec3 m_pos = (inverse(model) * w_pos).xyz;\n"
			"	vec3 diff = abs(m_pos);\n"
			"	if(diff.x > 0.5 || diff.y > 0.5 || diff.z > 0.5) discard;\n"
			"	tex_space_in = m_pos.xy - 0.5;\n"
		);
	}
	else
	{
		str_cat(&gbuffer, "tex_space_in = texcoord;\n");
	}

	str_cat(&gbuffer, "time_in = scene.time;\n");
	vicall_foreach_unlinked_input(type->tmp_material_instance,
	                              _generate_uniform_assigment, &gbuffer);
	vicall_iterate_dependencies(call, (vil_link_cb)material_generate_assignments,
	                            (vil_call_cb)material_generate_call, &gbuffer);

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
		"	TILES2.r = float(tile_array[4] % 256u);\n"
		"	TILES2.g = float(tile_array[4] / 256u);\n"
		"	TILES2.b = float(tile_array[5] % 256u);\n"
		"	TILES2.a = float(tile_array[5] / 256u);\n"
		"	TILES3.r = float(tile_array[6] % 256u);\n"
		"	TILES3.g = float(tile_array[6] / 256u);\n"
		"	TILES3.b = float(tile_array[7] % 256u);\n"
		"	TILES3.a = float(tile_array[7] / 256u);\n"
		"	TILES0 /= 255.0;\n"
		"	TILES1 /= 255.0;\n"
		"	TILES2 /= 255.0;\n"
		"	TILES3 /= 255.0;\n"
		"#elif defined(SHADOW_PASS)\n"
		"	Depth = encode_float_rgba(vertex_distance);\n"
		"#else\n");

	if (group == ref("opaque") || group == ref("decal"))
	{
		if (group == ref("opaque"))
		{
			str_cat(&gbuffer,
				"	vec3 norm = get_normal(result_in.normal);\n");
		}
		else if (group == ref("decal"))
		{
			str_cat(&gbuffer,
				"	vec3 norm = ((camera(view) * model) * vec4(result_in.normal, 0.0)).xyz;\n");
		}

		str_cat(&gbuffer,
			/* "	if (result_in.albedo.a < 0.7) discard;\n" */
			"	Alb = vec4(result_in.albedo.rgb, receive_shadows ? 1.0 : 0.5);\n"
			"	NMR.rg = encode_normal(norm);\n"
			"	NMR.b = result_in.metalness;\n"
			"	NMR.a = result_in.roughness;\n"
			"	Emi = result_in.emissive;\n");
	}
	else if(group == ref("transparent"))
	{
		str_cat(&gbuffer,
			"	vec2 pp = pixel_pos();\n"
			"	vec4 emit = resolveProperty(mat(emissive), texcoord, false);\n"
			"	vec4 final = result_in.albedo;\n"
			"	if (result_in.roughness > 0.0) {\n"
			"		vec3 nor = get_normal(result_in.normal);\n"
			"		vec2 coord = pp + nor.xy * (result_in.roughness * 0.03);\n"
			"		float mip = clamp(result_in.roughness * 3.0, 0.0, 3.0);\n"
			"		vec3 refracted = textureLod(refr.color, coord.xy, mip).rgb;\n"
			"		refracted = refracted * (1.0 - result_in.absorve);\n"
			"		final = vec4(mix(final.rgb, refracted, final.a), 1.0);\n"
			"	} else {\n"
			"		final = result_in.albedo;\n"
			"	}\n"
			"	final.rgb += result_in.emit;\n"
			"	FragColor = final;\n");
	}

	str_cat(&gbuffer,
		"#endif\n"
		"}\n");

	{
		const char *line = gbuffer;
		uint32_t line_num = 1u;
		while (true)
		{
			char *next_line = strchr(line, '\n');
			if (next_line)
			{
				printf("%d	%.*s\n", line_num, (int)(next_line - line), line);
				line = next_line+1;
			}
			else
			{
				printf("%d	%s\n", line_num, line);
				break;
			}
			line_num++;
		}
	}

	c_render_device(&SYS)->shader = NULL;
	c_render_device(&SYS)->frag_bound = NULL;

	char *name = str_new(64);
	char *name2 = str_new(64);
	char *q_name = str_new(64);
	char *q_name2 = str_new(64);
	char *d_name = str_new(64);
	char *d_name2 = str_new(64);

	char *query = str_new(64);
	str_cat(&query, "#define QUERY_PASS\n");
	str_cat(&query, gbuffer);

	char *depth = str_new(64);
	str_cat(&depth, "#define SHADOW_PASS\n");
	str_cat(&depth, gbuffer);


	uint32_t id = type - g_mat_types;
	str_catf(&d_name, "depth#%d", id);
	str_catf(&d_name2, "depth#%d.glsl", id);
	str_catf(&q_name, "query_mips#%d", id);
	str_catf(&q_name2, "query_mips#%d.glsl", id);
	str_catf(&name, "gbuffer#%d", id);
	str_catf(&name2, "gbuffer#%d.glsl", id);

	if (type->src)
	{
		if (strcmp(type->src, gbuffer))
		{
			shader_add_source(name2, (unsigned char*)gbuffer, str_len(gbuffer));
			shader_add_source(q_name2, (unsigned char*)query, str_len(query));
			shader_add_source(d_name2, (unsigned char*)depth, str_len(depth));

			str_free(type->src);
			type->src = gbuffer;
			fs_t *fs = fs_new("gbuffer");
			fs_update_variation(fs, id);

			fs = fs_new("query_mips");
			fs_update_variation(fs, id);

			fs = fs_new("depth");
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
		shader_add_source(name2, (unsigned char*)gbuffer, str_len(gbuffer));
		shader_add_source(q_name2, (unsigned char*)query, str_len(query));
		shader_add_source(d_name2, (unsigned char*)depth, str_len(depth));

		type->src = gbuffer;
		fs_t *fs = fs_new("gbuffer");
		fs_push_variation(fs, name);

		fs = fs_new("query_mips");
		fs_push_variation(fs, q_name);

		fs = fs_new("depth");
		fs_push_variation(fs, d_name);
	}

	str_free(query);
	str_free(q_name);
	str_free(q_name2);
	str_free(depth);
	str_free(d_name);
	str_free(d_name2);
	str_free(name);
	str_free(name2);
}

void _mat_type_changed(struct mat_type *type)
{
	vicall_t *opaque = vifunc_get(type->mat, "result");
	mat_type_changed(opaque, (slot_t){0}, type);
}

static void init_type(uint32_t tid)
{
	if (!g_mat_ctx.funcs)
		materials_init_vil();

	struct mat_type *type = &g_mat_types[tid];

	type->mat = vifunc_new(&g_mat_ctx, "material", NULL,
	                       sizeof(uint32_t), false);
	for (uint32_t i = 0; i < 128; i++)
	{
		type->next_free[i] = i + 1;
	}
	type->free_position = 0;

	vifunc_t *mat = type->mat;
	if (!vifunc_load(mat, "material.vil"))
	{
		float y = 0.0f;
		float inc = 100.0f;
		printf("FAILED TO LOAD\n");
		vifunc_t *v2 = vil_get(&g_mat_ctx, ref("vec2"));
		vifunc_t *tnum = vil_get(&g_mat_ctx, ref("number"));
		vifunc_t *v3 = vil_get(&g_mat_ctx, ref("vec3"));
		vifunc_t *opaque = vil_get(&g_mat_ctx, ref("opaque"));
		vicall_new(mat, opaque, "result", vec2(600, 10), ~0, V_IN);
		vicall_new(mat, v2, "tex_space", vec2(0, y += inc), ~0, V_OUT | V_LINKED);
		vicall_new(mat, tnum, "time", vec2(0, y += inc), ~0, V_OUT | V_LINKED);
		vicall_new(mat, v2, "screen_space", vec2(0, y += inc), ~0, V_OUT | V_LINKED);
		vicall_new(mat, v3, "world_space", vec2(0, y += inc), ~0, V_OUT | V_LINKED);
		vicall_new(mat, v3, "view_space", vec2(0, y += inc), ~0, V_OUT | V_LINKED);
		vicall_new(mat, v3, "vertex_normal", vec2(0, y += inc), ~0, V_OUT | V_LINKED);
		vicall_new(mat, v3, "obj_pos", vec2(0, y += inc), ~0, V_OUT | V_LINKED);
	}
	vicall_t *result = vifunc_get(mat, "result");
	vicall_watch(result, mat_type_changed, type);

	vifunc_t *placeholder = vifunc_new(&g_mat_ctx, "_placeholder", NULL, 0,
	                                   false);
	if (placeholder)
	{
		placeholder->usrptr = type;
		type->tmp_material_instance = vicall_new(placeholder, mat,
												 g_type_names[tid],
												 vec2(100, 10), ~0, V_BOTH);

		loader_push(g_candle->loader, (loader_cb)_mat_type_changed, type, NULL);
	}

}

void materials_init_vil()
{
	if (g_mat_ctx.funcs)
	    return;
	vicall_t *r, *g, *b, *a, *normal, *albedo;
	vil_t *ctx = &g_mat_ctx;

	vil_init(ctx);

	vifunc_t *tint = vifunc_new(ctx, "integer",
	                            (vifunc_gui_cb)_vint_gui,
			                    sizeof(int32_t), true);

	vifunc_t *tnum = vifunc_new(ctx, "number",
	                            (vifunc_gui_cb)_number_gui,
			                    sizeof(float), true);
	tnum->builtin_load = _number_load;
	tnum->builtin_save = _number_save;

	vifunc_t *v2 = vifunc_new(ctx, "vec2", NULL, sizeof(vec2_t), true);
		r = vicall_new(v2, tnum, "x", vec2(40, 10),  offsetof(vec2_t, x), V_BOTH);
		g = vicall_new(v2, tnum, "y", vec2(40, 260), offsetof(vec2_t, y), V_BOTH);
		vicall_color(r, vec4(1, 0, 0, 1));
		vicall_color(g, vec4(0, 1, 0, 1));

	vifunc_t *v3 = vifunc_new(ctx, "vec3", NULL, sizeof(vec3_t), true);
		r = vicall_new(v3, tnum, "x", vec2(40, 10),  offsetof(vec3_t, x), V_BOTH);
		g = vicall_new(v3, tnum, "y", vec2(40, 260), offsetof(vec3_t, y), V_BOTH);
		b = vicall_new(v3, tnum, "z", vec2(40, 360), offsetof(vec3_t, z), V_BOTH);
		vicall_color(r, vec4(1, 0, 0, 1));
		vicall_color(g, vec4(0, 1, 0, 1));
		vicall_color(b, vec4(0, 0, 1, 1));

	vifunc_t *tcol = vifunc_new(ctx, "_color_picker", (vifunc_gui_cb)_color_gui,
	                            0, false);

	vifunc_t *v4 = vifunc_new(ctx, "vec4", NULL, sizeof(vec4_t), true);
		vicall_new(v4, tcol, "color", vec2(100, 10), 0, V_IN);
		r = vicall_new(v4, tnum, "x", vec2(40, 10),  offsetof(vec4_t, x), V_BOTH);
		g = vicall_new(v4, tnum, "y", vec2(40, 260), offsetof(vec4_t, y), V_BOTH);
		b = vicall_new(v4, tnum, "z", vec2(40, 360), offsetof(vec4_t, z), V_BOTH);
		a = vicall_new(v4, tnum, "w", vec2(40, 360), offsetof(vec4_t, w), V_BOTH);
		vicall_color(r, vec4(1, 0, 0, 1));
		vicall_color(g, vec4(0, 1, 0, 1));
		vicall_color(b, vec4(0, 0, 1, 1));
		vicall_color(a, vec4(1, 1, 1, 1));

	vifunc_t *visin = vifunc_new(ctx, "sin", NULL, 0, false);
		vicall_new(visin, tnum, "x", vec2(40, 10),  ~0, V_IN);
		vicall_new(visin, tnum, "result", vec2(40, 360), ~0, V_OUT);

	vifunc_t *vimul = vifunc_new(ctx, "mul", NULL, 0, false);
		vicall_new(vimul, tnum, "x", vec2(40, 10),  ~0, V_IN);
		vicall_new(vimul, tnum, "y", vec2(40, 10),  ~0, V_IN);
		vicall_new(vimul, tnum, "result", vec2(40, 360), ~0, V_OUT);

	vifunc_t *vimix = vifunc_new(ctx, "mix_num", NULL, 0, false);
		vicall_new(vimix, tnum, "x", vec2(40, 10),  ~0, V_IN);
		vicall_new(vimix, tnum, "y", vec2(40, 10),  ~0, V_IN);
		vicall_new(vimix, tnum, "a", vec2(40, 10),  ~0, V_IN);
		vicall_new(vimix, tnum, "result", vec2(40, 360), ~0, V_OUT);

	vifunc_t *vimix2 = vifunc_new(ctx, "mix_vec2", NULL, 0, false);
		vicall_new(vimix2, v2, "x", vec2(40, 10),  ~0, V_IN);
		vicall_new(vimix2, v2, "y", vec2(40, 10),  ~0, V_IN);
		vicall_new(vimix2, tnum, "a", vec2(40, 10),  ~0, V_IN);
		vicall_new(vimix2, v2, "result", vec2(40, 360), ~0, V_OUT);

	vifunc_t *vimix3 = vifunc_new(ctx, "mix_vec3", NULL, 0, false);
		vicall_new(vimix3, v3, "x", vec2(40, 10),  ~0, V_IN);
		vicall_new(vimix3, v3, "y", vec2(40, 10),  ~0, V_IN);
		vicall_new(vimix3, tnum, "a", vec2(40, 10),  ~0, V_IN);
		vicall_new(vimix3, v3, "result", vec2(40, 360), ~0, V_OUT);

	vifunc_t *vimix4 = vifunc_new(ctx, "mix_vec4", NULL, 0, false);
		vicall_new(vimix4, v4, "x", vec2(40, 10),  ~0, V_IN);
		vicall_new(vimix4, v4, "y", vec2(40, 10),  ~0, V_IN);
		vicall_new(vimix4, tnum, "a", vec2(40, 10),  ~0, V_IN);
		vicall_new(vimix4, v4, "result", vec2(40, 360), ~0, V_OUT);

	vifunc_t *viadd = vifunc_new(ctx, "add", NULL, 0, false);
		vicall_new(viadd, tnum, "x", vec2(40, 10),  ~0, V_IN);
		vicall_new(viadd, tnum, "y", vec2(40, 10),  ~0, V_IN);
		vicall_new(viadd, tnum, "result", vec2(40, 360), ~0, V_OUT);

	vifunc_t *tprev = vifunc_new(ctx, "_tex_previewer",
	                             (vifunc_gui_cb)_tex_previewer_gui, 0, false);

	vifunc_t *ttex = vifunc_new(ctx, "texture", NULL,
	                            sizeof(_mat_sampler_t), false);
		vicall_new(ttex, tprev, "preview", vec2(100, 10), 0, V_IN);
		vicall_new(ttex, v2, "coord", vec2(100, 10), offsetof(_mat_sampler_t, coord), V_IN);
		vicall_new(ttex, tint, "tile", vec2(100, 10), offsetof(_mat_sampler_t, tile), V_IN | V_HID);
		vicall_new(ttex, v2, "size", vec2(100, 10), offsetof(_mat_sampler_t, size), V_IN | V_HID);
		vicall_new(ttex, v4, "result", vec2(300, 10), offsetof(_mat_sampler_t, result), V_OUT);

	vifunc_t *tflip = vifunc_new(ctx, "flipbook", NULL, 0, false);
		vicall_new(tflip, v2, "coord", vec2(100, 10), ~0, V_IN);
		vicall_new(tflip, tint, "rows", vec2(100, 10), ~0, V_IN);
		vicall_new(tflip, tint, "columns", vec2(100, 10), ~0, V_IN);
		vicall_new(tflip, tnum, "frame", vec2(100, 10), ~0, V_IN);
		vicall_new(tflip, v2, "result", vec2(300, 10), ~0, V_OUT);

	vifunc_t *tprop = vifunc_new(ctx, "property", NULL, 0, false);
		vicall_new(tprop, ttex, "texture", vec2(100, 10), ~0, V_IN);
		vicall_new(tprop, v4, "color", vec2(200, 10), ~0, V_IN);
		vicall_new(tprop, vimix4, "mix", vec2(300, 10), ~0, V_BOTH);
		vicall_new(tprop, tnum, "blend", vec2(400, 10), ~0, V_IN);
		vifunc_link(tprop, ref("texture.result"), ref("mix.x"));
		vifunc_link(tprop, ref("color"), ref("mix.y"));
		vifunc_link(tprop, ref("blend"), ref("mix.a"));

	vifunc_t *opaque = vifunc_new(ctx, "opaque", NULL, sizeof(struct opaque), false);
		albedo = vicall_new(opaque, v4, "albedo", vec2(100, 10),
		                    offsetof(struct opaque, albedo), V_IN);
		normal = vicall_new(opaque, v3, "normal", vec2(200, 10),
		                    offsetof(struct opaque, normal), V_IN);
		vicall_new(opaque, tnum, "roughness", vec2(300, 10),
		           offsetof(struct opaque, roughness), V_IN);
		vicall_new(opaque, tnum, "metalness", vec2(400, 10),
		           offsetof(struct opaque, metalness), V_IN);
		vicall_new(opaque, v3, "emissive", vec2(500, 10),
		           offsetof(struct opaque, emissive), V_IN);
		vicall_set_arg(albedo, ref("albedo"), &((vec4_t){.x = 0.5f, .y = 0.5f, .z = 0.5f, .w = 1.0f}));
		vicall_set_arg(normal, ref("normal"), &((vec3_t){.x = 0.5f, .y = 0.5f, .z = 1.0f}));

	vifunc_t *transparent = vifunc_new(ctx, "transparent", NULL, sizeof(struct transparent), false);
		vicall_new(transparent, v4, "albedo", vec2(100, 10),
		           offsetof(struct transparent, albedo), V_IN);
		vicall_new(transparent, v3, "absorve", vec2(200, 10),
		           offsetof(struct transparent, absorve), V_IN);
		vicall_new(transparent, v3, "normal", vec2(200, 10),
		           offsetof(struct transparent, normal), V_IN);
		vicall_new(transparent, tnum, "roughness", vec2(300, 10),
		           offsetof(struct transparent, roughness), V_IN);
		vicall_new(transparent, v3, "emissive", vec2(500, 10),
		           offsetof(struct transparent, emissive), V_IN);
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

void mat1t(mat_t *self, uint32_t ref, texture_t *value)
{
	_mat_sampler_t sampler;
	sampler.texture = value;
	sampler.size.x = value->width;
	sampler.size.y = value->height;
	vicall_set_arg(self->call, ref, &sampler);
}
