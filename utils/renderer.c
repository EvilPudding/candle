#include "renderer.h"
#include <components/camera.h>
#include <components/model.h>
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

static int renderer_update_screen_texture(renderer_t *self);

static void bind_pass(pass_t *pass, shader_t *shader);

#define FIRST_TEX 7
static void pass_unbind_textures(pass_t *pass)
{
	for (uint32_t i = 0; i < pass->bound_textures; i++)
	{
		glActiveTexture(GL_TEXTURE0 + FIRST_TEX + i);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
}

static int pass_bind_buffer(pass_t *pass, bind_t *bind, shader_t *shader)
{
	hash_bind_t *sb = &bind->vs_uniforms;

	texture_t *buffer = bind->buffer;

	int t;

	for(t = 0; t < buffer->bufs_size; t++)
	{
		if(((int)sb->buffer.u_tex[t]) != -1)
		{
			if(buffer->bufs[t].ready)
			{
				int i = pass->bound_textures++;
				glActiveTexture(GL_TEXTURE0 + FIRST_TEX + i);
				texture_bind(buffer, t);
				glUniform1i(shader_cached_uniform(shader, sb->buffer.u_tex[t]),
				            FIRST_TEX + i);
				glerr();
			}
		}
	}
	return 1;
}

void bind_get_uniforms(bind_t *bind, hash_bind_t *sb)
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
				char buffer[256];
				snprintf(buffer, sizeof(buffer), "%s.%s", bind->name,
				         bind->buffer->bufs[t].name);
				sb->buffer.u_tex[t] = ref(buffer);
			}
			break;
		default:
			sb->number.u = ref(bind->name);
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
			bind_get_uniforms(bind, sb);
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
			glUniform1f(shader_cached_uniform(shader, sb->number.u), bind->number); glerr();
			glerr();
			break;
		case INT:
			if(bind->getter)
			{
				bind->integer = ((integer_getter)bind->getter)(bind->usrptr);
			}
			glUniform1i(shader_cached_uniform(shader, sb->integer.u), bind->integer); glerr();
			glerr();
			break;
		case VEC2:
			if(bind->getter)
			{
				bind->vec2 = ((vec2_getter)bind->getter)(bind->usrptr);
			}
			glUniform2f(shader_cached_uniform(shader, sb->vec2.u), bind->vec2.x, bind->vec2.y);
			glerr();
			break;
		case VEC3:
			if(bind->getter)
			{
				bind->vec3 = ((vec3_getter)bind->getter)(bind->usrptr);
			}
			glUniform3f(shader_cached_uniform(shader, sb->vec3.u), _vec3(bind->vec3));
			glerr();
			break;
		case VEC4:
			if(bind->getter)
			{
				bind->vec4 = ((vec4_getter)bind->getter)(bind->usrptr);
			}
			glUniform4f(shader_cached_uniform(shader, sb->vec4.u), _vec4(bind->vec4));
			glerr();
			break;
		case CALLBACK:
			bind->getter(bind->usrptr);
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
	self->outputs[self->outputs_num++] = (pass_output_t){
		.resolution = resolution,
		.hash = ref(name), .buffer = buffer};
	strncpy(buffer->name, name, sizeof(buffer->name) - 1);
}

static void update_ubo(renderer_t *self, int32_t camid)
{
	if(!self->ubo_changed[camid]) return;
	self->ubo_changed[camid] = false;
	// TODO this should be gl thread only
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

void renderer_set_model(renderer_t *self, int32_t camid, mat4_t *model)
{
	vec3_t pos = mat4_mul_vec4(*model, vec4(0.0f, 0.0f, 0.0f, 1.0f)).xyz;

	if(self->output && self->cubemap)
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
			self->ubo_changed[i] = true;
		}
	}
	else
	{
		if (self->stored_camera_frame[camid] != self->frame)
		{
			self->glvars[camid].previous_view = self->glvars[camid].inv_model;
			self->stored_camera_frame[camid] = self->frame;
		}
		self->glvars[camid].pos = pos;
		self->glvars[camid].model = *model;
		self->glvars[camid].inv_model = mat4_invert(*model);
		self->ubo_changed[camid] = true;

	}
}

void renderer_add_kawase(renderer_t *self, texture_t *t1, texture_t *t2,
		int from_mip, int to_mip)
{
	renderer_add_pass(self, "kawase_p",
			to_mip == from_mip ? "copy" : "downsample",
			ref("quad"), 0, t2, NULL, to_mip,
		(bind_t[]){
			{TEX, "buf", .buffer = t1},
			{INT, "level", .integer = from_mip},
			{NONE}
		}
	);

	renderer_add_pass(self, "kawase_0", "kawase", ref("quad"), 0,
			t1, NULL, to_mip,
		(bind_t[]){
			{TEX, "buf", .buffer = t2},
			{INT, "distance", .integer = 0},
			{INT, "level", .integer = to_mip},
			{NONE}
		}
	);

	renderer_add_pass(self, "kawase_1", "kawase", ref("quad"), 0,
			t2, NULL, to_mip,
		(bind_t[]){
			{TEX, "buf", .buffer = t1},
			{INT, "distance", .integer = 1},
			{INT, "level", .integer = to_mip},
			{NONE}
		}
	);

	renderer_add_pass(self, "kawase_2", "kawase", ref("quad"), 0,
			t1, NULL, to_mip,
		(bind_t[]){
			{TEX, "buf", .buffer = t2},
			{INT, "distance", .integer = 2},
			{INT, "level", .integer = to_mip},
			{NONE}
		}
	);

/* 	renderer_add_pass(self, "kawase_3", "kawase", ref("quad"), 0, */
/* 			t2, NULL, to_mip, */
/* 		(bind_t[]){ */
/* 			{TEX, "buf", .buffer = t1}, */
/* 			{INT, "distance", .integer = 2}, */
/* 			{INT, "level", .integer = to_mip}, */
/* 			{NONE} */
/* 		} */
/* 	); */

/* 	renderer_add_pass(self, "kawase_4", "kawase", ref("quad"), 0, */
/* 			t1, NULL, to_mip, */
/* 		(bind_t[]){ */
/* 			{TEX, "buf", .buffer = t2}, */
/* 			{INT, "distance", .integer = 3}, */
/* 			{INT, "level", .integer = to_mip}, */
/* 			{NONE} */
/* 		} */
/* 	); */
}

/* bool_t svt_stage; */
void *renderer_process_query_mips(renderer_t *self)
{
	texture_t *tex = renderer_tex(self, ref("query_mips"));
	if (!tex->framebuffer_ready) return NULL;

	uint32_t size = tex->width * tex->height * 4;
	bool_t svt_stage = true;
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
		svt_stage = false;
	}

	struct{
		uint16_t _[2];
	} *mips[4];

	mips[0] = alloca(size);
	mips[1] = alloca(size);
	mips[2] = alloca(size);
	mips[3] = alloca(size);

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); glerr();
	glBindFramebuffer(GL_READ_FRAMEBUFFER, tex->frame_buffer[0]); glerr();
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1); glerr();

	glReadBuffer(GL_COLOR_ATTACHMENT0); glerr();

	glBindBuffer(GL_PIXEL_PACK_BUFFER, tex->bufs[1].pbo); glerr();
	if (svt_stage)
	{
		glGetBufferSubData(GL_PIXEL_PACK_BUFFER, 0, size, mips[0]);
	}
	glReadPixels(0, 0, tex->width, tex->height, tex->bufs[1].format,
			GL_UNSIGNED_BYTE, NULL); glerr();

	glReadBuffer(GL_COLOR_ATTACHMENT1); glerr();

	glBindBuffer(GL_PIXEL_PACK_BUFFER, tex->bufs[2].pbo);
	if (svt_stage)
	{
		glGetBufferSubData(GL_PIXEL_PACK_BUFFER, 0, size, mips[1]);
	}
	glReadPixels(0, 0, tex->width, tex->height, tex->bufs[2].format,
			GL_UNSIGNED_BYTE, NULL); glerr();

	glReadBuffer(GL_COLOR_ATTACHMENT2); glerr();

	glBindBuffer(GL_PIXEL_PACK_BUFFER, tex->bufs[3].pbo);
	if (svt_stage)
	{
		glGetBufferSubData(GL_PIXEL_PACK_BUFFER, 0, size, mips[2]);
	}
	glReadPixels(0, 0, tex->width, tex->height, tex->bufs[3].format,
			GL_UNSIGNED_BYTE, NULL); glerr();

	glReadBuffer(GL_COLOR_ATTACHMENT3); glerr();

	glBindBuffer(GL_PIXEL_PACK_BUFFER, tex->bufs[4].pbo);
	if (svt_stage)
	{
		glGetBufferSubData(GL_PIXEL_PACK_BUFFER, 0, size, mips[3]);
	}
	glReadPixels(0, 0, tex->width, tex->height, tex->bufs[4].format,
			GL_UNSIGNED_BYTE, NULL); glerr();

	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	if (svt_stage)
	{
		uint32_t max_loads = 64;
		for (int y = 0; y < tex->height; y++) {
			for (int x = 0; x < tex->width; x++) {
				int i = y * tex->width + x;
				for (int c = 0; c < 4; c++)
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
	svt_stage = true;

end:
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0); glerr();
	return NULL;
}

void renderer_default_pipeline(renderer_t *self)
{
	/* texture_t *query_mips =	texture_new_2D(0, 0, 0, */
	/* 	buffer_new("mips",		true, 4), */
	/* 	buffer_new("coords",	true, 3), /1* Texcoords, Matid *1/ */
	/* 	buffer_new("depth",		true, -1) */
	/* ); */
	texture_t *query_mips = texture_new_2D(0, 0, 0,
		buffer_new("depth",		true, -1),
		buffer_new("tiles0",	false, 4),
		buffer_new("tiles1",	false, 4),
		buffer_new("tiles2",	false, 4),
		buffer_new("tiles3",	false, 4));

	texture_t *gbuffer = texture_new_2D(0, 0, 0,
		buffer_new("depth",		true, -1),
		buffer_new("albedo",	true, 4),
		buffer_new("nn",		true, 2),
		buffer_new("mr",		false, 2),
		buffer_new("emissive",	false, 3));

	/* texture_t *gbuffer =	texture_new_2D(0, 0, 0, */
	/* ); */

	texture_t *ssao =		texture_new_2D(0, 0, 0,
		buffer_new("occlusion",	true, 1)
	);
	texture_t *volum =	texture_new_2D(0, 0, TEX_INTERPOLATE,
		buffer_new("color",	false, 4)
	);
	/* texture_t *volum_tmp =	texture_new_2D(0, 0, TEX_INTERPOLATE, */
	/* 	buffer_new("color",	false, 4) */
	/* ); */
	texture_t *light =	texture_new_2D(0, 0, 0,
		buffer_new("color",	true, 4)
	);
	texture_t *refr =		texture_new_2D(0, 0, TEX_MIPMAP,
		buffer_new("color",	true, 4)
	);
	texture_t *tmp =		texture_new_2D(0, 0, TEX_MIPMAP,
		buffer_new("color",	true, 4)
	);
	texture_t *final =		texture_new_2D(0, 0, TEX_INTERPOLATE | TEX_MIPMAP,
		buffer_new("color",	true, 4)
	);
	/* texture_t *bloom =		texture_new_2D(0, 0, TEX_INTERPOLATE, */
		/* buffer_new("color",	true, 4) */
	/* ); */
	/* texture_t *bloom2 =		texture_new_2D(0, 0, TEX_INTERPOLATE, */
		/* buffer_new("color",	true, 4) */
	/* ); */

	texture_t *selectable = texture_new_2D(0, 0, 0,
		buffer_new("depth",		true, -1),
		buffer_new("ids",		false, 4));

	renderer_add_tex(self, "query_mips",	0.1f, query_mips);
	renderer_add_tex(self, "gbuffer",		1.0f, gbuffer);
	renderer_add_tex(self, "ssao",			1.0f / 2.0f, ssao);
	renderer_add_tex(self, "light",			1.0f, light);
	renderer_add_tex(self, "volum",			1.0f / 2.0f, volum);
	/* renderer_add_tex(self, "volum_tmp",		1.0f, volum_tmp); */
	renderer_add_tex(self, "refr",			1.0f, refr);
	renderer_add_tex(self, "tmp",			1.0f, tmp);
	renderer_add_tex(self, "final",			1.0f, final);
	renderer_add_tex(self, "selectable",	1.0f, selectable);

	/* renderer_add_tex(self, "bloom",		0.3f, bloom); */
	/* renderer_add_tex(self, "bloom2",		0.3f, bloom2); */

	renderer_add_pass(self, "query_mips", "query_mips", ref("visible"), 0,
			query_mips, query_mips, 0,
		(bind_t[]){
			{CLEAR_DEPTH, .number = 1.0f},
			{CLEAR_COLOR, .vec4 = vec4(0.0f)},
			{SKIP, .integer = 16},
			{NONE}
		}
	);

	renderer_add_pass(self, "query_mips", "query_mips", ref("decals"), 0,
			query_mips, NULL, 0,
		(bind_t[]){
			{SKIP, .integer = 16},
			{NONE}
		}
	);

	renderer_add_pass(self, "query_mips", "query_mips", ref("transparent"), 0,
			query_mips, query_mips, 0,
		(bind_t[]){
			{SKIP, .integer = 16},
			{NONE}
		}
	);

	renderer_add_pass(self, "svt", NULL, -1, 0,
			query_mips, query_mips, 0,
		(bind_t[]){
			{CALLBACK, .getter = (getter_cb)renderer_process_query_mips, .usrptr = self},
			{SKIP, .integer = 16},
			{NONE}
		}
	);

	renderer_add_pass(self, "gbuffer", "gbuffer", ref("visible"), 0, gbuffer,
			gbuffer, 0,
		(bind_t[]){
			{CLEAR_DEPTH, .number = 1.0f},
			{CLEAR_COLOR, .vec4 = vec4(0.0f)},
			{NONE}
		}
	);

	renderer_add_pass(self, "selectable", "select", ref("selectable"),
			0, selectable, selectable, 0,
		(bind_t[]){
			{CLEAR_DEPTH, .number = 1.0f},
			{CLEAR_COLOR, .vec4 = vec4(0.0f)},
			{NONE}
		}
	);

	/* DECAL PASS */
	renderer_add_pass(self, "decals_pass", "gbuffer", ref("decals"), BLEND,
			gbuffer, NULL, 0,
		(bind_t[]){
			{TEX, "gbuffer", .buffer = gbuffer},
			{NONE}
		}
	);

	renderer_add_pass(self, "ambient_light_pass", "phong", ref("ambient"),
			ADD, light, NULL, 0,
		(bind_t[]){
			{CLEAR_COLOR, .vec4 = vec4(0.0f)},
			{TEX, "gbuffer", .buffer = gbuffer},
			{NONE}
		}
	);

	renderer_add_pass(self, "render_pass", "phong", ref("light"),
			ADD, light, NULL, 0,
		(bind_t[]){
			{TEX, "gbuffer", .buffer = gbuffer},
			{NONE}
		}
	);

	renderer_add_pass(self, "volum_pass", "volum", ref("light"),
			ADD | CULL_DISABLE, volum, NULL, 0,
		(bind_t[]){
			{TEX, "gbuffer", .buffer = gbuffer},
			{CLEAR_COLOR, .vec4 = vec4(0.0f)},
			{NONE}
		}
	);
	/* renderer_add_kawase(self, volum, volum_tmp, 0, 0); */

	/* renderer_add_pass(self, "sea", "sea", ref("quad"), 0, light, */
	/* 		NULL, 0, */
	/* 	(bind_t[]){ */
	/* 		{TEX, "gbuffer", .buffer = gbuffer}, */
	/* 		{NONE} */
	/* 	} */
	/* ); */

	renderer_add_pass(self, "refraction", "copy", ref("quad"), 0,
			refr, NULL, 0,
		(bind_t[]){
			{TEX, "buf", .buffer = light},
			{CLEAR_COLOR, .vec4 = vec4(0.0f)},
			{INT, "level", .integer = 0},
			{NONE}
		}
	);

	renderer_add_kawase(self, refr, tmp, 0, 1);
	renderer_add_kawase(self, refr, tmp, 1, 2);
	renderer_add_kawase(self, refr, tmp, 2, 3);

	renderer_add_pass(self, "transp_1", "gbuffer", ref("transparent"),
			0, gbuffer, gbuffer, 0, (bind_t[]){ {NONE} });

	renderer_add_pass(self, "transp", "transparency", ref("transparent"),
			DEPTH_LOCK | DEPTH_EQUAL, light, gbuffer, 0,
		(bind_t[]){
			{TEX, "refr", .buffer = refr},
			{NONE}
		}
	);

	renderer_add_pass(self, "ssao_pass", "ssao", ref("quad"), 0,
			ssao, NULL, 0,
		(bind_t[]){
			{TEX, "gbuffer", .buffer = gbuffer},
			{NUM, "power", .number = 1.0},
			{CLEAR_COLOR, .vec4 = vec4(0.0f)},
			{NONE}
		}
	);
	/* renderer_add_kawase(self, ssao, tmp, 0, 0); */


	renderer_add_pass(self, "final", "ssr", ref("quad"), TRACK_BRIGHT, final,
			NULL, 0,
		(bind_t[]){
			{CLEAR_COLOR, .vec4 = vec4(0.0f)},
			{TEX, "gbuffer", .buffer = gbuffer},
			{TEX, "light", .buffer = light},
			{TEX, "refr", .buffer = refr},
			{TEX, "ssao", .buffer = ssao},
			{TEX, "volum", .buffer = volum},
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

	/* renderer_tex(self, ref(light))->mipmaped = 1; */

	/* self->output = ssao; */
	self->output = final;
}

void renderer_update_projection(renderer_t *self)
{
	self->glvars[0].projection = mat4_perspective(
			self->fov,
			((float)self->width) / self->height,
			self->near, self->far
	);
	self->glvars[0].inv_projection = mat4_invert(self->glvars[0].projection); 
	self->ubo_changed[0] = true;

	uint32_t f;
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

static texture_t *renderer_draw_pass(renderer_t *self, pass_t *pass,
                                     uint32_t *profile)
{
	if(!pass->active) return NULL;
	if (pass->shader_name[0] && !pass->shader)
	{
		pass->shader = fs_new(pass->shader_name);
		if (!pass->shader) return NULL;
	}

	pass->bound_textures = 0;
	c_render_device_t *rd = c_render_device(&SYS);
	if (!rd) return NULL;
	c_render_device_rebind(rd, (void*)bind_pass, pass);
	if (pass->binds && pass->binds[0].type == CALLBACK)
	{
		bind_pass(pass, NULL);
		return NULL;
	}
	if (self->frame % pass->draw_every != 0) return NULL;
	if(pass->shader)
	{
		fs_bind(pass->shader);
	}

	if (profile)
	{
		*profile = SDL_GetTicks();
		glFinish(); glerr();
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
	if(pass->blend)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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
	rd->cull_invert = pass->cull_invert;

	glDepthMask(pass->depth_update); glerr();

	if(pass->clear)
	{
		glClearColor(_vec4(pass->clear_color));
		glClearDepth(pass->clear_depth);
	}

	if(self->cubemap)
	{
		glEnable(GL_SCISSOR_TEST);
		glScissor(self->pos.x, self->pos.y, self->size.x * 2, self->size.y * 3);
		texture_target(pass->output, pass->depth, 0);
		if(pass->clear) glClear(pass->clear);
		glDisable(GL_SCISSOR_TEST);

		for(uint32_t f = 0; f < 6; f++)
		{
			uvec2_t pos;

			pos.x = self->pos.x + (f % 2) * self->size.x;
			pos.y = self->pos.y + (f / 2) * self->size.y;

			update_ubo(self, f);
			c_render_device_bind_ubo(rd, 19, self->ubos[f]);
			texture_target_sub(pass->output, pass->depth, 0,
			                   pos.x, pos.y, self->size.x, self->size.y);

			draw_group(pass->draw_signal);
		}
	}
	else
	{
		c_render_device_bind_ubo(rd, 19, self->ubos[pass->camid]);
		if (self->size.x > 0)
		{
			texture_target_sub(pass->output, pass->depth, pass->framebuffer_id,
			                   self->pos.x, self->pos.y,
			                   self->size.x, self->size.y);
		}
		else
		{
			texture_target(pass->output, pass->depth, pass->framebuffer_id);
		}

		if(pass->clear) glClear(pass->clear);

		draw_group(pass->draw_signal);

	}
	pass_unbind_textures(pass);
	glerr();

	glDisable(GL_DEPTH_TEST);

	glDisable(GL_BLEND);

	int gen_mip = 0;
	if(pass->auto_mip && pass->output->mipmaped)
	{
		texture_bind(pass->output, 0);
		glGenerateMipmap(pass->output->target); glerr();
		gen_mip = 1;
	}

	if(pass->track_brightness && self->frame % 4 == 0)
	{
		if(!gen_mip)
		{
			texture_bind(pass->output, 0);
			glGenerateMipmap(pass->output->target); glerr();
		}
		texture_update_brightness(pass->output);
	}

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); glerr();
	rd->cull_invert = false;

	if (profile)
	{
		glFinish(); glerr();
		*profile = SDL_GetTicks() - (*profile);
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
		self->glvars[f].projection = self->glvars[f].inv_projection =
			self->glvars[f].previous_view = self->glvars[f].model = mat4();
	}
	self->near = 0.1f;
	self->far = 100.0f;
	self->fov = M_PI / 2.0f;
	self->cubemap = false;

	self->resolution = resolution;

	return self;
}

unsigned int renderer_geom_at_pixel(renderer_t *self, int x, int y,
		float *depth)
{
	uint32_t result;
	texture_t *tex = renderer_tex(self, ref("selectable"));
	if(!tex) return entity_null;

	uint32_t res = texture_get_pixel(tex, 0,
			x * self->resolution, y * self->resolution, depth);
	res >>= 16;
	res &= 0xFF;
	result = res - 1;
	return result;
}

entity_t renderer_entity_at_pixel(renderer_t *self, int x, int y,
		float *depth)
{
	entity_t result;
	texture_t *tex = renderer_tex(self, ref("selectable"));
	if(!tex) return entity_null;

	uint32_t pos = texture_get_pixel(tex, 0,
			x * self->resolution, y * self->resolution, depth);
	pos &= 0xFF;
	struct { uint32_t pos, uid; } *cast = (void*)&result;

	cast->pos = pos;
	cast->uid = g_ecm->entities_info[pos].uid;
	return result;
}


extern texture_t *g_cache;
extern texture_t *g_indir;
extern texture_t *g_probe_cache;
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

void renderer_add_pass(
		renderer_t *self,
		const char *name,
		const char *shader_name,
		uint32_t draw_signal,
		enum pass_options flags,
		texture_t *output,
		texture_t *depth,
		uint32_t framebuffer,
		bind_t binds[])
{
	if(!output) exit(1);
	char buffer[32];
	snprintf(buffer, sizeof(buffer), name, self->passes_size);
	unsigned int hash = ref(buffer);
	/* TODO add pass replacement */
	int i = -1;
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
	pass->framebuffer_id = framebuffer;
	pass->auto_mip = !!(flags & GEN_MIP);
	pass->track_brightness = !!(flags & TRACK_BRIGHT);

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
		else
		{
			pass->depth_func = GL_LEQUAL;
		}
	}

	pass->draw_signal = draw_signal;
	pass->additive = !!(flags & ADD);
	pass->multiply = !!(flags & MUL);
	pass->blend = !!(flags & BLEND);
	pass->cull = !(flags & CULL_DISABLE);
	pass->cull_invert = !!(flags & CULL_INVERT);
	pass->clear_depth = 1.0f;
	strncpy(pass->name, buffer, sizeof(pass->name) - 1);

	int bind_count;
	for(bind_count = 0; binds[bind_count].type != NONE; bind_count++);

	pass->draw_every = 1;
	pass->binds = malloc(sizeof(bind_t) * bind_count);
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
		if(binds[i].type == SKIP)
		{
			pass->draw_every = binds[i].integer;
			continue;
		}

		bind_t *bind = &pass->binds[pass->binds_size++];
		*bind = binds[i];

		hash_bind_t *sb = &bind->vs_uniforms;

		sb->cached = false;
		int t;
		for(t = 0; t < 16; t++)
		{
			sb->buffer.u_tex[t] = -1;
		}
		bind->hash = ref(bind->name);
	}
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

int renderer_draw(renderer_t *self)
{
	self->frame++;

	glerr();
	uint32_t i;

	if(!self->width || !self->height) return CONTINUE;

	if(!self->output) renderer_default_pipeline(self);

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
	return CONTINUE;
}

