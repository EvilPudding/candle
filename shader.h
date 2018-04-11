#ifndef SHADER_H
#define SHADER_H

#include "glutil.h"
#include "texture.h"
#include <ecm.h>

typedef struct
{
	char *code;
} vertex_modifier_t;

typedef struct
{
	char *name;
	int index;
	vertex_modifier_t modifiers[16];
	int modifier_num;
	GLuint program;
	int ready;
	char *code;
} vs_t;

typedef struct
{
	char *code;
	GLuint program;

	shader_t *combinations[16];

	char *filename;
	int ready;
} fs_t;

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
	fs_t *fs;
	int index;
	int bound_textures;
	int frame_bind;

	GLuint program;
	GLuint u_mvp;
	GLuint u_m;
	GLuint u_v;
	GLuint u_mv;
	GLuint u_projection;
	GLuint u_inv_projection;
	GLuint u_perlin_map;
	GLuint u_angle4;


	GLuint u_ambient_map;
	GLuint u_probe_pos;

	GLuint u_camera_pos;
	GLuint u_camera_model;
	GLuint u_exposure;
	GLuint u_shadow_map;
	GLuint u_light_intensity;
	GLuint u_light_pos;
	GLuint u_light_color;
	GLuint u_light_ambient;
	GLuint u_light_radius;

	GLuint u_has_tex;
	GLuint u_id;
	GLuint u_id_filter;

	GLuint u_screen_size;

	u_prop_t u_diffuse;
	u_prop_t u_specular;
	u_prop_t u_transparency;
	u_prop_t u_normal;
	u_prop_t u_emissive;

	GLuint u_horizontal_blur; /* TODO: make this dynamic */
	GLuint u_output_size;


	mat4_t vp;

	int ready;
} shader_t;

shader_t *vs_bind(vs_t *vs);
void fs_bind(fs_t *fs);

vs_t *vs_new(const char *name, int num_modifiers, ...);
fs_t *fs_new(const char *filename);
vertex_modifier_t vertex_modifier_new(const char *code);

shader_t *shader_new(fs_t *fs, vs_t *vs);
void shader_update(shader_t *self, mat4_t *model_matrix);
void shader_bind(shader_t *self);
void shader_bind_camera(shader_t *self, const vec3_t pos, mat4_t *view,
		mat4_t *projection, mat4_t *model, float exposure, float angle4);
GLuint shader_uniform(shader_t *self, const char *uniform, const char *member);
void shader_bind_light(shader_t *self, entity_t light);
void shader_bind_mesh(shader_t *self, mesh_t *mesh, unsigned int id);
void shader_bind_gbuffer(shader_t *self, texture_t *buffer);
void shader_bind_screen(shader_t *self, texture_t *buffer, float sx, float sy);
void shader_destroy(shader_t *self);
void shader_bind_ambient(shader_t *self, texture_t *ambient);
void shader_bind_probe(shader_t *self, entity_t probe);
void shader_add_source(const char *name, unsigned char data[],
		unsigned int len);
void shaders_reg(void);

/* TODO this should not be shared */
extern vs_t g_vs[16];
extern fs_t g_fs[16];

#endif /* !SHADER_H */
