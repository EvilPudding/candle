#include "render_device.h"
#include <components/camera.h>
#include <components/model.h>
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
		gllight->pos = light->tile.pos;
		gllight->lod = light->lod;
		gllight->radius = light->radius;
	}
}

int c_render_device_update(c_render_device_t *self)
{
	if(self->updates_ram)
	{
		c_render_device_update_lights(self);
		self->scene.test_color = vec3(0, 1, 0);
		self->scene.time = ((float)self->frame) * 0.01f;
		SDL_AtomicSet((SDL_atomic_t*)&self->updates_ubo, 1);
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

		SDL_AtomicSet((SDL_atomic_t*)&self->updates_ubo, 0);
	}
}

void fs_bind(fs_t *fs)
{
	c_render_device_t *render_device = c_render_device(&SYS);
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
		fs_t *fs = rd->frag_bound;
		if (!fs) return NULL;

		if (fs_variation >= fs->variations_num)
		{
			fs_variation = 0;
		}
		shader_t **sh = &fs->variations[fs_variation].combinations[vs->index];
		if (!*sh) *sh = shader_new(fs, fs_variation, vs);
		rd->shader = *sh; 
	}

	shader_bind(rd->shader);

	loc = shader_cached_uniform(rd->shader, ref("g_cache"));
	glUniform1i(loc, 3);
	glActiveTexture(GL_TEXTURE0 + 3);
	texture_bind(g_cache, 0);

	loc = shader_cached_uniform(rd->shader, ref("g_indir"));
	glUniform1i(loc, 4);
	glActiveTexture(GL_TEXTURE0 + 4);
	texture_bind(g_indir, 0);

	loc = shader_cached_uniform(rd->shader, ref("g_probes"));
	glUniform1i(loc, 5);
	glActiveTexture(GL_TEXTURE0 + 5);
	texture_bind(g_probe_cache, 1);
	glerr();


	/* GLint size; */
	if (rd->bound_ubos[20])
	{
		loc = glGetUniformBlockIndex(rd->shader->program, "scene_t");
		glUniformBlockBinding(rd->shader->program, loc, 20); glerr();
		/* glGetActiveUniformBlockiv(rd->shader->program, loc, GL_UNIFORM_BLOCK_DATA_SIZE, &size); */
		glBindBufferBase(GL_UNIFORM_BUFFER, 20, rd->bound_ubos[20]); glerr();
	}

	if (rd->bound_ubos[19])
	{
		loc = glGetUniformBlockIndex(rd->shader->program, "renderer_t");
		glUniformBlockBinding(rd->shader->program, loc, 19); glerr();
		/* glGetActiveUniformBlockiv(rd->shader->program, loc, GL_UNIFORM_BLOCK_DATA_SIZE, &size); */
		glBindBufferBase(GL_UNIFORM_BUFFER, 19, rd->bound_ubos[19]); glerr();
	}

	if (rd->bound_ubos[21] && rd->shader->has_skin)
	{
		loc = glGetUniformBlockIndex(rd->shader->program, "skin_t");
		glUniformBlockBinding(rd->shader->program, loc, 21); glerr();
		glBindBufferBase(GL_UNIFORM_BUFFER, 21, rd->bound_ubos[21]); glerr();
	}

	if (rd->bound_ubos[22])
	{
		loc = glGetUniformBlockIndex(rd->shader->program, "cms_t"); glerr();
		if (loc != ~0)
		{
			glUniformBlockBinding(rd->shader->program, loc, 22); glerr();
			glBindBufferBase(GL_UNIFORM_BUFFER, 22, rd->bound_ubos[22]); glerr();
		}
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
	SDL_AtomicSet((SDL_atomic_t*)&self->updates_ram, 1);
	svp_init();

	return self;
}

void world_changed()
{
	c_render_device_t *rd = c_render_device(&SYS);
	if(rd) SDL_AtomicSet((SDL_atomic_t*)&rd->updates_ram, 1);
}

int c_render_device_draw(c_render_device_t *self)
{
	if(self->updates_ubo)
	{
		c_render_device_update_ubo(self);
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

void c_render_device_destroy(c_render_device_t *self)
{
	svp_destroy();
}

REG()
{
	ct_t *ct = ct_new("render_device", sizeof(c_render_device_t),
			NULL, c_render_device_destroy, 1, ref("window"));

	ct_listener(ct, WORLD, ref("world_update"), c_render_device_update);

	ct_listener(ct, WORLD | 100, ref("world_draw"), c_render_device_draw);

	ct_listener(ct, ENTITY, ref("entity_created"), c_render_device_created);

	signal_init(sig("world_pre_draw"), sizeof(void*));
}
