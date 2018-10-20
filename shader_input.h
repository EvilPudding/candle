#ifndef SHADER_INPUT_H
#define SHADER_INPUT_H

struct gl_camera
{
	mat4_t inv_model;
	mat4_t model;
	mat4_t projection;
	mat4_t inv_projection;
	vec3_t pos;
	float exposure;
};

struct gl_light
{
	vec4_t color;
	uint64_t shadow_map;
	float radius;
	float padding;
};

struct gl_property
{
	vec4_t color;
	float blend;
	float scale;
	uint64_t texture;
};

struct gl_material
{
	struct gl_property albedo;
	struct gl_property roughness;
	struct gl_property metalness;
	struct gl_property transparency;
	struct gl_property normal;
	struct gl_property emissive;
};

struct gl_pass
{
	vec2_t screen_size;
	vec2_t padding;
};

struct gl_scene
{
	/* RENDERER */
	struct gl_camera camera;
	struct gl_pass passes[32];
	/* GLOBAL */
	struct gl_material materials[255];
	struct gl_light lights[255];
	vec4_t test_color;
};

struct gl_bones
{
	mat4_t bones[30];
};

#endif /* !SHADER_INPUT_H */
