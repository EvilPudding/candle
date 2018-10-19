#line 1
#extension GL_ARB_explicit_uniform_location : require
#extension GL_ARB_texture_query_levels : enable
#extension GL_ARB_bindless_texture : enable
#extension GL_EXT_shader_texture_lod: enable
#extension GL_OES_standard_derivatives : enable

struct camera_t
{
	mat4 projection;
	mat4 inv_projection;
	mat4 model;
	mat4 view;
	vec3 pos;
	float exposure;
};

struct light_t
{
	vec4 color;
	samplerCube shadow_map;
	float radius;
	float padding;
};

struct property_t
{
	vec4 color;
	float blend;
	float scale;
	sampler2D texture;
};

struct material_t
{
	property_t albedo;
	property_t roughness;
	property_t metalness;
	property_t transparency;
	property_t normal;
	property_t emissive;
};

struct pass_t
{
	vec2 screen_size;
	vec2 padding;
};

layout(bindless_sampler) uniform;
layout(std140, binding = 20) uniform scene_t
{
	/* RENDERER */
	camera_t camera;
	pass_t passes[32];
	/* GLOBAL */
	material_t materials[255];
	light_t lights[255];
	vec4 test_color;
} scene;

layout(std140, binding = 21) uniform bones_t
{
	mat4 transforms[30];
} bones;

layout(location = 23) uniform uint passid;
layout(location = 24) uniform uint has_tex;

#define mat(prop) (scene.materials[matid].prop)
#define light(prop) (scene.lights[matid].prop)
#define pass(prop) (scene.passes[passid].prop)
#define camera(prop) (scene.camera.prop)

// vim: set ft=c:
