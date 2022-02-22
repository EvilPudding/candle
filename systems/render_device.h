#ifndef RENDER_DEVICE_H
#define RENDER_DEVICE_H

#include "../utils/shader.h"
#include "../ecs/ecm.h"

struct gl_light
{
	vec3_t color;
	float volumetric;
	ivec2_t pos;
	uint32_t lod;
	float radius;
};

struct gl_scene
{
	struct gl_light lights[62];
	vec3_t test_color;
	float time;
};

struct gl_bones
{
	mat4_t bones[30];
};

typedef void(*rd_bind_cb)(void *usrptr, shader_t *shader);

typedef struct c_render_device
{
	c_t super;

	rd_bind_cb bind_function;
	void *usrptr;

	fs_t *frag_bound;
	shader_t *shader;
	uint32_t ubo;
	struct gl_scene scene;
	int32_t updates_ram;
	int32_t updates_ubo;
	int32_t frame;
	uint32_t update_frame;
	bool_t cull_invert;
	uint32_t bound_ubos[32];
} c_render_device_t;

DEF_CASTER(ct_render_device, c_render_device, c_render_device_t)

c_render_device_t *c_render_device_new(void);
void c_render_device_rebind(
		c_render_device_t *self,
		void(*bind_function)(void *usrptr, shader_t *shader),
		void *usrptr);
void world_changed(void);
void c_render_device_bind_ubo(c_render_device_t *self, uint32_t base,
                              uint32_t ubo);

#endif /* !RENDER_DEVICE_H */
