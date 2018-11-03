#include "shader.h"
#include <candle.h>
#include "components/light.h"
#include "components/spacial.h"
#include "components/node.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

static void checkShaderError(GLuint shader, const char *errorMessage,
		const char *name, const char *code);
static char *string_preprocess(const char *src, int len, int defines);

struct source
{
	size_t len;
	char *filename;
	char *src;
};

static char *shader_preprocess(struct source source, int defines);

vs_t g_vs[32];
int g_vs_num = 0;

fs_t g_fs[32];
int g_fs_num = 0;

static struct source *g_sources = NULL;

static int g_sources_num = 0;

void shaders_common_glsl_reg(void);
void shaders_depth_glsl_reg(void);
void shaders_gbuffer_glsl_reg(void);
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
void shaders_downsample_glsl_reg(void);
void shaders_border_glsl_reg(void);

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
	shaders_downsample_glsl_reg();
	shaders_border_glsl_reg();
	shaders_bright_glsl_reg();
	shaders_common_glsl_reg();
	shaders_copy_glsl_reg();
	shaders_decals_glsl_reg();
	shaders_depth_glsl_reg();
	shaders_gbuffer_glsl_reg();
	shaders_editmode_glsl_reg();
	shaders_highlight_glsl_reg();
	shaders_phong_glsl_reg();
	shaders_quad_glsl_reg();
	shaders_select_glsl_reg();
	shaders_sprite_glsl_reg();
	shaders_ssao_glsl_reg();
	shaders_ssr_glsl_reg();
	shaders_transparency_glsl_reg();
	shaders_uniforms_glsl_reg();
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

	checkShaderError(self->program, "vertex shader!",
			self->name, self->code);
	self->ready = 1;
	printf("vs ready %s\n", self->code);

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

	const char modifier[] = 
			"#include \"uniforms.glsl\"\n"
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
			"layout (location = 6) in uvec4 BID;\n"
			"layout (location = 7) in vec4 WEI;\n"

			"layout (location = 8) in mat4 M;\n"
			"layout (location = 12) in uvec4 PROPS;\n"
			/* PROPS.x = material PROPS.y = NOTHING PROPS.zw = id */
#ifdef MESH4
			"layout (location = 13) in float ANG4;\n"
#endif

			"out flat uvec2 id;\n"
			"out flat uint matid;\n"
			"out vec2 object_id;\n"
			"out vec2 poly_id;\n"
			"out vec3 poly_color;\n"
			"out flat vec3 obj_pos;\n"
			"out vec3 vertex_position;\n"
			"\n"
			"out vec2 texcoord;\n"
			"\n"
			"out mat4 model;\n"
			"out mat3 TM;\n"
			"\n"
			"void main()\n"
			"{\n"
			"	vec4 pos = vec4(P.xyz, 1.0f);\n"
			"	obj_pos = (M * vec4(0, 0, 0, 1)).xyz;\n"
			"	model = M;\n"
			"	poly_color = COL;\n"
			"	matid = PROPS.x;\n"
			"	poly_id = ID;\n"
			"	id = PROPS.zw;\n"
			"	texcoord = UV;\n";

	self->modifiers[0] = vertex_modifier_new(string_preprocess(modifier, sizeof(modifier), 1));

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

static char *string_preprocess(const char *src, int len, int defines)
{
	size_t lsize = len;

	char defs[][64] = { "#version 420\n"
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
	strcat(buffer, src);

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

		long command_size = end_offset - offset;

		const struct source inc = shader_source(include_name);
		if(!inc.src)
		{
			/* printf("Could not find '%s' shader to include in '%s'\n", */
					/* include_name, source.filename); */
			break;
		}
		else
		{
			char *include_buffer = shader_preprocess(inc, 0);
			size_t inc_size = strlen(include_buffer);

			long nsize = offset + inc_size + lsize - command_size;

			char *new_buffer = calloc(nsize + 1, 1);

			memcpy(new_buffer, buffer, offset);

			memcpy(new_buffer + offset, include_buffer, inc_size);

			memcpy(new_buffer + offset + inc_size, buffer + end_offset,
					lsize - end_offset);

			lsize = nsize;

			free(include_buffer);
			free(buffer);
			buffer = new_buffer;
		}
	}
	return buffer;
}

static char *shader_preprocess(struct source source, int defines)
{
	if(!source.src) return NULL;
	return string_preprocess(source.src, source.len, defines);
}

static void checkShaderError(GLuint shader, const char *errorMessage,
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
		if(!name)
		{
			puts(code);
		}
	}

	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (success != GL_TRUE)
	{ 
		printf("%s -> %s\n", name, errorMessage);
		exit(1);
	}
}

static int shader_new_loader(shader_t *self)
{
	self->program = glCreateProgram(); glerr();

	glAttachShader(self->program, self->fs->program); glerr();

	glAttachShader(self->program, g_vs[self->index].program); glerr();

	glLinkProgram(self->program); glerr();

	self->ready = 1;
	/* printf("shader ready f:%s v:%s\n", self->fs->filename, g_vs[self->index].name); */
	return 1;
}

static int fs_new_loader(fs_t *self)
{
	self->program = 0;

	char buffer[256];
	snprintf(buffer, sizeof(buffer), "%s.glsl", self->filename);

	self->code  = shader_preprocess(shader_source(buffer), 1);

	if(!self->code) exit(1);

	self->program = glCreateShader(GL_FRAGMENT_SHADER); glerr();

	glShaderSource(self->program, 1, (const GLchar**)&self->code, NULL);
	glerr();
	glCompileShader(self->program); glerr();

	checkShaderError(self->program, "fragment shader!",
			self->filename, self->code);
	self->ready = 1;
	/* printf("fs ready %s\n", buffer); */

	return 1;
}

fs_t *fs_new(const char *filename)
{
	if(!filename) return NULL;
	int i;
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

	self->ready = 0;
	loader_push_wait(g_candle->loader, (loader_cb)shader_new_loader, self, NULL);
	return self;
}

void shader_bind(shader_t *self)
{
	/* printf("shader bound f:%s v:%s\n", self->fs->filename, g_vs[self->index].name); */
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
