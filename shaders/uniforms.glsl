#ifndef UNIFORMS
#define UNIFORMS
#extension GL_ARB_explicit_uniform_location : require
#extension GL_ARB_texture_query_levels : enable
#extension GL_ARB_bindless_texture : enable

struct camera_t
{
	mat4 view;
	mat4 model;
	mat4 projection;
	mat4 inv_projection;
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

layout(bindless_sampler) uniform;
layout(std140, binding = 19) uniform renderer_t
{
	camera_t camera;
} renderer;
layout(std140, binding = 20) uniform scene_t
{
	material_t materials[255];
	light_t lights[255];
	vec4 test_color;
} scene;

layout(std140, binding = 21) uniform skin_t
{
	mat4 bones[30];
} skin;

layout(location = 22) uniform vec2 screen_size;
layout(location = 23) uniform uint has_tex;
layout(location = 24) uniform bool has_normals;
layout(location = 25) uniform uint receive_shadows;

/* layout(location = 25) uniform camera_t camera; */

#define mat(prop) (scene.materials[matid].prop)
#define light(prop) (scene.lights[matid].prop)
#define camera(prop) (renderer.camera.prop)

#endif
// vim: set ft=c:

