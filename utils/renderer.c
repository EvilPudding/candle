#include "renderer.h"
#include <components/camera.h>
#include <components/model.h>
#include <components/spacial.h>
#include <components/probe.h>
#include <components/light.h>
#include <components/ambient.h>
#include <components/node.h>
#include <components/name.h>
#include <systems/window.h>
#include <systems/render_device.h>
/* #include <systems/editmode.h> */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <candle.h>
#include <utils/noise.h>
#include <utils/nk.h>
#include <utils/material.h>

static texture_t *renderer_draw_pass(renderer_t *self, pass_t *pass);

static int renderer_update_screen_texture(renderer_t *self);

static int renderer_pass(renderer_t *self, unsigned int hash);

static void bind_pass(pass_t *pass, shader_t *shader);

static int pass_bind_buffer(pass_t *pass, bind_t *bind, shader_t *shader)
{
	shader_bind_t *sb = &bind->vs_uniforms[shader->index];

	texture_t *buffer = bind->buffer;

	int t;

	for(t = 0; t < buffer->bufs_size; t++)
	{
		if(((int)sb->buffer.u_tex[t]) != -1)
		{
			if(!buffer->bufs[t].id)
			{
				printf("texture not ready yet??\n");
				exit(1);
			}
			glUniformHandleui64ARB(sb->buffer.u_tex[t], buffer->bufs[t].handle); glerr();
			/* glUniform1i(sb->buffer.u_tex[t], buffer->bufs[t].id); glerr(); */
		}
	}
	return 1;
}

void bind_get_uniforms(bind_t *bind,
		shader_bind_t *sb, shader_t *shader)
{
	int t;
	switch(bind->type)
	{
		case BIND_NONE:
			printf("Empty bind??\n");
			break;
		case BIND_TEX:
			for(t = 0; t < bind->buffer->bufs_size; t++)
			{
				sb->buffer.u_tex[t] = shader_uniform(shader, bind->name,
						bind->buffer->bufs[t].name);
			}
			break;
		default:
			sb->number.u = shader_uniform(shader, bind->name, NULL);
			if(sb->number.u >= 4294967295) { }
			break;
	}
	sb->cached = 1;

}

static void bind_pass(pass_t *pass, shader_t *shader)
{
	int i;

	if(!pass) return;

	/* if(self->shader->frame_bind == self->frame) return; */
	/* self->shader->frame_bind = self->frame; */

	glUniform2f(22, pass->output->width, pass->output->height);

	glerr();

	for(i = 0; i < pass->binds_size; i++)
	{
		bind_t *bind = &pass->binds[i];
		shader_bind_t *sb = &bind->vs_uniforms[shader->index];
		if(!sb->cached)
		{
			bind_get_uniforms(bind, sb, shader);
		}

		switch(bind->type)
		{
		case BIND_NONE: printf("error\n"); break;
		case BIND_TEX:
			if(!pass_bind_buffer(pass, bind, shader)) return;
			break;
		case BIND_NUM:
			if(bind->getter)
			{
				bind->number = ((number_getter)bind->getter)(bind->usrptr);
			}
			glUniform1f(sb->number.u, bind->number); glerr();
			glerr();
			break;
		case BIND_INT:
			if(bind->getter)
			{
				bind->integer = ((integer_getter)bind->getter)(bind->usrptr);
			}
			glUniform1i(sb->integer.u, bind->integer); glerr();
			glerr();
			break;
		case BIND_VEC2:
			if(bind->getter)
			{
				bind->vec2 = ((vec2_getter)bind->getter)(bind->usrptr);
			}
			glUniform2f(sb->vec2.u, bind->vec2.x, bind->vec2.y);
			glerr();
			break;
		case BIND_VEC3:
			if(bind->getter)
			{
				bind->vec3 = ((vec3_getter)bind->getter)(bind->usrptr);
			}
			glUniform3f(sb->vec3.u, bind->vec3.r, bind->vec3.g, bind->vec3.b);
			glerr();
			break;
		}

	}

	/* ct_t *ambients = ecm_get(ct_ambient); */
	/* c_ambient_t *ambient = (c_ambient_t*)ct_get_at(ambients, 0, 0); */
	/* if(ambient) */
	/* { */
	/* 	c_probe_t *probe = c_probe(ambient); */
	/* 	if(probe) shader_bind_ambient(shader, probe->map); */
	/* } */
}

void renderer_set_output(renderer_t *self, unsigned int hash)
{
}


texture_t *renderer_tex(renderer_t *self, unsigned int hash)
{
	int i;
	if(!hash) return NULL;
	for(i = 0; i < self->outputs_num; i++)
	{
		pass_output_t *output = &self->outputs[i];
		if(output->hash == hash) return output->buffer;
	}
	return NULL;
}


void renderer_add_tex(renderer_t *self, const char *name,
		float resolution, texture_t *buffer)
{
	self->outputs[self->outputs_num++] = (pass_output_t){
		.resolution = resolution,
		.hash = ref(name), .buffer = buffer};
	strncpy(buffer->name, name, sizeof(buffer->name));
}

static int renderer_gl(renderer_t *self)
{
	int W = self->width * self->resolution;
	int H = self->height * self->resolution;

	if(!W || !H) return 0;

	self->fallback_depth =	texture_new_2D(W, H, 0, 0,
		buffer_new("depth",	1, -1)
	);
	renderer_add_tex(self, "fallback_depth", 1.0f, self->fallback_depth);


	texture_t *gbuffer =	texture_new_2D(W, H, 0, 0,
		buffer_new("nmr",		1, 4),
		buffer_new("albedo",	1, 4),
		buffer_new("depth",		1, -1)
	);

	texture_t *ssao =		texture_new_2D(W, H, 0,
		buffer_new("occlusion",	1, 1)
	);
	texture_t *rendered =	texture_new_2D(W, H, 0,
		buffer_new("color",	1, 4)
	);
	texture_t *refr =		texture_new_2D(W, H, TEX_MIPMAP,
		buffer_new("color",	1, 4)
	);
	texture_t *final =		texture_new_2D(W, H, TEX_INTERPOLATE,
		buffer_new("color",	1, 4)
	);
	final->track_brightness = 1;
	/* texture_t *bloom =		texture_new_2D(W, H, TEX_INTERPOLATE, */
		/* buffer_new("color",	1, 4) */
	/* ); */
	/* texture_t *bloom2 =		texture_new_2D(W, H, TEX_INTERPOLATE, */
		/* buffer_new("color",	1, 4) */
	/* ); */
	texture_t *tmp =		texture_new_2D(W, H, TEX_INTERPOLATE,
		buffer_new("color",	1, 4)
	);
	texture_t *selectable =	texture_new_2D(W, H, 0,
		buffer_new("geomid",	1, 2),
		buffer_new("id",		1, 2),
		buffer_new("depth",		1, -1)
	);

	renderer_add_tex(self, "gbuffer",		1.0f, gbuffer);
	renderer_add_tex(self, "ssao",		1.0f, ssao);
	renderer_add_tex(self, "rendered",	1.0f, rendered);
	renderer_add_tex(self, "refr",		1.0f, refr);
	renderer_add_tex(self, "final",		1.0f, final);
	renderer_add_tex(self, "selectable",	1.0f, selectable);
	renderer_add_tex(self, "tmp",			1.0f, tmp);
	/* renderer_add_tex(self, "bloom",		0.3f, bloom); */
	/* renderer_add_tex(self, "bloom2",		0.3f, bloom2); */

	renderer_add_pass(self, "gbuffer", "gbuffer", ref("visible"),
			PASS_CLEAR_DEPTH | PASS_CLEAR_COLOR, gbuffer, gbuffer,
		(bind_t[]){ {BIND_NONE} }
	);

	renderer_add_pass(self, "selectable", "select", ref("selectable"),
			PASS_CLEAR_DEPTH | PASS_CLEAR_COLOR, selectable, selectable,
		(bind_t[]){ {BIND_NONE} }
	);

	/* DECAL PASS */
	renderer_add_pass(self, "decals_pass", "decals", ref("decals"),
			PASS_DEPTH_LOCK | PASS_DEPTH_EQUAL | PASS_DEPTH_GREATER,
			gbuffer, gbuffer,
		(bind_t[]){
			{BIND_TEX, "gbuffer", .buffer = gbuffer},
			{BIND_NONE}
		}
	);

	renderer_add_pass(self, "ssao_pass", "ssao", ref("quad"), 0,
			renderer_tex(self, ref("ssao")), NULL,
		(bind_t[]){
			{BIND_TEX, "gbuffer", .buffer = gbuffer},
			{BIND_NONE}
		}
	);


	renderer_add_pass(self, "ambient_light_pass", "phong", ref("ambient"),
			PASS_ADDITIVE | PASS_CLEAR_COLOR , rendered, NULL,
		(bind_t[]){
			{BIND_TEX, "gbuffer", .buffer = gbuffer},
			{BIND_NONE}
		}
	);

	renderer_add_pass(self, "render_pass", "phong", ref("light"),
			PASS_DEPTH_LOCK | PASS_ADDITIVE | 
			PASS_DEPTH_EQUAL | PASS_DEPTH_GREATER, rendered, gbuffer,
		(bind_t[]){
			{BIND_TEX, "gbuffer", .buffer = gbuffer},
			{BIND_NONE}
		}
	);


	renderer_add_pass(self, "refraction", "copy", ref("quad"), 0,
			refr, NULL,
		(bind_t[]){
			{BIND_TEX, "buf", .buffer = rendered},
			{BIND_NONE}
		}
	);

	renderer_add_pass(self, "transp", "transparency", ref("transparent"),
			PASS_DEPTH_EQUAL, rendered, gbuffer,
		(bind_t[]){
			{BIND_TEX, "refr", .buffer = refr},
			{BIND_NONE}
		}
	);

	/* renderer_tex(self, ref(rendered))->mipmaped = 1; */
	renderer_add_pass(self, "final", "ssr", ref("quad"), PASS_CLEAR_COLOR,
			final, NULL,
		(bind_t[]){
			{BIND_TEX, "gbuffer", .buffer = gbuffer},
			{BIND_TEX, "rendered", .buffer = rendered},
			{BIND_TEX, "ssao", .buffer = ssao},
			{BIND_NONE}
		}
	);

	/* renderer_add_pass(self, "bloom_%d", "bright", ref("quad"), 0, */
	/* 		renderer_tex(self, ref("bloom")), NULL, */
	/* 	(bind_t[]){ */
	/* 		{BIND_TEX, "buf", .buffer = renderer_tex(self, ref("final"))}, */
	/* 		{BIND_NONE} */
	/* 	} */
	/* ); */
	/* int i; */
	/* for(i = 0; i < 2; i++) */
	/* { */
	/* 	renderer_add_pass(self, "bloom_%d", "blur", ref("quad"), 0, */
	/* 			renderer_tex(self, ref("bloom2")), NULL */
	/* 		(bind_t[]){ */
	/* 			{BIND_TEX, "buf", .buffer = renderer_tex(self, ref("bloom"))}, */
	/* 			{BIND_INT, "horizontal", .integer = 1}, */
	/* 			{BIND_NONE} */
	/* 		} */
	/* 	); */
	/* 	renderer_add_pass(self, "bloom_%d", "blur", ref("quad"), 0, */
	/* 			renderer_tex(self, ref("bloom")), NULL, */
	/* 		(bind_t[]){ */
	/* 			{BIND_TEX, "buf", .buffer = renderer_tex(self, ref("bloom2"))}, */
	/* 			{BIND_INT, "horizontal", .integer = 0}, */
	/* 			{BIND_NONE} */
	/* 		} */
	/* 	); */
	/* } */
	/* renderer_add_pass(self, "bloom_%d", "copy", ref("quad"), PASS_ADDITIVE, */
	/* 		renderer_tex(self, ref("final")), NULL, */
	/* 	(bind_t[]){ */
	/* 		{BIND_TEX, "buf", .buffer = renderer_tex(self, ref("bloom"))}, */
	/* 		{BIND_NONE} */
	/* 	} */
	/* ); */

	/* renderer_tex(self, ref(rendered))->mipmaped = 1; */

	self->output = renderer_tex(self, ref("final"));

	glerr();
	return 1;
}

static int renderer_update_screen_texture(renderer_t *self)
{
	int w = self->width * self->resolution;
	int h = self->height * self->resolution;

	int i;
	for(i = 0; i < self->outputs_num; i++)
	{
		pass_output_t *output = &self->outputs[i];

		if(output->resolution)
		{
			int W = w * output->resolution;
			int H = h * output->resolution;

			texture_2D_resize(output->buffer, W, H);
		}
	}

	self->ready = 1;
	return 1;
}

int renderer_resize(renderer_t *self, int width, int height)
{
    self->width = width;
    self->height = height;

	if(!self->output)
	{
		loader_push_wait(g_candle->loader, (loader_cb)renderer_gl, self, NULL);
	}
	renderer_update_screen_texture(self);
	return CONTINUE;
}


static texture_t *renderer_draw_pass(renderer_t *self, pass_t *pass)
{
	c_render_device_rebind(c_render_device(&SYS), (void*)bind_pass, pass);
	if(!pass->active) return NULL;

	if(!pass->shader) /* Maybe drawing without OpenGL */
	{
		/* CALL DRAW */
		entity_signal(c_entity(self), pass->draw_signal, NULL, NULL);
		return pass->output;
	}

	if(!texture_target(pass->output,
				pass->depth ? pass->depth : self->fallback_depth, 0))
	{
		printf("could not target\n");
		return NULL;
	}

	fs_bind(pass->shader);
	glerr();

	if(pass->additive)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	}
	if(pass->multiply)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_DST_COLOR, GL_ZERO);
	}
	glEnable(GL_CULL_FACE); glerr();

	glDepthMask(pass->depth_update); glerr();

	if(pass->clear)
	{
		glClear(pass->clear); glerr();
	}
	if(pass->depth)
	{
		glDepthFunc(pass->depth_func);
		glEnable(GL_DEPTH_TEST);
	}

	/* CALL DRAW */
	/* entity_signal(c_entity(self), pass->draw_signal, NULL, NULL); */
	draw_group(pass->draw_signal);

	glDisable(GL_DEPTH_TEST);

	glDisable(GL_BLEND);

	if(pass->output->mipmaped)
	{
		texture_bind(pass->output, 0);
		glGenerateMipmap(pass->output->target); glerr();
		if(pass->output->track_brightness && self->frame % 30 == 0)
		{
			texture_update_brightness(pass->output);
		}
	}

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); glerr();

	return pass->output;
}

void renderer_set_resolution(renderer_t *self, float resolution)
{
	self->resolution = resolution;
	renderer_update_screen_texture(self);
}

/* int init_perlin(renderer_t *self) */
/* { */
	/* int texes = 8; */
	/* int m = self->perlin_size * texes; */
	/* self->perlin = texture_new_3D(m, m, m, 4); */
	/* loader_wait(g_candle->loader); */

	/* int x, y, z; */

	/* for(z = 0; z < m; z++) */
	/* { */
	/* 	for(y = 0; y < m; y++) for(x = 0; x < m; x++) */
	/* 	{ */
	/* 		float n = (cnoise(vec3(((float)x) / 13, ((float)y) / 13, ((float)z) */
	/* 						/ 13)) + 1) / 2; */
	/* 		n += (cnoise(vec3((float)x / 2, (float)y / 2, (float)z / 2))) / 8; */
	/* 		n = n * 1.75; */

	/* 		float_clamp(&n, 0.0, 1.0); */
	/* 		texture_set_xyz(self->perlin, x, y, z, (int)round(n * 255), 0, 0, */
	/* 				255); */
	/* 	} */
	/* } */
	/* texture_update_gl(self->perlin); */
	/* printf("perlin end\n"); */
	/* return 1; */
/* } */

renderer_t *renderer_new(float resolution, int auto_exposure,
		int roughness, int lock_fps)
{
	SDL_GL_SetSwapInterval(lock_fps);

	renderer_t *self = calloc(1, sizeof(*self));

	self->resolution = resolution;
	self->auto_exposure = auto_exposure;
	self->roughness = roughness;

	return self;
}

unsigned int renderer_geom_at_pixel(renderer_t *self, int x, int y,
		float *depth)
{
	entity_t result;
	texture_t *tex = renderer_tex(self, ref("selectable"));
	if(!tex) return entity_null;

	unsigned int res = texture_get_pixel(tex, 1,
			x * self->resolution, y * self->resolution, depth) & 0xFFFF;
	result = res - 1;
	return result;
}

entity_t renderer_entity_at_pixel(renderer_t *self, int x, int y,
		float *depth)
{
	entity_t result;
	texture_t *tex = renderer_tex(self, ref("selectable"));
	if(!tex) return entity_null;

	unsigned int pos = texture_get_pixel(tex, 0,
			x * self->resolution, y * self->resolution, depth);
	struct { unsigned int pos, uid; } *cast = (void*)&result;

	cast->pos = pos;
	cast->uid = g_ecm->entities_info[pos].uid;
	return result;
}


/* int renderer_component_menu(renderer_t *self, void *ctx) */
/* { */
/* 	nk_layout_row_dynamic(ctx, 0, 1); */
/* 	int i; */
/* 	if(nk_button_label(ctx, "Fullscreen")) */
/* 	{ */
/* 		c_window_toggle_fullscreen(c_window(self)); */
/* 	} */

/* 	char fps[12]; sprintf(fps, "%d", g_candle->fps); */
/* 	nk_layout_row_begin(ctx, NK_DYNAMIC, 30, 2); */
/* 		nk_layout_row_push(ctx, 0.35); */
/* 		nk_label(ctx, "FPS: ", NK_TEXT_LEFT); */
/* 		nk_layout_row_push(ctx, 0.65); */
/* 		nk_label(ctx, fps, NK_TEXT_RIGHT); */
/* 	nk_layout_row_end(ctx); */
/* 	nk_layout_row_dynamic(ctx, 0, 1); */

/* 	for(i = 1; i < self->outputs_num; i++) */
/* 	{ */
/* 		pass_output_t *output = &self->outputs[i]; */
/* 		if(output->buffer) */
/* 		{ */
/* 			if (nk_button_label(ctx, output->buffer->name)) */
/* 			{ */
/* 				c_editmode_open_texture(c_editmode(self), output->buffer); */
/* 			} */
/* 		} */
/* 	} */
/* 	return CONTINUE; */
/* } */



/* REG() */
/* { */
	/* ct_t *ct = ct_new("renderer", sizeof(renderer_t), */
			/* NULL, NULL, 0); */

	/* ct_listener(ct, WORLD, ref("component_menu"), renderer_component_menu); */
/* } */



static int renderer_pass(renderer_t *self, unsigned int hash)
{
	int i;
	if(!hash) return -1;
	for(i = 0; i < self->passes_size; i++)
	{
		pass_t *pass = &self->passes[i];
		if(pass->hash == hash) return i;
	}
	return -1;
}

void renderer_toggle_pass(renderer_t *self, uint hash, int active)
{
	int i = renderer_pass(self, hash);
	if(i >= 0)
	{
		self->passes[i].active = active;
	}
}

void renderer_add_pass(renderer_t *self, const char *name,
		const char *shader_name, uint32_t draw_signal,
		int flags, texture_t *output, texture_t *depth, bind_t binds[])
{
	char buffer[32];
	snprintf(buffer, sizeof(buffer), name, self->passes_size);
	unsigned int hash = ref(buffer);
	int i = renderer_pass(self, hash);
	if(i == -1)
	{
		i = self->passes_size++;
	}
	else
	{
		printf("Replacing\n");
	}

	pass_t *pass = &self->passes[i];
	pass->shader = fs_new(shader_name);
	pass->hash = hash;

	pass->clear = 0;
	if(flags & PASS_CLEAR_COLOR) pass->clear |= GL_COLOR_BUFFER_BIT;
	if(flags & PASS_CLEAR_DEPTH) pass->clear |= GL_DEPTH_BUFFER_BIT;

	pass->depth_update =	!(flags & PASS_DEPTH_LOCK);
	pass->depth = NULL;

	pass->output = output;
	pass->depth = depth;

	if(!(flags & PASS_DEPTH_EQUAL))
	{
		if(flags & PASS_DEPTH_GREATER)
		{
			pass->depth_func = GL_GREATER;
		}
		else
		{
			pass->depth_func = GL_LESS;
		}
	}
	else
	{
		if(flags & PASS_DEPTH_GREATER)
		{
			pass->depth_func = GL_GEQUAL;
		}
		else
		{
			pass->depth_func = GL_LEQUAL;
		}
	}

	pass->draw_signal = draw_signal;
	pass->additive = flags & PASS_ADDITIVE;
	pass->multiply = flags & PASS_MULTIPLY;
	strncpy(pass->name, buffer, sizeof(pass->name));

	for(pass->binds_size = 0; binds[pass->binds_size].type != BIND_NONE;
			pass->binds_size++);

	pass->binds = malloc(sizeof(bind_t) * pass->binds_size);
	int j;
	for(i = 0; i < pass->binds_size; i++)
	{
		pass->binds[i] = binds[i];
		for(j = 0; j < 16; j++)
		{
			shader_bind_t *sb = &pass->binds[i].vs_uniforms[j];

			sb->cached = 0;
			int t;
			for(t = 0; t < 16; t++)
			{
				sb->buffer.u_tex[t] = -1;
			}
			pass->binds[i].hash = ref(pass->binds[i].name);
		}
	}
	self->ready = 0;
	pass->active = 1;
}

void renderer_clear_shaders(renderer_t *self, shader_t *shader)
{
	int i;
	for(i = 0; i < self->passes_size; i++)
	{
		/* shader_destroy(self->passes[i].shader); */
	}
	self->passes_size = 0;
}

void renderer_update_ubo(renderer_t *self)
{
	if(!self->ubo)
	{
		glGenBuffers(1, &self->ubo); glerr();
		glBindBuffer(GL_UNIFORM_BUFFER, self->ubo); glerr();
		glBufferData(GL_UNIFORM_BUFFER, sizeof(self->glvars), &self->glvars,
				GL_DYNAMIC_DRAW); glerr();
		glBindBufferBase(GL_UNIFORM_BUFFER, 19, self->ubo); glerr();
	}
	/* else if(self->scene_changes) */
	else
	{
		glBindBuffer(GL_UNIFORM_BUFFER, self->ubo); glerr();
		GLvoid* p = glMapBuffer(GL_UNIFORM_BUFFER, GL_WRITE_ONLY); glerr();
		memcpy(p, &self->glvars, sizeof(self->glvars));
		glUnmapBuffer(GL_UNIFORM_BUFFER); glerr();
	}
}

int renderer_draw(renderer_t *self)
{
	glerr();
	uint i;

	if(!self->width || !self->height) return CONTINUE;
	/* if(!self->output) renderer_gl(self); */

	self->frame++;

	renderer_update_ubo(self);

	if(!self->ready)
	{
		renderer_update_screen_texture(self);
	}

	/* glBindBuffer(GL_UNIFORM_BUFFER, renderer->ubo); */

	glerr();

	for(i = 0; i < self->passes_size; i++)
	{
		renderer_draw_pass(self, &self->passes[i]);
	}

	c_render_device_rebind(c_render_device(&SYS), NULL, NULL);

	glerr();
	return CONTINUE;
}

