#ifndef EDITLOOK_H
#define EDITLOOK_H

#include <ecm.h>

typedef struct
{
	c_t super; /* extends c_t */
	float win_min_side;
	float sensitivity;
	int pressed_r;
	int controlling;
	int panning;
	vec3_t pan_diff;
} c_editlook_t;

DEF_CASTER(ct_editlook, c_editlook, c_editlook_t)

c_editlook_t *c_editlook_new(void);

void c_editlook_register(ecm_t *ecm);

vec3_t c_editlook_up(c_editlook_t *self);

#endif /* !EDITLOOK_H */
