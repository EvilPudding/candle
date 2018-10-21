#include "camera.h"
#include <systems/window.h>
#include <systems/editmode.h>
#include "spacial.h"
#include "node.h"
#include <utils/nk.h>
#include <candle.h>
#include <utils/renderer.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int g_camera_num;

static int c_camera_update_projection(c_camera_t *self);
static void c_camera_update_renderer(c_camera_t *self);
static int c_camera_changed(c_camera_t *self);

c_camera_t *c_camera_new(float fov, float near, float far, renderer_t *renderer)
{
	c_camera_t *self = component_new("camera");

	self->renderer = renderer;

	c_window_t *win = c_window(&SYS);

	self->near = near;
	self->far = far;
	self->fov = fov;
	self->exposure = 0.25f;
	self->id = g_camera_num++;

	if(win->renderer == NULL)
	{
		c_camera_assign(self);
	}

	return self;
}

c_camera_t *c_camera_clone(c_camera_t *self)
{
	c_camera_t *clone = malloc(sizeof *clone);

	clone->near = self->near;
	clone->far = self->far;
	clone->fov = self->fov;
	clone->exposure = self->exposure;

	c_camera_update_projection(clone);
	c_camera_changed(clone);

	return clone;
}

static int c_camera_changed(c_camera_t *self)
{
	c_camera_update_renderer(self);
	return CONTINUE;
}

vec3_t c_camera_screen_pos(c_camera_t *self, vec3_t pos)
{
	mat4_t VP = mat4_mul(self->projection_matrix, self->view_matrix); 

	vec4_t viewSpacePosition = mat4_mul_vec4(VP, vec4(_vec3(pos), 1.0f));
	viewSpacePosition = vec4_div_number(viewSpacePosition, viewSpacePosition.w);

	viewSpacePosition.xyz = vec3_scale(vec3_add_number(viewSpacePosition.xyz, 1.0f), 0.5);
    return viewSpacePosition.xyz;
}

/* float c_camera_linearize(c_camera_t *self, float depth) */
/* { */
    /* return = 2.0 * zNear * zFar / (zFar + zNear - (2.0 * z_b - 1.0) * (zFar - zNear)); */
/* } */

float c_camera_unlinearize(c_camera_t *self, float depth)
{
	return self->far * (1.0f - (self->near / depth)) / (self->far - self->near);
}

vec3_t c_camera_real_pos(c_camera_t *self, float depth, vec2_t coord)
{
	/* float z = depth; */
	if(depth < 0.01f) depth *= 100.0f;
    float z = depth * 2.0 - 1.0;
	coord = vec2_sub_number(vec2_scale(coord, 2.0f), 1.0);

	mat4_t projInv = mat4_invert(self->projection_matrix);
	mat4_t viewInv = mat4_invert(self->view_matrix);

    vec4_t clipSpacePosition = vec4(_vec2(coord), z, 1.0);
    vec4_t viewSpacePosition = mat4_mul_vec4(projInv, clipSpacePosition);

    // Perspective division
    viewSpacePosition = vec4_div_number(viewSpacePosition, viewSpacePosition.w);

    vec4_t worldSpacePosition = mat4_mul_vec4(viewInv, viewSpacePosition);

    return worldSpacePosition.xyz;
}


void c_camera_update_view(c_camera_t *self)
{
	c_node_t *n = c_node(self);
	c_node_update_model(n);
	self->model_matrix = n->model;

	self->pos = mat4_mul_vec4(n->model, vec4(0.0, 0.0, 0.0, 1.0)).xyz;
	self->view_matrix = mat4_invert(n->model);

}

int c_camera_component_menu(c_camera_t *self, void *ctx)
{
	float before = self->fov;
	nk_property_float(ctx, "fov:", 1, &self->fov, 179, 1, 1);
	if(self->fov != before)
	{
		c_camera_update_projection(self);
	}
	before = self->near;
	nk_property_float(ctx, "near:", 0.01, &self->near, self->far - 0.01, 0.01, 0.1);
	if(self->near != before)
	{
		c_camera_update_projection(self);
	}
	before = self->far;
	nk_property_float(ctx, "far:", self->near + 0.01, &self->far, 10000, 0.01, 0.1);
	if(self->far != before)
	{
		c_camera_update_projection(self);
	}
	return CONTINUE;
}

static void c_camera_update_renderer(c_camera_t *self)
{
	if(!self->renderer) return;

	struct gl_camera *glcamera = &self->renderer->glvars.camera;

	c_camera_update_view(self);

	glcamera->model = self->model_matrix;
	glcamera->inv_model = self->view_matrix;
	glcamera->pos = self->pos;

	mat4_t inv_projection = mat4_invert(self->projection_matrix);
	glcamera->projection = self->projection_matrix;
	glcamera->inv_projection = inv_projection;
	glcamera->exposure = self->exposure;

}

int c_camera_update(c_camera_t *self, float *dt)
{
	if(self->renderer && self->renderer->output)
	{
		float brightness = self->renderer->output->brightness *2.0f + 0.1;
		float targetExposure = 0.3 + 1.0f / brightness;
		if(targetExposure > 5) targetExposure = 5;
		if(targetExposure < 0.1) targetExposure = 0.1;

		float step = (targetExposure - self->exposure) / 30;
		if(step > 0.3f) step = 0.3f;
		if(step < -0.3f) step = -0.3f;
		self->exposure += step;
		/* printf("%f %f\n", targetExposure, self->exposure); */
	}
	return CONTINUE;
}

int c_camera_resize(c_camera_t *self, window_resize_data *event)
{
	self->width = event->width;
	self->height = event->height;

	renderer_resize(self->renderer, self->width, self->height);
	c_camera_update_projection(self);
	return CONTINUE;
}

void c_camera_assign(c_camera_t *self)
{
	c_window_t *win = c_window(&SYS);
	win->renderer = self->renderer;
	self->width = win->width;
	self->height = win->height;

	c_camera_update_projection(self);
	renderer_resize(self->renderer, win->width, win->height);
}

static int c_camera_update_projection(c_camera_t *self)
{
	self->projection_matrix = mat4_perspective(
		self->fov * (M_PI / 180.0f),
		((float)self->width) / self->height,
		/* ((float)event->width / 2) / event->height, */
		self->near, self->far
	);
	c_camera_update_renderer(self);
	return CONTINUE;
}

REG()
{
	ct_t *ct = ct_new("camera", sizeof(c_camera_t), NULL, NULL, 1, ref("node"));

	ct_listener(ct, ENTITY, sig("node_changed"), c_camera_changed);
	ct_listener(ct, WORLD, sig("window_resize"), c_camera_resize);
	ct_listener(ct, WORLD, sig("world_update"), c_camera_update);

	ct_listener(ct, WORLD, sig("component_menu"), c_camera_component_menu);
}
