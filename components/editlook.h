#ifndef EDITLOOK_H
#define EDITLOOK_H

#include <ecs/ecm.h>

typedef struct
{
	c_t super; /* extends c_t */
	float win_min_side;
	float sensitivity;
	int pressed_r;
	int panning;
	vec3_t pan_diff;
} c_editlook_t;

DEF_CASTER("editLook", c_editlook, c_editlook_t)

c_editlook_t *c_editlook_new(void);

vec3_t c_editlook_up(c_editlook_t *self);

#endif /* !EDITLOOK_H */
