#ifndef CAMERA_H
#define CAMERA_H

#include <ecs/ecm.h>
#include <utils/glutil.h>
#include <utils/renderer.h>

typedef struct
{
	c_t super; /* extends c_t */
	int view_cached;
	float exposure;
	int width;
	int height;
	renderer_t *renderer;
	int camid;
	int active;
	int window;
	int auto_exposure;
	int modified;
} c_camera_t;

DEF_CASTER("camera", c_camera, c_camera_t)

c_camera_t *c_camera_new(float fov, float near, float far,
		int auto_exposure, int active, int window, renderer_t *renderer);
c_camera_t *c_camera_clone(c_camera_t *self);
vec3_t c_camera_real_pos(c_camera_t *cam, float depth, vec2_t coord);
vec3_t c_camera_screen_pos(c_camera_t *self, vec3_t pos);
vec3_t c_camera_screen_pos_flat(c_camera_t *self, vec3_t pos);
void c_camera_update_view(c_camera_t *self);
float c_camera_unlinearize(c_camera_t *self, float depth);
void c_camera_assign(c_camera_t *self);
int c_camera_pre_draw(c_camera_t *self);

#endif /* !CAMERA_H */
