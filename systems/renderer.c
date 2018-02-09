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
#define bloom_scale 0.5f

unsigned long ct_renderer;

unsigned long world_changed;

static void c_renderer_add_gbuffer(c_renderer_t *self, const char *name,
		unsigned int draw_filter);

static int c_renderer_probe_update_map(c_renderer_t *self, ecm_t *ecm,
		entity_t probe, shader_t *shader);
static void c_renderer_update_probes(c_renderer_t *self, ecm_t *ecm);

static texture_t *c_renderer_draw_pass(c_renderer_t *self, pass_t *pass, entity_t camera);

static int c_renderer_resize(c_renderer_t *self, window_resize_data *event);

static void c_renderer_update_screen_texture(c_renderer_t *self);

int c_renderer_scene_changed(c_renderer_t *self, entity_t *entity)
{
	if(!entity)
	{
		self->probe_update_id++;
		return 1;
	}
	c_model_t *model = c_model(*entity);
	/* c_node_t *node = c_node(*entity); */
	/* if(model && model->cast_shadow) */
	if(model)
			/* || (node && node->children_size) ) */
	{
		self->probe_update_id++;
	}
	return 1;
}

/* static void init_context_a(c_renderer_t *self) */
/* { */
/* 	SDL_CreateWindowAndRenderer(self->width, self->height, SDL_WINDOW_OPENGL, */
/* 			&self->window, &self->display); */
/* 	mainWindow = self->window; */
	/* glInit(); */
/* } */

static int c_renderer_bind_passes(c_renderer_t *self)
{
	int i, j;

	for(i = 1; i < self->passes_size; i++)
	{
		pass_t *pass = &self->passes[i];
		pass->binds_size = 0;
		for(j = i - 1; j >= 0; j--)
		{
			pass_t *pass_done = &self->passes[j];

			char buffer[256];

			snprintf(buffer, sizeof(buffer), "%s.texture",
					pass_done->feed_name);

			GLuint u_texture = glGetUniformLocation(pass->shader->program,
					buffer); glerr();

			if(u_texture >= 4294967295) continue;

			snprintf(buffer, sizeof(buffer), "%s.brightness",
					pass_done->feed_name);

			GLuint u_brightness = glGetUniformLocation(pass->shader->program,
					buffer); glerr();

			pass_bind_t *bind = &pass->binds[pass->binds_size++];
			bind->u_texture = u_texture;
			bind->u_brightness = u_brightness;
			bind->pass = pass_done;
		}
	}
	printf("PASSES BOUND\n");
	self->ready = 1;
	return 1;
}

void c_renderer_get_pixel(c_renderer_t *self, int gbuffer, int buffer,
		int x, int y)
{
	texture_get_pixel(self->gbuffers[gbuffer].buffer, buffer, x, y);
}

static void c_renderer_init(c_renderer_t *self)
{
	self->super = component_new(ct_renderer);

	self->gbuffer_shader = shader_new("gbuffer");
	self->quad_shader = shader_new("quad");

	filter_blur_init();
	filter_bloom_init();

	c_renderer_add_gbuffer(self, "opaque", 0);
	c_renderer_add_gbuffer(self, "transp", 1);

	c_renderer_add_pass(self, 0, 1, 1.0f, 1.0f, 0, 0,
			"ssao", "ssao", "opaque");

	c_renderer_add_pass(self, 1, 1, 1.0f, 1.0f, 0, 0,
			"phong", "phong", "opaque");

	c_renderer_add_pass(self, 0, 1, 1.0f, 1.0f, self->roughness, 0,
			"transp", "transparency", "transp");

	c_renderer_add_pass(self, 0, 1, 1.0f, 1.0f, 0, self->auto_exposure,
			"final", "ssr", "opaque");

	self->quad = entity_new(candle->ecm, 2,
			c_name_new("renderer_quad"),
			c_model_new(mesh_quad(), 0));
	c_model(self->quad)->visible = 0;


	/* entity_t ambient = entity_new(c_ecm(self), 1, c_ambient_new(64)); */
	/* c_spacial_set_pos(c_spacial(ambient), vec3(0, 1, 0)); */
	/* c_renderer_debug_gbuffer(self); */
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
										   /* COLOR_TEX + 0 DIFFUSE	*/
		texture_add_buffer(gb->buffer, 1, 1, 0); /* COLOR_TEX + 1 SPECULAR */
		texture_add_buffer(gb->buffer, 1, 1, 0); /* COLOR_TEX + 2 REFLECTION */
		texture_add_buffer(gb->buffer, 1, 0, 0); /* COLOR_TEX + 3 NORMAL	*/
		texture_add_buffer(gb->buffer, 1, 1, 0); /* COLOR_TEX + 4 TRANSPARENCY	*/
		texture_add_buffer(gb->buffer, 1, 0, 0); /* COLOR_TEX + 5 c_POSITION */
		texture_add_buffer(gb->buffer, 1, 0, 0); /* COLOR_TEX + 6 w_POSITION */
		texture_add_buffer(gb->buffer, 1, 0, 0); /* COLOR_TEX + 7 ID */

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

	c_renderer_update_screen_texture(self);

	return 1;
}

static texture_t *c_renderer_draw_pass(c_renderer_t *self, pass_t *pass, entity_t camera)
{
	unsigned long i;

	if(!pass->output) return NULL;

	self->bound = pass->output;
	texture_target(pass->output, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); glerr();

	shader_bind(pass->shader);

	c_camera_t *cam = c_camera(camera);
#ifdef MESH4
	shader_bind_camera(pass->shader, cam->pos, &cam->view_matrix,
			&cam->projection_matrix, cam->exposure, cam->angle4);
#else
	shader_bind_camera(pass->shader, cam->pos, &cam->view_matrix,
			&cam->projection_matrix, cam->exposure);
#endif

	if(pass->gbuf_id >= 0)
	{
		shader_bind_gbuffer(pass->shader,
				self->gbuffers[pass->gbuf_id].buffer);
	}

	ct_t *ambients = ecm_get(c_ecm(self), ct_ambient);
	c_ambient_t *ambient = (c_ambient_t*)ct_get_at(ambients, 0);

	if(ambient)
	{
		c_probe_t *probe = c_probe(c_entity(ambient));
		if(probe) shader_bind_ambient(pass->shader, probe->map);
	}


	for(i = 0; i < pass->binds_size; i++)
	{
		pass_bind_t *bind = &pass->binds[i];

		glUniform1i(bind->u_texture, 13 + i); glerr();
		glActiveTexture(GL_TEXTURE0 + 13 + i); glerr();
		texture_bind(bind->pass->output, -1);

		if(bind->pass->record_brightness)
		{
			float brightness = bind->pass->output->brightness;
			glUniform1f(bind->u_brightness, brightness);
		}

	}

	c_mesh_gl_t *quad = c_mesh_gl(self->quad);
	if(pass->for_each_light)
	{
		ct_t *lights = ecm_get(c_ecm(self), ct_light);
		c_light_t *light;

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		for(i = 0; i < lights->components_size; i++)
		{
			light = (c_light_t*)ct_get_at(lights, i);

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
		float percent_of_screen)
{
	c_renderer_t *self = calloc(1, sizeof *self);

	self->resolution = resolution;
	self->auto_exposure = auto_exposure;
	self->roughness = roughness;
	self->percent_of_screen = percent_of_screen;

	c_renderer_init(self);

	return self;
}

entity_t c_renderer_entity_at_pixel(c_renderer_t *self, int x, int y)
{
	entity_t result;
	result.id = texture_get_pixel(self->gbuffers[0].buffer, 7, x, y);

	result.ecm = c_ecm(self);

	return result;
}

int c_renderer_mouse_release(c_renderer_t *self, mouse_button_data *event)
{
	if(event->button == SDL_BUTTON_LEFT)
	{
		entity_t result = c_renderer_entity_at_pixel(self, event->x, event->y);
		/* TODO entity 0 should be valid */
		if(result.id != 0)
		{
			if(c_name(result))
			{
				printf("released entity %s\n", c_name(result)->name);
			}
			else
			{
				printf("released entity %ld\n", result.id);
			}
			/* c_spacial(result)-> */
			/* entity_signal_same(result, model_release, NULL); */
		}
	}
	return 1;
}


void c_renderer_register(ecm_t *ecm)
{
	ct_t *ct = ecm_register(ecm, &ct_renderer, sizeof(c_renderer_t),
			(init_cb)c_renderer_init, 1, ct_window);

	ct_register_listener(ct, WORLD, spacial_changed,
			(signal_cb)c_renderer_scene_changed);

	ct_register_listener(ct, WORLD, window_resize,
			(signal_cb)c_renderer_resize);

	ct_register_listener(ct, WORLD, world_draw,
			(signal_cb)c_renderer_draw);

	/* ct_register_listener(ct, WORLD, mouse_press, */
			/* (signal_cb)c_renderer_mouse_press); */

	ct_register_listener(ct, WORLD, mouse_release,
			(signal_cb)c_renderer_mouse_release);

}



static int c_renderer_probe_update_map(c_renderer_t *self, ecm_t *ecm,
		entity_t probe, shader_t *shader)
{
	unsigned long i;
	int face;

	/* c_probe_t *lc = c_probe(probe); */
	c_probe_t *pp = c_probe(probe);
	c_spacial_t *ps = c_spacial(probe);

	for(face = 0; face < 6; face++)
	{
		self->bound = pp->map;
		texture_target(pp->map, face);
		glClear(GL_DEPTH_BUFFER_BIT);

		shader_bind_probe(shader, probe);
#ifdef MESH4
		shader_bind_camera(shader, ps->position, &pp->views[face],
				&pp->projection, 1.0f, 0.0f);
#else
		shader_bind_camera(shader, ps->position, &pp->views[face],
				&pp->projection, 1.0f);
#endif

		ct_t *models = ecm_get(ecm, ct_model);
		int res = 1;

		for(i = 0; i < models->components_size; i++)
		{
			c_model_t *model = (c_model_t*)ct_get_at(models, i);

			res |= c_model_render(model, 0, shader);
		}
		if(!res) return 0;
	}
	return 1;
}

static void c_renderer_update_probes(c_renderer_t *self, ecm_t *ecm)
{
	c_probe_t *probe;
	unsigned long i;
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
	glerr();
	/* c_renderer_target(self); */

	ct_t *probes = ecm_get(ecm, ct_probe);

	glEnable(GL_CULL_FACE); glerr();
	glEnable(GL_DEPTH_TEST); glerr();

	for(i = 0; i < probes->components_size; i++)
	{
		probe = (c_probe_t*)ct_get_at(probes, i);
		entity_t entity = probe->super.entity;

		if(probe->before_draw && !probe->before_draw((c_t*)probe)) continue;

		shader_bind(probe->shader);

		if(probe->last_update != self->probe_update_id)
		{
			if(c_renderer_probe_update_map(self, ecm, entity, probe->shader))
			{
				probe->last_update = self->probe_update_id;
			}
		}
	}
	glerr();

	glDisable(GL_CULL_FACE); glerr();
	glDisable(GL_DEPTH_TEST); glerr();

}

int c_renderer_gbuffer(c_renderer_t *self, const char *name)
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

void c_renderer_add_pass(c_renderer_t *self, int for_each_light,
		int screen_scale, float wid, float hei, int mipmaped, int record_brightness,
		const char *feed_name, const char *shader_name, const char *gbuffer_name)
{
	shader_t *shader = shader_new(shader_name);
	int i = self->passes_size++;
	pass_t *pass = &self->passes[i];
	pass->shader = shader;
	pass->for_each_light = for_each_light;
	pass->mipmaped = mipmaped;
	pass->record_brightness = record_brightness;
	strncpy(pass->feed_name, feed_name, sizeof(pass->feed_name));
	pass->screen_scale = screen_scale;
	pass->gbuf_id = c_renderer_gbuffer(self, gbuffer_name);
	if(screen_scale)
	{
		pass->screen_scale_x = wid;
		pass->screen_scale_y = hei;
		pass->output = NULL;
	}
	else
	{
		pass->output = texture_new_2D((int)wid, (int)hei, 32, GL_RGBA16F, 1, 0);
	}
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

		c_camera_t *cam = c_camera(camera);
#ifdef MESH4
		shader_bind_camera(self->gbuffer_shader, cam->pos, &cam->view_matrix,
				&cam->projection_matrix, cam->exposure, cam->angle4);
#else
		shader_bind_camera(self->gbuffer_shader, cam->pos, &cam->view_matrix,
				&cam->projection_matrix, cam->exposure);
#endif

		glEnable(GL_CULL_FACE); glerr();
		glEnable(GL_DEPTH_TEST); glerr();

		ct_t *models = ecm_get(c_ecm(self), ct_model);
		for(i = 0; i < models->components_size; i++)
		{
			c_model_t *model = (c_model_t*)ct_get_at(models, i);
			res |= c_model_render(model, gb->draw_filter, self->gbuffer_shader);
		}

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

	c_mesh_gl_draw(c_mesh_gl(self->quad), NULL, 0);
}

int c_renderer_draw(c_renderer_t *self)
{
	unsigned long i;
	if(!self->ready)
	{
		c_renderer_bind_passes(self);
	}

	entity_t camera = ecm_get_camera(c_ecm(self));
	if(!self->perlin_size)
	{
		self->perlin_size = 13;
		SDL_CreateThread((int(*)(void*))init_perlin, "perlin", self);
	}

	if(entity_is_null(camera)) return 0;
	if(!self->gbuffer_shader) return 0;

	c_camera_update_view(c_camera(camera));

	c_renderer_update_probes(self, c_ecm(self));

	glerr();

	c_renderer_update_gbuffers(self, camera);


	/* -------------- */

	/* DRAW WITH VISUAL SHADER */

	texture_t *output = NULL;

	for(i = 0; i < self->passes_size; i++)
	{
		output = c_renderer_draw_pass(self, &self->passes[i], camera);
	}

	/* ----------------------- */

	if(!output) return 1;

	filter_bloom(self, bloom_scale * self->resolution, output);


	/* ---------- */

	/* UPDATE SCREEN */

	c_window_rect(c_window(c_entity(self)), 0, 0, self->width, self->height,
			output);

	/* ------------- */

	return 1;
}

