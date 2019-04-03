#include "shader.h"
#include <candle.h>
#include "components/light.h"
#include "components/node.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

static const char default_vs[] = 
	"#include \"uniforms.glsl\"\n"
	"#line 2\n"
#ifdef MESH4
	"layout (location = 0) in vec4 P;\n"
#else
	"layout (location = 0) in vec3 P;\n"
#endif
	"layout (location = 1) in vec3 N;\n"
	"layout (location = 2) in vec2 UV;\n"
	"layout (location = 3) in vec3 TG;\n"
	"layout (location = 4) in vec2 ID;\n"
	"layout (location = 5) in vec3 COL;\n"
	"layout (location = 6) in vec4 BID;\n"
	"layout (location = 7) in vec4 WEI;\n"

	"layout (location = 8) in mat4 M;\n"
	"layout (location = 12) in vec4 PROPS;\n"
	/* PROPS.x = material PROPS.y = NOTHING PROPS.zw = id */
#ifdef MESH4
	"layout (location = 13) in float ANG4;\n"
#endif

	"flat out uvec2 $id;\n"
	"flat out uint $matid;\n"
	"flat out vec2 $object_id;\n"
	"flat out uvec2 $poly_id;\n"
	"flat out vec3 $obj_pos;\n"
	"flat out mat4 $model;\n"
	"out vec3 $poly_color;\n"
	"out vec3 $vertex_position;\n"
	"out vec2 $texcoord;\n"
	"out mat3 $TM;\n"
	"void main()\n"
	"{\n"
	"	vec4 pos = vec4(P.xyz, 1.0);\n"
	"	$obj_pos = (M * vec4(0.0, 0.0, 0.0, 1.0)).xyz;\n"
	"	$model = M;\n"
	"	$poly_color = COL;\n"
	"	$matid = uint(PROPS.x);\n"
	"	$poly_id = uvec2(ID);\n"
	"	$id = uvec2(PROPS.zw);\n"
	"	$texcoord = UV;\n";
static const char default_vs_end[] = 
	"\n"
	"	gl_Position = pos;\n"
	"	}\n";

static const char default_gs[] = 
	"#version 300 es\n"
	"#extension GL_EXT_geometry_shader4 : enable\n"
	"#extension GL_EXT_gpu_shader4 : enable\n"
	"layout(points) in;\n"
	"layout(triangle_strip, max_vertices = 3) out;\n"
	"flat in uvec2 $id[1];\n"
	"flat in uint $matid[1];\n"
	"flat in vec2 $object_id[1];\n"
	"flat in uvec2 $poly_id[1];\n"
	"flat in vec3 $obj_pos[1];\n"
	"flat in mat4 $model[1];\n"
	"in vec3 $poly_color[1];\n"
	"in vec3 $vertex_position[1];\n"
	"in vec2 $texcoord[1];\n"
	"in mat3 $TM[1];\n"
	"flat out uvec2 id;\n"
	"flat out uint matid;\n"
	"flat out vec2 object_id;\n"
	"flat out uvec2 poly_id;\n"
	"flat out vec3 obj_pos;\n"
	"flat out mat4 model;\n"
	"out vec3 poly_color;\n"
	"out vec3 vertex_position;\n"
	"out vec2 texcoord;\n"
	"out mat3 TM;\n";

static const char default_gs_end[] = "";

static void checkShaderError(GLuint shader, const char *name, const char *code);
static char *string_preprocess(const char *src, uint32_t len, bool_t defines,
                               bool_t has_gshader, bool_t has_skin);

struct source
{
	size_t len;
	char *filename;
	char *src;
};

static char *shader_preprocess(struct source source, bool_t defines,
                               bool_t has_gshader, bool_t has_skin);

vs_t g_vs[32];
uint32_t g_vs_num = 0;

fs_t g_fs[32];
uint32_t g_fs_num = 0;

static struct source *g_sources = NULL;

static uint32_t g_sources_num = 0;

void shaders_common_glsl_reg(void);
void shaders_depth_glsl_reg(void);
void shaders_gbuffer_glsl_reg(void);
void shaders_query_mips_glsl_reg(void);
void shaders_select_glsl_reg(void);
void shaders_decals_glsl_reg(void);
void shaders_ambient_glsl_reg(void);
void shaders_bright_glsl_reg(void);
void shaders_copy_glsl_reg(void);
void shaders_quad_glsl_reg(void);
void shaders_sprite_glsl_reg(void);
void shaders_ssr_glsl_reg(void);
void shaders_blur_glsl_reg(void);
void shaders_kawase_glsl_reg(void);
void shaders_motion_glsl_reg(void);
void shaders_downsample_glsl_reg(void);
void shaders_upsample_glsl_reg(void);
void shaders_border_glsl_reg(void);
void shaders_marching_glsl_reg(void);

void shaders_phong_glsl_reg(void);
void shaders_ssao_glsl_reg(void);
void shaders_transparency_glsl_reg(void);
void shaders_highlight_glsl_reg(void);
void shaders_editmode_glsl_reg(void);
void shaders_uniforms_glsl_reg(void);

void shaders_reg()
{
	shaders_ambient_glsl_reg();
	shaders_blur_glsl_reg();
	shaders_kawase_glsl_reg();
	shaders_motion_glsl_reg();
	shaders_downsample_glsl_reg();
	shaders_upsample_glsl_reg();
	shaders_border_glsl_reg();
	shaders_bright_glsl_reg();
	shaders_common_glsl_reg();
	shaders_copy_glsl_reg();
	shaders_decals_glsl_reg();
	shaders_depth_glsl_reg();
	shaders_gbuffer_glsl_reg();
	shaders_query_mips_glsl_reg();
	shaders_editmode_glsl_reg();
	shaders_highlight_glsl_reg();
	shaders_phong_glsl_reg();
	shaders_marching_glsl_reg();
	shaders_quad_glsl_reg();
	shaders_select_glsl_reg();
	shaders_sprite_glsl_reg();
	shaders_ssao_glsl_reg();
	shaders_ssr_glsl_reg();
	shaders_transparency_glsl_reg();
	shaders_uniforms_glsl_reg();
}

vertex_modifier_t _vertex_modifier_new(bool_t has_skin, const char *code,
                                       bool_t defines, bool_t has_gshader)
{
	vertex_modifier_t self;
	self.type = 0;
	self.code = string_preprocess(code, strlen(code), defines, has_gshader,
	                              has_skin);
	return self;
}

vertex_modifier_t vertex_modifier_new(const char *code)
{
	return _vertex_modifier_new(false, code, false, false);
}

vertex_modifier_t _geometry_modifier_new(const char *code, bool_t defines)
{
	vertex_modifier_t self;
	self.type = 1;
	self.code = string_preprocess(code, strlen(code), defines, true,
	                              false);
	return self;
}


vertex_modifier_t geometry_modifier_new(const char *code)
{
	return _geometry_modifier_new(code, false);
}


uint32_t vs_new_loader(vs_t *self)
{
	uint32_t i;
	uint32_t vcode_size = 0;
	uint32_t gcode_size = 0;

	for(i = 0; i < self->vmodifier_num; i++)
	{
		vcode_size += strlen(self->vmodifiers[i].code) + 1;
	}

	for(i = 0; i < self->gmodifier_num; i++)
	{
		gcode_size += strlen(self->gmodifiers[i].code) + 1;
	}

	if(vcode_size)
	{
		self->vcode = malloc(vcode_size); self->vcode[0] = '\0';
		for(i = 0; i < self->vmodifier_num; i++)
		{
			strcat(self->vcode, self->vmodifiers[i].code);
		}
		self->vprogram = glCreateShader(GL_VERTEX_SHADER); glerr();
		glShaderSource(self->vprogram, 1, (const GLchar**)&self->vcode, NULL);
		glCompileShader(self->vprogram); glerr();
		checkShaderError(self->vprogram, self->name, self->vcode);
	}
	if(gcode_size)
	{
		self->gcode = malloc(gcode_size); self->gcode[0] = '\0';
		for(i = 0; i < self->gmodifier_num; i++)
		{
			strcat(self->gcode, self->gmodifiers[i].code);
		}
		self->gprogram = glCreateShader(GL_GEOMETRY_SHADER); glerr();
		glShaderSource(self->gprogram, 1, (const GLchar**)&self->gcode, NULL);
		glCompileShader(self->gprogram); glerr();
		checkShaderError(self->gprogram, self->name, self->gcode);
	}
	self->ready = 1;
	printf("vs ready %s\n", self->name);
	return 1;
}

vs_t *vs_new(const char *name, bool_t has_skin, uint32_t num_modifiers, ...)
{
	uint32_t i = g_vs_num++;
	vs_t *self = &g_vs[i];
	self->index = i;
	self->name = strdup(name);
	self->has_skin = has_skin;

	self->ready = 0;
	va_list va;

	self->vmodifier_num = 1;
	self->gmodifier_num = 0;

	va_start(va, num_modifiers);
	for(i = 0; i < num_modifiers; i++)
	{
		vertex_modifier_t vst = va_arg(va, vertex_modifier_t);
		if(vst.type == 1)
		{
			// Skip over the first geometry modifier
			if(self->gmodifier_num == 0) self->gmodifier_num = 1;

			self->gmodifiers[self->gmodifier_num++] = vst;
		}
		else
		{
			self->vmodifiers[self->vmodifier_num++] = vst;
		}
	}
	va_end(va);

	self->vmodifiers[0] = _vertex_modifier_new(has_skin, default_vs, true,
	                                           self->gmodifier_num > 0);

	if(self->gmodifier_num > 0)
	{
		self->gmodifiers[0] = geometry_modifier_new(default_gs);
		self->gmodifiers[self->gmodifier_num++] = geometry_modifier_new(default_gs_end);
	}

	self->vmodifiers[self->vmodifier_num++] = vertex_modifier_new(default_vs_end);

	loader_push_wait(g_candle->loader, (loader_cb)vs_new_loader, self, NULL);

	return self;
}

void shader_add_source(const char *name, unsigned char data[],
		uint32_t len)
{
	uint32_t i = g_sources_num++;
	g_sources = realloc(g_sources, (sizeof *g_sources) * g_sources_num);
	g_sources[i].len = len + 1;
	g_sources[i].filename = strdup(name);

	g_sources[i].src = malloc(len + 1);
	memcpy(g_sources[i].src, data, len);
	g_sources[i].src[len] = '\0';
}

static const struct source shader_source(const char *filename)
{
	struct source res = {0};
	uint32_t i;
	for(i = 0; i < g_sources_num; i++)
	{
		if(!strcmp(filename, g_sources[i].filename))
		{
			return g_sources[i];
		}
	}

#define prefix "resauces/shaders/"
	char name[] = prefix "XXXXXXXXXXXXXXXXXXXXXXXXXXX";
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

char *replace(char *buffer, long offset, long end_offset,
              const char *str, size_t *lsize)
{
	long command_size = end_offset - offset;
	size_t inc_size = strlen(str);

	long nsize = offset + inc_size + (*lsize) - command_size;

	char *new_buffer = calloc(nsize + 1, 1);

	memcpy(new_buffer, buffer, offset);

	memcpy(new_buffer + offset, str, inc_size);

	memcpy(new_buffer + offset + inc_size, buffer + end_offset,
			(*lsize) - end_offset);

	(*lsize) = nsize;

	free(buffer);
	return new_buffer;
}

static char *string_preprocess(const char *src, bool_t len, bool_t defines,
                               bool_t has_gshader, bool_t has_skin)
{
	size_t lsize = len;

	/* char defs[][64] = { "#version 420\n" */
	const char version_str[] =
		  "#version 300 es\n";
	const char skin_str[] =
		  "#define HAS_SKIN\n";

	char defs[][64] = {
		  "#ifndef DEFINES\n"
		, "#define DEFINES\n"
		, "precision mediump float;\n"
		, "precision mediump int;\n"
#ifdef MESH4
		, "#define MESH4\n"
#endif
#ifdef __EMSCRIPTEN__
		, "#define EMSCRIPTEN\n"
#endif
		, "#endif\n"
	};
	uint32_t i;
	char *buffer = NULL;
	if(defines)
	{
		lsize += strlen(version_str);
		if (has_skin)
		{
			lsize += strlen(skin_str);
		}
		for(i = 0; i < sizeof(defs)/sizeof(*defs); i++)
		{
			lsize += strlen(defs[i]);
		}
	}

	buffer = malloc(lsize + 9);
	buffer[0] = '\0';

	if(defines)
	{
		strcat(buffer, version_str);
		if (has_skin)
		{
			strcat(buffer, skin_str);
		}
		for(i = 0; i < sizeof(defs)/sizeof(*defs); i++)
		{
			strcat(buffer, defs[i]);
		}
	}
	strcat(buffer, src);

	char *token = NULL;
	while((token = strstr(buffer, "#include")))
	{
		long offset = token - buffer;
		char include_name[512];
		char *start = strchr(token, '"') + 1;
		char *end = strchr(start, '"');
		long end_offset = end - buffer + 1;
		memcpy(include_name, start, end - start);
		include_name[end - start] = '\0';
		const struct source inc = shader_source(include_name);
		if(!inc.src)
		{
			/* printf("Could not find '%s' shader to include in '%s'\n", */
					/* include_name, source.filename); */
			break;
		}
		else
		{
			char *include_buffer = shader_preprocess(inc, false, has_gshader, false);
			buffer = replace(buffer, offset, end_offset, include_buffer, &lsize);
			free(include_buffer);
		}
	}

	while((token = strstr(buffer, "$")))
	{
		long offset = token - buffer;
		long end_offset = offset + 1;
		buffer = replace(buffer, offset, end_offset,
		                 has_gshader ? "_G_" : "", &lsize);
	}

	return buffer;
}

static char *shader_preprocess(struct source source, bool_t defines,
                               bool_t has_gshader, bool_t has_skin)
{
	if(!source.src) return NULL;
	return string_preprocess(source.src, source.len, defines, has_gshader, has_skin);
}

static void checkShaderError(GLuint shader,
		const char *name, const char *code)
{
	GLint success = 0;
	GLint bufflen;

	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &bufflen);
	if (bufflen > 1)
	{
		GLchar log_string[bufflen + 1];
		glGetShaderInfoLog(shader, bufflen, 0, log_string);
		printf("Log found for '%s':\n%s", name, log_string);
		printf("%s\n", code);
	}

	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (success != GL_TRUE)
	{ 
		printf("%s\n", name);
		exit(1);
	}
}

static uint32_t shader_new_loader(shader_t *self)
{
	uint32_t fprogram = self->fs->program;
	uint32_t vprogram = g_vs[self->index].vprogram;
	uint32_t gprogram = g_vs[self->index].gprogram;

	self->program = glCreateProgram(); glerr();

	glAttachShader(self->program, vprogram); glerr();
	glAttachShader(self->program, fprogram); glerr();

	if(gprogram) // not all programs need a geometry shader
	{
		glAttachShader(self->program, g_vs[self->index].gprogram); glerr();
	}

	glLinkProgram(self->program); glerr();

	int32_t isLinked = 1;
/* #ifdef DEBUG */
	glValidateProgram(self->program);
	/* checkShaderError(self->program, self->fs->filename, NULL); */

	GLint bufflen;

	glGetProgramiv(self->program, GL_INFO_LOG_LENGTH, &bufflen);
	if (bufflen > 1)
	{
		GLchar log_string[bufflen + 1];
		glGetProgramInfoLog(self->program, bufflen, 0, log_string);
		printf("Log found for '%s':\n%s\n", self->fs->filename, log_string);
	}

	glGetProgramiv(self->program, GL_LINK_STATUS, &isLinked);
/* #endif */
	self->ready = 1;
	printf("shader %d ready f:%s v:%s %d\n", self->program, self->fs->filename,
			g_vs[self->index].name, isLinked);
	if (!isLinked)
	{
		printf("%u %u %u\n", fprogram, vprogram, gprogram);
		printf("VS: %s\n", g_vs[self->index].vcode);
		printf("GS: %s\n", g_vs[self->index].gcode);
		exit(1);
	}
	return 1;
}

static uint32_t fs_new_loader(fs_t *self)
{
	self->program = 0;

	char buffer[256];
	snprintf(buffer, sizeof(buffer), "%s.glsl", self->filename);

	self->code  = shader_preprocess(shader_source(buffer), true, false, false);

	if(!self->code) exit(1);

	self->program = glCreateShader(GL_FRAGMENT_SHADER); glerr();

	glShaderSource(self->program, 1, (const GLchar**)&self->code, NULL);
	glerr();
	glCompileShader(self->program); glerr();

	checkShaderError(self->program, self->filename, self->code);
	self->ready = 1;
	/* printf("fs ready %s\n", buffer); */

	return 1;
}

fs_t *fs_new(const char *filename)
{
	if(!filename) return NULL;
	uint32_t i;
	for(i = 0; i < g_fs_num; i++)
	{
		if(!strcmp(filename, g_fs[i].filename)) return &g_fs[i];
	}

	fs_t *self = &g_fs[g_fs_num++];

	self->program = 0;
	self->ready = 0;

	for(i = 0; i < 32; i++)
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
	self->has_skin = vs->has_skin;

	self->ready = 0;
	loader_push_wait(g_candle->loader, (loader_cb)shader_new_loader, self, NULL);
	return self;
}

void shader_bind(shader_t *self)
{
	glUseProgram(self->program); glerr();
}

GLuint _shader_uniform(shader_t *self, const char *uniform, const char *member)
{
	if(member)
	{
		char buffer[256];
		snprintf(buffer, sizeof(buffer), "%s_%s", uniform, member);
		return glGetUniformLocation(self->program, buffer); glerr();
	}
	return glGetUniformLocation(self->program, uniform); glerr();
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
	uint32_t i;
	for(i = 0; i < g_vs_num; i++)
	{
		shader_destroy(self->combinations[i]);
	}

	glDeleteShader(self->program); glerr();
}

void shader_destroy(shader_t *self)
{
	uint32_t fprogram = self->fs->program;
	uint32_t vprogram = g_vs[self->index].vprogram;
	uint32_t gprogram = g_vs[self->index].gprogram;

	glDetachShader(self->program, fprogram); glerr();
	glDetachShader(self->program, vprogram); glerr();
	if(gprogram)
	{
		glDetachShader(self->program, gprogram); glerr();
	}

	glDeleteProgram(self->program); glerr();

	free(self);
}
