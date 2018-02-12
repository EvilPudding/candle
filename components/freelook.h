#ifndef FREELOOK_H
#define FREELOOK_H

#include <ecm.h>
#include <glutil.h>

typedef struct
{
	c_t super; /* extends c_t */
	float win_min_side;
	float sensitivity;
	entity_t x_control, y_control, force_down;
} c_freelook_t;

DEF_CASTER(ct_freelook, c_freelook, c_freelook_t)

c_freelook_t *c_freelook_new(entity_t force_down, float sensitivity);

void c_freelook_set_controls(c_freelook_t *self,
		entity_t x_control, entity_t y_control);

void c_freelook_register(ecm_t *ecm);

vec3_t c_freelook_up(c_freelook_t *self);

#endif /* !FREELOOK_H */
