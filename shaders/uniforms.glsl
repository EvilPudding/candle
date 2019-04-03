#ifndef UNIFORMS
#define UNIFORMS

struct camera_t
{
	mat4 previous_view;
	mat4 model;
	mat4 view;
	mat4 projection;
	mat4 inv_projection;
	vec3 pos;
	float exposure;
};

struct light_t
{
	vec4 color;
	uvec2 pos;
	int lod;
	float radius;
};

struct property_t
{
	vec4 color;
	uvec2 size;
	float blend;
	float scale;
	vec3 padding;
	uint layer;
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

layout(std140) uniform renderer_t
{
	camera_t camera;
} renderer;

layout(std140) uniform scene_t
{
	material_t materials[128];
	light_t lights[62];
	vec4 test_color;
} scene;

#ifdef HAS_SKIN
layout(std140) uniform skin_t
{
	mat4 bones[30];
} skin;
#endif

/* layout(location = 22) */ uniform vec2 screen_size;
/* layout(location = 23) */ uniform bool has_tex;
/* layout(location = 24) */ uniform bool has_normals;
/* layout(location = 25) */ uniform bool receive_shadows;
/* layout(location = 26) */ uniform sampler2D g_cache;
/* layout(location = 27) */ uniform sampler2D g_indir;
/* layout(location = 28) */ uniform sampler2D g_probes;

/* layout(location = 25) uniform camera_t camera; */

#define mat(prop) (scene.materials[matid].prop)
#define light(prop) (scene.lights[matid].prop)
#define camera(prop) (renderer.camera.prop)


#endif
// vim: set ft=c:

