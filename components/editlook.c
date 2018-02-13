#include "editlook.h"
#include <candle.h>
#include <systems/window.h>
#include <systems/editmode.h>
#include <components/spacial.h>
/* #include <components/camera.h> */
#include <mouse.h>
#include <keyboard.h>
#include <math.h>

DEC_CT(ct_editlook);

void c_editlook_init(c_editlook_t *self)
{
	self->super = component_new(ct_editlook);
	self->win_min_side = 1080;

	self->panning = 0;
	self->pressed_r = 0;
}

c_editlook_t *c_editlook_new()
{
	c_editlook_t *self = malloc(sizeof *self);
	c_editlook_init(self);

	self->sensitivity = 0.8;

	return self;
}

static int c_editlook_window_resize(c_editlook_t *self,
		window_resize_data *event)
{
	self->win_min_side = (event->width < event->height) ?
		event->width : event->height;
	return 1;
}

int c_editlook_mouse_wheel(c_editlook_t *self, mouse_button_data *event)
{
	c_editmode_t *edit = c_editmode(&candle->systems);
	if(!edit->control) return 1;

	c_spacial_t *sc = c_spacial(self);

	float inc = 1.0f - (event->y * 0.1f);

	c_spacial_set_pos(sc, vec3_mix(edit->mouse_position, sc->pos, inc));

	return 1;
}

/* TODO remove this extern reference */
extern SDL_Window *mainWindow;

int start_x;
int start_y;
float fake_x;
float fake_y;
int c_editlook_mouse_press(c_editlook_t *self, mouse_button_data *event)
{
	if(event->button == SDL_BUTTON_RIGHT)
	{
		self->pressed_r = 1;
		fake_x = start_x = event->x;
		fake_y = start_y = event->y;

		SDL_ShowCursor(SDL_FALSE); 
		/* // SDL_SetWindowGrab(mainWindow, SDL_TRUE); */
		SDL_SetRelativeMouseMode(SDL_TRUE);
	}
	return 1;
}

int c_editlook_mouse_release(c_editlook_t *self, mouse_button_data *event)
{
	self->panning = 0;
	if(event->button == SDL_BUTTON_RIGHT)
	{
		if(self->pressed_r)
		{
			/* // SDL_SetWindowGrab(mainWindow, SDL_FALSE); */
			SDL_SetRelativeMouseMode(SDL_FALSE);
			SDL_WarpMouseInWindow(mainWindow, start_x, start_y);
			SDL_ShowCursor(SDL_TRUE); 
		}
		self->pressed_r = 0;
	}
	return 1;
}

int c_editlook_mouse_move(c_editlook_t *self, mouse_move_data *event)
{
	c_editmode_t *edit = c_editmode(&candle->systems);
	c_renderer_t *renderer = c_renderer(&candle->systems);
	if(!edit->control) return 1;
	if(!self->pressed_r) return 1;

	c_spacial_t *sc = c_spacial(self);
	c_camera_t *cam = c_camera(self);
	if(!cam) return 1;

	fake_x += event->sx;
	fake_y += event->sy;
	if(candle->shift)
	{
		float px = fake_x / renderer->width;
		float py = 1.0f - fake_y / renderer->height;

		if(!self->panning)
		{
			self->panning = 1;
			c_editmode_update_mouse(edit, fake_x, fake_y);

			self->pan_diff = vec3_sub(sc->pos, edit->mouse_position);
		}

		vec3_t old_pos = sc->pos;

		c_camera_update_view(cam);
		vec3_t pos = c_camera_real_pos(cam, edit->mouse_depth, vec2(px, py));

		vec3_t new_pos = vec3_add(self->pan_diff, pos);
		c_spacial_set_pos(sc, new_pos);

		vec3_t diff = vec3_sub(sc->pos, old_pos);

		self->pan_diff = vec3_sub(self->pan_diff, diff);


		c_camera_update_view(cam);
		new_pos = vec3_add(self->pan_diff, edit->mouse_position);
		c_spacial_set_pos(sc, new_pos);

		return 0;
	}


	float frac = self->sensitivity / self->win_min_side;

	float inc_y = -(event->sx * frac) * self->pressed_r;
	float inc_x = -(event->sy * frac) * self->pressed_r;


	/* if(rot > max_up) rot = max_up; */
	/* if(rot < max_down) rot = max_down; */
	const vec3_t pivot = edit->mouse_position;

	vec3_t diff = vec3_sub(sc->pos, pivot);
	/* float radius = vec3_len(diff); */

	float cosy = cosf(inc_y);
	float siny = sinf(inc_y);
	float cosx = cosf(inc_x);
	float sinx = sinf(inc_x);

	c_spacial_rotate_X(sc, inc_x);
	c_spacial_rotate_Y(sc, inc_y);


	c_spacial_set_pos(sc, vec3_add(pivot,
				vec3_rotate(diff, sc->upwards, cosy, siny)));

	diff = vec3_sub(sc->pos, pivot);

	c_spacial_set_pos(sc, vec3_add(pivot,
				vec3_rotate(diff, sc->forward, cosx, sinx)));

	return 0;
}

void c_editlook_register(ecm_t *ecm)
{
	ct_t *ct = ecm_register(ecm, "EditLook", &ct_editlook,
			sizeof(c_editlook_t), (init_cb)c_editlook_init,
			1, ct_spacial);

	ct_register_listener(ct, WORLD, mouse_wheel,
			(signal_cb)c_editlook_mouse_wheel);

	ct_register_listener(ct, WORLD, mouse_move,
			(signal_cb)c_editlook_mouse_move);

	ct_register_listener(ct, WORLD, window_resize,
			(signal_cb)c_editlook_window_resize);

	ct_register_listener(ct, WORLD, mouse_press,
			(signal_cb)c_editlook_mouse_press);

	ct_register_listener(ct, WORLD, mouse_release,
			(signal_cb)c_editlook_mouse_release);

}

