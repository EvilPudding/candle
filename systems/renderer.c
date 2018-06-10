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
#include <systems/editmode.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <candle.h>
#include <utils/noise.h>
#include <utils/nk.h>

static void c_renderer_update_probes(c_renderer_t *self);

static texture_t *c_renderer_draw_pass(c_renderer_t *self, pass_t *pass);

static int c_renderer_resize(c_renderer_t *self, window_resize_data *event);

static int c_renderer_update_screen_texture(c_renderer_t *self);

int c_renderer_scene_changed(c_renderer_t *self)
{
	g_update_id++;
	return CONTINUE;
}

static int c_renderer_bind_buffer(c_renderer_t *self, pass_t *pass,
		bind_t *bind)
{

	shader_bind_t *sb = &bind->vs_uniforms[self->shader->index];

	texture_t *buffer = bind->buffer;

	glUniform2f(self->shader->u_screen_size, buffer->width, buffer->height);
	glerr();
	float brightness = buffer->brightness;
	glUniform1f(sb->buffer.u_brightness, brightness); glerr();

	int t;
	int (*i) = &self->shader->bound_textures;

	for(t = 0; t < buffer->bufs_size; t++)
	{
		if(((int)sb->buffer.u_tex[t]) != -1)
		{
			glUniform1i(sb->buffer.u_tex[t], (*i) + 64); glerr();
			glActiveTexture(GL_TEXTURE0 + (*i) + 64); glerr();
			texture_bind(buffer, t);
			(*i)++;
		}
	}
	return 1;
}

static void c_renderer_bind_camera(c_renderer_t *self, pass_t *pass,
		bind_t *bind)
{
	shader_bind_t *sb = &bind->vs_uniforms[self->shader->index];
	if(bind->getter)
	{
		bind->camera = ((camera_getter)bind->getter)(bind->usrptr);
	}
	if(!bind->camera) return;

	c_camera_t *cam = c_camera(&bind->camera);
	c_camera_update_view(cam);

	self->shader->vp = cam->vp;

	glUniformMatrix4fv(sb->camera.u_model, 1, GL_FALSE,
			(void*)cam->model_matrix._);

	glUniformMatrix4fv(sb->camera.u_view, 1, GL_FALSE,
			(void*)cam->view_matrix._);
	self->bound_camera_pos = cam->pos;

	glUniform3f(sb->camera.u_pos, cam->pos.x, cam->pos.y, cam->pos.z);
	glUniform1f(sb->camera.u_exposure, cam->exposure);

	/* TODO unnecessary? */
	mat4_t inv_projection = mat4_invert(cam->projection_matrix);
	glUniformMatrix4fv(sb->camera.u_projection, 1, GL_FALSE,
			(void*)cam->projection_matrix._);
	glUniformMatrix4fv(sb->camera.u_inv_projection, 1, GL_FALSE,
			(void*)inv_projection._);
}

void c_renderer_bind_get_uniforms(c_renderer_t *self, bind_t *bind,
		shader_bind_t *sb)
{

	int t;
	switch(bind->type)
	{
		case BIND_NONE:
			printf("Empty bind??\n");
			break;
		case BIND_CAM:
			sb->camera.u_view =
				shader_uniform(self->shader, bind->name, "view");
			sb->camera.u_pos =
				shader_uniform(self->shader, bind->name, "pos");
			sb->camera.u_exposure =
				shader_uniform(self->shader, bind->name, "exposure");
			sb->camera.u_model =
				shader_uniform(self->shader, bind->name, "model");
			sb->camera.u_projection =
				shader_uniform(self->shader, bind->name, "projection");
			sb->camera.u_inv_projection =
				shader_uniform(self->shader, bind->name, "inv_projection");

			break;
		case BIND_TEX:
			for(t = 0; t < bind->buffer->bufs_size; t++)
			{
				sb->buffer.u_tex[t] = shader_uniform(self->shader, bind->name,
						bind->buffer->bufs[t].name);
			}
			sb->buffer.u_brightness = shader_uniform(self->shader,
					bind->name, "brightness"); glerr();
			break;
		default:
			sb->number.u = shader_uniform(self->shader, bind->name, NULL);
			if(sb->number.u >= 4294967295) { }
			break;
	}
	sb->cached = 1;

}

int c_renderer_bind_pass(c_renderer_t *self, pass_t *pass)
{
	int i;

	if(!self->shader || !self->shader->ready) return 0;

	if(self->bound_probe)
	{
		shader_bind_probe(self->shader, self->bound_probe);
		shader_bind_camera(self->shader, self->bound_camera_pos,
				self->bound_view, self->bound_projection,
				self->bound_camera_model, self->bound_exposure);
	}
	if(self->bound_light)
	{
		shader_bind_light(self->shader, self->bound_light);
	}

	if(!pass) return 0;

	glUniform2f(self->shader->u_screen_size,
			pass->output->width,
			pass->output->height); glerr();

	if(self->shader->frame_bind == self->frame) return 0;
	self->shader->frame_bind = self->frame;

	for(i = 0; i < pass->binds_size; i++)
	{
		bind_t *bind = &pass->binds[i];
		shader_bind_t *sb = &bind->vs_uniforms[self->shader->index];
		if(!sb->cached)
		{
			c_renderer_bind_get_uniforms(self, bind, sb);
		}

		switch(bind->type)
		{
		case BIND_NONE: printf("error\n"); break;
		case BIND_TEX:
			if(!c_renderer_bind_buffer(self, pass, bind)) return 0;
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
		case BIND_CAM: c_renderer_bind_camera(self, pass, bind); break;
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
		case BIND_OUT: break;
		case BIND_DEPTH: break;
		}

	}

	/* ct_t *ambients = ecm_get(ct_ambient); */
	/* c_ambient_t *ambient = (c_ambient_t*)ct_get_at(ambients, 0, 0); */
	/* if(ambient) */
	/* { */
	/* 	c_probe_t *probe = c_probe(ambient); */
	/* 	if(probe) shader_bind_ambient(self->shader, probe->map); */
	/* } */
	return 1;
}

void fs_bind(fs_t *fs)
{
	c_renderer_t *renderer = c_renderer(&SYS);
	if(renderer->frag_bound == fs) return;
	glerr();

	renderer->frag_bound = fs;
	renderer->shader = NULL;
	/* printf("fs_bind %s\n", fs->filename); */
}

shader_t *vs_bind(vs_t *vs)
{
	c_renderer_t *renderer = c_renderer(&SYS);

	if(renderer->shader && renderer->shader->index == vs->index)
	{
		shader_bind(renderer->shader);
		c_renderer_bind_pass(renderer, renderer->current_pass);
		return renderer->shader;
	}
	/* printf("vs_bind %s\n", vs->name); */
	glerr();

	fs_t *fs = renderer->frag_bound;

	shader_t **sh = &fs->combinations[vs->index];
	if(!*sh) *sh = shader_new(fs, vs);
	if(!(*sh)->ready) return NULL;

	renderer->shader = *sh; 

	shader_bind(renderer->shader);

	c_renderer_bind_pass(renderer, renderer->current_pass);
	return *sh;
}

entity_t c_renderer_get_camera(c_renderer_t *self)
{
	return self->camera;
}

void c_renderer_set_output(c_renderer_t *self, unsigned int hash)
{
	self->output = c_renderer_tex(self, hash);

	texture_bind(self->output, -1);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
}


texture_t *c_renderer_tex(c_renderer_t *self, unsigned int hash)
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


void c_renderer_add_tex(c_renderer_t *self, const char *name,
		float resolution, texture_t *buffer)
{
	self->outputs[self->outputs_num++] = (pass_output_t){
		.resolution = resolution,
		.hash = ref(name), .buffer = buffer};
	strncpy(buffer->name, name, sizeof(buffer->name));
}

static int c_renderer_gl(c_renderer_t *self)
{
	self->fallback_depth =	texture_new_2D(1, 1, 0, 0,
		buffer_new("depth",	1, -1)
	);
	c_renderer_add_tex(self, "fallback_depth", 1.0f, self->fallback_depth);


	texture_t *gbuffer =	texture_new_2D(1, 1, 0, 0,
		buffer_new("normal",	1, 2),
		buffer_new("specular",	1, 4),
		buffer_new("diffuse",	1, 4),
		buffer_new("depth",		1, -1)
	);
	texture_t *ssao =		texture_new_2D(1, 1, 0,
		buffer_new("occlusion",	1, 4)
	);
	texture_t *rendered =	texture_new_2D(1, 1, 0,
		buffer_new("color",	1, 4)
	);
	texture_t *refr =		texture_new_2D(1, 1, TEX_MIPMAP,
		buffer_new("color",	1, 4)
	);
	texture_t *final =		texture_new_2D(1, 1, TEX_INTERPOLATE,
		buffer_new("color",	1, 4)
	);
	texture_t *bloom =		texture_new_2D(1, 1, TEX_INTERPOLATE,
		buffer_new("color",	1, 4)
	);
	texture_t *bloom2 =		texture_new_2D(1, 1, TEX_INTERPOLATE,
		buffer_new("color",	1, 4)
	);
	texture_t *selectable =	texture_new_2D(1, 1, 0,
		buffer_new("geomid",	1, 2),
		buffer_new("id",		1, 2),
		buffer_new("depth",		1, -1)
	);

	c_renderer_add_tex(self, "gbuffer",		1.0f, gbuffer);
	c_renderer_add_tex(self, "ssao",		1.0f, ssao);
	c_renderer_add_tex(self, "rendered",	1.0f, rendered);
	c_renderer_add_tex(self, "refr",		1.0f, refr);
	c_renderer_add_tex(self, "final",		1.0f, final);
	c_renderer_add_tex(self, "selectable",	1.0f, selectable);
	c_renderer_add_tex(self, "bloom",		0.3f, bloom);
	c_renderer_add_tex(self, "bloom2",		0.3f, bloom2);

	c_renderer_add_pass(self, "gbuffer", "gbuffer", sig("render_visible"),
			PASS_CLEAR_DEPTH | PASS_CLEAR_COLOR,
		(bind_t[]){
			{BIND_OUT, .buffer = c_renderer_tex(self, ref("gbuffer"))},
			{BIND_DEPTH, .buffer = c_renderer_tex(self, ref("gbuffer"))},
			{BIND_CAM, "camera", (getter_cb)c_renderer_get_camera, self},
			{BIND_NONE}
		}
	);

	c_renderer_add_pass(self, "selectable", "select", sig("render_selectable"),
			PASS_CLEAR_DEPTH | PASS_CLEAR_COLOR,
		(bind_t[]){
			{BIND_OUT, .buffer = c_renderer_tex(self, ref("selectable"))},
			{BIND_DEPTH, .buffer = c_renderer_tex(self, ref("selectable"))},
			{BIND_CAM, "camera", (getter_cb)c_renderer_get_camera, self},
			{BIND_NONE}
		}
	);

	/* DECAL PASS */
	c_renderer_add_pass(self, "decals_pass", "decals", sig("render_decals"),
			PASS_LOCK_DEPTH,
		(bind_t[]){
			{BIND_OUT, .buffer = c_renderer_tex(self, ref("gbuffer"))},
			{BIND_TEX, "gbuffer", .buffer = c_renderer_tex(self, ref("gbuffer"))},
			{BIND_CAM, "camera", (getter_cb)c_renderer_get_camera, self},
			{BIND_NONE}
		}
	);


	c_renderer_add_pass(self, "ssao_pass", "ssao", sig("render_quad"), 0,
		(bind_t[]){
			{BIND_OUT, .buffer = c_renderer_tex(self, ref("ssao"))},
			{BIND_TEX, "gbuffer", .buffer = c_renderer_tex(self, ref("gbuffer"))},
			{BIND_CAM, "camera", (getter_cb)c_renderer_get_camera, self},
			{BIND_NONE}
		}
	);

	c_renderer_add_pass(self, "render_pass", "phong", sig("render_lights"),
			PASS_ADDITIVE | PASS_CLEAR_COLOR,
		(bind_t[]){
			{BIND_OUT, .buffer = c_renderer_tex(self, ref("rendered"))},
			{BIND_TEX, "gbuffer", .buffer = c_renderer_tex(self, ref("gbuffer"))},
			{BIND_TEX, "ssao", .buffer = c_renderer_tex(self, ref("ssao"))},
			{BIND_CAM, "camera", (getter_cb)c_renderer_get_camera, self},
			{BIND_NONE}
		}
	);

	c_renderer_add_pass(self, "refraction", "copy", sig("render_quad"),
			0,
		(bind_t[]){
			{BIND_OUT, .buffer = c_renderer_tex(self, ref("refr"))},
			{BIND_TEX, "buf", .buffer = c_renderer_tex(self, ref("rendered"))},
			{BIND_NONE}
		}
	);

	/* c_renderer_add_pass(self, "gbuffer transp", "gbuffer", sig("render_transparent"), 0, */
	/* 	(bind_t[]){ */
	/* 		{BIND_OUT, .buffer = c_renderer_tex(self, ref("gbuffer"))}, */
	/* 		{BIND_DEPTH, .buffer = c_renderer_tex(self, ref("gbuffer"))}, */
	/* 		{BIND_CAM, "camera", (getter_cb)c_renderer_get_camera, self}, */
	/* 		{BIND_NONE} */
	/* 	} */
	/* ); */
	c_renderer_add_pass(self, "transp", "transparency", sig("render_transparent"),
			0,
		(bind_t[]){
			{BIND_OUT, .buffer = c_renderer_tex(self, ref("rendered"))},
			{BIND_DEPTH, .buffer = c_renderer_tex(self, ref("gbuffer"))},
			{BIND_TEX, "refr", .buffer = c_renderer_tex(self, ref("refr"))},
			{BIND_CAM, "camera", (getter_cb)c_renderer_get_camera, self},
			{BIND_NONE}
		}
	);


	c_renderer_add_pass(self, "emissive", "transparency", sig("render_emissive"),
			PASS_ADDITIVE,
		(bind_t[]){
			{BIND_OUT, .buffer = c_renderer_tex(self, ref("rendered"))},
			{BIND_TEX, "gbuffer", .buffer = c_renderer_tex(self, ref("gbuffer"))},
			{BIND_TEX, "refr", .buffer = c_renderer_tex(self, ref("refr"))},
			{BIND_CAM, "camera", (getter_cb)c_renderer_get_camera, self},
			{BIND_NONE}
		}
	);

	c_renderer_add_pass(self, "final", "ssr", sig("render_quad"),
			PASS_CLEAR_COLOR,
		(bind_t[]){
			{BIND_OUT, .buffer = c_renderer_tex(self, ref("final"))},
			{BIND_TEX, "gbuffer", .buffer = c_renderer_tex(self, ref("gbuffer"))},
			{BIND_TEX, "rendered", .buffer = c_renderer_tex(self, ref("rendered"))},
			{BIND_CAM, "camera", (getter_cb)c_renderer_get_camera, self},
			{BIND_NONE}
		}
	);

	/* c_renderer_add_pass(self, "bloom_%d", "bright", sig("render_quad"), 0, */
	/* 	(bind_t[]){ */
	/* 		{BIND_OUT, .buffer = c_renderer_tex(self, ref("bloom"))}, */
	/* 		{BIND_TEX, "buf", .buffer = c_renderer_tex(self, ref("final"))}, */
	/* 		{BIND_NONE} */
	/* 	} */
	/* ); */
	/* int i; */
	/* for(i = 0; i < 2; i++) */
	/* { */
	/* 	c_renderer_add_pass(self, "bloom_%d", "blur", sig("render_quad"), 0, */
	/* 		(bind_t[]){ */
	/* 			{BIND_OUT, .buffer = c_renderer_tex(self, ref("bloom2"))}, */
	/* 			{BIND_TEX, "buf", .buffer = c_renderer_tex(self, ref("bloom"))}, */
	/* 			{BIND_INT, "horizontal", .integer = 1}, */
	/* 			{BIND_NONE} */
	/* 		} */
	/* 	); */
	/* 	c_renderer_add_pass(self, "bloom_%d", "blur", sig("render_quad"), 0, */
	/* 		(bind_t[]){ */
	/* 			{BIND_OUT, .buffer = c_renderer_tex(self, ref("bloom"))}, */
	/* 			{BIND_TEX, "buf", .buffer = c_renderer_tex(self, ref("bloom2"))}, */
	/* 			{BIND_INT, "horizontal", .integer = 0}, */
	/* 			{BIND_NONE} */
	/* 		} */
	/* 	); */
	/* } */
	/* c_renderer_add_pass(self, "bloom_%d", "copy", sig("render_quad"), */
	/* 		PASS_ADDITIVE, */
	/* 	(bind_t[]){ */
	/* 		{BIND_OUT, .buffer = c_renderer_tex(self, ref("final"))}, */
	/* 		{BIND_TEX, "buf", .buffer = c_renderer_tex(self, ref("bloom"))}, */
	/* 		{BIND_NONE} */
	/* 	} */
	/* ); */

	c_renderer_set_output(self, ref("final"));

	return 1;
}

static int c_renderer_created(c_renderer_t *self)
{

	loader_push(g_candle->loader, (loader_cb)c_renderer_gl, NULL,
			(c_t*)self);
	return CONTINUE;
}

static int c_renderer_update_screen_texture(c_renderer_t *self)
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

static int c_renderer_resize(c_renderer_t *self, window_resize_data *event)
{
    self->width = event->width;
    self->height = event->height;

	c_renderer_update_screen_texture(self);

	return CONTINUE;
}


static texture_t *c_renderer_draw_pass(c_renderer_t *self, pass_t *pass)
{
	self->frame++;

	if(!pass->output) exit(1);

	if(!pass->shader) /* Maybe drawing without OpenGL */
	{
		/* CALL DRAW */
		entity_signal(c_entity(self), pass->draw_signal, NULL);
		return pass->output;
	}
	else if(!pass->shader->ready)
	{
		printf("pass not ready\n");
		return NULL;
	}


	if(!texture_target(pass->output,
				pass->depth ? pass->depth : self->fallback_depth, 0))
	{
		printf("could not target\n");
		return NULL;
	}

	self->current_pass = pass;

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
		glDepthFunc(pass->invert_depth ? GL_GREATER : GL_LESS);
		glEnable(GL_DEPTH_TEST);
	}

	self->bound_light = entity_null;

	/* CALL DRAW */
	entity_signal(c_entity(self), pass->draw_signal, NULL);

	glDisable(GL_DEPTH_TEST);

	glDisable(GL_BLEND);

	if(pass->output->mipmaped)
	{
		texture_bind(pass->output, 0);
		glGenerateMipmap(pass->output->target); glerr();

		{
			/* texture_update_brightness(pass->output); */

			/* float brightness = pass->output->brightness; */
			/* if(!brightness) brightness = 1.0f; */
			/* float targetExposure = 0.3 + 0.4 / brightness; */

			/* c_camera_t *cam = c_camera(&self->camera); */
			/* cam->exposure += (targetExposure - cam->exposure) / 100; */
		}
	}
	self->current_pass = NULL;

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); glerr();

	return pass->output;
}

void c_renderer_set_resolution(c_renderer_t *self, float resolution)
{
	self->resolution = resolution;
	c_renderer_update_screen_texture(self);
}

int init_perlin(c_renderer_t *self)
{
	int texes = 8;
	int m = self->perlin_size * texes;
	self->perlin = texture_new_3D(m, m, m, 4);
	loader_wait(g_candle->loader);

	int x, y, z;

	for(z = 0; z < m; z++)
	{
		for(y = 0; y < m; y++) for(x = 0; x < m; x++)
		{
			float n = (cnoise(vec3(((float)x) / 13, ((float)y) / 13, ((float)z)
							/ 13)) + 1) / 2;
			n += (cnoise(vec3((float)x / 2, (float)y / 2, (float)z / 2))) / 8;
			n = n * 1.75;

			float_clamp(&n, 0.0, 1.0);
			texture_set_xyz(self->perlin, x, y, z, (int)round(n * 255), 0, 0,
					255);
		}
	}
	texture_update_gl(self->perlin);
	printf("perlin end\n");
	return 1;
}

c_renderer_t *c_renderer_new(float resolution, int auto_exposure,
		int roughness, int lock_fps)
{
	SDL_GL_SetSwapInterval(lock_fps);

	c_renderer_t *self = component_new("renderer");

	self->resolution = resolution;
	self->auto_exposure = auto_exposure;
	self->roughness = roughness;

	return self;
}

void c_renderer_add_camera(c_renderer_t *self, entity_t camera)
{
	self->camera = camera;
}

unsigned int c_renderer_geom_at_pixel(c_renderer_t *self, int x, int y,
		float *depth)
{
	entity_t result;
	texture_t *tex = c_renderer_tex(self, ref("selectable"));
	if(!tex) return entity_null;

	unsigned int res = texture_get_pixel(tex, 1,
			x * self->resolution, y * self->resolution, depth) & 0xFFFF;
	result = res - 1;
	return result;
}

entity_t c_renderer_entity_at_pixel(c_renderer_t *self, int x, int y,
		float *depth)
{
	entity_t result;
	texture_t *tex = c_renderer_tex(self, ref("selectable"));
	if(!tex) return entity_null;

	unsigned int res = texture_get_pixel(tex, 0,
			x * self->resolution, y * self->resolution, depth) & 0xFFFF;
	result = res - 1;
	return result;
}


int c_renderer_component_menu(c_renderer_t *self, void *ctx)
{
	nk_layout_row_dynamic(ctx, 0, 1);
	int i;
	if(nk_button_label(ctx, "Fullscreen"))
	{
		c_window_toggle_fullscreen(c_window(self));
	}

	char fps[12]; sprintf(fps, "%d", g_candle->fps);
	nk_layout_row_begin(ctx, NK_DYNAMIC, 30, 2);
		nk_layout_row_push(ctx, 0.35);
		nk_label(ctx, "FPS: ", NK_TEXT_LEFT);
		nk_layout_row_push(ctx, 0.65);
		nk_label(ctx, fps, NK_TEXT_RIGHT);
	nk_layout_row_end(ctx);
	nk_layout_row_dynamic(ctx, 0, 1);

	for(i = 1; i < self->outputs_num; i++)
	{
		pass_output_t *output = &self->outputs[i];
		if(output->buffer)
		{
			if (nk_button_label(ctx, output->buffer->name))
			{
				c_editmode_open_texture(c_editmode(self), output->buffer);
			}
		}
	}
	return CONTINUE;
}

int c_renderer_mouse_press(c_renderer_t *self, mouse_button_data *event)
{
	float depth;
	entity_t ent = c_renderer_entity_at_pixel(self, event->x, event->y, &depth);
	unsigned int geom = c_renderer_geom_at_pixel(self, event->x, event->y, &depth);

	if(ent)
	{
		model_button_data ev = {
			.x = event->x, .y = event->y, .direction = event->direction,
			.depth = depth, .geom = geom, .button = event->button
		};
		return entity_signal_same(ent, sig("model_press"), &ev);
	}
	return CONTINUE;
}

int c_renderer_mouse_release(c_renderer_t *self, mouse_button_data *event)
{

	float depth;
	entity_t ent = c_renderer_entity_at_pixel(self, event->x, event->y, &depth);
	unsigned int geom = c_renderer_geom_at_pixel(self, event->x, event->y, &depth);

	if(ent)
	{
		model_button_data ev = {
			.x = event->x, .y = event->y, .direction = event->direction,
			.depth = depth, .geom = geom, .button = event->button
		};
		return entity_signal_same(ent, sig("model_release"), &ev);
	}
	return CONTINUE;
}

REG()
{
	ct_t *ct = ct_new("renderer", sizeof(c_renderer_t),
			NULL, NULL, 1, ref("window"));

	ct_listener(ct, WORLD, sig("window_resize"), c_renderer_resize);

	ct_listener(ct, WORLD | 100, sig("world_draw"), c_renderer_draw);

	ct_listener(ct, ENTITY, sig("entity_created"), c_renderer_created);

	ct_listener(ct, WORLD, sig("component_menu"), c_renderer_component_menu);

	ct_listener(ct, WORLD | 100, sig("mouse_press"), c_renderer_mouse_press);
	ct_listener(ct, WORLD | 100, sig("mouse_release"), c_renderer_mouse_release);

	signal_init(sig("model_press"), 0);
	signal_init(sig("model_release"), 0);

	signal_init(sig("offscreen_render"), 0);
	signal_init(sig("render_visible"), sizeof(shader_t));
	signal_init(sig("render_selectable"), sizeof(shader_t));
	signal_init(sig("render_lights"), sizeof(shader_t));
	signal_init(sig("render_transparent"), sizeof(shader_t));
	signal_init(sig("render_emissive"), sizeof(shader_t));
	signal_init(sig("render_quad"), sizeof(shader_t));
	signal_init(sig("render_decals"), sizeof(shader_t));
}



static void c_renderer_update_probes(c_renderer_t *self)
{
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
	glerr();
	/* c_renderer_target(self); */

	glEnable(GL_CULL_FACE); glerr();
	glEnable(GL_DEPTH_TEST); glerr();

	entity_signal(c_entity(self), sig("offscreen_render"), NULL);

	glerr();

	glDisable(GL_CULL_FACE); glerr();
	glDisable(GL_DEPTH_TEST); glerr();

}

static int c_renderer_pass(c_renderer_t *self, unsigned int hash)
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

void c_renderer_add_pass(c_renderer_t *self, const char *name,
		const char *shader_name, ulong draw_signal, int flags, bind_t binds[])
{
	char buffer[32];
	snprintf(buffer, sizeof(buffer), name, self->passes_size);
	unsigned int hash = ref(buffer);
	int i = c_renderer_pass(self, hash);
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

	pass->depth_update =	!(flags & PASS_LOCK_DEPTH);
	pass->depth = NULL;

	pass->invert_depth = flags & PASS_INVERT_DEPTH;

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
			sb->buffer.u_brightness = -1;
			int t;
			for(t = 0; t < 16; t++)
			{
				sb->buffer.u_tex[t] = -1;
			}
			pass->binds[i].hash = ref(pass->binds[i].name);
		}
		if(pass->binds[i].type == BIND_OUT)
		{
			pass->output = pass->binds[i].buffer;
		}
		else if(pass->binds[i].type == BIND_DEPTH)
		{
			pass->depth = pass->binds[i].buffer;
		}
	}
	self->ready = 0;
}

void c_renderer_clear_shaders(c_renderer_t *self, shader_t *shader)
{
	int i;
	for(i = 0; i < self->passes_size; i++)
	{
		/* shader_destroy(self->passes[i].shader); */
	}
	self->passes_size = 0;
}

/* void pass_set_model(pass_t *self, mat4_t model) */
/* { */
/* 	mat4_t mvp = mat4_mul(self->shader->vp, model); */

/* 	glUniformMatrix4fv(self->u_mvp, 1, GL_FALSE, (void*)mvp._); */
/* 	glUniformMatrix4fv(self->u_m, 1, GL_FALSE, (void*)model._); */

/* 	/1* shader_update(shader, &node->model); *1/ */
/* } */


int c_renderer_draw(c_renderer_t *self)
{
	uint i;

	if(self->camera == entity_null) return CONTINUE;
	if(!self->width || !self->height) return CONTINUE;

	self->frame++;

	c_renderer_update_probes(self);


	if(!self->ready && self->frame > 20)
	{
		c_renderer_update_screen_texture(self);
	}

	glerr();

	for(i = 0; i < self->passes_size; i++)
	{
		c_renderer_draw_pass(self, &self->passes[i]);
	}

	/* ----------------------- */

	/* UPDATE SCREEN */

	if(self->output)
	{
		c_window_rect(c_window(self), 0, 0, self->width, self->height,
				self->output);
	}

	/* ------------- */

	return CONTINUE;
}

