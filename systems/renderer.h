#ifndef RENDERER_H
#define RENDERER_H

#include "../glutil.h"
#include "../material.h"
#include "../texture.h"
#include "../mesh.h"
#include "../shader.h"
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
	PASS_MIPMAPED 		   		= 1 << 0,
	PASS_RECORD_BRIGHTNESS 		= 1 << 1,
	PASS_GBUFFER		   		= 1 << 2,
	PASS_CUBEMAP 		   		= 1 << 3,
	PASS_CLEAR_COLOR	   		= 1 << 4,
	PASS_CLEAR_DEPTH	   		= 1 << 5,
	PASS_DISABLE_DEPTH	   		= 1 << 6,
	PASS_DISABLE_DEPTH_UPDATE	= 1 << 7,
	PASS_INVERT_DEPTH			= 1 << 8,
	PASS_ADDITIVE				= 1 << 9
} pass_options;

typedef enum
{
	BIND_NONE,
	BIND_PASS_OUTPUT,
	BIND_PREV_PASS_OUTPUT,
	BIND_NUMBER,
	BIND_VEC2,
	BIND_VEC3,
	BIND_INTEGER,
	BIND_GBUFFER,
	BIND_CAMERA
} bind_type_t;

typedef vec2_t(*vec2_getter)(void *usrptr);
typedef vec3_t(*vec3_getter)(void *usrptr);
typedef float(*number_getter)(void *usrptr);
typedef int(*integer_getter)(void *usrptr);
typedef void*(*getter_cb)(void *usrptr);
typedef entity_t(*camera_getter)(void *usrptr);

typedef struct
{
	int cached;
	union
	{
		struct {
			uint u_brightness;
			uint u_texture;
			uint u_depth;
		} pass_output;
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
#ifdef MESH4
			uint u_angle4;
#endif
		} camera;
		struct {
			uint u_depth;
			uint u_diffuse;
			uint u_specular;
			uint u_transparency;
			uint u_position;
			uint u_id;
			uint u_geomid;
			uint u_normal;
		} gbuffer;
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
		struct {
			pass_t *pass;
			char name[32];
		} pass_output;
		float number;
		vec2_t vec2;
		vec3_t vec3;
		int integer;
		entity_t camera;
		int gbuffer;
	};
	shader_bind_t vs_uniforms[16];
} bind_t;

typedef void(*bind_cb)(uniform_t *self, shader_t *shader);

typedef struct pass_t
{
	fs_t *shader;
	texture_t *output;
	char feed_name[32];
	int gbuffer;
	int additive;
	int disable_depth;
	int disable_depth_update;
	int invert_depth;
	unsigned int clear;
	ulong draw_signal;

	float resolution;
	int binds_size;
	bind_t *binds;

	int mipmaped;
	int record_brightness;

	int output_from;
	int id;

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

	/* int gbuffers_size; */
	/* gbuffer_t gbuffers[16]; */

	int probe_update_id;
	entity_t bound_probe;
	vec3_t bound_camera_pos;
	mat4_t *bound_view;
	mat4_t *bound_projection;
	mat4_t *bound_camera_model;
	float bound_exposure;
	float bound_angle4;
	entity_t bound_light;

	int passes_size;
	pass_t passes[32];
	char output[32];
	int output_id;

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
	int passes_bound;
} c_renderer_t;

DEF_SIG(render_visible);
DEF_SIG(render_lights);
DEF_SIG(render_transparent);
DEF_SIG(render_decals);
DEF_SIG(render_quad);
DEF_SIG(offscreen_render);

DEF_CASTER(ct_renderer, c_renderer, c_renderer_t)

c_renderer_t *c_renderer_new(float resolution, int auto_exposure,
		int roughness, int lock_fps);
int c_renderer_draw(c_renderer_t *self);
void c_renderer_register(void);
void c_renderer_set_resolution(c_renderer_t *self, float resolution);
void c_renderer_add_camera(c_renderer_t *self, entity_t camera);

void c_renderer_set_output(c_renderer_t *self, const char *name);

void c_renderer_add_pass(c_renderer_t *self, const char *feed_name,
		const char *shader_name, ulong draw_signal, float resolution,
		int flags, bind_t binds[]);
void c_renderer_replace_pass(c_renderer_t *self, const char *feed_name,
		const char *shader_name, ulong draw_signal, float resolution,
		int flags, bind_t binds[]);

entity_t c_renderer_get_camera(c_renderer_t *self);

void c_renderer_clear_shader(c_renderer_t *self, shader_t *shader);
int c_renderer_scene_changed(c_renderer_t *self);
entity_t c_renderer_entity_at_pixel(c_renderer_t *self, int x, int y,
		float *depth);
entity_t c_renderer_geom_at_pixel(c_renderer_t *self, int x, int y,
		float *depth);

void pass_set_model(pass_t *self, mat4_t model);

#endif /* !RENDERER_H */
