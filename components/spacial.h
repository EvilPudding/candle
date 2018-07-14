#ifndef SPACIAL_H
#define SPACIAL_H

#include <ecm.h>
#include <utils/glutil.h>

typedef struct
{
	c_t super; /* extends c_t */

	vec3_t pos;
	vec3_t rot;
	vec3_t scale;
	vec4_t rot_quat;
	mat4_t model_matrix;
	int lock_count;
	int modified;

	int update_id;
#ifdef MESH4
	float angle4;
#endif
} c_spacial_t;

DEF_CASTER("spacial", c_spacial, c_spacial_t)

c_spacial_t *c_spacial_new(void);
void c_spacial_init(c_spacial_t *self);
vec3_t c_spacial_up(c_spacial_t *self);

void c_spacial_lock(c_spacial_t *self);
void c_spacial_unlock(c_spacial_t *self);

void c_spacial_scale(c_spacial_t *self, vec3_t scale);
void c_spacial_look_at(c_spacial_t *self, vec3_t center, vec3_t up);
void c_spacial_set_pos(c_spacial_t *self, vec3_t pos);
void c_spacial_set_model(c_spacial_t *self, mat4_t model);
void c_spacial_set_rot(c_spacial_t *self, float x, float y, float z, float angle);
void c_spacial_set_scale(c_spacial_t *self, vec3_t scale);
void c_spacial_update_model_matrix(c_spacial_t *self);

void c_spacial_rotate_X(c_spacial_t *self, float angle);
void c_spacial_rotate_Y(c_spacial_t *self, float angle);
void c_spacial_rotate_Z(c_spacial_t *self, float angle);

vec3_t c_spacial_forward(c_spacial_t *self);
vec3_t c_spacial_sideways(c_spacial_t *self);
vec3_t c_spacial_upwards(c_spacial_t *self);

void c_spacial_rotate_axis(c_spacial_t *self, vec3_t axis, float angle);

#endif /* !SPACIAL_H */
