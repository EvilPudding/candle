#include "renderer.h"
#include "../components/camera.h"
#include "../components/model.h"
#include "../components/spacial.h"
#include "../components/probe.h"
#include "../components/light.h"
#include "../components/ambient.h"
#include "../components/node.h"
#include "../components/name.h"
#include "../filters/bloom.h"
#include "../filters/blur.h"
#include "../systems/window.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <candle.h>
#include "noise.h"
#include <nk.h>
#define bloom_scale 0.5f

DEC_CT(ct_renderer);

DEC_SIG(offscreen_render);
DEC_SIG(render_visible);
DEC_SIG(render_transparent);
/* DEC_SIG(world_changed); */

static int c_renderer_gbuffer(c_renderer_t *self, const char *name);

static void c_renderer_add_gbuffer(c_renderer_t *self, const char *name,
		unsigned int draw_filter);

static void c_renderer_update_probes(c_renderer_t *self);

static texture_t *c_renderer_draw_pass(c_renderer_t *self, pass_t *pass, entity_t camera);

static int c_renderer_resize(c_renderer_t *self, window_resize_data *event);

static void c_renderer_update_screen_texture(c_renderer_t *self);

int c_renderer_scene_changed(c_renderer_t *self)
{
	g_update_id++;
	return 1;
}

static int c_renderer_bind_passes(c_renderer_t *self)
{
	int i, j, b;

	for(i = 0; i < self->passes_size; i++)
	{
		pass_t *pass = &self->passes[i];
		if(pass->cached) continue;
		pass->cached =  1;
		for(b = 0; b < pass->binds_size; b++)
		{
			pass->binds[b] = pass->binds[b];
			bind_t *bind = &pass->binds[b];
			switch(bind->type)
			{
				case BIND_NONE:
					printf("Empty bind??\n");
					break;
				case BIND_PASS_OUTPUT:
					printf("Passes as input should not be explicit!\n");
					break;
				case BIND_GBUFFER:
					bind->gbuffer.u_diffuse =
						shader_uniform(pass->shader, bind->name, "diffuse");
					bind->gbuffer.u_specular =
						shader_uniform(pass->shader, bind->name, "specular");
					bind->gbuffer.u_reflection =
						shader_uniform(pass->shader, bind->name, "reflection");
					bind->gbuffer.u_normal =
						shader_uniform(pass->shader, bind->name, "normal");
					bind->gbuffer.u_transparency =
						shader_uniform(pass->shader, bind->name, "transparency");
					bind->gbuffer.u_cposition =
						shader_uniform(pass->shader, bind->name, "cposition");
					bind->gbuffer.u_wposition =
						shader_uniform(pass->shader, bind->name, "wposition");
					bind->gbuffer.u_id =
						shader_uniform(pass->shader, bind->name, "id"); glerr();
					bind->gbuffer.id = c_renderer_gbuffer(self, bind->gbuffer.name);
					break;
				default:
					bind->number.u = glGetUniformLocation(pass->shader->program,
							bind->name);
					if(bind->number.u >= 4294967295)
					{
						printf("Explicit bind '%s' is not present in shader '%s'!\n",
								bind->name, pass->shader->filename);
					}
					break;
			}
		}
	}

	for(i = 1; i < self->passes_size; i++)
	{
		pass_t *pass = &self->passes[i];
		if(pass->cached == 2) continue;
		pass->cached = 2;
		for(j = i - 1; j >= 0; j--)
		{
			pass_t *pass_done = &self->passes[j];

			uint u_texture = shader_uniform(pass->shader, pass_done->feed_name,
					"texture"); glerr();

			if(u_texture >= 4294967295) continue;

			uint u_brightness = shader_uniform(pass->shader, pass_done->feed_name,
					"brightness"); glerr();

			bind_t *bind = &pass->binds[pass->binds_size++];
			bind->type = BIND_PASS_OUTPUT;
			bind->pass_output.u_texture = u_texture;
			bind->pass_output.u_brightness = u_brightness;
			bind->pass_output.pass = pass_done;
		}
	}
	c_renderer_resize(self, &(window_resize_data){.width = c_window(self)->width,
			.height = c_window(self)->height });
	printf("PASSES BOUND\n\n");
	self->ready = 1;
	return 1;
}

static void c_renderer_init(c_renderer_t *self) { } 

static int c_renderer_created(c_renderer_t *self)
{
	self->gbuffer_shader = shader_new("gbuffer");
	self->quad_shader = shader_new("quad");

	filter_blur_init();
	filter_bloom_init();

	c_renderer_add_gbuffer(self, "opaque", render_visible);
	c_renderer_add_gbuffer(self, "transp", render_transparent);

#ifdef MESH4
	self->angle4 = 0.01f;
	self->angle4 = M_PI * 0.45;
#endif

	c_renderer_add_pass(self, "ssao",
			PASS_SCREEN_SCALE,
			1.0f, 1.0f, "ssao",
		(bind_t[]){
			{BIND_GBUFFER, "gbuffer", .gbuffer.name = "opaque"},
			{BIND_NONE}
		}
	);

	c_renderer_add_pass(self, "phong",
			PASS_FOR_EACH_LIGHT | PASS_SCREEN_SCALE,
			1.0f, 1.0f, "phong",
		(bind_t[]){
			{BIND_GBUFFER, "gbuffer", .gbuffer.name = "opaque"},
			{BIND_NONE}
		}
	);

	c_renderer_add_pass(self, "transp",
			PASS_SCREEN_SCALE | (self->roughness * PASS_MIPMAPED),
			1.0f, 1.0f, "transparency",
		(bind_t[]){
			{BIND_GBUFFER, "gbuffer", .gbuffer.name = "transp"},
			{BIND_NONE}
		}
	);

	c_renderer_add_pass(self, "refl",
			PASS_SCREEN_SCALE | (self->auto_exposure * PASS_RECORD_BRIGHTNESS),
			1.0f, 1.0f, "ssr",
		(bind_t[]){
			{BIND_GBUFFER, "gbuffer", .gbuffer.name = "opaque"},
			{BIND_NONE}
		}
	);


	self->quad = entity_new(c_name_new("renderer_quad"),
			c_model_new(mesh_quad(), NULL, 0));
	c_model(&self->quad)->visible = 0;

	c_renderer_resize(self, &(window_resize_data){.width = c_window(self)->width,
			.height = c_window(self)->height });
	return 1;
}

static int c_renderer_gl_update_textures(c_renderer_t *self)
{
	int w = self->width * self->resolution;
	int h = self->height * self->resolution;

	int i;
	for(i = 0; i < self->passes_size; i++)
	{
		pass_t *pass = &self->passes[i];
		if(pass->screen_scale)
		{
			int bound = 0;
			if(pass->output)
			{
				bound = self->bound == pass->output;

				texture_destroy(pass->output);
			}
			pass->output = texture_new_2D( w * pass->screen_scale_x,
										   h * pass->screen_scale_y,
										   32, GL_RGBA16F, 1, 0);
			if(bound)
			{
				self->bound = pass->output;
				texture_target(pass->output, 0);
			}
		}
	}

	for(i = 0; i < self->gbuffers_size; i++)
	{
		gbuffer_t *gb = &self->gbuffers[i];
		int bound = 0;

		if(gb->buffer)
		{
			bound = self->bound == gb->buffer;
			texture_destroy(gb->buffer);
		}
		gb->buffer = texture_new_2D(w, h, 32, GL_RGBA16F, 1, 0);
		/* texture_add_buffer(gb->buffer, "diffuse", 1, 1, 0); */
		texture_add_buffer(gb->buffer, "specular", 1, 1, 0);
		texture_add_buffer(gb->buffer, "reflection", 1, 1, 0);
		texture_add_buffer(gb->buffer, "normal", 1, 0, 0);
		texture_add_buffer(gb->buffer, "transparency", 1, 1, 0);
		texture_add_buffer(gb->buffer, "cameraspace position", 1, 0, 0);
		texture_add_buffer(gb->buffer, "worldspace position", 1, 0, 0);
		texture_add_buffer(gb->buffer, "id", 1, 0, 0);

		texture_draw_id(gb->buffer, COLOR_TEX); /* DRAW DIFFUSE */

		if(bound)
		{
			self->bound = gb->buffer;
			texture_target(gb->buffer, 0);
		}
	}

	if(self->temp_buffers[0]) texture_destroy(self->temp_buffers[0]);
	if(self->temp_buffers[1]) texture_destroy(self->temp_buffers[1]);
	self->temp_buffers[0] = texture_new_2D(w * self->resolution * bloom_scale,
			h * self->resolution * bloom_scale, 32, GL_RGBA16F, 1, 0);
	self->temp_buffers[1] = texture_new_2D(w * self->resolution * bloom_scale,
			h * self->resolution * bloom_scale, 32, GL_RGBA16F, 1, 0);

	return 1;
}

static void c_renderer_update_screen_texture(c_renderer_t *self)
{
	loader_push(candle->loader, (loader_cb)c_renderer_gl_update_textures, NULL,
			(c_t*)self);
}

static int c_renderer_resize(c_renderer_t *self, window_resize_data *event)
{
    self->width = event->width * self->percent_of_screen;
    self->height = event->height;
	printf("%d %d\n", self->width, self->height);

	c_renderer_update_screen_texture(self);

	return 1;
}

static void c_renderer_bind_pass_output(c_renderer_t *self, pass_t *pass,
		bind_t *bind, int i)
{
	glUniform1i(bind->pass_output.u_texture, 13 + i); glerr();
	glActiveTexture(GL_TEXTURE0 + 13 + i); glerr();
	texture_bind(bind->pass_output.pass->output, -1);

	if(bind->pass_output.pass->record_brightness)
	{
		float brightness = bind->pass_output.pass->output->brightness;
		glUniform1f(bind->pass_output.u_brightness, brightness); glerr();
	}

}

static void c_renderer_bind_gbuffer(c_renderer_t *self, pass_t *pass,
		bind_t *bind)
{
	if(bind->gbuffer.id < 0) return;

	texture_t *gbuffer = self->gbuffers[bind->gbuffer.id].buffer;
	if(!gbuffer) return;

	glUniform1f(pass->shader->u_screen_scale_x, 1.0f); glerr();
	glUniform1f(pass->shader->u_screen_scale_y, 1.0f); glerr();

	glUniform1i(bind->gbuffer.u_diffuse, 1); glerr();
	glActiveTexture(GL_TEXTURE0 + 1); glerr();
	texture_bind(gbuffer, 1);

	glUniform1i(bind->gbuffer.u_specular, 2); glerr();
	glActiveTexture(GL_TEXTURE0 + 2); glerr();
	texture_bind(gbuffer, 2);

	glUniform1i(bind->gbuffer.u_reflection, 3); glerr();
	glActiveTexture(GL_TEXTURE0 + 3); glerr();
	texture_bind(gbuffer, 3);

	glUniform1i(bind->gbuffer.u_normal, 4); glerr();
	glActiveTexture(GL_TEXTURE0 + 4); glerr();
	texture_bind(gbuffer, 4);

	glUniform1i(bind->gbuffer.u_transparency, 5); glerr();
	glActiveTexture(GL_TEXTURE0 + 5); glerr();
	texture_bind(gbuffer, 5);

	glUniform1i(bind->gbuffer.u_cposition, 6); glerr();
	glActiveTexture(GL_TEXTURE0 + 6); glerr();
	texture_bind(gbuffer, 6);

	glUniform1i(bind->gbuffer.u_wposition, 7); glerr();
	glActiveTexture(GL_TEXTURE0 + 7); glerr();
	texture_bind(gbuffer, 7);

	glUniform1i(bind->gbuffer.u_id, 8); glerr();
	glActiveTexture(GL_TEXTURE0 + 8); glerr();
	texture_bind(gbuffer, 8);
}

static texture_t *c_renderer_draw_pass(c_renderer_t *self, pass_t *pass, entity_t camera)
{
	uint i;

	if(!pass->output) return NULL;

	self->bound = pass->output;
	texture_target(pass->output, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); glerr();

	shader_bind(pass->shader);

	c_camera_t *cam = c_camera(&camera);
#ifdef MESH4
	shader_bind_camera(pass->shader, cam->pos, &cam->view_matrix,
			&cam->projection_matrix, cam->exposure, self->angle4);
#else
	shader_bind_camera(pass->shader, cam->pos, &cam->view_matrix,
			&cam->projection_matrix, cam->exposure);
#endif

	ct_t *ambients = ecm_get(ct_ambient);
	c_ambient_t *ambient = (c_ambient_t*)ct_get_at(ambients, 0, 0);

	if(ambient)
	{
		c_probe_t *probe = c_probe(ambient);
		if(probe) shader_bind_ambient(pass->shader, probe->map);
	}

	glerr();
	for(i = 0; i < pass->binds_size; i++)
	{
		bind_t *bind = &pass->binds[i];
		switch(bind->type)
		{
		case BIND_NONE: printf("error\n"); break;
		case BIND_PASS_OUTPUT:
			c_renderer_bind_pass_output(self, pass, bind, i);
			glerr();
			break;
		case BIND_NUMBER:
			if(bind->getter)
			{
				bind->number.value = ((number_getter)bind->getter)(bind->entity);
			}
			glUniform1f(bind->number.u, bind->number.value); glerr();
			glerr();
			break;
		case BIND_INTEGER:
			if(bind->getter)
			{
				bind->integer.value = ((integer_getter)bind->getter)(bind->entity);
			}
			glUniform1i(bind->integer.u, bind->integer.value); glerr();
			glerr();
			break;
		case BIND_GBUFFER:
			c_renderer_bind_gbuffer(self, pass, bind);
			glerr();
			break;
		case BIND_VEC3:
			if(bind->getter)
			{
				bind->vec3.value = ((vec3_getter)bind->getter)(bind->entity);
			}
			glUniform3f(bind->vec3.u, bind->vec3.value.r,
					bind->vec3.value.g, bind->vec3.value.b);
			glerr();
			break;
		}

	}

	c_mesh_gl_t *quad = c_mesh_gl(&self->quad);
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

			shader_bind_light(pass->shader, c_entity(light));

			c_mesh_gl_draw(quad, NULL, 0);
		}
		glDisable(GL_BLEND);
	}
	else
	{
		c_mesh_gl_draw(quad, NULL, 0);
	}

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

			cam->exposure += (targetExposure - cam->exposure) / 100;
		}
	}

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
	self->perlin = texture_new_3D(m, m, m, 32);
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

c_renderer_t *c_renderer_new(float resolution, int auto_exposure, int roughness,
		float percent_of_screen, int lock_fps)
{
	SDL_GL_SetSwapInterval(lock_fps);

	c_renderer_t *self = component_new(ct_renderer);

	self->resolution = resolution;
	self->auto_exposure = auto_exposure;
	self->roughness = roughness;
	self->percent_of_screen = percent_of_screen;

	return self;
}

entity_t c_renderer_entity_at_pixel(c_renderer_t *self, int x, int y,
		float *depth)
{
	entity_t result;
	if(!self->gbuffers[0].buffer) return entity_null;
	result = texture_get_pixel(self->gbuffers[0].buffer, 7, x, y, depth);

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

	if (nk_button_label(ctx, "gbuffer"))
	{
		c_editmode_open_texture(c_editmode(self),
				c_renderer(self)->gbuffers[0].buffer);
	}

	for(i = 0; i < self->passes_size; i++)
	{
		pass_t *pass = &self->passes[i];
		if(pass->output)
		{
			if (nk_button_label(ctx, pass->feed_name))
			{
				c_editmode_open_texture(c_editmode(self), pass->output);
			}
		}
	}
	return 1;
}

void c_renderer_register()
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

static int c_renderer_gbuffer(c_renderer_t *self, const char *name)
{
	int i;
	if(!name) return -1;
	for(i = 0; i < self->gbuffers_size; i++)
	{
		gbuffer_t *gb = &self->gbuffers[i];
		if(!strncmp(gb->name, name, sizeof(gb->name)))
		{
			return i;
		}
	}
	return -1;
}

static void c_renderer_add_gbuffer(c_renderer_t *self, const char *name,
		unsigned int draw_filter)
{
	int i = self->gbuffers_size++;
	gbuffer_t *gb = &self->gbuffers[i];

	gb->draw_filter = draw_filter;
	strncpy(gb->name, name, sizeof(gb->name));
}

void c_renderer_add_pass(c_renderer_t *self, const char *feed_name, int flags,
		float wid, float hei, const char *shader_name, bind_t binds[])
{
	shader_t *shader = shader_new(shader_name);
	int i = self->passes_size++;
	pass_t *pass = &self->passes[i];
	pass->shader = shader;
	pass->cached = 0;
	pass->for_each_light = flags & PASS_FOR_EACH_LIGHT;
	pass->mipmaped = flags & PASS_MIPMAPED;
	pass->record_brightness = flags & PASS_RECORD_BRIGHTNESS;
	strncpy(pass->feed_name, feed_name, sizeof(pass->feed_name));
	pass->screen_scale = flags & PASS_SCREEN_SCALE;
	if(pass->screen_scale)
	{
		pass->screen_scale_x = wid;
		pass->screen_scale_y = hei;
		pass->output = NULL;
	}
	else
	{
		pass->output = texture_new_2D((int)wid, (int)hei, 32, GL_RGBA16F, 1, 0);
	}
	
	pass->binds_size = 0;
	for(i = 0;;i++)
	{
		bind_t *bind = &binds[i];
		if(bind->type == BIND_NONE) break;
		pass->binds[pass->binds_size++] = *bind;
	}
	self->ready = 0;
}

void c_renderer_clear_shaders(c_renderer_t *self, shader_t *shader)
{
	int i;
	for(i = 0; i < self->passes_size; i++)
	{
		shader_destroy(self->passes[i].shader);
	}
	self->passes_size = 0;
}

int c_renderer_update_gbuffers(c_renderer_t *self, entity_t camera)
{
	int i;
	int res = 1;
	for(i = 0; i < self->gbuffers_size; i++)
	{
		gbuffer_t *gb = &self->gbuffers[i];
		if(!gb->buffer) continue;

		self->bound = gb->buffer;
		texture_target(gb->buffer, 0);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); glerr();

		shader_bind(self->gbuffer_shader);

		c_camera_t *cam = c_camera(&camera);
#ifdef MESH4
		shader_bind_camera(self->gbuffer_shader, cam->pos, &cam->view_matrix,
				&cam->projection_matrix, cam->exposure, self->angle4);
#else
		shader_bind_camera(self->gbuffer_shader, cam->pos, &cam->view_matrix,
				&cam->projection_matrix, cam->exposure);
#endif

		glEnable(GL_CULL_FACE); glerr();
		glEnable(GL_DEPTH_TEST); glerr();

		entity_signal(c_entity(self), gb->draw_filter, self->gbuffer_shader);

		glDisable(GL_CULL_FACE); glerr();
		glDisable(GL_DEPTH_TEST); glerr();

	}

	return res;
}

void c_renderer_draw_to_texture(c_renderer_t *self, shader_t *shader,
		texture_t *screen, float scale, texture_t *buffer)
{
	shader = shader ? shader : self->quad_shader;
	shader_bind(shader);

	texture_target_sub(screen, buffer->width * scale, buffer->height * scale, 0);

	shader_bind_screen(shader, buffer, 1, 1);

	c_mesh_gl_draw(c_mesh_gl(&self->quad), NULL, 0);
}

int c_renderer_draw(c_renderer_t *self)
{
	uint i;
	if(!self->ready)
	{
		c_renderer_bind_passes(self);
	}

	/* if(!self->perlin_size) */
	/* { */
		/* self->perlin_size = 13; */
		/* SDL_CreateThread((int(*)(void*))init_perlin, "perlin", self); */
	/* } */

	if(self->camera == entity_null) return 1;
	if(!self->gbuffer_shader) return 1;

	c_camera_update_view(c_camera(&self->camera));

	c_renderer_update_probes(self);

	glerr();

	c_renderer_update_gbuffers(self, self->camera);


	/* -------------- */

	/* DRAW WITH VISUAL SHADER */

	texture_t *output = NULL;

	for(i = 0; i < self->passes_size; i++)
	{
		output = c_renderer_draw_pass(self, &self->passes[i], self->camera);
	}

	/* ----------------------- */

	if(!output) return 1;

	filter_bloom(self, bloom_scale * self->resolution, output);


	/* ---------- */

	/* UPDATE SCREEN */

	c_window_rect(c_window(self), 0, 0, self->width, self->height,
			output);

	/* ------------- */

	return 1;
}

