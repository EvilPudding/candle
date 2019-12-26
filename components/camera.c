#include "camera.h"
#include <systems/window.h>
#include <systems/editmode.h>
#include <components/name.h>
#include <components/model.h>
#include "node.h"
#include <utils/nk.h>
#include <candle.h>
#include <utils/renderer.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static mat_t *g_mat;
static mesh_t *g_cam;
void c_camera_frustum(c_camera_t *self)
{
	mat4_t proj = mat4_invert(mat4_perspective(
			self->proj_fov * (M_PI / 180.0f),
			((float)self->width) / self->height,
			self->proj_near, self->proj_far
	));

	int v, v0, v1, v2, v3, e0;
	mesh_lock(self->mesh);
	mesh_clear(self->mesh);

	v = mesh_add_vert(self->mesh, Z3);
	v0 = mesh_add_vert(self->mesh, mat4_project(proj, vec3(-1.0f, 1.0f, 1.0f)));
	v1 = mesh_add_vert(self->mesh, mat4_project(proj, vec3( 1.0f, 1.0f, 1.0f)));
	v2 = mesh_add_vert(self->mesh, mat4_project(proj, vec3( 1.0f,-1.0f, 1.0f)));
	v3 = mesh_add_vert(self->mesh, mat4_project(proj, vec3(-1.0f,-1.0f, 1.0f)));

	e0 = mesh_add_edge(self->mesh, v, -1, -1, Z3, Z2);
	mesh_add_edge(self->mesh, v0, -1, e0, Z3, Z2);

	e0 = mesh_add_edge(self->mesh, v, -1, -1, Z3, Z2);
	mesh_add_edge(self->mesh, v1, -1, e0, Z3, Z2);

	e0 = mesh_add_edge(self->mesh, v, -1, -1, Z3, Z2);
	mesh_add_edge(self->mesh, v2, -1, e0, Z3, Z2);

	e0 = mesh_add_edge(self->mesh, v, -1, -1, Z3, Z2);
	mesh_add_edge(self->mesh, v3, -1, e0, Z3, Z2);

	e0 = mesh_add_edge(self->mesh, v0, -1, -1, Z3, Z2);
	mesh_add_edge(self->mesh, v1, -1, e0, Z3, Z2);

	e0 = mesh_add_edge(self->mesh, v1, -1, -1, Z3, Z2);
	mesh_add_edge(self->mesh, v2, -1, e0, Z3, Z2);

	e0 = mesh_add_edge(self->mesh, v2, -1, -1, Z3, Z2);
	mesh_add_edge(self->mesh, v3, -1, e0, Z3, Z2);

	e0 = mesh_add_edge(self->mesh, v3, -1, -1, Z3, Z2);
	mesh_add_edge(self->mesh, v0, -1, e0, Z3, Z2);

	mesh_unlock(self->mesh);
}

static int c_camera_changed(c_camera_t *self);

/* c_camera_t *c_camera_clone(c_camera_t *self) */
/* { */
/* 	c_camera_t *clone = malloc(sizeof *clone); */

/* 	clone->near = self->near; */
/* 	clone->proj_far = self->proj_far; */
/* 	clone->proj_fov = self->proj_fov; */
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
	return self->proj_far * (1.0f - (self->proj_near / depth)) /
		(self->proj_far - self->proj_near);
}

vec3_t c_camera_real_pos(c_camera_t *self, float depth, vec2_t coord)
{
	return renderer_real_pos(self->renderer, depth, coord);
}


int c_camera_component_menu(c_camera_t *self, void *ctx)
{
	bool_t changed = false;
	float new, before;

	new = self->proj_fov;

	nk_property_float(ctx, "fov:", 1, &new, 179, 1, 1);
	if(self->proj_fov != new)
	{
		self->proj_fov = new;
		changed = true;
	}
	before = self->proj_near;
	nk_property_float(ctx, "near:", 0.01, &self->proj_near,
			self->proj_far - 0.01, 0.01, 0.1);
	if(self->proj_near != before)
	{
		changed = true;
	}
	before = self->proj_far;
	nk_property_float(ctx, "far:", self->proj_near + 0.01,
			&self->proj_far, 10000, 0.01, 0.1);
	if(self->proj_far != before)
	{
		changed = true;
	}
	if (changed)
	{
		c_camera_frustum(self);
	}
	if (self->renderer)
	{
		if (changed)
		{
			renderer_update_projection(self->renderer);
			self->renderer->proj_near = self->proj_near;
			self->renderer->proj_far = self->proj_far;
			self->renderer->proj_fov = self->proj_fov * (M_PI / 180.0f);
		}
		renderer_component_menu(self->renderer, ctx);
	}
	return CONTINUE;
}

int c_camera_update(c_camera_t *self, float *dt)
{
#ifndef __EMSCRIPTEN__
	if(self->auto_exposure && self->renderer && self->renderer->output)
	{
		float brightness = (self->renderer->output->brightness - 0.5) * 2.0;
		float targetExposure = -brightness * 2.0f;
		const float max_exposure = 10.0f;
		const float min_exposure = -4.0f;
		float step;

		if (targetExposure > max_exposure)
		{
			targetExposure = max_exposure;
		}
		if (targetExposure < min_exposure)
		{
			targetExposure = min_exposure;
		}

		step = (targetExposure - self->exposure) / 30.0f;
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

	if (self->renderer)
	{
		renderer_resize(self->renderer, self->width, self->height);
	}
	c_camera_frustum(self);
	return CONTINUE;
}

void c_camera_assign(c_camera_t *self)
{
	c_window_t *win = c_window(&SYS);
	if(self->window)
	{
		win->renderer = self->renderer;
	}
	self->width = win->width;
	self->height = win->height;

	renderer_resize(self->renderer, win->width, win->height);
}

int c_camera_pre_draw(c_camera_t *self)
{
	if(self->auto_transform && self->modified)
	{
		c_node_t *node = c_node(self);
		c_node_update_model(node);
		drawable_set_transform(&self->draw, node->model);
		drawable_set_transform(&self->frustum, node->model);
		if (self->renderer)
		{
			renderer_set_model(self->renderer, self->camid, &node->model);
		}
		self->modified = 0;

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

void c_camera_init(c_camera_t *self)
{
	self->proj_near = 0.01f;
	self->proj_far = 100.0f;
	self->proj_fov = 90.0f;
	self->width = 1;
	self->height = 1;
	if (!g_mat)
	{
		int e0, e1, e2, e3;
		mat4_t saved;
		float w = 0.05f;
		float h = 0.1f;
		g_mat = mat_new("m", "default");
		mat4f(g_mat, ref("emissive.color"), vec4(0.3f, 0.1f, 0.9f, 0.5f));
		mat4f(g_mat, ref("albedo.color"), vec4(1, 1, 1, 1.0f));
		g_cam = mesh_new();
		mesh_lock(g_cam);

		mesh_unselect(g_cam, SEL_EDITING, MESH_ANY, -1);
		e0 = mesh_add_edge(g_cam, mesh_add_vert(g_cam, vec3(+w, -h, 0.4f)), -1, -1, Z3, Z2);
		mesh_select(g_cam, SEL_EDITING, MESH_EDGE, e0);
		e1 = mesh_add_edge(g_cam, mesh_add_vert(g_cam, vec3(-w, -h, 0.4f)), -1, e0, Z3, Z2);
		mesh_select(g_cam, SEL_EDITING, MESH_EDGE, e1);
		e2 = mesh_add_edge(g_cam, mesh_add_vert(g_cam, vec3(-w, +h, 0.4f)), -1, e1, Z3, Z2);
		mesh_select(g_cam, SEL_EDITING, MESH_EDGE, e2);
		e3 = mesh_add_edge(g_cam, mesh_add_vert(g_cam, vec3(+w, +h, 0.4f)), e0, e2, Z3, Z2);
		mesh_select(g_cam, SEL_EDITING, MESH_EDGE, e3);

		mesh_extrude_edges(g_cam, 1, vec3(0.0f, 0.0f, -0.3f), 1.0f, NULL, NULL, NULL);
		mesh_extrude_edges(g_cam, 1, Z3, 0.3f, NULL, NULL, NULL);
		mesh_extrude_edges(g_cam, 1, vec3(0.0f, 0.0f, -0.1f), 2.3f, NULL, NULL, NULL);

		saved = mesh_save(g_cam);
		mesh_translate(g_cam, vec3(0.0f, 0.20f, 0.2f));
		mesh_circle(g_cam, 0.15f, 23, vec3(1.0f, 0.0f, 0.0f));
		mesh_translate(g_cam, vec3(0.0f, -0.05f, 0.2f));
		mesh_circle(g_cam, 0.1f, 23, vec3(1.0f, 0.0f, 0.0f));
		mesh_restore(g_cam, saved);

		mesh_unselect(g_cam, SEL_EDITING, MESH_ANY, -1);
		mesh_select(g_cam, SEL_EDITING, MESH_FACE, ~0);
		mesh_remove_faces(g_cam);
		mesh_unlock(g_cam);
	}

	self->mesh = mesh_new();
	drawable_init(&self->frustum, ref("widget"));
	drawable_set_entity(&self->frustum, c_entity(self));
	drawable_set_vs(&self->frustum, model_vs());
	drawable_set_mat(&self->frustum, g_mat);
	drawable_set_mesh(&self->frustum, self->mesh);

	drawable_init(&self->draw, ref("widget"));
	drawable_add_group(&self->draw, ref("selectable"));
	drawable_set_entity(&self->draw, c_entity(self));
	drawable_set_vs(&self->draw, model_vs());
	drawable_set_mat(&self->draw, g_mat);
	drawable_set_mesh(&self->draw, g_cam);
	drawable_set_xray(&self->draw, true);

	c_camera_frustum(self);
}

void c_camera_destroy(c_camera_t *self)
{
	if (self->renderer)
	{
		renderer_destroy(self->renderer);
	}
	drawable_set_mesh(&self->draw, NULL);
	drawable_set_mesh(&self->frustum, NULL);
	mesh_destroy(self->mesh);
}

void ct_camera(ct_t *self)
{
	ct_init(self, "camera", sizeof(c_camera_t));
	ct_dependency(self, ct_node);
	ct_set_init(self, (init_cb)c_camera_init);
	ct_set_destroy(self, (destroy_cb)c_camera_destroy);

	ct_listener(self, ENTITY, 0, sig("node_changed"), c_camera_changed);
	ct_listener(self, WORLD, 0, sig("window_resize"), c_camera_resize);
	ct_listener(self, WORLD, 0, sig("world_update"), c_camera_update);
	ct_listener(self, WORLD, 50, sig("world_pre_draw"), c_camera_pre_draw);
	ct_listener(self, WORLD, 10, sig("world_draw"), c_camera_draw);

	ct_listener(self, WORLD, 0, sig("component_menu"), c_camera_component_menu);
}

c_camera_t *c_camera_new(float proj_fov, float proj_near, float proj_far,
		int auto_exposure, int active, int window, renderer_t *renderer)
{
	c_camera_t *self = component_new(ct_camera);

	self->auto_exposure = auto_exposure;
	self->exposure = 0.25f;
	self->active = active;
	self->window = window;
	self->auto_transform = 1;
	self->proj_near = proj_near;
	self->proj_far = proj_far;
	self->proj_fov = proj_fov;

	if(renderer)
	{
		self->renderer = renderer;
		self->renderer->proj_near = self->proj_near;
		self->renderer->proj_far = self->proj_far;
		self->renderer->proj_fov = self->proj_fov * (M_PI / 180.0f);
		self->camid = self->renderer->camera_count++;
		c_camera_assign(self);
	}

	return self;
}

