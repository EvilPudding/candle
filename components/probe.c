#include "probe.h"
#include "spacial.h"
#include "candle.h"
#include <stdlib.h>

DEC_CT(ct_probe);

void c_probe_init(c_probe_t *self)
{
	self->super = component_new(ct_probe);
}

c_probe_t *c_probe_new(int map_size, shader_t *shader)
{
	c_probe_t *self = malloc(sizeof *self);
	c_probe_init(self);
	self->shader = shader;

	self->map = texture_cubemap(map_size, map_size, 1);

	self->last_update = 0;

	return self;
}

static int c_probe_update_position(c_probe_t *self)
{
	vec3_t pos = c_spacial(&self->super.entity)->pos;

	self->projection = mat4_perspective(M_PI / 2.0f, 1.0f, 0.5f, 100.5f); 

	self->views[0] = mat4_look_at(pos, vec3_add(pos, vec3(1.0, 0.0, 0.0)),
			vec3(0.0,-1.0,0.0));

	self->views[1] = mat4_look_at(pos, vec3_add(pos, vec3(-1.0, 0.0, 0.0)),
			vec3(0.0, -1.0, 0.0));

	self->views[2] = mat4_look_at(pos, vec3_add(pos, vec3(0.0, 1.0, 0.0)),
			vec3(0.0, 0.0, 1.0));

	self->views[3] = mat4_look_at(pos, vec3_add(pos, vec3(0.0, -1.0, 0.0)),
			vec3(0.0, 0.0, -1.0));

	self->views[4] = mat4_look_at(pos, vec3_add(pos, vec3(0.0, 0.0, 1.0)),
			vec3(0.0, -1.0, 0.0));

	self->views[5] = mat4_look_at(pos, vec3_add(pos, vec3(0.0, 0.0, -1.0)),
			vec3(0.0, -1.0, 0.0));

	self->last_update++;

	return 1;
}

int c_probe_render(c_probe_t *self, uint signal, shader_t *shader)
{
	int face;

	shader_bind(shader);

	c_spacial_t *ps = c_spacial(self);

	if(self->last_update == g_update_id) return 0;

	for(face = 0; face < 6; face++)
	{
		texture_target(self->map, face);
		glClear(GL_DEPTH_BUFFER_BIT);

		shader_bind_probe(shader, c_entity(self));
#ifdef MESH4
		shader_bind_camera(shader, ps->pos, &self->views[face],
				&self->projection, 1.0f, c_renderer(&candle->systems)->angle4);
#else
		shader_bind_camera(shader, ps->pos, &self->views[face],
				&self->projection, 1.0f);
#endif

		int res = entity_signal(c_entity(self), signal, shader);
		if(!res) return 0;
		self->last_update = g_update_id;
	}
	return 1;
}


void c_probe_destroy(c_probe_t *self)
{
	free(self);
}

void c_probe_register(ecm_t *ecm)
{
	ct_t *ct = ecm_register(ecm, "Probe", &ct_probe,
			sizeof(c_probe_t), (init_cb)c_probe_init, 1, ct_spacial);

	ct_register_listener(ct, SAME_ENTITY, spacial_changed,
			(signal_cb)c_probe_update_position);
}


