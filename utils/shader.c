#include "shader.h"
#include <candle.h>
#include "components/light.h"
#include "components/probe.h"
#include "components/spacial.h"
#include "components/node.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

static void checkShaderError(GLuint shader, const char *errorMessage,
		const char *name);

struct source
{
	size_t len;
	char *filename;
	char *src;
};

vs_t g_vs[16];
int g_vs_num = 0;

fs_t g_fs[16];
int g_fs_num = 0;

static struct source *g_sources = NULL;

static int g_sources_num = 0;

void shaders_common_frag_reg(void);
 void shaders_depth_frag_reg(void);
 void shaders_gbuffer_frag_reg(void);
 void shaders_select_frag_reg(void);
 void shaders_decals_frag_reg(void);
 void shaders_ambient_frag_reg(void);
 void shaders_bright_frag_reg(void);
 void shaders_copy_frag_reg(void);
 void shaders_quad_frag_reg(void);
 void shaders_sprite_frag_reg(void);
 void shaders_ssr_frag_reg(void);
 void shaders_blur_frag_reg(void);

 void shaders_phong_frag_reg(void);
 void shaders_ssao_frag_reg(void);
 void shaders_transparency_frag_reg(void);
 void shaders_highlight_frag_reg(void);

void shaders_reg()
{
	shaders_common_frag_reg();
	shaders_ambient_frag_reg();
	shaders_bright_frag_reg();
	shaders_copy_frag_reg();
	shaders_depth_frag_reg();
	shaders_gbuffer_frag_reg();
	shaders_select_frag_reg();
	shaders_decals_frag_reg();
	shaders_phong_frag_reg();
	shaders_quad_frag_reg();
	shaders_sprite_frag_reg();
	shaders_ssr_frag_reg();
	shaders_transparency_frag_reg();
	shaders_highlight_frag_reg();
	shaders_blur_frag_reg();

	shaders_ssao_frag_reg();
}

vertex_modifier_t vertex_modifier_new(const char *code)
{
	vertex_modifier_t self;
	self.code = strdup(code);
	return self;
}

int vs_new_loader(vs_t *self)
{
	int i;
	int size = 0;

	for(i = 0; i < self->modifier_num; i++)
	{
		size += strlen(self->modifiers[i].code) + 1;
	}

	self->code = malloc(size);
	self->code[0] = '\0';

	for(i = 0; i < self->modifier_num; i++)
	{
		strcat(self->code, self->modifiers[i].code);
	}

	self->program = glCreateShader(GL_VERTEX_SHADER); glerr();

	glShaderSource(self->program, 1, (const GLchar**)&self->code, NULL);
	glerr();
	glCompileShader(self->program); glerr();

	checkShaderError(self->program, "Error compiling vertex shader!", NULL);
	self->ready = 1;
	printf("vs ready %s\n", self->name);

	return 1;
}

vs_t *vs_new(const char *name, int num_modifiers, ...)
{
	int i = g_vs_num++;
	vs_t *self = &g_vs[i];
	self->index = i;
	self->name = strdup(name);

	self->ready = 0;
	va_list va;

	self->modifier_num = num_modifiers + 2;

	self->modifiers[0] = vertex_modifier_new(
			"#version 400\n"
#ifdef MESH4
			"#define MESH4\n"
#endif
			"#ifdef MESH4\n"
			"layout (location = 0) in vec4 P;\n"
			"#else\n"
			"layout (location = 0) in vec3 P;\n"
			"#endif\n"
			"layout (location = 1) in vec3 N;\n"
			"layout (location = 2) in vec2 UV;\n"
			"layout (location = 3) in vec3 TG;\n"
			"/* layout (location = 4) in vec3 BT; */\n"
			"/* layout (location = 5) in vec4 COL; */\n"
			"layout (location = 4) in vec2 ID;\n"
			"\n"
			"struct camera_t\n"
			"{\n"
			"	vec3 pos;\n"
			"	float exposure;\n"
			"	mat4 projection;\n"
			"	mat4 inv_projection;\n"
			"	mat4 view;\n"
			"	mat4 model;\n"
			"#ifdef MESH4\n"
			"	float angle4;\n"
			"#endif\n"
			"};\n"
			"\n"
			"\n"
			"uniform mat4 MVP;\n"
			"uniform mat4 M;\n"
			"uniform camera_t camera;\n"
			"uniform vec2 id;\n"
			"uniform float cameraspace_normals;\n"
			"\n"
			"out vec3 tgspace_light_dir;\n"
			"out vec3 tgspace_eye_dir;\n"
			"out vec3 worldspace_position;\n"
			"out vec3 cameraspace_light_dir;\n"
			"\n"
			"uniform vec2 screen_size;\n"
			"\n"
			"out vec2 poly_id;\n"
			"out vec2 object_id;\n"
			"\n"
			"out vec3 vertex_position;\n"
			"out vec3 vertex_normal;\n"
			"out vec3 vertex_tangent;\n"
			"out vec3 vertex_bitangent;\n"
			"\n"
			"out vec2 texcoord;\n"
			"/* out vec4 vertex_color; */\n"
			"\n"
			"out mat4 model;\n"
			"out mat3 TM;\n"
			"\n"
			"uniform float ratio;\n"
			"uniform float has_tex;\n"
			"uniform vec3 light_pos;\n"
			"\n"
			"void main()\n"
			"{\n"
			"	vec4 pos = vec4(P.xyz, 1.0f);\n"
			"	model = M;\n"
			"	texcoord = UV;\n");


	va_start(va, num_modifiers);
	for(i = 0; i < num_modifiers; i++)
	{
		vertex_modifier_t vst = va_arg(va, vertex_modifier_t);
		self->modifiers[i + 1] = vst;
	}
	va_end(va);

	self->modifiers[i + 1] = vertex_modifier_new( "\n\tgl_Position = pos; }");

	loader_push_wait(g_candle->loader, (loader_cb)vs_new_loader, self, NULL);

	return self;
}

void shader_add_source(const char *name, unsigned char data[],
		unsigned int len)
{
	int i = g_sources_num++;
	g_sources = realloc(g_sources, (sizeof *g_sources) * g_sources_num);
	g_sources[i].len = len + 1;
	g_sources[i].filename = strdup(name);

	g_sources[i].src = malloc(len + 1);
	memcpy(g_sources[i].src, data, len);
	g_sources[i].src[len] = '\0';
	printf("adding source %s\n", name);
}

static const struct source shader_source(const char *filename)
{
	struct source res = {0};
	int i;
	for(i = 0; i < g_sources_num; i++)
	{
		if(!strcmp(filename, g_sources[i].filename))
		{
			return g_sources[i];
		}
	}

#define prefix "resauces/shaders/"
	char name[] = prefix "XXXXXXXXXXXXXXXXXX";
	strncpy(name + (sizeof(prefix) - 1), filename, sizeof(name) - (sizeof(prefix) - 1));
	FILE *fp;
	char *buffer = NULL;

	fp = fopen(name, "rb");
	if(!fp)
	{
		return res;
	}

	fseek(fp, 0L, SEEK_END);
	size_t lsize = ftell(fp);
	rewind(fp);

	buffer = calloc(1, lsize + 1);

	if(fread(buffer, lsize, 1, fp) != 1)
	{
		fclose(fp);
		free(buffer);
		return res;
	}

	fclose(fp);

	i = g_sources_num++;
	g_sources = realloc(g_sources, (sizeof *g_sources) * g_sources_num);
	g_sources[i].len = lsize;
	g_sources[i].filename = strdup(filename);
	g_sources[i].src = strdup(buffer);


	return g_sources[i];

}

static char *shader_preprocess(struct source source, int defines)
{
	if(!source.src) return NULL;
	size_t lsize = source.len;

	char defs[][64] = { "#version 400\n"
#ifdef MESH4
		, "#define MESH4\n"
#endif
	};
	int i;
	char *buffer = NULL;
	if(defines)
	{
		for(i = 0; i < sizeof(defs)/sizeof(*defs); i++)
		{
			lsize += strlen(defs[i]);
		}
	}

	buffer = malloc(lsize + 9);
	buffer[0] = '\0';

	if(defines)
	{
		for(i = 0; i < sizeof(defs)/sizeof(*defs); i++)
		{
			strcat(buffer, defs[i]);
		}
	}
	strcat(buffer, source.src);

	char *include = NULL;
	while((include = strstr(buffer, "#include")))
	{
		long offset = include - buffer;
		char include_name[512];
		char *start = strchr(include, '"') + 1;
		char *end = strchr(start, '"');
		long end_offset = end - buffer + 1;
		memcpy(include_name, start, end - start);
		include_name[end - start] = '\0';

		const struct source inc = shader_source(include_name);
		if(!inc.src)
		{
			printf("Could not find '%s' shader to include in '%s'\n",
					include_name, source.filename);
			break;
		}
		else
		{
			char *include_buffer = shader_preprocess(inc, 0);
			size_t inc_size = strlen(include_buffer);

			long nsize = offset + inc_size + lsize - (end_offset - offset);

			char *new_buffer = calloc(nsize + 8, 1);

			memcpy(new_buffer, buffer, offset);

			memcpy(new_buffer + offset, include_buffer, inc_size);

			memcpy(new_buffer + offset + inc_size, buffer + end_offset,
					lsize - (end_offset - offset));

			lsize = nsize;

			free(include_buffer);
			free(buffer);
			buffer = new_buffer;
		}
	}
	return buffer;
}

static void checkShaderError(GLuint shader, const char *errorMessage,
		const char *name)
{
	GLint success = 0;
	GLint bufflen;

	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &bufflen);
	if (bufflen > 1)
	{
		GLchar log_string[bufflen + 1];
		glGetShaderInfoLog(shader, bufflen, 0, log_string);
		printf("Log found for '%s':\n%s", name, log_string);
	}

	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (success != GL_TRUE)
	{ 
		printf("%s -> %s\n", name, errorMessage);
		exit(1);
	}
}

static void shader_get_prop_uniforms(shader_t *self, u_prop_t *prop,
		const char *name)
{
	char buffer[128];

	snprintf(buffer, sizeof(buffer), "%s.texture_blend", name);
	prop->texture_blend = glGetUniformLocation(self->program, buffer); glerr();

	snprintf(buffer, sizeof(buffer), "%s.texture_scale", name);
	prop->texture_scale = glGetUniformLocation(self->program, buffer); glerr();

	snprintf(buffer, sizeof(buffer), "%s.texture", name);
	prop->texture = glGetUniformLocation(self->program, buffer); glerr();

	snprintf(buffer, sizeof(buffer), "%s.color", name);
	prop->color = glGetUniformLocation(self->program, buffer); glerr();
}

static int shader_new_loader(shader_t *self)
{
	self->program = glCreateProgram(); glerr();

	glAttachShader(self->program, self->fs->program); glerr();

	glAttachShader(self->program, g_vs[self->index].program); glerr();

	glLinkProgram(self->program); glerr();

	/* LAYOUTS */

	/* POSITION */
	glBindAttribLocation(self->program, 0, "P"); glerr();
	/* NORMAL */
	glBindAttribLocation(self->program, 1, "N"); glerr();
	/* UV MAP */
	glBindAttribLocation(self->program, 2, "UV"); glerr();
	/* TANGENT */
	glBindAttribLocation(self->program, 3, "TG"); glerr();
	/* BITANGENT */
	/* glBindAttribLocation(self->program, 4, "BT"); glerr(); */
	/* ID */
	glBindAttribLocation(self->program, 4, "ID"); glerr();

	self->u_mvp = glGetUniformLocation(self->program, "MVP"); glerr();
	self->u_m = glGetUniformLocation(self->program, "M"); glerr();
	/* self->u_mv = glGetUniformLocation(self->program, "MV"); glerr(); */

	self->u_perlin_map = glGetUniformLocation(self->program, "perlin_map"); glerr();


	self->u_ambient_map = glGetUniformLocation(self->program, "ambient_map"); glerr();
	self->u_probe_pos = glGetUniformLocation(self->program, "probe_pos"); glerr();

	self->u_v = glGetUniformLocation(self->program, "camera.view"); glerr();
	self->u_projection = glGetUniformLocation(self->program, "camera.projection"); glerr();
	self->u_inv_projection = glGetUniformLocation(self->program, "camera.inv_projection"); glerr();
	self->u_camera_pos = glGetUniformLocation(self->program, "camera.pos"); glerr();
	self->u_camera_model = glGetUniformLocation(self->program, "camera.model"); glerr();
	self->u_exposure = glGetUniformLocation(self->program, "camera.exposure"); glerr();
#ifdef MESH4
	self->u_angle4 = glGetUniformLocation(self->program, "angle4"); glerr();
#endif

	self->u_shadow_map = glGetUniformLocation(self->program, "light_shadow_map"); glerr();
	self->u_light_pos = glGetUniformLocation(self->program, "light_pos"); glerr();
	self->u_light_intensity = glGetUniformLocation(self->program, "light_intensity"); glerr();
	self->u_light_color = glGetUniformLocation(self->program, "light_color"); glerr();
	self->u_light_radius = glGetUniformLocation(self->program, "light_radius"); glerr();

	self->u_has_tex = glGetUniformLocation(self->program, "has_tex"); glerr();

	self->u_id = glGetUniformLocation(self->program, "id"); glerr();
	self->u_id_filter = glGetUniformLocation(self->program, "id_filter"); glerr();

	self->u_screen_size = glGetUniformLocation(self->program, "screen_size"); glerr();


	self->u_horizontal_blur = glGetUniformLocation(self->program, "horizontal"); glerr();

	self->u_output_size = glGetUniformLocation(self->program, "output_size"); glerr();

	/* MATERIALS */
	shader_get_prop_uniforms(self, &self->u_diffuse, "diffuse");
	shader_get_prop_uniforms(self, &self->u_specular, "specular");
	shader_get_prop_uniforms(self, &self->u_transparency, "transparency");
	shader_get_prop_uniforms(self, &self->u_normal, "normal");
	shader_get_prop_uniforms(self, &self->u_emissive, "emissive");

	self->ready = 1;
	printf("shader ready f:%s v:%s\n", self->fs->filename, g_vs[self->index].name);
	return 1;
}

static int fs_new_loader(fs_t *self)
{
	self->program = 0;

	char buffer[256];
	snprintf(buffer, sizeof(buffer), "%s.frag", self->filename);

	self->code  = shader_preprocess(shader_source(buffer), 1);

	if(!self->code) exit(1);

	self->program = glCreateShader(GL_FRAGMENT_SHADER); glerr();

	glShaderSource(self->program, 1, (const GLchar**)&self->code, NULL);
	glerr();
	glCompileShader(self->program); glerr();

	checkShaderError(self->program, "Error compiling fragment shader!", self->filename);
	self->ready = 1;
	printf("fs ready %s\n", buffer);

	return 1;
}

fs_t *fs_new(const char *filename)
{
	int i;
	for(i = 0; i < g_fs_num; i++)
	{
		if(!strcmp(filename, g_fs[i].filename)) return &g_fs[i];
	}

	fs_t *self = &g_fs[g_fs_num++];

	self->program = 0;
	self->ready = 0;

	for(i = 0; i < 16; i++)
	{
		self->combinations[i] = NULL;
	}

	self->filename = strdup(filename);

	loader_push_wait(g_candle->loader, (loader_cb)fs_new_loader, self, NULL);
	return self;
}

shader_t *shader_new(fs_t *fs, vs_t *vs)
{
	shader_t *self = calloc(1, sizeof *self);
	self->fs = fs;
	self->index = vs->index;

	self->ready = 0;
	loader_push_wait(g_candle->loader, (loader_cb)shader_new_loader, self, NULL);
	return self;
}

void shader_bind_ambient(shader_t *self, texture_t *ambient)
{
	int i = self->bound_textures++;
	glUniform1i(self->u_ambient_map, i); glerr();
	glActiveTexture(GL_TEXTURE0 + i); glerr();
	texture_bind(ambient, COLOR_TEX);

	glerr();
}

void shader_bind_probe(shader_t *self, entity_t probe)
{
	/* shader_t *self = c_renderer(&g_systems)->shader; */
	const vec3_t lpos = c_spacial(&probe)->pos;

	glUniform3f(self->u_probe_pos, lpos.x, lpos.y, lpos.z);

	glerr();

	/* probe_c = c_probe(probe); */
	/* if(!probe_c->before_draw || probe_c->before_draw((c_t*)probe_c)) */
	/* { */
		/* glUniform1f(self->u_probe_intensity, c_probe(probe)->intensity); */
	/* } */
	/* else */
	/* { */
		/* glUniform1f(self->u_probe_intensity, 0); */
	/* } */
}

void shader_bind_light(shader_t *self, entity_t light)
{
	c_light_t *light_c;
	c_probe_t *probe_c;

	c_node_t *node = c_node(&light);
	c_node_update_model(node);
	vec3_t lp = mat4_mul_vec4(node->model, vec4(0.0, 0.0, 0.0, 1.0)).xyz;
	glUniform3f(self->u_light_pos, lp.x, lp.y, lp.z);

	light_c = c_light(&light);
	probe_c = c_probe(&light);
	/* if(!light_c->before_draw || light_c->before_draw((c_t*)light_c)) */
	/* { */
		glUniform1f(self->u_light_radius, light_c->radius);
		glUniform1f(self->u_light_intensity, light_c->intensity);
		glUniform3f(self->u_light_color,
				light_c->color.r,
				light_c->color.g,
				light_c->color.b);
	/* } */
	/* else */
	/* { */
		/* glUniform1f(self->u_light_intensity, 0); */
	/* } */

	if(probe_c && probe_c->map)
	{
		/* int i = self->bound_textures++; */
		glUniform1i(self->u_shadow_map, 19); glerr();
		glActiveTexture(GL_TEXTURE0 + 19); glerr();
		texture_bind(probe_c->map, COLOR_TEX);
	}

}

void shader_bind_camera(shader_t *self, const vec3_t pos, mat4_t *view,
		mat4_t *projection, mat4_t *model, float exposure, float angle4)
{
	/* shader_t *self = c_renderer(&g_systems)->shader; */
	self->vp = mat4_mul(*projection, *view);

	glUniformMatrix4fv(self->u_v, 1, GL_FALSE, (void*)view->_);
	glUniformMatrix4fv(self->u_v, 1, GL_FALSE, (void*)model->_);
	glUniform3f(self->u_camera_pos, pos.x, pos.y, pos.z);
#ifdef MESH4
	glUniform1f(self->u_angle4, angle4);
#endif
	glUniform1f(self->u_exposure, exposure);

	/* TODO unnecessary? */
	mat4_t inv_projection = mat4_invert(*projection);
	glUniformMatrix4fv(self->u_projection, 1, GL_FALSE, (void*)projection->_);
	glUniformMatrix4fv(self->u_inv_projection, 1, GL_FALSE, (void*)inv_projection._);

	glerr();
}

void shader_update(shader_t *self, mat4_t *model_matrix)
{
	/* shader_t *self = c_renderer(&g_systems)->shader; */
	mat4_t mvp = mat4_mul(self->vp, *model_matrix);

	glUniformMatrix4fv(self->u_mvp, 1, GL_FALSE, (void*)mvp._);
	glUniformMatrix4fv(self->u_m, 1, GL_FALSE, (void*)model_matrix->_);
	glerr();

	/* if(perlin) */
	/* { */
	/* 	glUniform1i(self->u_perlin_map, 12); glerr(); */
	/* 	glActiveTexture(GL_TEXTURE8 + 4); glerr(); */
	/* 	texture_bind(perlin, COLOR_TEX); */
	/* } */
	glerr();
}

void shader_bind_screen(shader_t *self, texture_t *buffer, float sx, float sy)
{
	/* shader_t *self = c_renderer(&g_systems)->shader; */

	glUniform2f(self->u_screen_size, sx * buffer->width, sy * buffer->height);
	glerr();

	int i = self->bound_textures++;
	glUniform1i(self->u_diffuse.texture, i); glerr();
	glActiveTexture(GL_TEXTURE0 + i); glerr();

	texture_bind(buffer, COLOR_TEX);
}

void shader_bind(shader_t *self)
{
	/* printf("shader bound f:%s v:%s\n", self->fs->filename, g_vs[self->index].name); */
	glUseProgram(self->program);
}

void shader_bind_mesh(shader_t *self, mesh_t *mesh, unsigned int id)
{
	glUniform1f(self->u_has_tex, (float)mesh->has_texcoords);
	vec2_t id_color = int_to_vec2(id);

	glUniform2f(self->u_id, id_color.x, id_color.y);
}

GLuint shader_uniform(shader_t *self, const char *uniform, const char *member)
{
	if(member)
	{
		char buffer[256];
		snprintf(buffer, sizeof(buffer), "%s.%s", uniform, member);
		return glGetUniformLocation(self->program, buffer); glerr();
	}
	return glGetUniformLocation(self->program, uniform); glerr();
}

void fs_destroy(fs_t *self)
{
	int i;
	for(i = 0; i < g_vs_num; i++)
	{
		shader_destroy(self->combinations[i]);
	}

	glDeleteShader(self->program); glerr();
}
void shader_destroy(shader_t *self)
{
	glDetachShader(self->program, self->fs->program); glerr();
	glDetachShader(self->program, g_vs[self->index].program); glerr();
	/* glDeleteShader(self->vs->program); glerr(); */

	glDeleteProgram(self->program); glerr();

	free(self);
}
