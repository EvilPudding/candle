#ifndef PROBE_H
#define PROBE_H

#include "../glutil.h"
#include <ecm.h>
#include "../texture.h"
#include "../material.h"

typedef struct c_probe_t
{
	c_t super;
	mat4_t projection;
	mat4_t views[6];
	texture_t *map;
	int last_update;
	before_draw_cb before_draw;
	shader_t *shader;
} c_probe_t;

DEF_CASTER(ct_probe, c_probe, c_probe_t)

c_probe_t *c_probe_new(int map_size, shader_t *shader);
void c_probe_destroy(c_probe_t *self);
void c_probe_register(ecm_t *ecm);

#endif /* !PROBE_H */
