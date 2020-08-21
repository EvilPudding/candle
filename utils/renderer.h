#ifndef RENDERER_H
#define RENDERER_H

#include "mesh.h"
#include "material.h"
#include "texture.h"
#include "shader.h"
#include "drawable.h"
#include "../ecs/ecm.h"

struct pass;

struct uniform;

/* typedef struct */
/* { */
/* 	char name[32]; */
/* 	texture_t *buffer; */
/* 	unsigned int draw_filter; */
/* } gbuffer_t; */

enum pass_options
{
	DEPTH_LOCK      = 1 << 0,
	DEPTH_GREATER   = 1 << 1,
	DEPTH_EQUAL     = 1 << 2,
	DEPTH_DISABLE   = 1 << 3,
	CULL_DISABLE    = 1 << 4,
	CULL_INVERT     = 1 << 5,
	ADD             = 1 << 6,
	MUL             = 1 << 7,
	BLEND           = 1 << 8,
	GEN_MIP         = 1 << 9,
	TRACK_BRIGHT    = 1 << 10
};

enum bind_type
{
	OPT_NONE,
	OPT_TEX,
	OPT_NUM,
	OPT_INT,
	OPT_UINT,
	OPT_VEC2,
	OPT_VEC3,
	OPT_VEC4,
	OPT_CAM,
	OPT_CLEAR_COLOR,
	OPT_CLEAR_DEPTH,
	OPT_CALLBACK,
	OPT_USRPTR,
	OPT_SKIP
};

typedef vec2_t     (*vec2_getter)(struct pass *pass, void *usrptr);
typedef vec3_t     (*vec3_getter)(struct pass *pass, void *usrptr);
typedef vec4_t     (*vec4_getter)(struct pass *pass, void *usrptr);
typedef float      (*number_getter)(struct pass *pass, void *usrptr);
typedef int        (*integer_getter)(struct pass *pass, void *usrptr);
typedef uint32_t   (*uinteger_getter)(struct pass *pass, void *usrptr);
typedef texture_t* (*tex_getter)(struct pass *pass, void *usrptr);
typedef void*      (*getter_cb)(struct pass *pass, void *usrptr);
typedef entity_t   (*camera_getter)(struct pass *pass, void *usrptr);

typedef struct
{
	float x, y, direction;
	int button;
	float depth;
	unsigned int geom;
} model_button_data;

typedef struct
{
	bool_t cached;

	union {
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
		struct {
			uint32_t u;
		} uinteger;
	} u;
} hash_bind_t;

typedef struct
{
	enum bind_type type;
	char name[32];
	getter_cb getter;

		texture_t *buffer;
		entity_t entity;
		float number;
		vec2_t vec2;
		vec3_t vec3;
		vec4_t vec4;
		int32_t integer;
		uint32_t uinteger;
		void *ptr;

	unsigned int hash;
	hash_bind_t vs_uniforms;
} bind_t;

typedef void(*bind_cb)(struct uniform *self, shader_t *shader);

typedef struct
{
	texture_t *buffer;
	float resolution;
	unsigned int hash;
} pass_output_t;

typedef struct pass
{
	fs_t *shader;
	char name[32];
	char shader_name[32];
	uint32_t hash;
	bool_t additive;
	bool_t multiply;
	bool_t blend;
	int32_t depth_update;
	int32_t cull;
	int32_t cull_invert;
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
	uint32_t camid;
	uint32_t bound_textures;
	uint32_t draw_every;

	/* int32_t sec; */
	/* uint64_t nano; */
	void *usrptr;
	void *renderer;
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

typedef struct renderer
{
	int frame;
	int update_frame;
	float resolution;
	int width, height;
	float proj_near, proj_far, proj_fov;

	/* texture_t *perlin; */

	int scene_changes;

	pass_output_t outputs[32];
	int outputs_num;

	int passes_size;
	pass_t passes[64];
	texture_t *output;

	int ready;

	int32_t camera_count;
	uint32_t stored_camera_frame[6];
	struct gl_camera glvars[6];
	uint32_t ubos[6];
	uint32_t ubo_changed[6];

	mat4_t relative_transform[6];
	uvec2_t pos[6];
	uvec2_t size[6];

	uint8_t *mips;
	uint32_t mips_buffer_size;
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
		uint32_t after_pass,
		uint32_t num_opts,
		...);

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
void renderer_set_model(renderer_t *self, uint32_t camid, mat4_t *model);
void renderer_update_projection(renderer_t *self);
vec3_t renderer_real_pos(renderer_t *self, float depth, vec2_t coord);
vec3_t renderer_screen_pos(renderer_t *self, vec3_t pos);
int renderer_component_menu(renderer_t *self, void *ctx);
void renderer_destroy(renderer_t *self);
void *pass_process_query_mips(pass_t *self);
void *pass_process_brightness(pass_t *self);

bind_t opt_none(void);
bind_t opt_tex(const char *name, texture_t *tex, getter_cb getter);
bind_t opt_num(const char *name, float value, getter_cb getter);
bind_t opt_int(const char *name, int32_t value, getter_cb getter);
bind_t opt_uint(const char *name, uint32_t value, getter_cb getter);
bind_t opt_vec2(const char *name, vec2_t value, getter_cb getter);
bind_t opt_vec3(const char *name, vec3_t value, getter_cb getter);
bind_t opt_vec4(const char *name, vec4_t value, getter_cb getter);
bind_t opt_cam(uint32_t camera, getter_cb getter);
bind_t opt_clear_color(vec4_t color, getter_cb getter);
bind_t opt_clear_depth(float depth, getter_cb getter);
bind_t opt_callback(getter_cb callback);
bind_t opt_usrptr(void *ptr);
bind_t opt_skip(uint32_t ticks);

#endif /* !RENDERER_H */
