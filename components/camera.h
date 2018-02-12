#ifndef CAMERA_H
#define CAMERA_H

#include <ecm.h>
#include <glutil.h>

typedef struct
{
	c_t super; /* extends c_t */

	int active;
	mat4_t projection_matrix;
	mat4_t view_matrix;
	vec3_t pos;
#ifdef MESH4
	float angle4;
#endif
	int view_cached;
	float near, far, fov;
	float exposure;
} c_camera_t;

DEF_CASTER(ct_camera, c_camera, c_camera_t)

c_camera_t *c_camera_new(int active, float fov, float near, float far,
		int width, int height);
void c_camera_update_view(c_camera_t *self);
void c_camera_register(ecm_t *ecm);

entity_t ecm_get_camera(ecm_t *self);

#endif /* !CAMERA_H */
