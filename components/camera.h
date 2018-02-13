#ifndef CAMERA_H
#define CAMERA_H

#include <ecm.h>
#include <glutil.h>

typedef struct
{
	c_t super; /* extends c_t */

	mat4_t projection_matrix;
	mat4_t view_matrix;
	vec3_t pos;
	int view_cached;
	float near, far, fov;
	float exposure;
} c_camera_t;

DEF_CASTER(ct_camera, c_camera, c_camera_t)

c_camera_t *c_camera_new(float fov, float near, float far);
c_camera_t *c_camera_clone(c_camera_t *self);
vec3_t c_camera_real_pos(c_camera_t *cam, float depth, vec2_t coord);
void c_camera_update_view(c_camera_t *self);

void c_camera_register(ecm_t *ecm);

#endif /* !CAMERA_H */
