#ifndef SPACIAL_H
#define SPACIAL_H

#include <ecs/ecm.h>

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
} c_spatial_t;

DEF_CASTER("spatial", c_spatial, c_spatial_t)

c_spatial_t *c_spatial_new(void);
void c_spatial_init(c_spatial_t *self);
vec3_t c_spatial_up(c_spatial_t *self);

void c_spatial_lock(c_spatial_t *self);
void c_spatial_unlock(c_spatial_t *self);

void c_spatial_scale(c_spatial_t *self, vec3_t scale);
void c_spatial_look_at(c_spatial_t *self, vec3_t center, vec3_t up);
void c_spatial_set_pos(c_spatial_t *self, vec3_t pos);
void c_spatial_set_model(c_spatial_t *self, mat4_t model);
void c_spatial_set_rot(c_spatial_t *self, float x, float y, float z, float angle);
void c_spatial_set_scale(c_spatial_t *self, vec3_t scale);
void c_spatial_update_model_matrix(c_spatial_t *self);
void c_spatial_assign(c_spatial_t *self, c_spatial_t *other);

void c_spatial_rotate_X(c_spatial_t *self, float angle);
void c_spatial_rotate_Y(c_spatial_t *self, float angle);
void c_spatial_rotate_Z(c_spatial_t *self, float angle);

vec3_t c_spatial_forward(c_spatial_t *self);
vec3_t c_spatial_sideways(c_spatial_t *self);
vec3_t c_spatial_upwards(c_spatial_t *self);

void c_spatial_rotate_axis(c_spatial_t *self, vec3_t axis, float angle);

#endif /* !SPACIAL_H */
