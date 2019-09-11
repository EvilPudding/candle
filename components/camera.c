#include "camera.h"
#include <systems/window.h>
#include <systems/editmode.h>
#include <components/name.h>
#include "node.h"
#include <utils/nk.h>
#include <candle.h>
#include <utils/renderer.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int c_camera_changed(c_camera_t *self);

c_camera_t *c_camera_new(float fov, float near, float far,
		int auto_exposure, int active, int window, renderer_t *renderer)
{
	c_camera_t *self = component_new("camera");

	self->auto_exposure = auto_exposure;
	self->exposure = 0.25f;
	self->active = active;
	self->window = window;
	self->auto_transform = 1;

	if(renderer)
	{
		self->renderer = renderer;
		self->renderer->near = near;
		self->renderer->far = far;
		self->renderer->fov = fov * (M_PI / 180.0f);
		self->camid = self->renderer->camera_count++;

		c_camera_assign(self);
	}

	return self;
}

/* c_camera_t *c_camera_clone(c_camera_t *self) */
/* { */
/* 	c_camera_t *clone = malloc(sizeof *clone); */

/* 	clone->near = self->near; */
/* 	clone->far = self->far; */
/* 	clone->fov = self->fov; */
/* 	clone->exposure = self->exposure; */

/* 	c_camera_changed(clone); */

/* 	return clone; */
/* } */

static int c_camera_changed(c_camera_t *self)
{
	self->modified = 1;
	return CONTINUE;
}

vec3_t c_camera_screen_pos(c_camera_t *self, vec3_t pos)
{
	return renderer_screen_pos(self->renderer, pos);
}

/* float c_camera_linearize(c_camera_t *self, float depth) */
/* { */
    /* return = 2.0 * zNear * zFar / (zFar + zNear - (2.0 * z_b - 1.0) * (zFar - zNear)); */
/* } */

float c_camera_unlinearize(c_camera_t *self, float depth)
{
	return self->renderer->far * (1.0f - (self->renderer->near / depth)) /
		(self->renderer->far - self->renderer->near);
}

vec3_t c_camera_real_pos(c_camera_t *self, float depth, vec2_t coord)
{
	return renderer_real_pos(self->renderer, depth, coord);
}


int c_camera_component_menu(c_camera_t *self, void *ctx)
{
	float new = self->renderer->fov * (180.0f / M_PI);
	nk_property_float(ctx, "fov:", 1, &new, 179, 1, 1);
	if(self->renderer->fov != new)
	{
		self->renderer->fov = new * (M_PI / 180.0f);
		renderer_update_projection(self->renderer);
	}
	float before = self->renderer->near;
	nk_property_float(ctx, "near:", 0.01, &self->renderer->near,
			self->renderer->far - 0.01, 0.01, 0.1);
	if(self->renderer->near != before)
	{
		renderer_update_projection(self->renderer);
	}
	before = self->renderer->far;
	nk_property_float(ctx, "far:", self->renderer->near + 0.01,
			&self->renderer->far, 10000, 0.01, 0.1);
	if(self->renderer->far != before)
	{
		renderer_update_projection(self->renderer);
	}
	renderer_component_menu(self->renderer, ctx);
	return CONTINUE;
}

int c_camera_update(c_camera_t *self, float *dt)
{
#ifndef __EMSCRIPTEN__
	if(self->auto_exposure && self->renderer && self->renderer->output)
	{
		float brightness = (self->renderer->output->brightness - 0.5) * 2.0;
		float targetExposure = -brightness * 2.0f;
		if(targetExposure > 5.0) targetExposure = 5.0;
		if(targetExposure < -4.0) targetExposure = -4.0;

		float step = (targetExposure - self->exposure) / 30;
		if(step > 0.3f) step = 0.3f;
		if(step < -0.3f) step = -0.3f;
		self->exposure += step;
		self->renderer->glvars[0].exposure = self->exposure;
	}
#endif
	return CONTINUE;
}

int c_camera_resize(c_camera_t *self, window_resize_data *event)
{
	self->width = event->width;
	self->height = event->height;

	renderer_resize(self->renderer, self->width, self->height);
	return CONTINUE;
}

void c_camera_assign(c_camera_t *self)
{
	c_window_t *win = c_window(&SYS);
	if(self->window && !win->renderer)
	{
		win->renderer = self->renderer;
	}
	self->width = win->width;
	self->height = win->height;

	renderer_resize(self->renderer, win->width, win->height);
}

int c_camera_pre_draw(c_camera_t *self)
{
	if(self->auto_transform && self->modified && self->renderer)
	{
		c_node_t *node = c_node(self);
		c_node_update_model(node);
		renderer_set_model(self->renderer, self->camid, &node->model);
		/* self->modified = 0; */
	}
	return CONTINUE;
}

int c_camera_draw(c_camera_t *self)
{
	if(self->renderer && self->active)
	{
		renderer_draw(self->renderer);
	}
	return CONTINUE;
}

REG()
{
	ct_t *ct = ct_new("camera", sizeof(c_camera_t), NULL, NULL, 1, ref("node"));

	ct_listener(ct, ENTITY, sig("node_changed"), c_camera_changed);
	ct_listener(ct, WORLD, sig("window_resize"), c_camera_resize);
	ct_listener(ct, WORLD, sig("world_update"), c_camera_update);
	ct_listener(ct, WORLD | 50, sig("world_pre_draw"), c_camera_pre_draw);
	ct_listener(ct, WORLD | 21, sig("world_draw"), c_camera_draw);

	ct_listener(ct, WORLD, sig("component_menu"), c_camera_component_menu);
}
