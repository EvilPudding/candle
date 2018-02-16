#ifndef LIGHT_H
#define LIGHT_H

#include "../glutil.h"
#include <ecm.h>
#include "../texture.h"
#include "../material.h"

typedef struct c_light_t
{
	c_t super;

	vec4_t color;
	float intensity;
	float shadow_size;

} c_light_t;

DEF_SIG(render_shadows);

DEF_CASTER(ct_light, c_light, c_light_t)

c_light_t *c_light_new(float intensity, vec4_t color, int shadow_size);
void c_light_destroy(c_light_t *self);
void c_light_register(void);

#endif /* !LIGHT_H */
