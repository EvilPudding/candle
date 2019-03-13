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

enum pass_options
{
	DEPTH_LOCK		= 1 << 4,
	DEPTH_GREATER	= 1 << 5,
	DEPTH_EQUAL		= 1 << 6,
	DEPTH_DISABLE	= 1 << 7,
	CULL_DISABLE	= 1 << 8,
	ADD				= 1 << 9,
	MUL				= 1 << 10,
	GEN_MIP			= 1 << 11,
	TRACK_BRIGHT	= 1 << 12
};

enum bind_type
{
	NONE,
	TEX,
	NUM,
	INT,
	VEC2,
	VEC3,
	VEC4,
	CAM,
	CLEAR_COLOR,
	CLEAR_DEPTH,
	CALLBACK,
	SKIP
};

typedef vec2_t(*vec2_getter)(void *usrptr);
typedef vec3_t(*vec3_getter)(void *usrptr);
typedef vec4_t(*vec4_getter)(void *usrptr);
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
			uint32_t u_brightness;
			uint32_t u_tex[16];
		} buffer;
		struct {
			uint32_t u;
		} number;
		struct {
			uint32_t u;
		} vec2;
		struct {
			uint32_t u;
		} vec3;
		struct {
			uint32_t u;
		} vec4;
		struct {
			uint32_t u;
		} integer;
	};
} shader_bind_t;

typedef struct
{
	enum bind_type type;
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
		vec4_t vec4;
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
	char shader_name[32];
	uint32_t hash;
	int32_t additive;
	int32_t multiply;
	int32_t depth_update;
	int32_t cull;
	uint32_t depth_func;
	unsigned int clear;
	uint32_t draw_signal;
	entity_t camera;

	int32_t binds_size;
	bind_t *binds;

	texture_t *output;

	int32_t framebuffer_id;
	texture_t *depth;
	vec4_t clear_color;
	float clear_depth;
	int32_t auto_mip;
	int32_t track_brightness;

	int32_t active;
	int32_t camid;
	uint32_t bound_textures;
	uint32_t draw_every;
} pass_t;

struct gl_camera
{
	mat4_t previous_view;
	mat4_t model;
	mat4_t inv_model;
	mat4_t projection;
	mat4_t inv_projection;
	vec3_t pos;
	float exposure;
};

typedef struct c_renderer_t
{
	int frame;
	int update_frame;
	float resolution;
	int width, height;
	float near, far;
	float fov;

	/* texture_t *perlin; */

	int scene_changes;

	pass_output_t outputs[32];
	int outputs_num;

	int passes_size;
	pass_t passes[64];
	texture_t *output;

	int ready;

	// GL PROPS
	int depth_inverted;

	int32_t camera_count;
	struct gl_camera glvars[6];
	uint32_t ubos[6];
	uint32_t ubo_changed[6];
	int moved[6];

	bool_t cubemap;
	ivec2_t pos;
	uvec2_t size;
} renderer_t;

renderer_t *renderer_new(float resolution);
int renderer_draw(renderer_t *self);
void renderer_set_resolution(renderer_t *self, float resolution);

void renderer_set_output(renderer_t *self, texture_t *tex);

texture_t *renderer_tex(renderer_t *self, unsigned int hash);
void renderer_add_tex(renderer_t *self, const char *name,
		float resolution, texture_t *buffer);

void renderer_add_pass(
		renderer_t *self,
		const char *name,
		const char *shader_name,
		uint32_t draw_signal,
		enum pass_options flags,
		texture_t *output,
		texture_t *depth,
		uint32_t framebuffer,
		bind_t binds[]);

void renderer_add_kawase(renderer_t *self, texture_t *t1, texture_t *t2,
		int from_mip, int to_mip);

void renderer_toggle_pass(renderer_t *self, uint32_t hash, int active);

entity_t renderer_get_camera(renderer_t *self);

entity_t renderer_entity_at_pixel(renderer_t *self, int x, int y,
		float *depth);
unsigned int renderer_geom_at_pixel(renderer_t *self, int x, int y,
		float *depth);

void renderer_invert_depth(renderer_t *self, int inverted);
int renderer_resize(renderer_t *self, int width, int height);

pass_t *renderer_pass(renderer_t *self, unsigned int hash);
void renderer_default_pipeline(renderer_t *self);
void renderer_set_model(renderer_t *self, int32_t camid, mat4_t *model);
void renderer_update_projection(renderer_t *self);
vec3_t renderer_real_pos(renderer_t *self, float depth, vec2_t coord);
vec3_t renderer_screen_pos(renderer_t *self, vec3_t pos);
int renderer_component_menu(renderer_t *self, void *ctx);
void renderer_destroy(renderer_t *self);
void *renderer_process_query_mips(renderer_t *self);

#endif /* !RENDERER_H */
