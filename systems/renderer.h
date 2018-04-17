#ifndef RENDERER_H
#define RENDERER_H

#include <utils/glutil.h>
#include <utils/material.h>
#include <utils/texture.h>
#include <utils/mesh.h>
#include <utils/shader.h>
#include <ecm.h>

typedef struct pass_t pass_t;

typedef struct uniform_t uniform_t;

/* typedef struct */
/* { */
/* 	char name[32]; */
/* 	texture_t *buffer; */
/* 	unsigned int draw_filter; */
/* } gbuffer_t; */

enum
{
	PASS_CUBEMAP   		= 1 << 0,
	PASS_CLEAR_COLOR	= 1 << 1,
	PASS_CLEAR_DEPTH	= 1 << 2,
	PASS_LOCK_DEPTH		= 1 << 4,
	PASS_INVERT_DEPTH	= 1 << 5,
	PASS_ADDITIVE		= 1 << 6
} pass_options;

typedef enum
{
	BIND_NONE,
	BIND_OUT,
	BIND_DEPTH,
	BIND_TEX,
	BIND_NUM,
	BIND_VEC2,
	BIND_VEC3,
	BIND_INT,
	BIND_CAM
} bind_type_t;

typedef vec2_t(*vec2_getter)(void *usrptr);
typedef vec3_t(*vec3_getter)(void *usrptr);
typedef float(*number_getter)(void *usrptr);
typedef int(*integer_getter)(void *usrptr);
typedef void*(*getter_cb)(void *usrptr);
typedef entity_t(*camera_getter)(void *usrptr);

typedef struct
{
	float x, y, direction;
	int button;
	float depth;
	unsigned int geom;
} model_button_data;

typedef struct
{
	int cached;
	union
	{
		struct {
			uint u_brightness;
			uint u_tex[16];
		} buffer;
		struct {
			uint u;
		} number;
		struct {
			uint u;
		} vec2;
		struct {
			uint u;
		} vec3;
		struct {
			uint u;
		} integer;
		struct {
			uint u_view;
			uint u_model;
			uint u_pos;
			uint u_exposure;
			uint u_projection;
			uint u_inv_projection;
#ifdef MESH4
			uint u_angle4;
#endif
		} camera;
	};
} shader_bind_t;

typedef struct
{
	bind_type_t type;
	char name[32];
	getter_cb getter;
	void *usrptr;
	union
	{
		texture_t *buffer;
		float number;
		vec2_t vec2;
		vec3_t vec3;
		int integer;
		entity_t camera;
	};
	unsigned int hash;
	shader_bind_t vs_uniforms[16];
} bind_t;

typedef void(*bind_cb)(uniform_t *self, shader_t *shader);

typedef struct
{
	texture_t *buffer;
	float resolution;
	unsigned int hash;
} pass_output_t;

typedef struct pass_t
{
	fs_t *shader;
	char name[32];
	unsigned int hash;
	int additive;
	int depth_update;
	int invert_depth;
	unsigned int clear;
	ulong draw_signal;

	int binds_size;
	bind_t *binds;

	texture_t *output;
	texture_t *depth;

} pass_t;

typedef struct c_renderer_t
{
	c_t super;

	int frame;
	int width;
	int height;
	float resolution;

	texture_t *perlin;
	int perlin_size;

	int probe_update_id;
	entity_t bound_probe;
	vec3_t bound_camera_pos;
	mat4_t *bound_view;
	mat4_t *bound_projection;
	mat4_t *bound_camera_model;
	float bound_exposure;
	float bound_angle4;
	entity_t bound_light;

	pass_output_t outputs[32];
	int outputs_num;

	int passes_size;
	pass_t passes[32];
	texture_t *output;
	texture_t *fallback_depth;

	pass_t *current_pass;
	fs_t *frag_bound;
	shader_t *shader;

	int auto_exposure; /* AUTO EXPOSURE */
	int roughness; /* ssr roughness */

	entity_t camera;
#ifdef MESH4
	float angle4;
#endif

	int ready;
} c_renderer_t;

DEF_CASTER("renderer", c_renderer, c_renderer_t)

c_renderer_t *c_renderer_new(float resolution, int auto_exposure,
		int roughness, int lock_fps);
int c_renderer_draw(c_renderer_t *self);
void c_renderer_register(void);
void c_renderer_set_resolution(c_renderer_t *self, float resolution);
void c_renderer_add_camera(c_renderer_t *self, entity_t camera);

void c_renderer_set_output(c_renderer_t *self, unsigned int hash);

texture_t *c_renderer_tex(c_renderer_t *self, unsigned int hash);
void c_renderer_add_pass(c_renderer_t *self, const char *name,
		const char *shader_name, ulong draw_signal,
		int flags, bind_t binds[]);
void c_renderer_replace_pass(c_renderer_t *self, const char *name,
		const char *shader_name, ulong draw_signal,
		int flags, bind_t binds[]);

entity_t c_renderer_get_camera(c_renderer_t *self);

void c_renderer_clear_shader(c_renderer_t *self, shader_t *shader);
int c_renderer_scene_changed(c_renderer_t *self);
entity_t c_renderer_entity_at_pixel(c_renderer_t *self, int x, int y,
		float *depth);
unsigned int c_renderer_geom_at_pixel(c_renderer_t *self, int x, int y,
		float *depth);

void pass_set_model(pass_t *self, mat4_t model);

#endif /* !RENDERER_H */
