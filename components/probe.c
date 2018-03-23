#include "probe.h"
#include "spacial.h"
#include "candle.h"
#include <stdlib.h>
#include <components/model.h>


void c_probe_init(c_probe_t *self) { }

c_probe_t *c_probe_new(int map_size)
{
	c_probe_t *self = component_new(ct_probe);

	self->map = texture_cubemap(map_size, map_size, 1);

	self->last_update = -1;

	self->projection = mat4_perspective(M_PI / 2.0f, 1.0f, 0.5f, 100.5f); 

	return self;
}

int c_probe_update_position(c_probe_t *self)
{
	vec3_t pos = c_spacial(self)->pos;

	self->views[0] = mat4_look_at(pos, vec3_add(pos, vec3(1.0, 0.0, 0.0)),
			vec3(0.0,-1.0,0.0));
	self->models[0] = mat4_invert(self->views[0]);

	self->views[1] = mat4_look_at(pos, vec3_add(pos, vec3(-1.0, 0.0, 0.0)),
			vec3(0.0, -1.0, 0.0));
	self->models[1] = mat4_invert(self->views[0]);

	self->views[2] = mat4_look_at(pos, vec3_add(pos, vec3(0.0, 1.0, 0.0)),
			vec3(0.0, 0.0, 1.0));
	self->models[2] = mat4_invert(self->views[0]);

	self->views[3] = mat4_look_at(pos, vec3_add(pos, vec3(0.0, -1.0, 0.0)),
			vec3(0.0, 0.0, -1.0));
	self->models[3] = mat4_invert(self->views[0]);

	self->views[4] = mat4_look_at(pos, vec3_add(pos, vec3(0.0, 0.0, 1.0)),
			vec3(0.0, -1.0, 0.0));
	self->models[4] = mat4_invert(self->views[0]);

	self->views[5] = mat4_look_at(pos, vec3_add(pos, vec3(0.0, 0.0, -1.0)),
			vec3(0.0, -1.0, 0.0));
	self->models[5] = mat4_invert(self->views[0]);

	self->last_update = -1;

	return 1;
}

int c_probe_render(c_probe_t *self, uint signal)
{
	int face;

	if(!self->map) return 0;
	c_spacial_t *ps = c_spacial(self);

	if(self->last_update == g_update_id) return 0;

	c_renderer_t *renderer = c_renderer(&candle->systems);
	renderer->bound_probe = c_entity(self);
	renderer->bound_camera_pos = ps->pos;
	renderer->bound_projection = &self->projection;
#ifdef MESH4
	renderer->bound_angle4 = renderer->angle4;
#endif

	for(face = 0; face < 6; face++)
	{
		texture_target(self->map, face);
		glClear(GL_DEPTH_BUFFER_BIT);

		renderer->bound_view = &self->views[face];
		renderer->bound_camera_model = &self->models[face];
		renderer->bound_exposure = 1.0f;

		int res = entity_signal(c_entity(self), signal, NULL);
		if(!res) return 0;

	}

	self->last_update = g_update_id;
	c_renderer(&candle->systems)->bound_probe = entity_null;
	return 1;
}


void c_probe_destroy(c_probe_t *self)
{
	free(self);
}

DEC_CT(ct_probe)
{
	ct_t *ct = ct_new("c_probe", &ct_probe, sizeof(c_probe_t),
			(init_cb)c_probe_init, 1, ct_spacial);

	ct_listener(ct, ENTITY, spacial_changed, c_probe_update_position);
}


