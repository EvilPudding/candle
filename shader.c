#include "shader.h"
#include <candle.h>
#include "components/light.h"
#include "components/probe.h"
#include "components/spacial.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct source
{
	size_t len;
	char *filename;
	char *src;
};

static struct source *g_sources = NULL;

static int g_sources_num = 0;

void shaders_common_frag_reg(void);
void shaders_ambient_vert_reg(void); void shaders_ambient_frag_reg(void);
void shaders_depth_vert_reg(void); void shaders_depth_frag_reg(void);
void shaders_gbuffer_vert_reg(void); void shaders_gbuffer_frag_reg(void);
void shaders_ambient_vert_reg(void); void shaders_ambient_frag_reg(void);
void shaders_bright_vert_reg(void); void shaders_bright_frag_reg(void);
void shaders_normal_vert_reg(void); void shaders_normal_frag_reg(void);
void shaders_quad_vert_reg(void); void shaders_quad_frag_reg(void);
void shaders_ssr_vert_reg(void); void shaders_ssr_frag_reg(void);
void shaders_blinn_phong_vert_reg(void); void shaders_blinn_phong_frag_reg(void);
void shaders_blur_vert_reg(void); void shaders_blur_frag_reg(void);
void shaders_diffuse_vert_reg(void); void shaders_diffuse_frag_reg(void);
void shaders_gooch_vert_reg(void); void shaders_gooch_frag_reg(void);
void shaders_noanimation_vert_reg(void);
void shaders_phong_vert_reg(void); void shaders_phong_frag_reg(void);
void shaders_position_vert_reg(void); void shaders_position_frag_reg(void);
void shaders_shadow_vert_reg(void); void shaders_shadow_frag_reg(void);
void shaders_ssao_vert_reg(void); void shaders_ssao_frag_reg(void);
void shaders_transparency_vert_reg(void); void shaders_transparency_frag_reg(void);
void shaders_highlight_vert_reg(void); void shaders_highlight_frag_reg(void);

void shaders_reg()
{
	shaders_common_frag_reg();
	shaders_ambient_vert_reg();shaders_ambient_frag_reg();
	shaders_blinn_phong_vert_reg();shaders_blinn_phong_frag_reg();
	shaders_bright_vert_reg();shaders_bright_frag_reg();
	shaders_depth_vert_reg();shaders_depth_frag_reg();
	shaders_gbuffer_vert_reg();shaders_gbuffer_frag_reg();
	shaders_gooch_vert_reg();shaders_gooch_frag_reg();
	shaders_phong_vert_reg();shaders_phong_frag_reg();
	shaders_quad_vert_reg();shaders_quad_frag_reg();
	shaders_shadow_vert_reg();shaders_shadow_frag_reg();
	shaders_ssr_vert_reg();shaders_ssr_frag_reg();
	shaders_transparency_vert_reg();shaders_transparency_frag_reg();
	shaders_highlight_vert_reg();shaders_highlight_frag_reg();
	shaders_blur_vert_reg();shaders_blur_frag_reg();
	shaders_diffuse_vert_reg();shaders_diffuse_frag_reg();
	shaders_noanimation_vert_reg();
	shaders_normal_vert_reg();shaders_normal_frag_reg();
	shaders_position_vert_reg();shaders_position_frag_reg();
	shaders_ssao_vert_reg();shaders_ssao_frag_reg();
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

	buffer = malloc(lsize + 8);
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
	self->frag_shader = 0;
	self->vert_shader = 0;
	self->geom_shader = 0;

	char buffer[256];
	snprintf(buffer, sizeof(buffer), "%s.frag", self->filename);

	char *frag_src = shader_preprocess(shader_source(buffer), 1);

	snprintf(buffer, sizeof(buffer), "%s.vert", self->filename);
	char *vert_src = shader_preprocess(shader_source(buffer), 1);

	snprintf(buffer, sizeof(buffer), "%s.geom", self->filename);
	char *geom_src = shader_preprocess(shader_source(buffer), 1);

	if(frag_src)
	{
		self->frag_shader = glCreateShader(GL_FRAGMENT_SHADER); glerr();
	}
	if(vert_src)
	{
		self->vert_shader = glCreateShader(GL_VERTEX_SHADER); glerr();
	}
	if(geom_src)
	{
		self->geom_shader = glCreateShader(GL_GEOMETRY_SHADER); glerr();
	}

	if(!frag_src || !vert_src)
	{
		exit(1);
	}

	self->frag_shader = glCreateShader(GL_FRAGMENT_SHADER); glerr();
	self->vert_shader = glCreateShader(GL_VERTEX_SHADER); glerr();
	self->geom_shader = glCreateShader(GL_GEOMETRY_SHADER); glerr();

	if(frag_src)
	{
		glShaderSource(self->frag_shader, 1, (const GLchar**)&frag_src, NULL);
		glerr();
		free(frag_src);

		glCompileShader(self->frag_shader); glerr();

		checkShaderError(self->frag_shader,
				"Error compiling fragment shader!", self->filename);
	}

	if(vert_src)
	{
		glShaderSource(self->vert_shader, 1, (const GLchar**)&vert_src, NULL);
		glerr();
		free(vert_src);
		glCompileShader(self->vert_shader);

		checkShaderError(self->vert_shader,
				"Error compiling vertex shader!", self->filename);

	}

	if(geom_src)
	{
		glShaderSource(self->geom_shader, 1, (const GLchar**)&geom_src, NULL);
		glerr();
		free(geom_src);

		glCompileShader(self->geom_shader);
		checkShaderError(self->geom_shader,
				"Error compiling geometry shader!", self->filename);
	}

	self->program = glCreateProgram(); glerr();

	if(frag_src)
	{
		glAttachShader(self->program, self->frag_shader); glerr();
	}
	if(vert_src)
	{
		glAttachShader(self->program, self->vert_shader); glerr();
	}
	if(geom_src)
	{
		glAttachShader(self->program, self->geom_shader); glerr();
	}

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
	glBindAttribLocation(self->program, 4, "BT"); glerr();
	/* COLOR */
	glBindAttribLocation(self->program, 5, "COL"); glerr();

	self->u_mvp = glGetUniformLocation(self->program, "MVP"); glerr();
	self->u_projection = glGetUniformLocation(self->program, "projection"); glerr();
	self->u_m = glGetUniformLocation(self->program, "M"); glerr();
	self->u_v = glGetUniformLocation(self->program, "V"); glerr();
	/* self->u_mv = glGetUniformLocation(self->program, "MV"); glerr(); */

#ifdef MESH4
	self->u_angle4 = glGetUniformLocation(self->program, "angle4"); glerr();
#endif
	self->u_perlin_map = glGetUniformLocation(self->program, "perlin_map"); glerr();

	self->u_shadow_map = glGetUniformLocation(self->program, "shadow_map"); glerr();
	self->u_light_pos = glGetUniformLocation(self->program, "light_pos"); glerr();

	self->u_ambient_map = glGetUniformLocation(self->program, "ambient_map"); glerr();
	self->u_probe_pos = glGetUniformLocation(self->program, "probe_pos"); glerr();

	self->u_camera_pos = glGetUniformLocation(self->program, "camera_pos"); glerr();
	self->u_exposure = glGetUniformLocation(self->program, "exposure"); glerr();
	self->u_light_intensity = glGetUniformLocation(self->program, "light_intensity"); glerr();

	self->u_has_tex = glGetUniformLocation(self->program, "has_tex"); glerr();

	self->u_id = glGetUniformLocation(self->program, "id"); glerr();
	self->u_id_filter = glGetUniformLocation(self->program, "id_filter"); glerr();

	self->u_screen_scale_x = glGetUniformLocation(self->program, "screen_scale_x"); glerr();
	self->u_screen_scale_y = glGetUniformLocation(self->program, "screen_scale_y"); glerr();

	self->u_ambient_light = glGetUniformLocation(self->program, "ambient_light"); glerr();

	self->u_horizontal_blur = glGetUniformLocation(self->program, "horizontal"); glerr();

	self->u_output_size = glGetUniformLocation(self->program, "output_size"); glerr();

	/* MATERIALS */
	shader_get_prop_uniforms(self, &self->u_diffuse, "diffuse");
	shader_get_prop_uniforms(self, &self->u_specular, "specular");
	shader_get_prop_uniforms(self, &self->u_reflection, "reflection");
	shader_get_prop_uniforms(self, &self->u_normal, "normal");
	shader_get_prop_uniforms(self, &self->u_transparency, "transparency");

	return 1;
}

shader_t *shader_new(const char *filename)
{
	shader_t *self = calloc(1, sizeof *self);
	self->filename = strdup(filename);

	loader_push(candle->loader, (loader_cb)shader_new_loader, self, NULL);

	return self;
}

void shader_bind_projection(shader_t *self, mat4_t *projection_matrix)
{
	glUniformMatrix4fv(self->u_projection, 1, GL_FALSE,
			(void*)projection_matrix);
	glerr();
}


void shader_bind_ambient(shader_t *self, texture_t *ambient)
{
	glUniform1i(self->u_ambient_map, 8); glerr();
	glActiveTexture(GL_TEXTURE0 + 8); glerr();
	texture_bind(ambient, COLOR_TEX);

	glerr();
}

void shader_bind_probe(shader_t *self, entity_t probe)
{
	const vec3_t lpos = c_spacial(probe)->pos;

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

	const vec3_t lpos = c_spacial(light)->pos;

	glUniform3f(self->u_light_pos, lpos.x, lpos.y, lpos.z);

	light_c = c_light(light);
	probe_c = c_probe(light);
	/* if(!light_c->before_draw || light_c->before_draw((c_t*)light_c)) */
	/* { */
		glUniform1f(self->u_light_intensity, light_c->intensity);
	/* } */
	/* else */
	/* { */
		/* glUniform1f(self->u_light_intensity, 0); */
	/* } */

	if(probe_c && probe_c->map)
	{
		glUniform1i(self->u_shadow_map, 0); glerr();
		glActiveTexture(GL_TEXTURE0); glerr();
		texture_bind(probe_c->map, COLOR_TEX);
	}

}

#ifdef MESH4
void shader_bind_camera(shader_t *self, const vec3_t pos, mat4_t *view,
		mat4_t *projection, float exposure, float angle4)
#else
void shader_bind_camera(shader_t *self, const vec3_t pos, mat4_t *view,
		mat4_t *projection, float exposure)
#endif
{
	self->vp = mat4_mul(*projection, *view);

	glUniformMatrix4fv(self->u_v, 1, GL_FALSE, (void*)view->_);
	glUniform3f(self->u_camera_pos, pos.x, pos.y, pos.z);
#ifdef MESH4
	glUniform1f(self->u_angle4, angle4);
#endif
	glUniform1f(self->u_exposure, exposure);

	/* TODO unnecessary? */
	glUniformMatrix4fv(self->u_projection, 1, GL_FALSE, (void*)projection);

	glerr();
}

void shader_update(shader_t *self, mat4_t *model_matrix)
{
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
	glUniform1f(self->u_screen_scale_x, sx); glerr();
	glUniform1f(self->u_screen_scale_y, sy); glerr();

	glUniform1i(self->u_diffuse.texture, 1); glerr();
	glActiveTexture(GL_TEXTURE0 + 1); glerr();

	texture_bind(buffer, COLOR_TEX);
}

void shader_bind(shader_t *self)
{
	glUseProgram(self->program);
}

void shader_bind_mesh(shader_t *self, mesh_t *mesh, unsigned int id)
{
	glUniform1f(self->u_has_tex, (float)mesh->has_texcoords);
	vec3_t id_color = int_to_vec3(id);

	glUniform3f(self->u_id, id_color.r, id_color.g, id_color.b);
}

GLuint shader_uniform(shader_t *self, const char *uniform, const char *member)
{
	char buffer[256];
	snprintf(buffer, sizeof(buffer), "%s.%s", uniform, member);
	GLuint res = glGetUniformLocation(self->program, buffer); glerr();

	return res;
}

void shader_destroy(shader_t *self)
{

	if(self->frag_shader)
	{
		glDetachShader(self->program, self->frag_shader); glerr();
		glDeleteShader(self->frag_shader); glerr();
	}
	if(self->vert_shader)
	{
		glDetachShader(self->program, self->vert_shader); glerr();
		glDeleteShader(self->vert_shader); glerr();
	}
	if(self->geom_shader)
	{
		glDetachShader(self->program, self->geom_shader); glerr();
		glDeleteShader(self->geom_shader); glerr();
	}
	glDeleteProgram(self->program); glerr();

	free(self);
}
