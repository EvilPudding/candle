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

#include "shader_input.h"

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
} c_render_device_t;

DEF_CASTER("render_device", c_render_device, c_render_device_t)

c_render_device_t *c_render_device_new(void);
void c_render_device_rebind(
		c_render_device_t *self,
		void(*bind_function)(void *usrptr, shader_t *shader),
		void *usrptr);
void world_changed(void);

#endif /* !RENDER_DEVICE_H */
