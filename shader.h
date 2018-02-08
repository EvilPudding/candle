#ifndef SHADER_H
#define SHADER_H

#include "glutil.h"
#include "texture.h"
#include <ecm.h>

typedef struct
{
	GLuint texture_blend;
	GLuint texture_scale;
	GLuint texture;
	GLuint color;
} u_prop_t;

typedef struct light_t light_t;
typedef struct shader_t
{
	GLuint frag_shader;
	GLuint vert_shader;
	GLuint geom_shader;

	GLuint program;
	GLuint u_mvp;
	GLuint u_m;
	GLuint u_v;
	GLuint u_mv;
	GLuint u_projection;
	GLuint u_perlin_map;
	GLuint u_angle4;

	GLuint u_shadow_map;
	GLuint u_light_pos;

	GLuint u_ambient_map;
	GLuint u_probe_pos;

	GLuint u_camera_pos;
	GLuint u_exposure;
	GLuint u_light_intensity;
	GLuint u_has_tex;

	GLuint u_screen_scale_x;
	GLuint u_screen_scale_y;

	u_prop_t u_diffuse;
	u_prop_t u_specular;
	u_prop_t u_reflection;
	u_prop_t u_normal;
	u_prop_t u_transparency;

	GLuint u_g_diffuse;
	GLuint u_g_specular;
	GLuint u_g_reflection;
	GLuint u_g_normal;
	GLuint u_g_transparency;
	GLuint u_g_wposition;
	GLuint u_g_cposition;

	GLuint u_horizontal_blur; /* TODO: make this dynamic */
	GLuint u_output_size;

	GLuint u_ambient_light;

	mat4_t vp;

	char *filename;
} shader_t;

shader_t *shader_new(const char *filename);
void shader_update(shader_t *self, mat4_t *model_matrix);
void shader_bind(shader_t *self);
#ifdef MESH4
void shader_bind_camera(shader_t *self, const vec3_t pos, mat4_t *view,
		mat4_t *projection, float exposure, float angle4);
#else
void shader_bind_camera(shader_t *self, const vec3_t pos, mat4_t *view,
		mat4_t *projection, float exposure);
#endif
void shader_bind_light(shader_t *self, entity_t light);
void shader_bind_mesh(shader_t *self, mesh_t *mesh);
void shader_bind_gbuffer(shader_t *self, texture_t *buffer);
void shader_bind_screen(shader_t *self, texture_t *buffer, float sx, float sy);
void shader_destroy(shader_t *self);
void shader_bind_ambient(shader_t *self, texture_t *ambient);
void shader_bind_probe(shader_t *self, entity_t probe);
void shader_bind_projection(shader_t *self, mat4_t *projection_matrix);
void shader_add_source(const char *name, unsigned char data[],
		unsigned int len);
void shaders_reg(void);

#endif /* !SHADER_H */
