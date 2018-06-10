#include "probe.h"
#include "spacial.h"
#include "candle.h"
#include <stdlib.h>
#include <components/model.h>


c_probe_t *c_probe_new(int map_size)
{
	c_probe_t *self = component_new("probe");

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

	return CONTINUE;
}

int c_probe_render(c_probe_t *self, uint signal)
{
	int f;

	if(!self->map) return STOP;
	c_spacial_t *ps = c_spacial(self);

	if(self->last_update == g_update_id) return STOP;

	c_renderer_t *renderer = c_renderer(&SYS);
	renderer->bound_probe = c_entity(self);
	renderer->bound_camera_pos = ps->pos;
	renderer->bound_projection = &self->projection;

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glDepthMask(GL_TRUE);
	for(f = 0; f < 6; f++)
	{
		texture_target(self->map, NULL, f);
		glClear(GL_DEPTH_BUFFER_BIT);

		renderer->bound_view = &self->views[f];
		renderer->bound_camera_model = &self->models[f];
		renderer->bound_exposure = 1.0f;

		int res = entity_signal(c_entity(self), signal, NULL);
		if(res == STOP) return STOP;
	}

	self->last_update = g_update_id;
	renderer->bound_probe = entity_null;
	return CONTINUE;
}


void c_probe_destroy(c_probe_t *self)
{
	/* TODO free textures */
}

REG()
{
	ct_t *ct = ct_new("probe", sizeof(c_probe_t),
			NULL, c_probe_destroy, 1, ref("spacial"));

	ct_listener(ct, ENTITY, sig("spacial_changed"), c_probe_update_position);
}


