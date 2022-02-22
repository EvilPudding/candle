#include "renderer.h"
#include "../components/camera.h"
#include "../components/model.h"
#include "../components/light.h"
#include "../components/ambient.h"
#include "../components/node.h"
#include "../components/name.h"
#include "../systems/window.h"
#include "../systems/render_device.h"
#include "../systems/editmode.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../candle.h"
#include "../utils/noise.h"
#include "../utils/nk.h"
#include "../utils/material.h"

#ifdef __WIN32
#  ifndef _GLFW_WIN32 
#    define _GLFW_WIN32 
#  endif
#else
#  ifndef _GLFW_WIN32 
#    define _GLFW_X11
#  endif
#endif
#include "../third_party/glfw/include/GLFW/glfw3.h"

static int renderer_update_screen_texture(renderer_t *self);

static void bind_pass(pass_t *pass, shader_t *shader);

#define FIRST_TEX 8
static void pass_unbind_textures(pass_t *pass)
{
	uint32_t i;
	for (i = 0; i < pass->bound_textures; i++)
	{
		glActiveTexture(GL_TEXTURE0 + FIRST_TEX + i);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
}

static int pass_bind_buffer(pass_t *pass, bind_t *bind, shader_t *shader)
{
	int t;
	hash_bind_t *sb = &bind->vs_uniforms;
	texture_t *buffer;

	if(bind->getter)
	{
		bind->buffer = ((tex_getter)bind->getter)(pass, pass->usrptr);
	}

	buffer = bind->buffer;


	for(t = 0; t < buffer->bufs_size; t++)
	{
		if(((int)sb->u.buffer.u_tex[t]) != -1)
		{
			if(buffer->bufs[t].ready)
			{
				int i = pass->bound_textures++;
				glActiveTexture(GL_TEXTURE0 + FIRST_TEX + i);
				texture_bind(buffer, t);
				glUniform1i(shader_cached_uniform(shader, sb->u.buffer.u_tex[t]),
				            FIRST_TEX + i);
				glerr();
			}
		}
	}
	return 1;
}

void bind_get_uniforms(bind_t *bind, hash_bind_t *sb, pass_t *pass)
{
	int t;
	switch(bind->type)
	{
		case OPT_NONE:
			printf("Empty bind??\n");
			break;
		case OPT_TEX:
			if(bind->getter)
			{
				bind->buffer = ((tex_getter)bind->getter)(pass, pass->usrptr);
			}
			for(t = 0; t < bind->buffer->bufs_size; t++)
			{
				char buffer[256];
				sprintf(buffer, "%s.%s", bind->name, bind->buffer->bufs[t].name);
				sb->u.buffer.u_tex[t] = ref(buffer);
			}
			break;
		default:
			sb->u.number.u = ref(bind->name);
			break;
	}
	sb->cached = true;

}

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
void glGetBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, GLvoid *data)
{
	EM_ASM_(
	{
		Module.ctx.getBufferSubData(Module.ctx.PIXEL_PACK_BUFFER, 0, HEAPU8.subarray($0, $0 + $1));
	}, data, size);
}
#endif
static void bind_pass(pass_t *pass, shader_t *shader)
{
	/* this function allows null shader for CALLBACK only */
	int i;

	if (!pass) return;

	/* if(self->shader->frame_bind == self->frame) return; */
	/* self->shader->frame_bind = self->frame; */

	if (shader)
	{
		if (!shader->ready)
			return;
		if (pass->output->target != GL_TEXTURE_2D)
		{
			glUniform2f(shader_cached_uniform(shader, ref("screen_size")),
					pass->output->width, pass->output->height);
		}
		else
		{
			uvec2_t size = pass->output->sizes[pass->framebuffer_id];
			glUniform2f(shader_cached_uniform(shader, ref("screen_size")),
					size.x, size.y);
		}
	}

	glerr();
	pass->bound_textures = 0;

	for(i = 0; i < pass->binds_size; i++)
	{
		bind_t *bind = &pass->binds[i];
		hash_bind_t *sb = &bind->vs_uniforms;
		if (!sb->cached && shader)
		{
			bind_get_uniforms(bind, sb, pass);
		}

		switch(bind->type)
		{
		case OPT_NONE: printf("error\n"); break;
		case OPT_TEX:
			if(!pass_bind_buffer(pass, bind, shader)) return;
			break;
		case OPT_NUM:
			if(bind->getter)
			{
				bind->number = ((number_getter)bind->getter)(pass, pass->usrptr);
			}
			glUniform1f(shader_cached_uniform(shader, sb->u.number.u), (GLfloat)bind->number); glerr();
			glerr();
			break;
		case OPT_INT:
			if(bind->getter)
			{
				bind->integer = ((integer_getter)bind->getter)(pass, pass->usrptr);
			}
			glUniform1i(shader_cached_uniform(shader, sb->u.integer.u), (GLint)bind->integer); glerr();
			glerr();
			break;
		case OPT_UINT:
			if(bind->getter)
			{
				bind->uinteger = ((integer_getter)bind->getter)(pass, pass->usrptr);
			}
			glUniform1ui(shader_cached_uniform(shader, sb->u.uinteger.u), (GLuint)bind->uinteger); glerr();
			glerr();
			break;
		case OPT_UVEC2:
			if(bind->getter)
			{
				bind->uvec2 = ((uvec2_getter)bind->getter)(pass, pass->usrptr);
			}
			glUniform2ui(shader_cached_uniform(shader, sb->u.uvec2.u), (GLuint)bind->uvec2.x, (GLuint)bind->uvec2.y);
			glerr();
			break;
		case OPT_IVEC2:
			if(bind->getter)
			{
				bind->ivec2 = ((ivec2_getter)bind->getter)(pass, pass->usrptr);
			}
			glUniform2i(shader_cached_uniform(shader, sb->u.ivec2.u), (GLint)bind->ivec2.x, (GLint)bind->ivec2.y);
			glerr();
			break;
		case OPT_VEC2:
			if(bind->getter)
			{
				bind->vec2 = ((vec2_getter)bind->getter)(pass, pass->usrptr);
			}
			glUniform2f(shader_cached_uniform(shader, sb->u.vec2.u), (GLfloat)bind->vec2.x, (GLfloat)bind->vec2.y);
			glerr();
			break;
		case OPT_VEC3:
			if(bind->getter)
			{
				bind->vec3 = ((vec3_getter)bind->getter)(pass, pass->usrptr);
			}
			glUniform3f(shader_cached_uniform(shader, sb->u.vec3.u), _vec3(bind->vec3));
			glerr();
			break;
		case OPT_VEC4:
			if(bind->getter)
			{
				bind->vec4 = ((vec4_getter)bind->getter)(pass, pass->usrptr);
			}
			glUniform4f(shader_cached_uniform(shader, sb->u.vec4.u), _vec4(bind->vec4));
			glerr();
			break;
		case OPT_CALLBACK:
			bind->getter(pass, pass->usrptr);
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
	pass_output_t *output = &self->outputs[self->outputs_num++];
	output->resolution = resolution;
	output->hash = ref(name);
	output->buffer = buffer;
	strncpy(buffer->name, name, sizeof(buffer->name) - 1);
}

static void update_ubo(renderer_t *self, int32_t camid)
{
	if(!self->ubo_changed[camid]) return;
	self->ubo_changed[camid] = false;
	if(!self->ubos[camid])
	{
		glGenBuffers(1, &self->ubos[camid]); glerr();
		glBindBuffer(GL_UNIFORM_BUFFER, self->ubos[camid]); glerr();
		glBufferData(GL_UNIFORM_BUFFER, sizeof(self->glvars[camid]),
				&self->glvars[camid], GL_DYNAMIC_DRAW); glerr();
	}
	glBindBuffer(GL_UNIFORM_BUFFER, self->ubos[camid]);
	/* void *p = glMapBuffer(GL_UNIFORM_BUFFER, GL_WRITE_ONLY); */
	/* memcpy(p, &self->glvars[camid], sizeof(self->glvars[camid])); */

	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(self->glvars[camid]),
	                &self->glvars[camid]); glerr();

	/* glUnmapBuffer(GL_UNIFORM_BUFFER); glerr(); */
}

void renderer_set_model(renderer_t *self, uint32_t camid, mat4_t *model)
{
	self->render_device_frame = 0;
	if(camid == ~0)
	{
		int32_t i;
		for(i = 0; i < self->camera_count; i++)
		{
			struct gl_camera *var = &self->glvars[i];
			var->model = mat4_mul(*model, self->relative_transform[i]);
			var->inv_model = mat4_invert(var->model);
			var->pos = vec4_xyz(mat4_mul_vec4(var->model, vec4(0.0f, 0.0f, 0.0f, 1.0f)));
			if (self->stored_camera_frame[i] != self->frame)
			{
				self->glvars[i].previous_view = self->glvars[i].inv_model;
				self->stored_camera_frame[i] = self->frame;
			}
			self->ubo_changed[i] = true;
		}
	}
	else
	{
		struct gl_camera *var = &self->glvars[camid];
		var->model = mat4_mul(*model, self->relative_transform[camid]);
		var->pos = vec4_xyz(mat4_mul_vec4(var->model, vec4(0.0f, 0.0f, 0.0f, 1.0f)));
		var->inv_model = mat4_invert(*model);
		if (self->stored_camera_frame[camid] != self->frame)
		{
			var->previous_view = var->inv_model;
			self->stored_camera_frame[camid] = self->frame;
		}
		self->ubo_changed[camid] = true;

	}
}

void renderer_add_kawase(renderer_t *self, texture_t *t1, texture_t *t2,
		int from_mip, int to_mip)
{
	renderer_add_pass(self, "kawase_p",
			to_mip == from_mip ? "copy" : "candle:downsample",
			ref("quad"), 0, t2, NULL, to_mip, ~0, 2,
			opt_tex("buf", t1, NULL),
			opt_int("level", from_mip, NULL)
	);

	renderer_add_pass(self, "kawase_0", "candle:kawase", ref("quad"), 0,
			t1, NULL, to_mip, ~0, 3,
			opt_tex( "buf", t2, NULL),
			opt_int( "distance", 0, NULL),
			opt_int( "level", to_mip, NULL)
	);

	renderer_add_pass(self, "kawase_1", "candle:kawase", ref("quad"), 0,
			t2, NULL, to_mip, ~0, 3,
			opt_tex("buf", t1, NULL),
			opt_int("distance", 1, NULL),
			opt_int("level", to_mip, NULL)
	);

	renderer_add_pass(self, "kawase_2", "candle:kawase", ref("quad"), 0,
			t1, NULL, to_mip, ~0, 3,
			opt_tex("buf", t2, NULL),
			opt_int("distance", 2, NULL),
			opt_int("level", to_mip, NULL)
	);

/* 	renderer_add_pass(self, "kawase_3", "candle:kawase", ref("quad"), 0, */
/* 			t2, NULL, to_mip, ~0, */
/* 		(bind_t[]){ */
/* 			{TEX, "buf", .buffer = t1}, */
/* 			{INT, "distance", .integer = 2}, */
/* 			{INT, "level", .integer = to_mip}, */
/* 			{NONE} */
/* 		} */
/* 	); */

/* 	renderer_add_pass(self, "kawase_4", "candle:kawase", ref("quad"), 0, */
/* 			t1, NULL, to_mip, ~0, */
/* 		(bind_t[]){ */
/* 			{TEX, "buf", .buffer = t2}, */
/* 			{INT, "distance", .integer = 3}, */
/* 			{INT, "level", .integer = to_mip}, */
/* 			{NONE} */
/* 		} */
/* 	); */
}

void *pass_process_query_mips(pass_t *self)
{
	uint32_t size;
	texture_t *tex = self->output;
	bool_t second_stage = true;
	renderer_t *renderer = self->renderer;
	struct{
		uint16_t _[2];
	} *mips[4];

	if (!tex->framebuffer_ready) return NULL;

	size = tex->width * tex->height * 4;
	if (!tex->bufs[1].pbo)
	{
		glGenBuffers(1, &tex->bufs[1].pbo);
		glGenBuffers(1, &tex->bufs[2].pbo);
		glGenBuffers(1, &tex->bufs[3].pbo);
		glGenBuffers(1, &tex->bufs[4].pbo);

		glBindBuffer(GL_PIXEL_PACK_BUFFER, tex->bufs[1].pbo); glerr();
		glBufferData(GL_PIXEL_PACK_BUFFER, size, 0, GL_STREAM_READ);

		glBindBuffer(GL_PIXEL_PACK_BUFFER, tex->bufs[2].pbo); glerr();
		glBufferData(GL_PIXEL_PACK_BUFFER, size, 0, GL_STREAM_READ);

		glBindBuffer(GL_PIXEL_PACK_BUFFER, tex->bufs[3].pbo); glerr();
		glBufferData(GL_PIXEL_PACK_BUFFER, size, 0, GL_STREAM_READ);

		glBindBuffer(GL_PIXEL_PACK_BUFFER, tex->bufs[4].pbo); glerr();
		glBufferData(GL_PIXEL_PACK_BUFFER, size, 0, GL_STREAM_READ);

		glerr();
		second_stage = false;
	}
	if (renderer->mips_buffer_size < size * 4)
	{
		if (renderer->mips)
		{
			free(renderer->mips);
		}
		renderer->mips = malloc(size * 4);
		renderer->mips_buffer_size = size * 4;
	}

	mips[0] = (void*)&renderer->mips[size * 0];
	mips[1] = (void*)&renderer->mips[size * 1];
	mips[2] = (void*)&renderer->mips[size * 2];
	mips[3] = (void*)&renderer->mips[size * 3];

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); glerr();
	glBindFramebuffer(GL_READ_FRAMEBUFFER, tex->frame_buffer[0]); glerr();
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1); glerr();

	glReadBuffer(GL_COLOR_ATTACHMENT0); glerr();

	glBindBuffer(GL_PIXEL_PACK_BUFFER, tex->bufs[1].pbo); glerr();
	if (second_stage)
	{
		glGetBufferSubData(GL_PIXEL_PACK_BUFFER, 0, size, mips[0]);
	}
	glReadPixels(0, 0, tex->width, tex->height, tex->bufs[1].format,
			GL_UNSIGNED_BYTE, NULL); glerr();

	glReadBuffer(GL_COLOR_ATTACHMENT1); glerr();

	glBindBuffer(GL_PIXEL_PACK_BUFFER, tex->bufs[2].pbo);
	if (second_stage)
	{
		glGetBufferSubData(GL_PIXEL_PACK_BUFFER, 0, size, mips[1]);
	}
	glReadPixels(0, 0, tex->width, tex->height, tex->bufs[2].format,
			GL_UNSIGNED_BYTE, NULL); glerr();

	glReadBuffer(GL_COLOR_ATTACHMENT2); glerr();

	glBindBuffer(GL_PIXEL_PACK_BUFFER, tex->bufs[3].pbo);
	if (second_stage)
	{
		glGetBufferSubData(GL_PIXEL_PACK_BUFFER, 0, size, mips[2]);
	}
	glReadPixels(0, 0, tex->width, tex->height, tex->bufs[3].format,
			GL_UNSIGNED_BYTE, NULL); glerr();

	glReadBuffer(GL_COLOR_ATTACHMENT3); glerr();

	glBindBuffer(GL_PIXEL_PACK_BUFFER, tex->bufs[4].pbo);
	if (second_stage)
	{
		glGetBufferSubData(GL_PIXEL_PACK_BUFFER, 0, size, mips[3]);
	}
	glReadPixels(0, 0, tex->width, tex->height, tex->bufs[4].format,
			GL_UNSIGNED_BYTE, NULL); glerr();

	load_tile_frame_inc();

	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	if (second_stage)
	{
		uint32_t max_loads = 64;
		int32_t y, x, i, c;
		for (y = 0; y < tex->height; y++) {
			for (x = 0; x < tex->width; x++) {
				i = y * tex->width + x;
				for (c = 0; c < 4; c++)
				{
					if (*((uint32_t*)&mips[c][i]) == 0) continue;

					max_loads -= load_tile_by_id(mips[c][i]._[0], max_loads);
					if (max_loads == 0) goto end;
					max_loads -= load_tile_by_id(mips[c][i]._[1], max_loads);
					if (max_loads == 0) goto end;
				}
			}
		}
	}

end:
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0); glerr();
	return NULL;
}

void *pass_process_brightness(pass_t *self)
{
	uint32_t mip, size, count, i;

	texture_t *tex = self->output;
	if (tex->width == 0 || tex->height == 0 || tex->bufs[0].ready == 0)
		return NULL;
	if (!tex->framebuffer_ready) return NULL;

	texture_bind(tex, 0);
	glGenerateMipmap(tex->target); glerr();

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); glerr();

	mip = MAX_MIPS - 1;
	size = tex->sizes[mip].x * tex->sizes[mip].y * 4 * sizeof(uint8_t);
	if (!tex->bufs[0].pbo)
	{
		glGenBuffers(1, &tex->bufs[0].pbo);
		glBindBuffer(GL_PIXEL_PACK_BUFFER, tex->bufs[0].pbo); glerr();
		glBufferData(GL_PIXEL_PACK_BUFFER, size, 0, GL_STREAM_READ);
		glerr();
	}
	else
	{
		float brightness;
		uint8_t *data;

		data = malloc(size);

		glBindBuffer(GL_PIXEL_PACK_BUFFER, tex->bufs[0].pbo); glerr();
		glGetBufferSubData(GL_PIXEL_PACK_BUFFER, 0, size, data);
		brightness = 0.0f;
		count = 0u;

		for (i = 0; i < size / 4; i += 4)
		{
			brightness += data[i + 0] + data[i + 1] + data[i + 2];
			count += 3;
		}
		brightness /= (float)count * 255.0f;
		tex->brightness = brightness;
		free(data);
	}

	/* glPixelStorei(GL_UNPACK_ALIGNMENT, 1); glerr(); */

	glBindFramebuffer(GL_READ_FRAMEBUFFER, tex->frame_buffer[mip]); glerr();
	glReadBuffer(GL_COLOR_ATTACHMENT0); glerr();
	glReadPixels(0, 0, tex->sizes[mip].x, tex->sizes[mip].y, tex->bufs[0].format,
			GL_UNSIGNED_BYTE, NULL); glerr();
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0); glerr();
	return NULL;
}


void renderer_default_pipeline(renderer_t *self,
                               float ssao_power,
							   float ssr_power,
							   float ssssss_power,
							   float bloom_power,
							   bool_t sscaustics,
							   bool_t decals,
							   bool_t volumetric_light,
							   bool_t transparency,
							   bool_t auto_exposure)
{
	texture_t *query_mips = texture_new_2D(0, 0, 0, 5,
		buffer_new("depth",		true, -1),
		buffer_new("tiles0",	false, 4),
		buffer_new("tiles1",	false, 4),
		buffer_new("tiles2",	false, 4),
		buffer_new("tiles3",	false, 4));

	texture_t *gbuffer = texture_new_2D(0, 0, 0, 5,
		buffer_new("depth",		true, -1),
		buffer_new("albedo",	true, 4),
		buffer_new("nn",		true, 2),
		buffer_new("mrs",		false, 3),
		buffer_new("emissive",	false, 3));

	texture_t *transp_gbuffer = texture_new_2D(0, 0, 0, 5,
		buffer_new("depth",		true, -1),
		buffer_new("albedo",	true, 4),
		buffer_new("nn",		true, 2),
		buffer_new("mrs",		false, 3),
		buffer_new("emissive",	false, 3));

	texture_t *ssao =		texture_new_2D(0, 0, 0, 1,
		buffer_new("occlusion",	true, 1)
	);
	texture_t *volum =	volumetric_light ? texture_new_2D(0, 0, TEX_INTERPOLATE, 1,
		buffer_new("color",	false, 4)
	) : NULL;
	texture_t *light =	texture_new_2D(0, 0, TEX_INTERPOLATE, 1,
		buffer_new("color",	true, 4)
	);
	texture_t *refr =		texture_new_2D(0, 0, TEX_MIPMAP | TEX_INTERPOLATE, 1,
		buffer_new("color",	true, 4)
	);
	texture_t *tmp =		texture_new_2D(0, 0, TEX_MIPMAP | TEX_INTERPOLATE, 1,
		buffer_new("color",	true, 4)
	);
	texture_t *final = texture_new_2D(0, 0, TEX_INTERPOLATE
	                                      | (auto_exposure ? TEX_MIPMAP : 0), 1,
		buffer_new("color",	true, 4)
	);
	texture_t *bloom = bloom_power > 0.0f ? texture_new_2D(0, 0, TEX_INTERPOLATE | TEX_MIPMAP, 1,
			buffer_new("color", false, 4)) : NULL;

	renderer_add_tex(self, "query_mips",  0.1f, query_mips);
	renderer_add_tex(self, "gbuffer",     1.0f, gbuffer);
	renderer_add_tex(self, "transp_gbuffer", 1.0f, transp_gbuffer);
	renderer_add_tex(self, "ssao",        1.0f / 2.0f, ssao);
	renderer_add_tex(self, "light",       1.0f, light);
	if (volumetric_light) renderer_add_tex(self, "volum", 1.0f / 2.0f, volum);
	renderer_add_tex(self, "refr",        1.0f, refr);
	renderer_add_tex(self, "tmp",         1.0f, tmp);
	renderer_add_tex(self, "final",       1.0f, final);
	if (bloom_power > 0.0f) renderer_add_tex(self, "bloom", 1.0f, bloom);

	renderer_add_pass(self, "query_visible", "candle:query_mips", ref("visible"), 0,
			query_mips, query_mips, 0, ~0, 3,
			opt_clear_depth(1.0f, NULL),
			opt_clear_color(Z4, NULL),
			opt_skip(16)
	);

	renderer_add_pass(self, "query_decals", "candle:query_mips", ref("decals"), 0,
			query_mips, NULL, 0, ~0, 1,
			opt_skip(16)
	);

	renderer_add_pass(self, "query_transp", "candle:query_mips", ref("transparent"), 0,
			query_mips, query_mips, 0, ~0, 1,
			opt_skip(16)
	);

	renderer_add_pass(self, "svt", NULL, -1, 0,
			query_mips, query_mips, 0, ~0, 2,
			opt_callback((getter_cb)pass_process_query_mips),
			opt_skip(16)
	);

	renderer_add_pass(self, "gbuffer", "candle:gbuffer", ref("visible"), 0, gbuffer,
			gbuffer, 0, ~0, 2,
			opt_clear_depth(1.0f, NULL),
			opt_clear_color(Z4, NULL)
	);

	renderer_add_pass(self, "framebuffer_pass", "candle:framebuffer_draw",
			ref("framebuffer"), 0, gbuffer, gbuffer, 0, ~0, 0
	);

	/* DECAL PASS */
	if (decals)
	{
		renderer_add_pass(self, "decals_pass", "candle:gbuffer", ref("decals"), BLEND,
			gbuffer, NULL, 0, ~0, 1,
			opt_tex("gbuffer", gbuffer, NULL)
		);
	}

	renderer_add_pass(self, "ambient_light_pass", "candle:pbr", ref("ambient"),
			ADD, light, NULL, 0, ~0, 3,
			opt_clear_color(Z4, NULL),
			opt_int("opaque_pass", true, NULL),
			opt_tex("gbuffer", gbuffer, NULL)
	);

	renderer_add_pass(self, "render_pass", "candle:pbr", ref("light"),
			ADD, light, NULL, 0, ~0, 2,
			opt_int("opaque_pass", true, NULL),
			opt_tex("gbuffer", gbuffer, NULL)
	);

	if (ssssss_power > 0.f)
	{
		renderer_add_pass(self, "sss_pass0", "candle:sss", ref("quad"),
				0, tmp, NULL, 0, ~0, 5,
				opt_vec2("pass_dir", vec2(1.0f, 0.0f), NULL),
				opt_tex("buf", light, NULL),
				opt_num("power", ssssss_power, NULL),
				opt_tex("gbuffer", gbuffer, NULL),
				opt_clear_color(Z4, NULL)
		);
		renderer_add_pass(self, "sss_pass1", "candle:sss", ref("quad"),
				0, light, NULL, 0, ~0, 4,
				opt_vec2("pass_dir", vec2(0.0f, 1.0f), NULL),
				opt_num("power", ssssss_power, NULL),
				opt_tex("buf", tmp, NULL),
				opt_tex("gbuffer", gbuffer, NULL)
		);
	}

	if (volumetric_light)
	{
		renderer_add_pass(self, "volum_pass", "candle:volum", ref("light"),
				ADD | CULL_DISABLE, volum, NULL, 0, ~0, 2,
				opt_tex("gbuffer", gbuffer, NULL),
				opt_clear_color(Z4, NULL)
		);
	}

	/* renderer_add_pass(self, "sea", "sea", ref("quad"), 0, light, */
	/* 		NULL, 0, ~0, */
	/* 	(bind_t[]){ */
	/* 		{TEX, "gbuffer", .buffer = gbuffer}, */
	/* 		{NONE} */
	/* 	} */
	/* ); */

	if (transparency)
	{
		renderer_add_pass(self, "refraction", "candle:copy", ref("quad"), 0,
				refr, NULL, 0, ~0, 4,
				opt_tex("buf", light, NULL),
				opt_clear_color(Z4, NULL),
				opt_ivec2("pos", ivec2(0, 0), NULL),
				opt_int("level", 0, NULL)
		);

		renderer_add_kawase(self, refr, tmp, 0, 1);
		renderer_add_kawase(self, refr, tmp, 1, 2);
		renderer_add_kawase(self, refr, tmp, 2, 3);

		renderer_add_pass(self, "transp_2", "candle:gbuffer", ref("transparent"),
				0, transp_gbuffer, transp_gbuffer, 0, ~0, 4,
				opt_tex("refr", refr, NULL),
				opt_tex("gbuffer", gbuffer, NULL),
				opt_clear_depth(1.0f, NULL),
				opt_clear_color(Z4, NULL)
		);

		renderer_add_pass(self, "render_pass_2", "candle:pbr", ref("light"),
				ADD, light, NULL, 0, ~0, 3,
				opt_int("opaque_pass", false, NULL),
				opt_clear_color(Z4, NULL),
				opt_tex("gbuffer", transp_gbuffer, NULL)
		);

		renderer_add_pass(self, "copy_gbuffer", "candle:copy_gbuffer", ref("quad"),
				0, gbuffer, gbuffer, 0, ~0, 1,
				opt_tex("gbuffer", transp_gbuffer, NULL)
		);
	}

	if (ssao_power > 0.0f)
	{
		renderer_add_pass(self, "ssao_pass", "candle:ssao", ref("quad"), 0,
				ssao, NULL, 0, ~0, 2,
				opt_tex( "gbuffer", gbuffer, NULL),
				opt_clear_color(Z4, NULL)
		);
	}

	renderer_add_pass(self, "final", "candle:final", ref("quad"), 0, final,
			NULL, 0, ~0, 7,
			opt_tex("gbuffer", gbuffer, NULL),
			opt_tex("light", light, NULL),
			opt_tex("refr", refr, NULL),
			opt_num("ssr_power", ssr_power, NULL),
			opt_tex("ssao", ssao, NULL),
			opt_num("ssao_power", ssao_power, NULL),
			opt_tex("volum", volum, NULL)
	);

	if (bloom_power > 0.0f)
	{
		renderer_add_pass(self, "bloom_0", "candle:bright", ref("quad"), 0,
				bloom, NULL, 0, ~0, 1,
				opt_tex("buf", final, NULL)
		);
		renderer_add_kawase(self, bloom, tmp, 0, 1);
		renderer_add_kawase(self, bloom, tmp, 1, 2);
		renderer_add_kawase(self, bloom, tmp, 2, 3);

		renderer_add_pass(self, "bloom_1", "candle:upsample", ref("quad"), ADD,
				final, NULL, 0, ~0, 3,
				opt_tex("buf", bloom, NULL),
				opt_int("level", 3, NULL),
				opt_num("alpha", bloom_power, NULL)
		);
	}
	if (auto_exposure)
	{
		renderer_add_pass(self, "luminance_calc", NULL, -1, 0,
			final, NULL, 0, ~0, 2,
			opt_callback((getter_cb)pass_process_brightness),
			opt_skip(16)
		);
	}

	/* renderer_tex(self, ref(light))->mipmaped = 1; */

	/* self->output = ssao; */
	self->output = final;
}

void renderer_update_projection(renderer_t *self)
{
	uint32_t f;

	self->glvars[0].projection = mat4_perspective(
			self->proj_fov,
			((float)self->width) / self->height,
			self->proj_near, self->proj_far
	);
	self->glvars[0].inv_projection = mat4_invert(self->glvars[0].projection); 
	self->ubo_changed[0] = true;

	for(f = 1; f < 6; f++)
	{
		self->glvars[f].projection = self->glvars[0].projection;
		self->glvars[f].inv_projection = self->glvars[0].inv_projection;
		self->glvars[f].pos = self->glvars[0].pos;
		self->ubo_changed[f] = true;
	}
}

vec3_t renderer_real_pos(renderer_t *self, float depth, vec2_t coord)
{
	float z;
	vec4_t clip_space, view_space, world_space;

	if(depth < 0.01f) depth *= 100.0f;
    z = depth * 2.0 - 1.0;
	coord = vec2_sub_number(vec2_scale(coord, 2.0f), 1.0);

    clip_space = vec4(_vec2(coord), z, 1.0);
	view_space = mat4_mul_vec4(self->glvars[0].inv_projection, clip_space);

    /* Perspective division */
    view_space = vec4_div_number(view_space, view_space.w);

    world_space = mat4_mul_vec4(self->glvars[0].model, view_space);

    return vec4_xyz(world_space);
}

vec3_t renderer_screen_pos(renderer_t *self, vec3_t pos)
{
	mat4_t VP = mat4_mul(self->glvars[0].projection,
			self->glvars[0].inv_model); 

	vec4_t viewSpacePosition = mat4_mul_vec4(VP, vec4(_vec3(pos), 1.0f));
	viewSpacePosition = vec4_div_number(viewSpacePosition, viewSpacePosition.w);

    return vec3_scale(vec3_add_number(vec4_xyz(viewSpacePosition), 1.0f), 0.5);
}

static int renderer_update_screen_texture(renderer_t *self)
{
	int i;
	int w = self->width * self->resolution;
	int h = self->height * self->resolution;

	if(self->output)
	{
		renderer_update_projection(self);
	}

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

static texture_t *renderer_draw_pass(renderer_t *self, pass_t *pass,
                                     bool_t *profile)
{
	uint32_t f;
	c_render_device_t *rd;

	if(!pass->active) return NULL;
	if (self->frame % pass->draw_every != 0) return NULL;
	if (pass->shader_name[0] && !pass->shader)
	{
		pass->shader = fs_new(pass->shader_name);
		if (!pass->shader) return NULL;
	}

	pass->bound_textures = 0;
	rd = c_render_device(&SYS);
	if (!rd) return NULL;
	self->render_device_frame = rd->update_frame;
	c_render_device_rebind(rd, (rd_bind_cb)bind_pass, pass);
	if (pass->binds && pass->binds[0].type == OPT_CALLBACK)
	{
		bind_pass(pass, NULL);
		return NULL;
	}
	if(pass->shader)
	{
		fs_bind(pass->shader);
	}

	if (profile)
	{
		*profile = glfwGetTime() * 1000;
		glFinish(); glerr();
	}

	if(pass->additive)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE); glerr();
	}
	if(pass->additive_no_alpha)
	{
		glEnable(GL_BLEND);
		/* glBlendFunc(GL_SRC_COLOR, GL_ONE); glerr(); */
		glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE,
		                    GL_ONE,
		                    GL_ZERO);
	}
	if(pass->multiply)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_DST_COLOR, GL_ZERO); glerr();
	}
	if(pass->blend)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); glerr();
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
		glDepthFunc(pass->depth_func); glerr();
	}
	else
	{
		glDisable(GL_DEPTH_TEST); glerr();
	}
	rd->cull_invert = pass->cull_invert;

	glDepthMask(pass->depth_update); glerr();

	if(pass->clear)
	{
		glClearColor(_vec4(pass->clear_color));
		glClearDepth(pass->clear_depth);
	}

	for (f = 0; f < self->camera_count; f++)
	{
		uvec2_t pos;
		uvec2_t size;
		if (pass->camid != ~0 && f != pass->camid) continue;

		c_render_device_bind_ubo(rd, 19, self->ubos[f]);

		if (!pass->ignore_cam_viewport && self->size[f].x > 0)
		{
			size = self->size[f];
			pos = self->pos[f];
		}
		else
		{
			size = uvec2(pass->output->width, pass->output->height);
			pos = uvec2(0, 0);
		}
		pos.x += pass->custom_viewport_pos.x * size.x;
		pos.y += pass->custom_viewport_pos.y * size.y;
		size.x *= pass->custom_viewport_size.x;
		size.y *= pass->custom_viewport_size.y;

		texture_target_sub(pass->output, pass->depth, pass->framebuffer_id,
				pos.x, pos.y, size.x, size.y);


		if (pass->clear)
		{
			glEnable(GL_SCISSOR_TEST);
			glScissor(pos.x, pos.y, size.x, size.y);
			glClear(pass->clear);
			glDisable(GL_SCISSOR_TEST);
		}
		pass->rendered_id = draw_group(pass->draw_signal);
	}
	pass_unbind_textures(pass);
	glerr();

	glDisable(GL_DEPTH_TEST);

	glDisable(GL_BLEND);

	if(pass->auto_mip && pass->output->mipmaped)
	{
		texture_bind(pass->output, 0);
		glGenerateMipmap(pass->output->target); glerr();
	}

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); glerr();
	rd->cull_invert = false;

	if (profile)
	{
		glFinish(); glerr();
		*profile = ((long)(glfwGetTime() * 1000)) - (*profile);
	}

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
	uint32_t f;
	renderer_t *self = calloc(1, sizeof(*self));

	for(f = 0; f < 6; f++)
	{
		self->relative_transform[f] =
			self->glvars[f].projection =
			self->glvars[f].inv_projection =
			self->glvars[f].previous_view =
			self->glvars[f].model = mat4();
	}
	self->proj_near = 0.1f;
	self->proj_far = 1000.0f;
	self->proj_fov = M_PI / 2.0f;

	self->resolution = resolution;

	return self;
}

extern texture_t *g_cache;
extern texture_t *g_indir;
extern texture_t *g_probe_cache;
extern texture_t *g_histogram_buffer;
extern texture_t *g_histogram_accum;
int renderer_component_menu(renderer_t *self, void *ctx)
{
	int i;
	char fps[12]; 

	nk_layout_row_dynamic(ctx, 0, 1);
	if(nk_button_label(ctx, "Fullscreen"))
	{
		c_window_toggle_fullscreen(c_window(&SYS));
	}

	sprintf(fps, "%d", g_candle->fps);
	nk_layout_row_begin(ctx, NK_DYNAMIC, 30, 2);
		nk_layout_row_push(ctx, 0.35);
		nk_label(ctx, "FPS: ", NK_TEXT_LEFT);
		nk_layout_row_push(ctx, 0.65);
		nk_label(ctx, fps, NK_TEXT_RIGHT);
	nk_layout_row_end(ctx);
	nk_layout_row_dynamic(ctx, 0, 1);

	for(i = 0; i < self->outputs_num; i++)
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
	if (nk_button_label(ctx, "histograma"))
	{
		c_editmode_open_texture(c_editmode(&SYS), g_histogram_accum);
	}
	if (nk_button_label(ctx, "histogram"))
	{
		c_editmode_open_texture(c_editmode(&SYS), g_histogram_buffer);
	}
	if (nk_button_label(ctx, "cache"))
	{
		c_editmode_open_texture(c_editmode(&SYS), g_cache);
	}
	if (nk_button_label(ctx, "indir"))
	{
		c_editmode_open_texture(c_editmode(&SYS), g_indir);
	}
	if (nk_button_label(ctx, "probes"))
	{
		c_editmode_open_texture(c_editmode(&SYS), g_probe_cache);
	}
	return CONTINUE;
}



pass_t *renderer_pass(renderer_t *self, unsigned int hash)
{
	int i;
	if(!hash) return NULL;
	for(i = 0; i < self->passes_size; i++)
	{
		pass_t *pass = &self->passes[i];
		if(pass->hash == hash) return pass;
	}
	return NULL;
}

void renderer_toggle_pass(renderer_t *self, uint32_t hash, int active)
{
	pass_t *pass = renderer_pass(self, hash);
	if(pass)
	{
		pass->active = active;
	}
}


bind_t opt_none()
{
	bind_t bind;
	bind.type = OPT_NONE;
	return bind;
}
bind_t opt_tex(const char *name, texture_t *tex, getter_cb getter)
{
	bind_t bind = {0};
	strncpy(bind.name, name, sizeof(bind.name) - 1);
	bind.type = OPT_TEX;
	bind.buffer = tex;
	bind.getter = getter;
	return bind;
}
bind_t opt_num(const char *name, float value, getter_cb getter)
{
	bind_t bind = {0};
	strncpy(bind.name, name, sizeof(bind.name) - 1);
	bind.type = OPT_NUM;
	bind.number = value;
	bind.getter = getter;
	return bind;
}
bind_t opt_int(const char *name, int32_t value, getter_cb getter)
{
	bind_t bind = {0};
	strncpy(bind.name, name, sizeof(bind.name) - 1);
	bind.type = OPT_INT;
	bind.integer = value;
	bind.getter = getter;
	return bind;
}
bind_t opt_uint(const char *name, uint32_t value, getter_cb getter)
{
	bind_t bind = {0};
	strncpy(bind.name, name, sizeof(bind.name) - 1);
	bind.type = OPT_UINT;
	bind.uinteger = value;
	bind.getter = getter;
	return bind;
}
bind_t opt_uvec2(const char *name, uvec2_t value, getter_cb getter)
{
	bind_t bind = {0};
	strncpy(bind.name, name, sizeof(bind.name) - 1);
	bind.type = OPT_UVEC2;
	bind.uvec2 = value;
	bind.getter = getter;
	return bind;
}
bind_t opt_ivec2(const char *name, ivec2_t value, getter_cb getter)
{
	bind_t bind = {0};
	strncpy(bind.name, name, sizeof(bind.name) - 1);
	bind.type = OPT_IVEC2;
	bind.ivec2 = value;
	bind.getter = getter;
	return bind;
}
bind_t opt_vec2(const char *name, vec2_t value, getter_cb getter)
{
	bind_t bind = {0};
	strncpy(bind.name, name, sizeof(bind.name) - 1);
	bind.type = OPT_VEC2;
	bind.vec2 = value;
	bind.getter = getter;
	return bind;
}
bind_t opt_vec3(const char *name, vec3_t value, getter_cb getter)
{
	bind_t bind = {0};
	strncpy(bind.name, name, sizeof(bind.name) - 1);
	bind.type = OPT_VEC3;
	bind.vec3 = value;
	bind.getter = getter;
	return bind;
}
bind_t opt_vec4(const char *name, vec4_t value, getter_cb getter)
{
	bind_t bind = {0};
	strncpy(bind.name, name, sizeof(bind.name) - 1);
	bind.type = OPT_VEC4;
	bind.vec4 = value;
	bind.getter = getter;
	return bind;
}
bind_t opt_cam(uint32_t camera, getter_cb getter)
{
	bind_t bind = {0};
	bind.type = OPT_CAM;
	bind.uinteger = camera;
	bind.getter = getter;
	return bind;
}
bind_t opt_clear_color(vec4_t color, getter_cb getter)
{
	bind_t bind = {0};
	bind.type = OPT_CLEAR_COLOR;
	bind.vec4 = color;
	bind.getter = getter;
	return bind;
}
bind_t opt_clear_depth(float depth, getter_cb getter)
{
	bind_t bind = {0};
	bind.type = OPT_CLEAR_DEPTH;
	bind.number = depth;
	bind.getter = getter;
	return bind;
}
bind_t opt_callback(getter_cb callback)
{
	bind_t bind = {0};
	bind.type = OPT_CALLBACK;
	bind.getter = callback;
	return bind;
}
bind_t opt_usrptr(void *ptr)
{
	bind_t bind = {0};
	bind.type = OPT_USRPTR;
	bind.ptr = ptr;
	return bind;
}
bind_t opt_skip(uint32_t ticks)
{
	bind_t bind = {0};
	bind.type = OPT_SKIP;
	bind.integer = ticks;
	return bind;
}
bind_t opt_viewport(vec2_t min, vec2_t size)
{
	bind_t bind = {0};
	bind.type = OPT_VIEWPORT;
	bind.vec4 = vec4(_vec2(min), _vec2(size));
	return bind;
}

void renderer_add_pass(
		renderer_t *self,
		const char *name,
		const char *shader_name,
		uint32_t draw_signal,
		enum pass_options flags,
		texture_t *output,
		texture_t *depth,
		uint32_t framebuffer,
		uint32_t after_pass,
		uint32_t num_opts,
		...)
{
	char buffer[32];
	va_list argptr;
	int i = -1;
	unsigned int hash;
	pass_t *pass;

	if(!output)
	{
		printf("Pass %s has no output\n", name);
		exit(1);
	}
	sprintf(buffer, name, self->passes_size);
	hash = ref(buffer);
	/* TODO add pass replacement */
	if (after_pass != ~0)
	{
		uint32_t pass_id, j;
		pass_t *after = renderer_pass(self, after_pass);
		assert(after);

		pass_id = after - self->passes;
		for (j = self->passes_size; j > pass_id; --j)
		{
			self->passes[j] = self->passes[j - 1];
		}
		self->passes_size++;
		i = pass_id;
	}
	else if(i == -1)
	{
		i = self->passes_size++;
	}
	else
	{
		printf("Replacing %s\n", name);
	}

	assert(self->passes_size < 64);


	pass = &self->passes[i];
	pass->renderer = self;
	pass->hash = hash;
	pass->framebuffer_id = framebuffer;
	pass->auto_mip = !!(flags & GEN_MIP);
	pass->track_brightness = !!(flags & TRACK_BRIGHT);
	pass->camid = 0;
	pass->custom_viewport_pos = vec2(0.0f, 0.0f);
	pass->custom_viewport_size = vec2(1.0f, 1.0f);

	if(shader_name)
	{
		strncpy(pass->shader_name, shader_name, sizeof(pass->shader_name) - 1);
	}
	pass->clear = 0;

	pass->depth_update = !(flags & DEPTH_LOCK) && depth;

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
		else if (flags & DEPTH_LESSER)
		{
			pass->depth_func = GL_LEQUAL;
		}
		else
		{
			pass->depth_func = GL_EQUAL;
		}
	}
	if(flags & IGNORE_CAM_VIEWPORT)
	{
		pass->ignore_cam_viewport = true;
	}

	pass->draw_signal = draw_signal;
	pass->additive = !!(flags & ADD);
	pass->additive_no_alpha = !!(flags & ADD_NO_ALPHA);
	pass->multiply = !!(flags & MUL);
	pass->blend = !!(flags & BLEND);
	pass->cull = !(flags & CULL_DISABLE);
	pass->cull_invert = !!(flags & CULL_INVERT);
	pass->clear_depth = 1.0f;
	strncpy(pass->name, buffer, sizeof(pass->name) - 1);

	pass->draw_every = 1;
	pass->binds = malloc(sizeof(bind_t) * num_opts);
	pass->binds_size = 0;

    va_start(argptr, num_opts);
	for (i = 0; i < num_opts; i++)
	{
		int t;
		bind_t *bind;
		bind_t opt = va_arg(argptr, bind_t);
		hash_bind_t *sb;

		if(opt.type == OPT_USRPTR)
		{
			pass->usrptr = opt.ptr;
			continue;
		}
		if(opt.type == OPT_CAM)
		{
			pass->camid = opt.uinteger;
			continue;
		}
		if(opt.type == OPT_CLEAR_COLOR)
		{
			pass->clear |= GL_COLOR_BUFFER_BIT;
			pass->clear_color = opt.vec4;
			continue;
		}
		if(opt.type == OPT_CLEAR_DEPTH)
		{
			pass->clear |= GL_DEPTH_BUFFER_BIT;
			pass->clear_depth = opt.number;
			continue;
		}
		if(opt.type == OPT_SKIP)
		{
			pass->draw_every = opt.integer;
			continue;
		}
		if(opt.type == OPT_VIEWPORT)
		{
			pass->custom_viewport_pos = XY(opt.vec4);
			pass->custom_viewport_size = ZW(opt.vec4);
			continue;
		}

		bind = &pass->binds[pass->binds_size++];
		*bind = opt;

		sb = &bind->vs_uniforms;

		sb->cached = false;
		for(t = 0; t < 16; t++)
		{
			sb->u.buffer.u_tex[t] = -1;
		}
		bind->hash = ref(bind->name);
	}
	va_end(argptr);

	pass->binds = realloc(pass->binds, sizeof(bind_t) * pass->binds_size);
	self->ready = 0;
	pass->active = 1;
}

void renderer_destroy(renderer_t *self)
{
	uint32_t i;
	for(i = 0; i < self->passes_size; i++)
	{
		if(self->passes[i].binds)
		{
			free(self->passes[i].binds);
		}
	}
	for(i = 0; i < 6; i++)
	{
		if(self->ubos[i])
		{
			glDeleteBuffers(1, &self->ubos[i]);
		}
	}
	for(i = 0; i < self->outputs_num; i++)
	{
		pass_output_t *output = &self->outputs[i];
		texture_destroy(output->buffer);
	}
}

void renderer_default_pipeline_config(renderer_t *self)
{
	renderer_default_pipeline(self, 1.0f, 1.0f, 1.0f, 0.5f, true, true, true,
	                          true, true);
}

bool_t renderer_updated(const renderer_t *self)
{
	uint32_t i;
	c_render_device_t *rd;
	rd = c_render_device(&SYS);

	if (!rd || self->render_device_frame != rd->update_frame)
		return false;

	for(i = 0; i < self->passes_size; i++)
	{
		const pass_t *pass = &self->passes[i];
		if(!pass->active)
			continue;
		if (self->frame % pass->draw_every != 0)
			continue;
		if (   pass->draw_signal != ~0
		    && draw_group_state_hash(pass->draw_signal) != pass->rendered_id)
			return false;
	}
	return true;
}

int renderer_draw(renderer_t *self)
{
	uint32_t i;

	self->frame++;

	if (renderer_updated(self))
		return false;

	glerr();

	if(!self->width || !self->height) return false;

	if(!self->output) renderer_default_pipeline_config(self);

	if(!self->ready) renderer_update_screen_texture(self);

	for(i = 0; i < self->camera_count; i++)
	{
		update_ubo(self, i);
	}

	for(i = 0; i < self->passes_size; i++)
	{
		uint32_t timer;
		const bool_t profile = false;
		/* const bool_t profile = self->frame % 64 == 0; */
		const texture_t *output = renderer_draw_pass(self, &self->passes[i],
		                                             profile ? &timer : NULL);
		if (profile && output)
		{
			printf("%s: %u\n", self->passes[i].name, timer);
		}
	}
	c_render_device_rebind(c_render_device(&SYS), NULL, NULL);

	glerr();
	return true;
}

