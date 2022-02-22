#include "../utils/glutil.h"
#include "render_device.h"
#include "../components/camera.h"
#include "../components/model.h"
#include "../components/light.h"
#include "../components/ambient.h"
#include "../components/node.h"
#include "../components/name.h"
#include "../systems/window.h"
#include "../systems/editmode.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../candle.h"
#include "../utils/noise.h"
#include "../utils/nk.h"
#include "../utils/material.h"

void c_render_device_update_lights(c_render_device_t *self)
{
	khiter_t k;
	ct_t *lights = ecm_get(ct_light);
	c_light_t *light;
	struct gl_light *gllight;
	if (!lights) return;
	for(k = kh_begin(lights->cs); k != kh_end(lights->cs); ++k)
	{
		if(!kh_exist(lights->cs, k)) continue;
		light = (c_light_t*)kh_value(lights->cs, k);

		gllight = &self->scene.lights[light->id];
		gllight->color = vec4_xyz(light->color);
		gllight->pos = light->tile.pos;
		gllight->lod = light->lod;
		gllight->radius = light->radius;
		gllight->volumetric = light->volumetric_intensity;
	}
}

int c_render_device_update(c_render_device_t *self)
{
	if(self->updates_ram)
	{
		c_render_device_update_lights(self);
		self->scene.test_color = vec3(0, 1, 0);
		self->scene.time = ((float)self->frame) * 0.01f;
		self->updates_ubo = true;
		self->updates_ram = false;
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
		c_render_device_bind_ubo(self, 20, self->ubo);
	}
	/* else if(self->scene_changes) */
	else
	{
		glBindBuffer(GL_UNIFORM_BUFFER, self->ubo); glerr();
		/* GLvoid* p = glMapBuffer(GL_UNIFORM_BUFFER, GL_WRITE_ONLY); glerr(); */
		/* memcpy(p, &self->scene, sizeof(self->scene)); */

		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(self->scene), &self->scene);glerr();

		/* glUnmapBuffer(GL_UNIFORM_BUFFER); glerr(); */

		self->updates_ubo = 0;
	}
}

void fs_bind(fs_t *fs)
{
	c_render_device_t *render_device = c_render_device(&SYS);
	if (!render_device) return;
	if(render_device->frag_bound == fs) return;
	glerr();

	render_device->frag_bound = fs;
	render_device->shader = NULL;
}

void c_render_device_bind_ubo(c_render_device_t *self, uint32_t base,
                              uint32_t ubo)
{
	self->bound_ubos[base] = ubo;
}

extern texture_t *g_cache;
extern texture_t *g_indir;
extern texture_t *g_probe_cache;
shader_t *vs_bind(vs_t *vs, uint32_t fs_variation)
{
	uint32_t loc;
	c_render_device_t *rd = c_render_device(&SYS);

	if(   !rd->shader || rd->shader->index != vs->index
	   || rd->shader->fs_variation != fs_variation)
	{
		shader_t **sh;
		fs_t *fs = rd->frag_bound;
		if (!fs) return NULL;

		if (fs_variation >= fs->variations_num)
		{
			fs_variation = 0;
		}
		sh = &fs->variations[fs_variation].combinations[vs->index];
		if (!*sh) *sh = shader_new(fs, fs_variation, vs);
		rd->shader = *sh; 
	}

	if (!rd->shader->ready)
		return NULL;

	shader_bind(rd->shader);

	loc = shader_cached_uniform(rd->shader, ref("g_cache"));
	glUniform1i(loc, 3);
	glActiveTexture(GL_TEXTURE0 + 3);
	texture_bind(g_cache, 0);

	loc = shader_cached_uniform(rd->shader, ref("g_indir"));
	glUniform1i(loc, 4);
	glActiveTexture(GL_TEXTURE0 + 4);
	texture_bind(g_indir, 0);

	loc = shader_cached_uniform(rd->shader, ref("g_probes_depth"));
	glUniform1i(loc, 5);
	glActiveTexture(GL_TEXTURE0 + 5);
	texture_bind(g_probe_cache, 0);
	glerr();

	loc = shader_cached_uniform(rd->shader, ref("g_probes"));
	glUniform1i(loc, 6);
	glActiveTexture(GL_TEXTURE0 + 6);
	texture_bind(g_probe_cache, 1);
	glerr();

	if (rd->bound_ubos[20] && rd->shader->scene_ubi != ~0)
	{
		glUniformBlockBinding(rd->shader->program, rd->shader->scene_ubi, 20); glerr();
		glBindBufferBase(GL_UNIFORM_BUFFER, 20, rd->bound_ubos[20]); glerr();
	}

	if (rd->bound_ubos[19] && rd->shader->renderer_ubi != ~0)
	{
		glUniformBlockBinding(rd->shader->program, rd->shader->renderer_ubi, 19); glerr();
		glBindBufferBase(GL_UNIFORM_BUFFER, 19, rd->bound_ubos[19]); glerr();
	}

	if (rd->bound_ubos[21] && rd->shader->has_skin && rd->shader->skin_ubi != ~0)
	{
		glUniformBlockBinding(rd->shader->program, rd->shader->skin_ubi, 21); glerr();
		glBindBufferBase(GL_UNIFORM_BUFFER, 21, rd->bound_ubos[21]); glerr();
	}

	if (rd->bound_ubos[22] && rd->shader->cms_ubi != ~0)
	{
		glUniformBlockBinding(rd->shader->program, rd->shader->cms_ubi, 22); glerr();
		glBindBufferBase(GL_UNIFORM_BUFFER, 22, rd->bound_ubos[22]); glerr();
	}


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

c_render_device_t *c_render_device_new()
{
	c_render_device_t *self = component_new(ct_render_device);

	c_render_device_update(self);
	self->updates_ubo = 1;

	svp_init();

	return self;
}

void world_changed()
{
	c_render_device_t *rd = c_render_device(&SYS);
	if(rd) rd->updates_ram = true;
}

int c_render_device_draw(c_render_device_t *self)
{
	if(self->updates_ubo)
	{
		c_render_device_update_ubo(self);
		self->update_frame++;
	}
	self->frame++;
	entity_signal(entity_null, sig("world_pre_draw"), NULL, NULL);

	return CONTINUE;
}

int world_frame(void)
{
	c_render_device_t *rd = c_render_device(&SYS);
	if (!rd) return 0;
	return rd->frame;
}

void c_render_device_destroy(c_render_device_t *self)
{
	svp_destroy();
}

void ct_render_device(ct_t *self)
{
	ct_init(self, "render_device", sizeof(c_render_device_t));
	ct_set_destroy(self, (destroy_cb)c_render_device_destroy);
	ct_add_dependency(self, ct_window);

	ct_add_listener(self, WORLD, 0, ref("world_update"), c_render_device_update);

	ct_add_listener(self, WORLD, 100, ref("world_draw"), c_render_device_draw);
}
