#include "render_device.h"
#include <components/camera.h>
#include <components/model.h>
#include <components/spacial.h>
#include <components/light.h>
#include <components/ambient.h>
#include <components/node.h>
#include <components/name.h>
#include <systems/window.h>
#include <systems/editmode.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <candle.h>
#include <utils/noise.h>
#include <utils/nk.h>
#include <utils/material.h>

static void copy_prop(prop_t *prop, struct gl_property *glprop)
{
	glprop->color = prop->color;
	glprop->blend = prop->blend;
	glprop->scale = prop->scale;
	if(prop->texture)
	{
		glprop->texture = texture_handle(prop->texture, 0);
	}
}

void c_render_device_update_lights(c_render_device_t *self)
{
	ct_t *lights = ecm_get(ref("light"));
	khiter_t k;
	for(k = kh_begin(lights->cs); k != kh_end(lights->cs); ++k)
	{
		if(!kh_exist(lights->cs, k)) continue;
		c_light_t *light = (c_light_t*)kh_value(lights->cs, k);

		struct gl_light *gllight = &self->scene.lights[light->id];
		gllight->color = light->color;
		gllight->radius = light->radius;
		if(light->renderer)
		{
			gllight->shadow_map = texture_handle(light->renderer->output, 1);
		}
	}
}

void c_render_device_update_materials(c_render_device_t *self)
{

	int i;
	for(i = 0; i < g_mats_num; i++)
	{
		mat_t *mat = g_mats[i];
		struct gl_material *glmat = &self->scene.materials[i];
		copy_prop(&mat->albedo, &glmat->albedo);
		copy_prop(&mat->roughness, &glmat->roughness);
		copy_prop(&mat->metalness, &glmat->metalness);
		copy_prop(&mat->transparency, &glmat->transparency);
		copy_prop(&mat->normal, &glmat->normal);
		copy_prop(&mat->emissive, &glmat->emissive);
	}
}

int c_render_device_update(c_render_device_t *self)
{
	if(self->updates & 0x1)
	{
		c_render_device_update_materials(self);
		c_render_device_update_lights(self);
		self->scene.test_color = vec4(0, 1, 0, 1);
		self->updates = 0x2;
	}
	return CONTINUE;
}

void c_render_device_update_ubo(c_render_device_t *self)
{
	if(!self->ubo)
	{
		glGenBuffers(1, &self->ubo); glerr();
		glBindBuffer(GL_UNIFORM_BUFFER, self->ubo); glerr();
		glBufferData(GL_UNIFORM_BUFFER, sizeof(self->scene), &self->scene,
				GL_STATIC_DRAW); glerr();
		glBindBufferBase(GL_UNIFORM_BUFFER, 20, self->ubo); glerr();
	}
	/* else if(self->scene_changes) */
	else
	{
		glBindBuffer(GL_UNIFORM_BUFFER, self->ubo); glerr();
		GLvoid* p = glMapBuffer(GL_UNIFORM_BUFFER, GL_WRITE_ONLY); glerr();
		/* if(self->scene_changes & 0x2) */
		/* { */
			memcpy(p, &self->scene, sizeof(self->scene));
		/* } */
		/* else */
		/* { */
			/* memcpy(p, &self->scene.camera, sizeof(self->scene.camera)); */
		/* } */
		glUnmapBuffer(GL_UNIFORM_BUFFER); glerr();
		/* self->scene_changes = 0; */
	}
}

void fs_bind(fs_t *fs)
{
	c_render_device_t *render_device = c_render_device(&SYS);
	if(render_device->frag_bound == fs) return;
	glerr();

	render_device->frag_bound = fs;
	render_device->shader = NULL;
	/* printf("fs_bind %s\n", fs->filename); */
}

shader_t *vs_bind(vs_t *vs)
{
	c_render_device_t *rd = c_render_device(&SYS);

	/* printf("vs_bind %s\n", vs->name); */

	if(!rd->shader || rd->shader->index != vs->index)
	{
		fs_t *fs = rd->frag_bound;

		shader_t **sh = &fs->combinations[vs->index];
		if(!*sh) *sh = shader_new(fs, vs);
		rd->shader = *sh; 
	}

	shader_bind(rd->shader);

	if(rd->shader && rd->bind_function)
	{
		rd->bind_function(rd->usrptr, rd->shader);
	}
	return rd->shader;
}

void c_render_device_rebind(
		c_render_device_t *self,
		void(*bind_function)(void *usrptr, shader_t *shader),
		void *usrptr)
{
	self->bind_function = bind_function;
	self->usrptr = usrptr;
}

static int c_render_device_gl(c_render_device_t *self)
{
	return 1;
}

static int c_render_device_created(c_render_device_t *self)
{
	loader_push(g_candle->loader, (loader_cb)c_render_device_gl, NULL,
			(c_t*)self);
	return CONTINUE;
}

c_render_device_t *c_render_device_new()
{
	c_render_device_t *self = component_new("render_device");
	self->updates = 0x1;

	return self;
}

void world_changed()
{
	c_render_device_t *rd = c_render_device(&SYS);
	if(rd) rd->updates |= 0x1;
}

int c_render_device_draw(c_render_device_t *self)
{
	if(self->updates & 0x2)
	{
		c_render_device_update_ubo(self);
		self->updates &= ~0x2;
	}
	self->frame++;
	entity_signal(entity_null, sig("world_pre_draw"), NULL, NULL);

	return CONTINUE;
}

int world_frame(void)
{
	c_render_device_t *rd = c_render_device(&SYS);
	return rd->frame;
}

REG()
{
	ct_t *ct = ct_new("render_device", sizeof(c_render_device_t),
			NULL, NULL, 1, ref("window"));

	ct_listener(ct, WORLD, ref("world_update"), c_render_device_update);

	ct_listener(ct, WORLD | 100, ref("world_draw"), c_render_device_draw);

	ct_listener(ct, ENTITY, ref("entity_created"), c_render_device_created);

	signal_init(sig("world_pre_draw"), sizeof(void*));
}
