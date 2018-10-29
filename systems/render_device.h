#ifndef RENDER_DEVICE_H
#define RENDER_DEVICE_H

#include <utils/glutil.h>
#include <utils/material.h>
#include <utils/texture.h>
#include <utils/mesh.h>
#include <utils/shader.h>
#include <utils/drawable.h>
#include <ecs/ecm.h>

typedef struct pass_t pass_t;

typedef struct uniform_t uniform_t;

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
	struct gl_material materials[255];
	struct gl_light lights[255];
	vec4_t test_color;
};

struct gl_bones
{
	mat4_t bones[30];
};

typedef struct c_render_device_t
{
	c_t super;

	void(*bind_function)(void *usrptr, shader_t *shader);
	void *usrptr;

	fs_t *frag_bound;
	shader_t *shader;
	unsigned int ubo;
	struct gl_scene scene;
	int updates;
	int frame;
} c_render_device_t;

DEF_CASTER("render_device", c_render_device, c_render_device_t)

c_render_device_t *c_render_device_new(void);
void c_render_device_rebind(
		c_render_device_t *self,
		void(*bind_function)(void *usrptr, shader_t *shader),
		void *usrptr);
void world_changed(void);

#endif /* !RENDER_DEVICE_H */
