#ifndef WINDOW_H
#define WINDOW_H

#include "../ecs/ecm.h"
#include "../utils/drawable.h"
#include "../utils/renderer.h"

extern int window_width;
extern int window_height;

typedef struct
{
	int width, height;
} window_resize_data;

typedef struct c_window_t
{
	c_t super;

	int width, height;
	int fullscreen;
	int swap_interval;

	void *window;
	void *context;

	drawable_t draw;
	renderer_t *renderer;
} c_window_t;

DEF_CASTER(ct_window, c_window, c_window_t)

void c_window_handle_resize(c_window_t *self, const void *event);
void c_window_toggle_fullscreen(c_window_t *self);

c_window_t *c_window_new(int width, int height);

void c_window_lock_fps(c_window_t *self, int32_t lock_fps);

#endif /* !WINDOW_H */
