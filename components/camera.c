#include "camera.h"
#include "../systems/window.h"
#include "spacial.h"
#include "node.h"
#include <nk.h>
#include <nk.h>

unsigned long ct_camera;

static int c_camera_update(c_camera_t *self, window_resize_data *event);
#ifdef MESH4
int c_camera_global_menu(c_camera_t *self, void *ctx);
#endif

static void c_camera_init(c_camera_t *self)
{
	self->super = component_new(ct_camera);
}

c_camera_t *c_camera_new(int active, float fov, float near, float far,
		int width, int height)
{
	c_camera_t *self = malloc(sizeof *self);
	c_camera_init(self);

	self->active = active;
	self->near = near;
	self->far = far;
	self->fov = fov * (M_PI / 180.0f);
	self->view_cached = 0;
	self->exposure = 0.25f;
#ifdef MESH4
	self->angle4 = 0.0f;
#endif

	c_camera_update(self, &(window_resize_data){width, height});

	return self;
}

/* int c_camera_changed(c_camera_t *self) */
/* { */
/* 	self->view_cached = 0; */
/* 	return 1; */
/* } */

void c_camera_register(ecm_t *ecm)
{
	ct_t *ct = ecm_register(ecm, &ct_camera, sizeof(c_camera_t),
			(init_cb)c_camera_init, 1, ct_spacial);

	ct_register_listener(ct, WORLD, window_resize, (signal_cb)c_camera_update);

#ifdef MESH4
	ct_register_listener(ct, WORLD, global_menu, (signal_cb)c_camera_global_menu);
#endif

}

void c_camera_update_view(c_camera_t *self)
{
	c_node_t *n = c_node(c_entity(self));
	c_node_update_model(n);
	self->pos = mat4_mul_vec4(n->model, vec4(0.0, 0.0, 0.0, 1.0)).xyz;
	self->view_matrix = mat4_invert(n->model);
}

#include<candle.h>
#include<systems/renderer.h>

#ifdef MESH4
int c_camera_global_menu(c_camera_t *self, void *ctx)
{
	nk_layout_row_dynamic(ctx, 22, 1);
	nk_property_float(ctx, "4D angle:", 0, &self->angle4, M_PI / 2, 0.1, 0.01);
	return 1;
}
#endif

static int c_camera_update(c_camera_t *self, window_resize_data *event)
{
	/* TODO: remove renderer reference, camera should update on render resize,
	 * not window */
	self->projection_matrix = mat4_perspective(
		self->fov,
		((float)c_renderer(candle->systems)->width) /
		c_renderer(candle->systems)->height,
		/* ((float)event->width / 2) / event->height, */
		self->near, self->far
	);
	return 1;
}

entity_t ecm_get_camera(ecm_t *self)
{
	unsigned long i;
	ct_t *cameras = ecm_get(self, ct_camera);
	c_camera_t *camera;

	for(i = 0; i < cameras->components_size; i++)
	{
		camera = (c_camera_t*)ct_get_at(cameras, i);
		if(camera->active) return camera->super.entity;
	}

	return self->none;
}


