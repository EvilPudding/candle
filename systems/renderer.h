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
	PASS_FOR_EACH_LIGHT    = 1 << 0,
	PASS_MIPMAPED 		   = 1 << 1,
	PASS_RECORD_BRIGHTNESS = 1 << 2,
	PASS_GBUFFER		   = 1 << 3,
	PASS_CUBEMAP 		   = 1 << 4,
	PASS_CLEAR_COLOR	   = 1 << 5,
	PASS_CLEAR_DEPTH	   = 1 << 6
} pass_options;

typedef enum
{
	BIND_NONE,
	BIND_PASS_OUTPUT,
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
	bind_type_t type;
	char name[32];
	union
	{
		struct {
			uint u_brightness;
			uint u_texture;
			pass_t *pass;
		} pass_output;
		struct {
			uint u;
			float value;
		} number;
		struct {
			uint u;
			vec2_t value;
		} vec2;
		struct {
			uint u;
			vec3_t value;
		} vec3;
		struct {
			uint u;
			int value;
		} integer;
		struct {
			uint u_view;
			uint u_pos;
			uint u_exposure;
			uint u_projection;
#ifdef MESH4
			uint u_angle4;
#endif
			entity_t entity;
		} camera;
		struct {
			uint u_depth;
			uint u_diffuse;
			uint u_specular;
			uint u_reflection;
			uint u_normal;
			uint u_transparency;
			uint u_wposition;
			uint u_cposition;
			uint u_id;
			int id;
			char name[32];
		} gbuffer;
	};
	getter_cb getter;
	void *usrptr;
} bind_t;

typedef void(*bind_cb)(uniform_t *self, shader_t *shader);

typedef struct pass_t
{
	shader_t *shader;
	texture_t *output;
	int for_each_light;
	float x;
	float y;
	char feed_name[32];
	int gbuffer;
	unsigned int clear;
	ulong draw_signal;

	int binds_size;
	bind_t binds[15];
	int cached;

	int mipmaped;
	int record_brightness;

	int output_from;
	/* TODO make this a ct_camera injected vertex modifier */
	/* uint u_mvp; */ 
	/* uint u_m; */ 
	/* mat4_t vp; */
	/* --------------------------------------------------- */
} pass_t;

typedef struct c_renderer_t
{
	c_t super;

	int width;
	int height;
	float resolution;

	texture_t *bound;

	texture_t *temp_buffers[2];

	texture_t *perlin;
	int perlin_size;

	/* int gbuffers_size; */
	/* gbuffer_t gbuffers[16]; */

	entity_t quad;
	shader_t *quad_shader;
	shader_t *gbuffer_shader;

	int probe_update_id;

	int passes_size;
	pass_t passes[16];

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
DEF_SIG(render_transparent);
DEF_SIG(render_quad);
DEF_SIG(offscreen_render);

DEF_CASTER(ct_renderer, c_renderer, c_renderer_t)

c_renderer_t *c_renderer_new(float resolution, int auto_exposure,
		int roughness, int lock_fps);
int c_renderer_draw(c_renderer_t *self);
void c_renderer_register(void);
void c_renderer_set_resolution(c_renderer_t *self, float resolution);
void c_renderer_draw_to_texture(c_renderer_t *self, shader_t *shader,
		texture_t *screen, float scale, texture_t *buffer);
void c_renderer_add_camera(c_renderer_t *self, entity_t camera);

void c_renderer_add_pass(c_renderer_t *self, const char *feed_name, int flags,
		ulong draw_signal, float wid, float hei, const char *shader_name,
		bind_t binds[]);

void c_renderer_clear_shader(c_renderer_t *self, shader_t *shader);
int c_renderer_scene_changed(c_renderer_t *self);
entity_t c_renderer_entity_at_pixel(c_renderer_t *self, int x, int y,
		float *depth);

void pass_set_model(pass_t *self, mat4_t model);

#endif /* !RENDERER_H */
