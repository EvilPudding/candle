#ifndef RENDERER_H
#define RENDERER_H

#include <utils/glutil.h>
#include <utils/material.h>
#include <utils/texture.h>
#include <utils/mesh.h>
#include <utils/shader.h>
#include <utils/drawable.h>
#include <ecs/ecm.h>

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
	PASS_DEPTH_LOCK		= 1 << 4,
	PASS_DEPTH_GREATER	= 1 << 5,
	PASS_DEPTH_EQUAL	= 1 << 6,
	PASS_ADDITIVE		= 1 << 7,
	PASS_MULTIPLY		= 1 << 8
} pass_options;

typedef enum
{
	BIND_NONE,
	BIND_TEX,
	BIND_NUM,
	BIND_VEC2,
	BIND_VEC3,
	BIND_INT
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
		entity_t entity;
		float number;
		vec2_t vec2;
		vec3_t vec3;
		int integer;
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
	int multiply;
	int depth_update;
	uint32_t depth_func;
	unsigned int clear;
	uint32_t draw_signal;
	entity_t camera;

	int binds_size;
	bind_t *binds;

	texture_t *output;
	texture_t *depth;

	int active;
} pass_t;

struct gl_camera
{
	mat4_t inv_model;
	mat4_t model;
	mat4_t projection;
	mat4_t inv_projection;
	vec3_t pos;
	float exposure;
};
struct gl_renderer
{
	struct gl_camera camera;
};

typedef struct c_renderer_t
{
	int frame;
	int width;
	int height;
	float resolution;

	/* texture_t *perlin; */

	int scene_changes;

	pass_output_t outputs[32];
	int outputs_num;

	int passes_size;
	pass_t passes[32];
	texture_t *output;
	texture_t *fallback_depth;

	int auto_exposure; /* AUTO EXPOSURE */
	int roughness; /* ssr roughness */

	int ready;

	// GL PROPS
	int depth_inverted;

	unsigned int ubo;
	struct gl_renderer glvars;
} renderer_t;

renderer_t *renderer_new(float resolution, int auto_exposure,
		int roughness, int lock_fps);
int renderer_draw(renderer_t *self);
void renderer_set_resolution(renderer_t *self, float resolution);

void renderer_set_output(renderer_t *self, unsigned int hash);

texture_t *renderer_tex(renderer_t *self, unsigned int hash);
void renderer_add_tex(renderer_t *self, const char *name,
		float resolution, texture_t *buffer);
void renderer_add_pass(renderer_t *self, const char *name,
		const char *shader_name, uint32_t draw_signal, int flags,
		texture_t *output, texture_t *depth, bind_t binds[]);
void renderer_toggle_pass(renderer_t *self, uint hash, int active);

entity_t renderer_get_camera(renderer_t *self);

void renderer_clear_shader(renderer_t *self, shader_t *shader);
entity_t renderer_entity_at_pixel(renderer_t *self, int x, int y,
		float *depth);
unsigned int renderer_geom_at_pixel(renderer_t *self, int x, int y,
		float *depth);

void renderer_invert_depth(renderer_t *self, int inverted);
int renderer_resize(renderer_t *self, int width, int height);

#endif /* !RENDERER_H */
