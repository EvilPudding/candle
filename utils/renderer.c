#include "renderer.h"
#include <components/camera.h>
#include <components/model.h>
#include <components/spacial.h>
#include <components/light.h>
#include <components/ambient.h>
#include <components/node.h>
#include <components/name.h>
#include <systems/window.h>
#include <systems/render_device.h>
#include <systems/editmode.h>
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
			glUniformHandleui64ARB(sb->buffer.u_tex[t], texture_handle(buffer, t)); glerr();
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
		case NONE:
			printf("Empty bind??\n");
			break;
		case TEX:
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
		case NONE: printf("error\n"); break;
		case TEX:
			if(!pass_bind_buffer(pass, bind, shader)) return;
			break;
		case NUM:
			if(bind->getter)
			{
				bind->number = ((number_getter)bind->getter)(bind->usrptr);
			}
			glUniform1f(sb->number.u, bind->number); glerr();
			glerr();
			break;
		case INT:
			if(bind->getter)
			{
				bind->integer = ((integer_getter)bind->getter)(bind->usrptr);
			}
			glUniform1i(sb->integer.u, bind->integer); glerr();
			glerr();
			break;
		case VEC2:
			if(bind->getter)
			{
				bind->vec2 = ((vec2_getter)bind->getter)(bind->usrptr);
			}
			glUniform2f(sb->vec2.u, bind->vec2.x, bind->vec2.y);
			glerr();
			break;
		case VEC3:
			if(bind->getter)
			{
				bind->vec3 = ((vec3_getter)bind->getter)(bind->usrptr);
			}
			glUniform3f(sb->vec3.u, _vec3(bind->vec3));
			glerr();
			break;
		case VEC4:
			if(bind->getter)
			{
				bind->vec4 = ((vec4_getter)bind->getter)(bind->usrptr);
			}
			glUniform4f(sb->vec4.u, _vec4(bind->vec4));
			glerr();
			break;
		default:
			printf("error\n");
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

void renderer_set_output(renderer_t *self, texture_t *tex)
{
	if(tex->target == GL_TEXTURE_CUBE_MAP)
	{
		self->fov = M_PI / 2.0f;
	}
	self->output = tex;
	self->ready = 0;
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

void renderer_set_model(renderer_t *self, int32_t camid, mat4_t *model)
{
	vec3_t pos = mat4_mul_vec4(*model, vec4(0.0f, 0.0f, 0.0f, 1.0f)).xyz;
	self->glvars[camid].pos = pos;
	self->glvars[camid].model = *model;
	self->glvars[camid].inv_model = mat4_invert(*model);

	if(self->output && self->output->target == GL_TEXTURE_CUBE_MAP)
	{
		int32_t i;
		vec3_t p1[6] = {
			vec3(1.0, 0.0, 0.0), vec3(-1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0),
			vec3(0.0, -1.0, 0.0), vec3(0.0, 0.0, 1.0), vec3(0.0, 0.0, -1.0)};

		vec3_t up[6] = {
			vec3(0.0,-1.0,0.0), vec3(0.0, -1.0, 0.0), vec3(0.0, 0.0, 1.0),
			vec3(0.0, 0.0, -1.0), vec3(0.0, -1.0, 0.0), vec3(0.0, -1.0, 0.0)};

		for(i = 0; i < 6; i++)
		{
			struct gl_camera *var = &self->glvars[i];

			var->inv_model = mat4_look_at(pos, vec3_add(pos, p1[i]), up[i]);
			var->model = mat4_invert(var->inv_model);
			var->pos = pos;
		}
	}
}

void renderer_default_pipeline(renderer_t *self)
{
	texture_t *gbuffer =	texture_new_2D(0, 0, 0, 0,
		buffer_new("nmr",		1, 4),
		buffer_new("albedo",	1, 4),
		buffer_new("depth",		1, -1)
	);

	texture_t *ssao =		texture_new_2D(0, 0, 0,
		buffer_new("occlusion",	1, 1)
	);
	texture_t *rendered =	texture_new_2D(0, 0, 0,
		buffer_new("color",	1, 4)
	);
	texture_t *refr =		texture_new_2D(0, 0, TEX_MIPMAP,
		buffer_new("color",	1, 4)
	);
	texture_t *final =		texture_new_2D(0, 0, TEX_INTERPOLATE,
		buffer_new("color",	1, 4)
	);
	final->track_brightness = 1;
	/* texture_t *bloom =		texture_new_2D(0, 0, TEX_INTERPOLATE, */
		/* buffer_new("color",	1, 4) */
	/* ); */
	/* texture_t *bloom2 =		texture_new_2D(0, 0, TEX_INTERPOLATE, */
		/* buffer_new("color",	1, 4) */
	/* ); */
	texture_t *tmp =		texture_new_2D(0, 0, TEX_INTERPOLATE,
		buffer_new("color",	1, 4)
	);
	texture_t *selectable =	texture_new_2D(0, 0, 0,
		buffer_new("geomid",	1, 2),
		buffer_new("id",		1, 2),
		buffer_new("depth",		1, -1)
	);

	renderer_add_tex(self, "gbuffer",		1.0f, gbuffer);
	renderer_add_tex(self, "ssao",			1.0f, ssao);
	renderer_add_tex(self, "rendered",		1.0f, rendered);
	renderer_add_tex(self, "refr",			1.0f, refr);
	renderer_add_tex(self, "final",			1.0f, final);
	renderer_add_tex(self, "selectable",	1.0f, selectable);
	renderer_add_tex(self, "tmp",			1.0f, tmp);

	/* renderer_add_tex(self, "bloom",		0.3f, bloom); */
	/* renderer_add_tex(self, "bloom2",		0.3f, bloom2); */

	renderer_add_pass(self, "gbuffer", "gbuffer", ref("visible"), 0, gbuffer,
			gbuffer,
		(bind_t[]){
			{CLEAR_DEPTH, .number = 1.0f},
			{CLEAR_COLOR, .vec4 = vec4(0.0f)},
			{NONE}
		}
	);

	renderer_add_pass(self, "selectable", "select", ref("selectable"),
			0, selectable, selectable,
		(bind_t[]){
			{CLEAR_DEPTH, .number = 1.0f},
			{CLEAR_COLOR, .vec4 = vec4(0.0f)},
			{NONE}
		}
	);

	/* DECAL PASS */
	renderer_add_pass(self, "decals_pass", "decals", ref("decals"),
			DEPTH_LOCK | DEPTH_EQUAL | DEPTH_GREATER,
			gbuffer, gbuffer,
		(bind_t[]){
			{TEX, "gbuffer", .buffer = gbuffer},
			{NONE}
		}
	);

	renderer_add_pass(self, "ssao_pass", "ssao", ref("quad"), 0,
			renderer_tex(self, ref("ssao")), NULL,
		(bind_t[]){
			{TEX, "gbuffer", .buffer = gbuffer},
			{NONE}
		}
	);


	renderer_add_pass(self, "ambient_light_pass", "phong", ref("ambient"),
			ADD, rendered, NULL,
		(bind_t[]){
			{CLEAR_COLOR, .vec4 = vec4(0.0f)},
			{TEX, "gbuffer", .buffer = gbuffer},
			{NONE}
		}
	);

	renderer_add_pass(self, "render_pass", "phong", ref("light"),
			DEPTH_LOCK | ADD | DEPTH_EQUAL | DEPTH_GREATER, rendered, gbuffer,
		(bind_t[]){
			{TEX, "gbuffer", .buffer = gbuffer},
			{NONE}
		}
	);


	renderer_add_pass(self, "refraction", "copy", ref("quad"), 0,
			refr, NULL,
		(bind_t[]){
			{TEX, "buf", .buffer = rendered},
			{NONE}
		}
	);

	renderer_add_pass(self, "transp", "transparency", ref("transparent"),
			DEPTH_EQUAL, rendered, gbuffer,
		(bind_t[]){
			{TEX, "refr", .buffer = refr},
			{NONE}
		}
	);

	renderer_add_pass(self, "transp_1", "gbuffer", ref("transparent"),
			DEPTH_EQUAL, gbuffer, gbuffer, (bind_t[]){ {NONE} });


	/* renderer_tex(self, ref(rendered))->mipmaped = 1; */
	renderer_add_pass(self, "final", "ssr", ref("quad"), 0, final, NULL,
		(bind_t[]){
			{CLEAR_COLOR, .vec4 = vec4(0.0f)},
			{TEX, "gbuffer", .buffer = gbuffer},
			{TEX, "rendered", .buffer = rendered},
			{TEX, "ssao", .buffer = ssao},
			{NONE}
		}
	);

	/* renderer_add_pass(self, "bloom_%d", "bright", ref("quad"), 0, */
	/* 		renderer_tex(self, ref("bloom")), NULL, */
	/* 	(bind_t[]){ */
	/* 		{TEX, "buf", .buffer = renderer_tex(self, ref("final"))}, */
	/* 		{NONE} */
	/* 	} */
	/* ); */
	/* int i; */
	/* for(i = 0; i < 2; i++) */
	/* { */
	/* 	renderer_add_pass(self, "bloom_%d", "blur", ref("quad"), 0, */
	/* 			renderer_tex(self, ref("bloom2")), NULL */
	/* 		(bind_t[]){ */
	/* 			{TEX, "buf", .buffer = renderer_tex(self, ref("bloom"))}, */
	/* 			{INT, "horizontal", .integer = 1}, */
	/* 			{NONE} */
	/* 		} */
	/* 	); */
	/* 	renderer_add_pass(self, "bloom_%d", "blur", ref("quad"), 0, */
	/* 			renderer_tex(self, ref("bloom")), NULL, */
	/* 		(bind_t[]){ */
	/* 			{TEX, "buf", .buffer = renderer_tex(self, ref("bloom2"))}, */
	/* 			{INT, "horizontal", .integer = 0}, */
	/* 			{NONE} */
	/* 		} */
	/* 	); */
	/* } */
	/* renderer_add_pass(self, "bloom_%d", "copy", ref("quad"), ADD, */
	/* 		renderer_tex(self, ref("final")), NULL, */
	/* 	(bind_t[]){ */
	/* 		{TEX, "buf", .buffer = renderer_tex(self, ref("bloom"))}, */
	/* 		{NONE} */
	/* 	} */
	/* ); */

	/* renderer_tex(self, ref(rendered))->mipmaped = 1; */

	self->output = renderer_tex(self, ref("final"));
}

void renderer_update_projection(renderer_t *self)
{
	self->glvars[0].projection = mat4_perspective(
			self->fov,
			((float)self->width) / self->height,
			self->near, self->far
	);
	self->glvars[0].inv_projection = mat4_invert(self->glvars[0].projection); 

	uint32_t f;
	for(f = 1; f < 6; f++)
	{
		self->glvars[f].projection = self->glvars[0].projection;
		self->glvars[f].inv_projection = self->glvars[0].inv_projection;
		self->glvars[f].pos = self->glvars[0].pos;
	}
}

vec3_t renderer_real_pos(renderer_t *self, float depth, vec2_t coord)
{
	/* float z = depth; */
	if(depth < 0.01f) depth *= 100.0f;
    float z = depth * 2.0 - 1.0;
	coord = vec2_sub_number(vec2_scale(coord, 2.0f), 1.0);

    vec4_t clipSpacePosition = vec4(_vec2(coord), z, 1.0);
	vec4_t viewSpacePosition = mat4_mul_vec4(self->glvars[0].inv_projection,
			clipSpacePosition);

    // Perspective division
    viewSpacePosition = vec4_div_number(viewSpacePosition, viewSpacePosition.w);

    vec4_t worldSpacePosition = mat4_mul_vec4(self->glvars[0].model, viewSpacePosition);

    return worldSpacePosition.xyz;
}

vec3_t renderer_screen_pos(renderer_t *self, vec3_t pos)
{
	mat4_t VP = mat4_mul(self->glvars[0].projection,
			self->glvars[0].inv_model); 

	vec4_t viewSpacePosition = mat4_mul_vec4(VP, vec4(_vec3(pos), 1.0f));
	viewSpacePosition = vec4_div_number(viewSpacePosition, viewSpacePosition.w);

	viewSpacePosition.xyz = vec3_scale(vec3_add_number(viewSpacePosition.xyz, 1.0f), 0.5);
    return viewSpacePosition.xyz;
}

static int renderer_update_screen_texture(renderer_t *self)
{
	int w = self->width * self->resolution;
	int h = self->height * self->resolution;

	if(self->output)
	{
		renderer_update_projection(self);
	}

	int i;
	for(i = 0; i < self->outputs_num; i++)
	{
		pass_output_t *output = &self->outputs[i];

		if(output->resolution && output->buffer->target == GL_TEXTURE_2D)
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
	self->ready = 0;
	return CONTINUE;
}


static texture_t *renderer_draw_pass(renderer_t *self, pass_t *pass)
{
	if(!pass->active) return NULL;
	if(pass->shader_name && !pass->shader) pass->shader = fs_new(pass->shader_name);

	c_render_device_rebind(c_render_device(&SYS), (void*)bind_pass, pass);

	if(pass->shader)
	{
		fs_bind(pass->shader);
	}

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
	if(pass->cull)
	{
		glEnable(GL_CULL_FACE); glerr();
	}
	else
	{
		glDisable(GL_CULL_FACE); glerr();
	}
	if(pass->depth)
	{
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(pass->depth_func);
	}
	else
	{
		glDisable(GL_DEPTH_TEST); glerr();
	}

	glDepthMask(pass->depth_update); glerr();

	texture_t *depth = pass->depth ? pass->depth : self->fallback_depth;

	if(pass->clear)
	{
		glClearColor(_vec4(pass->clear_color));
		glClearDepth(pass->clear_depth);
	}

	if(pass->output->target == GL_TEXTURE_CUBE_MAP)
	{
		uint32_t f;
		for(f = 0; f < 6; f++)
		{
			{
				glBindBuffer(GL_UNIFORM_BUFFER, self->ubo); glerr();
				void *p = glMapBuffer(GL_UNIFORM_BUFFER, GL_WRITE_ONLY);
				memcpy(p, &self->glvars[f], sizeof(self->glvars[f]));
				glUnmapBuffer(GL_UNIFORM_BUFFER); glerr();
			}

			texture_target(pass->output, depth, f);

			if(pass->clear) glClear(pass->clear);

			draw_group(pass->draw_signal);
		}
	}
	else
	{
		if(pass->camid != self->camera_bound)
		{
			glBindBuffer(GL_UNIFORM_BUFFER, self->ubo); glerr();
			void *p = glMapBuffer(GL_UNIFORM_BUFFER, GL_WRITE_ONLY);
			memcpy(p, &self->glvars[pass->camid],
					sizeof(self->glvars[pass->camid]));
			glUnmapBuffer(GL_UNIFORM_BUFFER); glerr();
			self->camera_bound = pass->camid;
		}
		texture_target(pass->output, depth, 0);

		if(pass->clear) glClear(pass->clear);

		draw_group(pass->draw_signal);
	}

	glDisable(GL_DEPTH_TEST);

	glDisable(GL_BLEND);

	if(pass->output->mipmaped)
	{
		texture_bind(pass->output, 0);
		glGenerateMipmap(pass->output->target); glerr();
		if(pass->output->track_brightness && self->frame % 20 == 0)
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
	self->ready = 0;
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

renderer_t *renderer_new(float resolution)
{
	renderer_t *self = calloc(1, sizeof(*self));

	self->near = 0.1f;
	self->far = 100.0f;

	self->fallback_depth =	texture_new_2D(1, 1, 0, 0,
		buffer_new("depth",	1, -1)
	);
	renderer_add_tex(self, "fallback_depth", 1.0f, self->fallback_depth);

	self->resolution = resolution;

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


int renderer_component_menu(renderer_t *self, void *ctx)
{
	nk_layout_row_dynamic(ctx, 0, 1);
	int i;
	if(nk_button_label(ctx, "Fullscreen"))
	{
		c_window_toggle_fullscreen(c_window(&SYS));
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
				c_editmode_open_texture(c_editmode(&SYS), output->buffer);
			}
		}
	}
	return CONTINUE;
}



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

void renderer_add_pass(
		renderer_t *self,
		const char *name,
		const char *shader_name,
		uint32_t draw_signal,
		enum pass_options flags,
		texture_t *output,
		texture_t *depth,
		bind_t binds[])
{
	if(!output) exit(1);
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
		printf("Replacing %s\n", name);
	}

	pass_t *pass = &self->passes[i];
	pass->hash = hash;

	if(shader_name)
	{
		strncpy(pass->shader_name, shader_name, sizeof(pass->shader_name));
	}
	pass->clear = 0;

	pass->depth_update =	!(flags & DEPTH_LOCK);
	pass->depth = NULL;

	pass->output = output;
	pass->depth = depth;

	if(flags & DEPTH_DISABLE)
	{
		pass->depth_func = GL_ALWAYS;
	}
	else if(!(flags & DEPTH_EQUAL))
	{
		if(flags & DEPTH_GREATER)
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
		if(flags & DEPTH_GREATER)
		{
			pass->depth_func = GL_GEQUAL;
		}
		else
		{
			pass->depth_func = GL_LEQUAL;
		}
	}

	pass->draw_signal = draw_signal;
	pass->additive = flags & ADD;
	pass->multiply = flags & MUL;
	pass->cull = !(flags & CULL_DISABLE);
	pass->clear_depth = 1.0f;
	strncpy(pass->name, buffer, sizeof(pass->name));

	int bind_count;
	for(bind_count = 0; binds[bind_count].type != NONE; bind_count++);

	pass->binds = malloc(sizeof(bind_t) * bind_count);
	int j;
	pass->binds_size = 0;
	for(i = 0; i < bind_count; i++)
	{
		if(binds[i].type == CAM)
		{
			pass->camid = binds[i].integer;
			continue;
		}
		if(binds[i].type == CLEAR_COLOR)
		{
			pass->clear |= GL_COLOR_BUFFER_BIT;
			pass->clear_color = binds[i].vec4;
			continue;
		}
		if(binds[i].type == CLEAR_DEPTH)
		{
			pass->clear |= GL_DEPTH_BUFFER_BIT;
			pass->clear_depth = binds[i].number;
			continue;
		}

		bind_t *bind = &pass->binds[pass->binds_size++];
		*bind = binds[i];

		for(j = 0; j < 16; j++)
		{
			shader_bind_t *sb = &bind->vs_uniforms[j];

			sb->cached = 0;
			int t;
			for(t = 0; t < 16; t++)
			{
				sb->buffer.u_tex[t] = -1;
			}
			bind->hash = ref(bind->name);
		}
	}
	pass->binds = realloc(pass->binds, sizeof(bind_t) * pass->binds_size);
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
		glBufferData(GL_UNIFORM_BUFFER, sizeof(self->glvars[0]), &self->glvars[0],
				GL_DYNAMIC_DRAW); glerr();
		glBindBufferBase(GL_UNIFORM_BUFFER, 19, self->ubo); glerr();
	}
}

int renderer_draw(renderer_t *self)
{
	glerr();
	uint i;

	if(!self->width || !self->height) return CONTINUE;

	if(!self->output) renderer_default_pipeline(self);

	self->frame++;

	if(!self->ready) renderer_update_screen_texture(self);

	renderer_update_ubo(self);

	glBindBufferBase(GL_UNIFORM_BUFFER, 19, self->ubo);

	for(i = 0; i < self->passes_size; i++)
	{
		renderer_draw_pass(self, &self->passes[i]);
	}
	/* TODO ubo binding should only happen when camera changes */
	self->camera_bound = -1;

	c_render_device_rebind(c_render_device(&SYS), NULL, NULL);

	glerr();
	return CONTINUE;
}

