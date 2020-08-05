#include "shader.h"
#include <candle.h>
#include "components/light.h"
#include "components/node.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <utils/str.h>
#include <utils/shaders_default.h>

static char default_vs[1024] = "";
static char default_vs_end[] = 
	"\n"
	"	gl_Position = pos;\n"
	"}\n";

static char default_gs[1024];

static const char default_gs_end[] = "";

static void checkShaderError(GLuint shader, const char *name, const char *code);
static char *string_preprocess(const char *src, uint32_t len, const char *filename,
                               bool_t defines, bool_t has_gshader, bool_t has_skin);

struct source
{
	size_t len;
	char filename[32];
	char *src;
};

static char *shader_preprocess(struct source source, bool_t defines,
                               bool_t has_gshader, bool_t has_skin);

vs_t g_vs[64];
uint32_t g_vs_num = 0;

fs_t g_fs[64];
uint32_t g_fs_num = 0;

static struct source *g_sources = NULL;

static uint32_t g_sources_num = 0;

void shaders_reg()
{
	shaders_candle();

	strcat(default_vs,
	"#include \"candle:uniforms.glsl\"\n"
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
	);
	strcat(default_vs,
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
	"out vec3 $vertex_world_position;\n"
	"out vec2 $texcoord;\n"
	);
	strcat(default_vs,
	"out vec3 $vertex_normal;\n"
	"out vec3 $vertex_tangent;\n"
	"void main()\n"
	"{\n"
	"	vec4 pos = vec4(P.xyz, 1.0);\n"
	"	$obj_pos = (M * vec4(0.0, 0.0, 0.0, 1.0)).xyz;\n"
	"	$model = M;\n"
	"	$poly_color = COL;\n"
	"	$matid = uint(PROPS.x);\n"
	"	$poly_id = uvec2(ID);\n"
	"	$id = uvec2(PROPS.zw);\n"
	"	$texcoord = UV;\n"
	"	$vertex_position = pos.xyz;\n"
	);

	strcat(default_gs,
	"#version 300 es\n"
	"#extension GL_EXT_geometry_shader : enable\n"
	"#extension GL_OES_geometry_shader : enable\n"
	"layout(points) in;\n"
	"layout(triangle_strip, max_vertices = 15) out;\n"
	"#include \"candle:uniforms.glsl\"\n"
	);
	strcat(default_gs,
	"flat in uvec2 $id[1];\n"
	"flat in uint $matid[1];\n"
	"flat in vec2 $object_id[1];\n"
	"flat in uvec2 $poly_id[1];\n"
	"flat in vec3 $obj_pos[1];\n"
	"flat in mat4 $model[1];\n"
	"in vec3 $poly_color[1];\n"
	"in vec3 $vertex_position[1];\n"
	"in vec3 $vertex_world_position[1];\n"
	"in vec2 $texcoord[1];\n"
	"in vec3 $vertex_normal[1];\n"
	"in vec3 $vertex_tangent[1];\n"
	);
	strcat(default_gs,
	"flat out uvec2 id;\n"
	"flat out uint matid;\n"
	"flat out vec2 object_id;\n"
	"flat out uvec2 poly_id;\n"
	"flat out vec3 obj_pos;\n"
	"flat out mat4 model;\n"
	"out vec3 poly_color;\n"
	"out vec3 vertex_position;\n"
	"out vec3 vertex_world_position;\n"
	"out vec2 texcoord;\n"
	"out vec3 vertex_normal;\n"
	"out vec3 vertex_tangent;\n"
	);
}

vertex_modifier_t vertex_modifier_new(const char *code)
{
	vertex_modifier_t self;
	self.type = 0;
	self.code = str_dup(code);
	return self;
}

vertex_modifier_t geometry_modifier_new(const char *code)
{
	vertex_modifier_t self;
	self.type = 1;
	self.code = str_dup(code);
	return self;
}


uint32_t vs_new_loader(vs_t *self)
{
	if(self->vmodifier_num)
	{
		unsigned int i;
		char *code = str_new(64);
		char *ncode;
		for(i = 0; i < self->vmodifier_num; i++)
		{
			str_cat(&code, self->vmodifiers[i].code);
		}
		ncode = string_preprocess(code, str_len(code), "vmodifier", true,
		                          self->gmodifier_num > 0, self->has_skin);
		self->vprogram = glCreateShader(GL_VERTEX_SHADER); glerr();
		glShaderSource(self->vprogram, 1, (const GLchar**)&ncode, NULL);
		glCompileShader(self->vprogram); glerr();
		checkShaderError(self->vprogram, self->name, ncode);
		str_free(code);
		free(ncode);
	}
	if(self->gmodifier_num)
	{
		unsigned int i;
		char *code = str_new(64);
		char *ncode;
		for(i = 0; i < self->gmodifier_num; i++)
		{
			str_cat(&code, self->gmodifiers[i].code);
		}
		ncode = string_preprocess(code, str_len(code), "gmodifier", false,
		                          true, self->has_skin);

		/* { */
		/* const char *line = ncode; */
		/* uint32_t line_num = 1u; */
		/* while (true) */
		/* { */
		/* 	char *next_line = strchr(line, '\n'); */
		/* 	if (next_line) */
		/* 	{ */
		/* 		printf("%d	%.*s\n", line_num, (int)(next_line - line), line); */
		/* 		line = next_line+1; */
		/* 	} */
		/* 	else */
		/* 	{ */
		/* 		printf("%d	%s\n", line_num, line); */
		/* 		break; */
		/* 	} */
		/* 	line_num++; */
		/* } */
		/* } */

		self->gprogram = glCreateShader(GL_GEOMETRY_SHADER); glerr();
		glShaderSource(self->gprogram, 1, (const GLchar**)&ncode, NULL);
		glCompileShader(self->gprogram); glerr();
		checkShaderError(self->gprogram, self->name, ncode);
		str_free(code);

		free(ncode);
	}
	self->ready = 1;
	printf("vs ready %s\n", self->name);
	return 1;
}

vs_t *vs_new(const char *name, bool_t has_skin, uint32_t num_modifiers, ...)
{
	va_list va;
	uint32_t i = g_vs_num++;
	vs_t *self = &g_vs[i];
	self->index = i;
	strcpy(self->name, name);
	self->has_skin = has_skin;

	self->ready = 0;

	self->vmodifier_num = 1;
	self->gmodifier_num = 0;

	va_start(va, num_modifiers);
	for(i = 0; i < num_modifiers; i++)
	{
		vertex_modifier_t vst = va_arg(va, vertex_modifier_t);
		if(vst.type == 1)
		{
			/* Skip over the first geometry modifier */
			if(self->gmodifier_num == 0) self->gmodifier_num = 1;

			self->gmodifiers[self->gmodifier_num++] = vst;
		}
		else
		{
			self->vmodifiers[self->vmodifier_num++] = vst;
		}
	}
	va_end(va);

	self->vmodifiers[0] = vertex_modifier_new(default_vs);

	if(self->gmodifier_num > 0)
	{
		self->gmodifiers[0] = geometry_modifier_new(default_gs);
		self->gmodifiers[self->gmodifier_num++] = geometry_modifier_new(default_gs_end);
	}

	self->vmodifiers[self->vmodifier_num++] = vertex_modifier_new(default_vs_end);

	loader_push(g_candle->loader, (loader_cb)vs_new_loader, self, NULL);

	return self;
}

void shader_add_source(const char *name, char data[], uint32_t len)
{
	uint32_t i;
	bool_t found = false;
	for (i = 0; i < g_sources_num; i++)
	{
		if(!strcmp(name, g_sources[i].filename))
		{
			found = true;
			break;
		}
	}
	if (!found)
	{
		i = g_sources_num++;
		g_sources = realloc(g_sources, (sizeof *g_sources) * g_sources_num);
	}
	else
	{
		free(g_sources[i].src);
	}
	g_sources[i].len = len + 1;
	strcpy(g_sources[i].filename, name);

	g_sources[i].src = malloc(len + 1);
	memcpy(g_sources[i].src, data, len);
	g_sources[i].src[len] = '\0';
}

static const struct source shader_source(const char *filename)
{
	FILE *fp;
	char *buffer = NULL;
#define prefix "resauces/shaders/"
	char name[] = prefix "XXXXXXXXXXXXXXXXXXXXXXXXXXX";
	struct source res = {0};
	size_t lsize;
	uint32_t i;

	for(i = 0; i < g_sources_num; i++)
	{
		if(!strcmp(filename, g_sources[i].filename))
		{
			return g_sources[i];
		}
	}

	strncpy(name + (sizeof(prefix) - 1), filename, (sizeof(name) - (sizeof(prefix) - 1)) - 1);

	fp = fopen(name, "rb");
	if(!fp)
	{
		return res;
	}

	fseek(fp, 0L, SEEK_END);
	lsize = ftell(fp);
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
	strcpy(g_sources[i].filename, filename);
	g_sources[i].src = str_dup(buffer);

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

static char *string_preprocess(const char *src, bool_t len, const char *filename,
                               bool_t defines, bool_t has_gshader,
                               bool_t has_skin)
{
	size_t lsize = len;
	const char *line;
#ifdef DEBUG
	uint32_t include_lines[128];
	uint32_t include_lines_num;
#endif
	uint32_t line_num;
	char *token = NULL;
	uint32_t include_num = 0;

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
		, "#define BUFFER uniform struct\n"
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

	line = src;
	line_num = 1u;
#ifdef DEBUG
	include_lines_num = 0u;
#endif

	while(line)
	{
#ifdef DEBUG
		if (!strncmp(line, "#include", strlen("#include")))
		{
			include_lines[include_lines_num++] = line_num;
		}
#endif
		line_num++;
		line = strchr(line, '\n');
		if (line) line++;
	}

	while((token = strstr(buffer, "#include")))
	{
		struct source inc;
		long offset = token - buffer;
		char include_name[512];
		char *start = strchr(token, '"') + 1;
		char *end = strchr(start, '"');
		long end_offset = end - buffer + 1;
		char *include;
		char *include_buffer;

		memcpy(include_name, start, end - start);
		include_name[end - start] = '\0';
		inc = shader_source(include_name);
		if(!inc.src) break;

		include = str_new(64);
		include_buffer = shader_preprocess(inc, false, has_gshader, false);
#ifdef DEBUG
		str_catf(&include, "#line 1 \"%s\"\n", include_name);
#endif
		str_cat(&include, include_buffer);
		free(include_buffer);
#ifdef DEBUG
		str_catf(&include, "\n#line %d \"%s\"\n", include_lines[include_num],
		         filename);
#endif
		
		buffer = replace(buffer, offset, end_offset, include, &lsize);
		str_free(include);
		include_num++;
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
	return string_preprocess(source.src, source.len, source.filename,
	                         defines, has_gshader, has_skin);
}

static void checkShaderError(GLuint shader,
		const char *name, const char *code)
{
	GLint success = 0;
	GLint bufflen;

	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &bufflen);
	if (bufflen > 1)
	{
		GLchar log_string[2048];
		glGetShaderInfoLog(shader, bufflen, 0, log_string);

		printf("Log found for '%s':\n%s", name, log_string);
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
	GLint bufflen;
	int32_t isLinked = 1;
	int32_t count;
	uint32_t i;
	uint32_t fprogram = self->fs->program;
	uint32_t vprogram = g_vs[self->index].vprogram;
	uint32_t gprogram = g_vs[self->index].gprogram;

	self->program = glCreateProgram(); glerr();

	glAttachShader(self->program, vprogram); glerr();
	glAttachShader(self->program, fprogram); glerr();

	if(gprogram) /* not all programs need a geometry shader */
	{
		glAttachShader(self->program, g_vs[self->index].gprogram); glerr();
	}

	glLinkProgram(self->program); glerr();

/* #ifdef DEBUG */
	glValidateProgram(self->program);
	/* checkShaderError(self->program, self->fs->filename, NULL); */

	glGetProgramiv(self->program, GL_INFO_LOG_LENGTH, &bufflen);
	if (bufflen > 1)
	{
		GLchar log_string[2048];
		glGetProgramInfoLog(self->program, bufflen, 0, log_string);
		printf("Log found for '%s':\n%s\n", self->fs->filename, log_string);
	}

	glGetProgramiv(self->program, GL_LINK_STATUS, &isLinked);
/* #endif */
	printf("shader %d ready f:%s v:%s %d\n", self->program, self->fs->filename,
			g_vs[self->index].name, isLinked);
	if (!isLinked)
	{
		printf("%u %u %u\n", fprogram, vprogram, gprogram);
		exit(1);
	}

	self->uniforms = kh_init(uniform);
	/* GET UNNIFORM LOCATIONS */
	glGetProgramiv(self->program, GL_ACTIVE_UNIFORMS, &count);

	for (i = 0; i < count; i++)
	{
		khiter_t k;
		uint32_t *uniform;
		int32_t ret;
		GLint size; /* size of the variable */
		GLenum type; /* type of the variable (float, vec3 or mat4, etc) */

		const GLsizei bufSize = 64; /* maximum name length */
		GLchar name[64]; /* variable name in GLSL */
		GLsizei length; /* name length */

		glGetActiveUniform(self->program, i, bufSize, &length, &size, &type, name);

		k = kh_put(uniform, self->uniforms, ref(name), &ret);
		uniform = &kh_value(self->uniforms, k);
		(*uniform) = glGetUniformLocation(self->program, name);
	}
	self->scene_ubi = glGetUniformBlockIndex(self->program, "scene_t");
	self->renderer_ubi = glGetUniformBlockIndex(self->program, "renderer_t");
	self->skin_ubi = glGetUniformBlockIndex(self->program, "skin_t");
	self->cms_ubi = glGetUniformBlockIndex(self->program, "cms_t");
	glerr();

	self->ready = true;
	return 1;
}

uint32_t shader_cached_uniform(shader_t *self, uint32_t ref)
{
	khiter_t k = kh_get(uniform, self->uniforms, ref);
	if (k == kh_end(self->uniforms))
	{
		return ~0;
	}
	return kh_value(self->uniforms, k);
}

static uint32_t fs_new_loader(fs_variation_t *self)
{
	char buffer[256];
	self->program = 0;

	sprintf(buffer, "%s.glsl", self->filename);

	printf("fetching %s\n", buffer);
	self->code = shader_preprocess(shader_source(buffer), true, false, false);

	if(!self->code) exit(1);

	self->program = glCreateShader(GL_FRAGMENT_SHADER); glerr();

	glShaderSource(self->program, 1, (const GLchar**)&self->code, NULL); glerr();
	glCompileShader(self->program); glerr();

	checkShaderError(self->program, self->filename, self->code);
	self->ready = 1;
	/* printf("fs ready %s\n", buffer); */

	return 1;
}

fs_t *fs_get(const char *filename)
{
	uint32_t i;
	for (i = 0; i < g_fs_num; i++)
	{
		if (!strcmp(filename, g_fs[i].filename)) return &g_fs[i];
	}
	return NULL;
}

void fs_update_variation(fs_t *self, uint32_t fs_variation)
{
	uint32_t i;
	fs_variation_t *var = &self->variations[fs_variation];
	for (i = 0; i < 32; i++)
	{
		if (var->combinations[i])
		{
			shader_destroy(var->combinations[i]);
		}
		var->combinations[i] = NULL;
	}

	loader_push(g_candle->loader, (loader_cb)fs_new_loader, var, NULL);
}

void fs_push_variation(fs_t *self, const char *filename)
{
	uint32_t i;
	fs_variation_t *var = &self->variations[self->variations_num];
	var->program = 0;
	var->ready = 0;
	self->variations_num++;

	for (i = 0; i < 32; i++)
	{
		var->combinations[i] = NULL;
	}

	strcpy(var->filename, filename);

	loader_push(g_candle->loader, (loader_cb)fs_new_loader, var, NULL);
}

fs_t *fs_new(const char *filename)
{
	uint32_t i;
	fs_t *self;

	if(!filename) return NULL;
	for (i = 0; i < g_fs_num; i++)
	{
		if (!strcmp(filename, g_fs[i].filename)) return &g_fs[i];
	}

	self = &g_fs[g_fs_num++];
	self->variations_num = 0;
	strcpy(self->filename, filename);

	if (   strncmp(filename, "candle:gbuffer", strlen("candle:gbuffer"))
	    && strncmp(filename, "candle:query_mips", strlen("candle:query_mips"))
	    && strncmp(filename, "candle:select", strlen("candle:select"))
	    && strncmp(filename, "candle:depth", strlen("candle:depth"))
	    && strncmp(filename, "candle:transparent", strlen("candle:transparent")))
	{
		fs_push_variation(self, filename);
	}

	return self;
}

shader_t *shader_new(fs_t *fs, uint32_t fs_variation, vs_t *vs)
{
	shader_t *self = calloc(1, sizeof *self);
	self->fs = &fs->variations[fs_variation];
	self->index = vs->index;
	self->fs_variation = fs_variation;
	self->has_skin = vs->has_skin;

	self->ready = 0;
	loader_push(g_candle->loader, (loader_cb)shader_new_loader, self, NULL);
	return self;
}

void shader_bind(shader_t *self)
{
	glUseProgram(self->program); glerr();
}

/* GLuint _shader_uniform(shader_t *self, const char *uniform, const char *member) */
/* { */
/* 	if(member) */
/* 	{ */
/* 		char buffer[256]; */
/* 		snprintf(buffer, sizeof(buffer) - 1, "%s_%s", uniform, member); */
/* 		return glGetUniformLocation(self->program, buffer); glerr(); */
/* 	} */
/* 	return glGetUniformLocation(self->program, uniform); glerr(); */
/* } */

/* GLuint shader_uniform(shader_t *self, const char *uniform, const char *member) */
/* { */
/* 	if(member) */
/* 	{ */
/* 		char buffer[256]; */
/* 		snprintf(buffer, sizeof(buffer) - 1, "%s.%s", uniform, member); */
/* 		return glGetUniformLocation(self->program, buffer); glerr(); */
/* 	} */
/* 	return glGetUniformLocation(self->program, uniform); glerr(); */
/* } */

void fs_destroy(fs_t *self)
{
	uint32_t i, j;
	for(i = 0; i < self->variations_num; i++)
	{
		for(j = 0; j < g_vs_num; j++)
		{
			shader_destroy(self->variations[i].combinations[j]);
		}
		glDeleteShader(self->variations[i].program); glerr();
	}
}

void shader_destroy(shader_t *self)
{
	uint32_t fprogram = self->fs->program;
	uint32_t vprogram = g_vs[self->index].vprogram;
	uint32_t gprogram = g_vs[self->index].gprogram;

	if (fprogram)
	{
		glDetachShader(self->program, fprogram); glerr();
	}
	if (vprogram)
	{
		glDetachShader(self->program, vprogram); glerr();
	}
	if (gprogram)
	{
		glDetachShader(self->program, gprogram); glerr();
	}

	glDeleteProgram(self->program); glerr();

	free(self);
}
