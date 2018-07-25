#ifndef LIGHT_H
#define LIGHT_H

#include <utils/glutil.h>
#include <ecs/ecm.h>
#include <utils/texture.h>
#include <utils/material.h>

typedef struct
{
	c_t super;

	vec4_t color;
	float shadow_size;
	float radius;

} c_light_t;

DEF_CASTER("light", c_light, c_light_t)

c_light_t *c_light_new(float radius, vec4_t color, int shadow_size);
void c_light_destroy(c_light_t *self);

#endif /* !LIGHT_H */
