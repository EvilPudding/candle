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
#include "noise.h"
#include <nk.h>


DEC_SIG(offscreen_render);
DEC_SIG(render_visible);
DEC_SIG(render_transparent);
DEC_SIG(render_quad);
DEC_SIG(render_decals);
/* DEC_SIG(world_changed); */

static int c_renderer_pass(c_renderer_t *self, const char *name);

static void c_renderer_update_probes(c_renderer_t *self);

static texture_t *c_renderer_draw_pass(c_renderer_t *self, pass_t *pass);

static int c_renderer_resize(c_renderer_t *self, window_resize_data *event);

static int c_renderer_update_screen_texture(c_renderer_t *self);

int c_renderer_scene_changed(c_renderer_t *self)
{
	g_update_id++;
	return 1;
}

static void c_renderer_bind_pass_output(c_renderer_t *self, pass_t *pass,
		bind_t *bind)
{

	shader_bind_t *sb = &bind->vs_uniforms[self->shader->index];

	/* int i = bind->pass_output.pass->output_from; */

	glUniform1i(sb->pass_output.u_texture, 13 + self->shader->bound_textures);
	glActiveTexture(GL_TEXTURE0 + 13 + self->shader->bound_textures); glerr();
	texture_bind(bind->pass_output.pass->output, COLOR_TEX);
	glerr();

	self->shader->bound_textures++;

	glUniform1i(sb->pass_output.u_depth, 13 + self->shader->bound_textures);
	glActiveTexture(GL_TEXTURE0 + 13 + self->shader->bound_textures); glerr();
	texture_bind(bind->pass_output.pass->output, DEPTH_TEX);
	glerr();

	self->shader->bound_textures++;

	if(bind->pass_output.pass->record_brightness)
	{
		float brightness = bind->pass_output.pass->output->brightness;
		glUniform1f(sb->pass_output.u_brightness, brightness); glerr();
	}

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
	glUniform3f(sb->camera.u_pos, cam->pos.x, cam->pos.y, cam->pos.z);
#ifdef MESH4
	glUniform1f(sb->camera.u_angle4, self->angle4);
#endif
	glUniform1f(sb->camera.u_exposure, cam->exposure);

	/* TODO unnecessary? */
	glUniformMatrix4fv(sb->camera.u_projection, 1, GL_FALSE,
			(void*)cam->projection_matrix._);
}

static void c_renderer_bind_gbuffer(c_renderer_t *self, pass_t *pass,
		bind_t *bind)
{
	shader_bind_t *sb = &bind->vs_uniforms[self->shader->index];
	if(bind->gbuffer < 0) return;

	texture_t *gbuffer = self->passes[bind->gbuffer].output;
	if(!gbuffer) return;

	glUniform2f(self->shader->u_screen_size,
			pass->output->width,
			pass->output->height); glerr();

	int i = 0;

	glUniform1i(sb->gbuffer.u_depth, i); glerr();
	glActiveTexture(GL_TEXTURE0 + i); glerr();
	texture_bind(gbuffer, i);
	i++;

	glUniform1i(sb->gbuffer.u_diffuse, i); glerr();
	glActiveTexture(GL_TEXTURE0 + i); glerr();
	texture_bind(gbuffer, i);
	i++;

	glUniform1i(sb->gbuffer.u_specular, i); glerr();
	glActiveTexture(GL_TEXTURE0 + i); glerr();
	texture_bind(gbuffer, i);
	i++;

	glUniform1i(sb->gbuffer.u_transparency, i); glerr();
	glActiveTexture(GL_TEXTURE0 + i); glerr();
	texture_bind(gbuffer, i);
	i++;

	glUniform1i(sb->gbuffer.u_position, i); glerr();
	glActiveTexture(GL_TEXTURE0 + i); glerr();
	texture_bind(gbuffer, i);
	i++;

	glUniform1i(sb->gbuffer.u_normal, i); glerr();
	glActiveTexture(GL_TEXTURE0 + i); glerr();
	texture_bind(gbuffer, i);
	i++;

	glUniform1i(sb->gbuffer.u_id, i); glerr();
	glActiveTexture(GL_TEXTURE0 + i); glerr();
	texture_bind(gbuffer, i);
	i++;

	glerr();
}

void c_renderer_bind_get_uniforms(c_renderer_t *self, bind_t *bind,
		shader_bind_t *sb, int pass_id)
{

	int prev = 0;
	switch(bind->type)
	{
		case BIND_NONE:
			printf("Empty bind??\n");
			break;
		case BIND_CAMERA:
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
#ifdef MESH4
			sb->camera.u_angle4 =
				shader_uniform(self->shader, bind->name, "angle4");
#endif

			break;
		case BIND_GBUFFER:
			sb->gbuffer.u_depth =
				shader_uniform(self->shader, bind->name, "depth");
			sb->gbuffer.u_diffuse =
				shader_uniform(self->shader, bind->name, "diffuse");
			sb->gbuffer.u_specular =
				shader_uniform(self->shader, bind->name, "specular");
			sb->gbuffer.u_transparency =
				shader_uniform(self->shader, bind->name, "transparency");
			sb->gbuffer.u_position =
				shader_uniform(self->shader, bind->name, "position");
			sb->gbuffer.u_id =
				shader_uniform(self->shader, bind->name, "id");
			sb->gbuffer.u_normal =
				shader_uniform(self->shader, bind->name, "normal");
			glerr();

			bind->gbuffer = c_renderer_pass(self, bind->name);
			break;

		case BIND_PREV_PASS_OUTPUT: prev = 1;
		case BIND_PASS_OUTPUT:
			sb->pass_output.u_texture = shader_uniform(self->shader,
					bind->name, "texture"); glerr();

			sb->pass_output.u_depth = shader_uniform(self->shader, bind->name,
					"depth"); glerr();
			sb->pass_output.u_brightness = shader_uniform(self->shader,
					bind->name, "brightness"); glerr();

			int id;
			if(prev)
			{
				id = pass_id - 1;
			}
			else if(bind->pass_output.name[0])
			{
				id = c_renderer_pass(self, bind->pass_output.name);
			}
			else
			{
				id = c_renderer_pass(self, bind->name);
			}

			bind->pass_output.pass = &self->passes[id];

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
				self->bound_camera_model, self->bound_exposure,
				self->bound_angle4);
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
			c_renderer_bind_get_uniforms(self, bind, sb, pass->id);
		}

		switch(bind->type)
		{
		case BIND_NONE: printf("error\n"); break;
		case BIND_PASS_OUTPUT:
		case BIND_PREV_PASS_OUTPUT:
			c_renderer_bind_pass_output(self, pass, bind);
			break;
		case BIND_NUMBER:
			if(bind->getter)
			{
				bind->number = ((number_getter)bind->getter)(bind->usrptr);
			}
			glUniform1f(sb->number.u, bind->number); glerr();
			glerr();
			break;
		case BIND_INTEGER:
			if(bind->getter)
			{
				bind->integer = ((integer_getter)bind->getter)(bind->usrptr);
			}
			glUniform1i(sb->integer.u, bind->integer); glerr();
			glerr();
			break;
		case BIND_CAMERA: c_renderer_bind_camera(self, pass, bind); break;
		case BIND_GBUFFER: c_renderer_bind_gbuffer(self, pass, bind); break;
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
	/* 	if(probe) shader_bind_ambient(self->shader, probe->map); */
	/* } */
	return 1;
}

void fs_bind(fs_t *fs)
{
	c_renderer_t *renderer = c_renderer(&candle->systems);
	if(renderer->frag_bound == fs) return;
	glerr();

	renderer->frag_bound = fs;
	renderer->shader = NULL;
	/* printf("fs_bind %s\n", fs->filename); */
}

shader_t *vs_bind(vs_t *vs)
{
	c_renderer_t *renderer = c_renderer(&candle->systems);

	if(renderer->shader && renderer->shader->index == vs->index)
	{
		shader_bind(renderer->shader);
		c_renderer_bind_pass(renderer, renderer->current_pass);
		renderer->shader->bound_textures = 0;
		return renderer->shader;
	}
	/* printf("vs_bind %s\n", vs->name); */
	glerr();

	fs_t *fs = renderer->frag_bound;

	shader_t **sh = &fs->combinations[vs->index];
	if(!*sh) *sh = shader_new(fs, vs);
	if(!(*sh)->ready) return NULL;

	renderer->shader = *sh; 
	renderer->shader->bound_textures = 0;

	shader_bind(renderer->shader);

	c_renderer_bind_pass(renderer, renderer->current_pass);
	return *sh;
}

static void c_renderer_init(c_renderer_t *self) { } 

entity_t c_renderer_get_camera(c_renderer_t *self)
{
	return self->camera;
}

void c_renderer_set_output(c_renderer_t *self, const char *name)
{
	strcpy(self->output, name);

	if(self->output[0])
	{
		self->output_id = c_renderer_pass(self, self->output);
	}
}

static int c_renderer_created(c_renderer_t *self)
{

#ifdef MESH4
	self->angle4 = 0.01f;
	self->angle4 = M_PI * 0.45;
#endif

	c_renderer_add_pass(self, "gbuffer", "gbuffer", render_visible, 1.0f,
			PASS_GBUFFER | PASS_CLEAR_DEPTH | PASS_CLEAR_COLOR,
		(bind_t[]){
			{BIND_CAMERA, "camera", (getter_cb)c_renderer_get_camera, self},
			{BIND_NONE}
		}
	);

	/* DECAL PASS */
	/* c_renderer_add_pass(self, "gbuffer", "decals", render_decals, 1.0f, */
	/* 		PASS_GBUFFER | PASS_DISABLE_DEPTH, */
	/* 	(bind_t[]){ */
	/* 		{BIND_CAMERA, "camera", (getter_cb)c_renderer_get_camera, self}, */
	/* 		{BIND_GBUFFER, "gbuffer"}, */
	/* 		{BIND_NONE} */
	/* 	} */
	/* ); */


	c_renderer_add_pass(self, "ssao", "ssao", render_quad, 1.0f,
			PASS_CLEAR_DEPTH | PASS_CLEAR_COLOR,
		(bind_t[]){
			{BIND_GBUFFER, "gbuffer"},
			{BIND_CAMERA, "camera", (getter_cb)c_renderer_get_camera, self},
			{BIND_NONE}
		}
	);

	c_renderer_add_pass(self, "rendered", "phong", render_quad, 1.0f,
			PASS_FOR_EACH_LIGHT | PASS_CLEAR_DEPTH | PASS_CLEAR_COLOR |
			(self->roughness * PASS_MIPMAPED),
		(bind_t[]){
			{BIND_GBUFFER, "gbuffer"},
			{BIND_PASS_OUTPUT, "ssao"},
			{BIND_CAMERA, "camera", (getter_cb)c_renderer_get_camera, self},
			{BIND_NONE}
		}
	);

	c_renderer_add_pass(self, "gbuffer", "gbuffer", render_transparent, 1.0f,
			PASS_GBUFFER,
		(bind_t[]){
			{BIND_CAMERA, "camera", (getter_cb)c_renderer_get_camera, self},
			{BIND_NONE}
		}
	);


	c_renderer_add_pass(self, "transp", "transparency", render_quad, 1.0f,
			PASS_CLEAR_DEPTH | PASS_CLEAR_COLOR,
		(bind_t[]){
			{BIND_GBUFFER, "gbuffer"},
			{BIND_PASS_OUTPUT, "rendered"},
			{BIND_CAMERA, "camera", (getter_cb)c_renderer_get_camera, self},
				{BIND_NONE}
		}
	);

	c_renderer_add_pass(self, "final", "ssr", render_quad, 1.0f,
			PASS_CLEAR_DEPTH | PASS_CLEAR_COLOR | self->auto_exposure * PASS_RECORD_BRIGHTNESS,
		(bind_t[]){
			{BIND_GBUFFER, "gbuffer"},
			{BIND_PASS_OUTPUT, "rendered"},
			{BIND_PASS_OUTPUT, "transp"},
			{BIND_CAMERA, "camera", (getter_cb)c_renderer_get_camera, self},
			{BIND_NONE}
		}
	);


	float bloom_scale = 0.2f;
	int i;
	for(i = 0; i < 6; i++)
	{
		c_renderer_add_pass(self, "pre_bloom", i?"copy":"bright", render_quad, 1.0f, 0,
			(bind_t[]){
				{BIND_PREV_PASS_OUTPUT, "buf"},
				{BIND_NONE}
			}
		);
		c_renderer_add_pass(self, "bloom_x", "blur", render_quad, bloom_scale, 0,
			(bind_t[]){
				{BIND_PREV_PASS_OUTPUT, "buf"},
				{BIND_INTEGER, "horizontal", .integer = 1},
				{BIND_NONE}
			}
		);
		c_renderer_add_pass(self, "bloom_xy", "blur", render_quad, bloom_scale, 0,
			(bind_t[]){
				{BIND_PREV_PASS_OUTPUT, "buf"},
				{BIND_INTEGER, "horizontal", .integer = 0},
				{BIND_NONE}
			}
		);
	}

	c_renderer_add_pass(self, "final", "copy", render_quad, 1.0f,
			PASS_ADDITIVE,
		(bind_t[]){
			{BIND_PREV_PASS_OUTPUT, "buf"},
			{BIND_NONE}
		}
	);

	/* c_renderer_set_output(self, "bloom_xy"); */
	c_renderer_set_output(self, "final");

	return 1;
}

static int c_renderer_update_screen_texture(c_renderer_t *self)
{
	int w = self->width * self->resolution;
	int h = self->height * self->resolution;

	int i;
	for(i = 0; i < self->passes_size; i++)
	{
		pass_t *pass = &self->passes[i];

		if(pass->output_from != i)
		{
			pass->output = self->passes[pass->output_from].output;
			continue;
		}
		int W = w * pass->resolution;
		int H = h * pass->resolution;
		if(pass->output)
		{
			if(pass->output->width == W && pass->output->height == H)
			{
				continue;
			}
			texture_destroy(pass->output);
		}

		pass->output = texture_new_2D( W, H, 4, 1, 0, 1);

		if(pass->gbuffer)
		{
			/* texture_add_buffer(pass->output, "diffuse", 1, 1, 0); */
			texture_add_buffer(pass->output, "specular", 1, 4, 0);
			texture_add_buffer(pass->output, "transparency", 1, 4, 0);
			texture_add_buffer(pass->output, "position", 1, 3, 0);
			texture_add_buffer(pass->output, "normal", 1, 2, 0);
			texture_add_buffer(pass->output, "id", 1, 2, 0);

			texture_draw_id(pass->output, COLOR_TEX); /* DRAW DIFFUSE */

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

	return 1;
}


static inline void pass_draw_object(c_renderer_t *self, pass_t *pass)
{
	if(pass->gbuffer)
	{
		glEnable(GL_CULL_FACE); glerr();
		glEnable(GL_DEPTH_TEST); glerr();
	}

	entity_signal(c_entity(self), pass->draw_signal, NULL);

	if(pass->gbuffer)
	{
		glDisable(GL_CULL_FACE); glerr();
		glDisable(GL_DEPTH_TEST); glerr();
	}
}

static texture_t *c_renderer_draw_pass(c_renderer_t *self, pass_t *pass)
{
	self->frame++;
	uint i;

	if(!pass->output) return NULL;

	if(!texture_target(pass->output, 0))
	{
		return NULL;
	}

	if(pass->clear)
	{
		glClear(pass->clear); glerr();
	}

	self->current_pass = pass;

	if(!pass->shader || !pass->shader->ready) return NULL;

	fs_bind(pass->shader);

	/* pass->shader.vp = mat4(); */
	glerr();

	glDepthMask(!pass->disable_depth);

	if(pass->additive)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	}

	if(pass->for_each_light)
	{
		int p;
		ct_t *lights = ecm_get(ct_light);
		c_light_t *light;

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		for(p = 0; p < lights->pages_size; p++)
		for(i = 0; i < lights->pages[p].components_size; i++)
		{
			light = (c_light_t*)ct_get_at(lights, p, i);

			self->bound_light = c_entity(light);

			pass_draw_object(self, pass);
		}
		self->bound_light = entity_null;
	}
	else
	{
		pass_draw_object(self, pass);
	}

	glDisable(GL_BLEND);

	if(pass->mipmaped || pass->record_brightness)
	{
		texture_bind(pass->output, -1);
		glGenerateMipmap(pass->output->target); glerr();

		if(pass->record_brightness)
		{
			texture_update_brightness(pass->output);

			float brightness = pass->output->brightness;
			if(!brightness) brightness = 1.0f;
			float targetExposure = 0.3 + 0.4 / brightness;
			/* float targetExposure = fmax(0.4 - brightness, 0.0f); */

			c_camera_t *cam = c_camera(&self->camera);
			cam->exposure += (targetExposure - cam->exposure) / 100;
		}
	}
	self->current_pass = NULL;

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
	loader_wait(candle->loader);

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

	c_renderer_t *self = component_new(ct_renderer);

	self->output_id = -1;
	self->resolution = resolution;
	self->auto_exposure = auto_exposure;
	self->roughness = roughness;

	return self;
}

void c_renderer_add_camera(c_renderer_t *self, entity_t camera)
{
	self->camera = camera;
}

entity_t c_renderer_entity_at_pixel(c_renderer_t *self, int x, int y,
		float *depth)
{
	entity_t result;
	if(!self->passes[0].output) return entity_null;

	unsigned int res = texture_get_pixel(self->passes[0].output, 5,
			x * self->resolution, y * self->resolution, depth);
	result = res - 1;
	return result;
}

int c_renderer_global_menu(c_renderer_t *self, void *ctx)
{
#ifdef MESH4
	nk_layout_row_begin(ctx, NK_DYNAMIC, 0, 2);
		nk_layout_row_push(ctx, 0.35);
		nk_label(ctx, "4D angle:", NK_TEXT_LEFT);
		nk_layout_row_push(ctx, 0.65);
		float before = self->angle4;
		nk_slider_float(ctx, 0, &self->angle4, M_PI / 2, 0.01);
		if(self->angle4 != before)
		{
			g_update_id++;
		}
	nk_layout_row_end(ctx);
#endif

	char fps[12]; sprintf(fps, "%d", candle->fps);

	nk_layout_row_begin(ctx, NK_DYNAMIC, 30, 2);
		nk_layout_row_push(ctx, 0.35);
		nk_label(ctx, "FPS: ", NK_TEXT_LEFT);
		nk_layout_row_push(ctx, 0.65);
		nk_label(ctx, fps, NK_TEXT_RIGHT);
	nk_layout_row_end(ctx);



	return 1;
}

int c_renderer_component_menu(c_renderer_t *self, void *ctx)
{
	nk_layout_row_dynamic(ctx, 0, 1);
	int i;
	if(nk_button_label(ctx, "Fullscreen"))
	{
		c_window_toggle_fullscreen(c_window(self));
	}

	for(i = 0; i < self->passes_size; i++)
	{
		pass_t *pass = &self->passes[i];
		if(pass->output)
		{
			if (nk_button_label(ctx, pass->shader->filename))
			{
				c_editmode_open_texture(c_editmode(self), pass->output);
			}
		}
	}
	return 1;
}

DEC_CT(ct_renderer)
{
	ct_t *ct = ct_new("c_renderer", &ct_renderer,
			sizeof(c_renderer_t), (init_cb)c_renderer_init, 1, ct_window);

	ct_listener(ct, WORLD, window_resize, c_renderer_resize);

	ct_listener(ct, WORLD, world_draw, c_renderer_draw);

	ct_listener(ct, ENTITY, entity_created, c_renderer_created);

	ct_listener(ct, WORLD, component_menu, c_renderer_component_menu);

	ct_listener(ct, WORLD, global_menu, c_renderer_global_menu);

	signal_init(&offscreen_render, 0);
	signal_init(&render_visible, sizeof(shader_t));
	signal_init(&render_transparent, sizeof(shader_t));
	signal_init(&render_quad, sizeof(shader_t));
	signal_init(&render_decals, sizeof(shader_t));
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

	entity_signal(c_entity(self), offscreen_render, NULL);

	glerr();

	glDisable(GL_CULL_FACE); glerr();
	glDisable(GL_DEPTH_TEST); glerr();

}

static int c_renderer_pass(c_renderer_t *self, const char *name)
{
	int i;
	if(!name) return -1;
	for(i = 0; i < self->passes_size; i++)
	{
		pass_t *gb = &self->passes[i];
		if(!strncmp(gb->feed_name, name, sizeof(gb->feed_name)))
		{
			printf("found prev %s\n", name);
			return i;
		}
	}
	return -1;
}

void _c_renderer_add_pass(c_renderer_t *self, int i, const char *feed_name,
		const char *shader_name, ulong draw_signal, float resolution,
		int flags, bind_t binds[])
{
	int prev_with_same_name = c_renderer_pass(self, feed_name);
	
	pass_t *pass = &self->passes[i];
	pass->id = i;
	pass->shader = fs_new(shader_name);

	pass->clear = 0;
	if(flags & PASS_CLEAR_COLOR)
	{
		pass->clear |= GL_COLOR_BUFFER_BIT;
	}
	pass->disable_depth = !!(flags & PASS_DISABLE_DEPTH);
	if(flags & PASS_CLEAR_DEPTH)
	{
		pass->clear |= GL_DEPTH_BUFFER_BIT;
	}

	pass->output_from = prev_with_same_name > -1 ? prev_with_same_name : i;

	pass->draw_signal = draw_signal;
	pass->gbuffer = flags & PASS_GBUFFER;
	pass->additive = flags & PASS_ADDITIVE;
	pass->for_each_light = flags & PASS_FOR_EACH_LIGHT;
	pass->mipmaped = flags & PASS_MIPMAPED;
	pass->record_brightness = flags & PASS_RECORD_BRIGHTNESS;
	strncpy(pass->feed_name, feed_name, sizeof(pass->feed_name));
	pass->resolution = resolution;
	pass->output = NULL;

	pass->binds_size = 0;
	for(i = 0;;i++)
	{
		bind_t *bind = &binds[i];
		if(bind->type == BIND_NONE) break;
		pass->binds_size++;
	}
	pass->binds = malloc(sizeof(bind_t) * pass->binds_size);
	int j;
	for(i = 0; i < pass->binds_size; i++)
	{
		pass->binds[i] = binds[i];
		for(j = 0; j < 16; j++)
		{
			pass->binds[i].vs_uniforms[j].cached = 0;
		}
	}
	self->passes_bound = 0;
	self->ready = 0;
}

void c_renderer_replace_pass(c_renderer_t *self, const char *feed_name,
		const char *shader_name, ulong draw_signal, float resolution,
		int flags, bind_t binds[])
{
	int prev = c_renderer_pass(self, feed_name);
	_c_renderer_add_pass(self, prev, feed_name, shader_name,
			draw_signal, resolution, flags, binds);
}

void c_renderer_add_pass(c_renderer_t *self, const char *feed_name,
		const char *shader_name, ulong draw_signal, float resolution,
		int flags, bind_t binds[])
{
	_c_renderer_add_pass(self, self->passes_size++, feed_name, shader_name,
			draw_signal, resolution, flags, binds);

	if(self->output[0])
	{
		self->output_id = c_renderer_pass(self, self->output);
	}
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

	if(self->camera == entity_null) return 1;

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

	if(self->output_id >= 0)
	{
		c_window_rect(c_window(self), 0, 0, self->width, self->height,
				self->passes[self->output_id].output);
	}

	/* ------------- */

	return 1;
}

