#ifndef PROBE_H
#define PROBE_H

#include <utils/glutil.h>
#include <ecs/ecm.h>
#include <utils/texture.h>
#include <utils/material.h>
#include <utils/shader.h>

typedef struct c_probe_t
{
	c_t super;
	mat4_t projection;
	mat4_t views[6];
	mat4_t models[6];
	texture_t *map;
	int last_update;
} c_probe_t;

DEF_CASTER("probe", c_probe, c_probe_t)

c_probe_t *c_probe_new(int map_size);
void c_probe_destroy(c_probe_t *self);
int c_probe_update_position(c_probe_t *self);
int c_probe_render(c_probe_t *self, uint signal);

#endif /* !PROBE_H */
