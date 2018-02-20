#include "camera.h"
#include <systems/window.h>
#include <systems/editmode.h>
#include "spacial.h"
#include "node.h"
#include <nk.h>
#include<candle.h>
#include<systems/renderer.h>


DEC_CT(ct_camera);

static void c_camera_init(c_camera_t *self)
{
}

c_camera_t *c_camera_new(float fov, float near, float far)
{
	c_camera_t *self = component_new(ct_camera);

	self->near = near;
	self->far = far;
	self->fov = fov * (M_PI / 180.0f);
	self->view_cached = 0;
	self->exposure = 0.25f;

	c_camera_update(self, NULL);

	return self;
}

c_camera_t *c_camera_clone(c_camera_t *self)
{
	c_camera_t *clone = malloc(sizeof *clone);
	c_camera_init(clone);

	clone->near = self->near;
	clone->far = self->far;
	clone->fov = self->fov;
	clone->view_cached = 0;
	clone->exposure = self->exposure;

	c_camera_update(self, NULL);

	return clone;
}

int c_camera_changed(c_camera_t *self)
{
	self->view_cached = 0;
	return 1;
}

void c_camera_register()
{
	ct_t *ct = ct_new("c_camera", &ct_camera, sizeof(c_camera_t),
			(init_cb)c_camera_init, 2, ct_spacial, ct_node);

	ct_listener(ct, ENTITY, spacial_changed, c_camera_changed);
	ct_listener(ct, WORLD, window_resize, c_camera_update);

}

vec3_t c_camera_real_pos(c_camera_t *self, float depth, vec2_t coord)
{
	/* float z = depth; */
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
	self->pos = mat4_mul_vec4(n->model, vec4(0.0, 0.0, 0.0, 1.0)).xyz;
	self->view_matrix = mat4_invert(n->model);
}

void c_camera_activate(c_camera_t *self)
{
	c_camera_update(self, NULL);
	c_renderer(&candle->systems)->camera = c_entity(self);
}

int c_camera_update(c_camera_t *self, void *event)
{
	/* TODO: remove renderer reference, camera should update on render resize,
	 * not window */
	self->width = c_renderer(&candle->systems)->width;
	self->height = c_renderer(&candle->systems)->height;
	self->projection_matrix = mat4_perspective(
		self->fov,
		((float)self->width) / self->height,
		/* ((float)event->width / 2) / event->height, */
		self->near, self->far
	);
	return 1;
}

