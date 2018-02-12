#ifndef PROBE_H
#define PROBE_H

#include "../glutil.h"
#include <ecm.h>
#include "../texture.h"
#include "../material.h"
#include <shader.h>

typedef struct c_probe_t
{
	c_t super;
	mat4_t projection;
	mat4_t views[6];
	texture_t *map;
	int last_update;
	shader_t *shader;
} c_probe_t;

DEF_CASTER(ct_probe, c_probe, c_probe_t)

c_probe_t *c_probe_new(int map_size, shader_t *shader);
void c_probe_destroy(c_probe_t *self);
void c_probe_register(ecm_t *ecm);
int c_probe_render(c_probe_t *self, uint signal, shader_t *shader);

#endif /* !PROBE_H */
