#ifndef WINDOW_H
#define WINDOW_H

#include <ecm.h>

extern int window_width;
extern int window_height;

DEF_SIG(window_resize);

typedef struct
{
	int width, height;
} window_resize_data;

typedef struct c_window_t
{
	c_t super;

	int width, height;

	SDL_Window *window;
	SDL_Renderer *display;
	SDL_RendererInfo info;
	SDL_GLContext *context;

	const unsigned char *key_state;

	entity_t quad;
	shader_t *quad_shader;
} c_window_t;

DEF_CASTER(ct_window, c_window, c_window_t)

int c_window_draw(c_window_t *self);
void c_window_rect(c_window_t *self, int x, int y, int width, int height,
		texture_t *texture);


c_window_t *c_window_new(int width, int height);
void c_window_register(void);

#endif /* !WINDOW_H */
