#include "editlook.h"
#include <candle.h>
#include <systems/window.h>
#include <systems/editmode.h>
#include <components/spacial.h>
#include <components/camera.h>
/* #include <components/camera.h> */
#include <systems/mouse.h>
#include <systems/keyboard.h>
#include <math.h>


void c_editlook_init(c_editlook_t *self)
{
	self->win_min_side = 1080;
}

c_editlook_t *c_editlook_new()
{
	c_editlook_t *self = component_new("editLook");

	self->sensitivity = 0.8;

	return self;
}

static int c_editlook_window_resize(c_editlook_t *self,
		window_resize_data *event)
{
	self->win_min_side = (event->width < event->height) ?
		event->width : event->height;
	return CONTINUE;
}

int c_editlook_mouse_wheel(c_editlook_t *self, mouse_button_data *event)
{
	c_editmode_t *edit = c_editmode(&SYS);
	if(!edit->control) return CONTINUE;

	c_spacial_t *sc = c_spacial(self);

	float inc = 1.0f - (event->y * 0.1f);

	c_spacial_set_pos(sc, vec3_mix(edit->mouse_position, sc->pos, inc));

	return CONTINUE;
}

float fake_x;
float fake_y;
int c_editlook_mouse_press(c_editlook_t *self, mouse_button_data *event)
{
	if(event->button == SDL_BUTTON_RIGHT)
	{
		self->pressed_r = 1;
		fake_x = event->x;
		fake_y = event->y;
	}
	return CONTINUE;
}

int c_editlook_mouse_release(c_editlook_t *self, mouse_button_data *event)
{
	if(event->button == SDL_BUTTON_RIGHT)
	{
		if(self->pressed_r)
		{
			candle_release_mouse(c_entity(self), !self->panning);
		}
		self->pressed_r = 0;
		self->panning = 0;
	}
	return CONTINUE;
}

int c_editlook_mouse_move(c_editlook_t *self, mouse_move_data *event)
{
	c_editmode_t *edit = c_editmode(&SYS);
	c_window_t *window = c_window(&SYS);
	if(!edit->control) return CONTINUE;
	if(!self->pressed_r) return CONTINUE;

	candle_grab_mouse(c_entity(self), 0);

	c_spacial_t *sc = c_spacial(self);
	c_camera_t *cam = c_camera(self);
	if(!cam) return CONTINUE;

	fake_x += event->sx;
	fake_y += event->sy;
	if(c_keyboard(&SYS)->shift)
	{
		float px = fake_x / window->width;
		float py = 1.0f - fake_y / window->height;

		if(!self->panning)
		{
			self->panning = 1;
			c_editmode_update_mouse(edit, fake_x, fake_y);

			self->pan_diff = vec3_sub(sc->pos, edit->mouse_position);
		}

		vec3_t old_pos = sc->pos;

		vec3_t pos = c_camera_real_pos(cam, edit->mouse_screen_pos.z, vec2(px, py));

		vec3_t new_pos = vec3_add(self->pan_diff, pos);
		c_spacial_set_pos(sc, new_pos);

		vec3_t diff = vec3_sub(sc->pos, old_pos);

		self->pan_diff = vec3_sub(self->pan_diff, diff);


		new_pos = vec3_add(self->pan_diff, edit->mouse_position);
		c_spacial_set_pos(sc, new_pos);

		return STOP;
	}


	float frac = self->sensitivity / self->win_min_side;

	float inc_y = -(event->sx * frac) * self->pressed_r;
	float inc_x = -(event->sy * frac) * self->pressed_r;


	/* if(rot > max_up) rot = max_up; */
	/* if(rot < max_down) rot = max_down; */
	const vec3_t pivot = c_keyboard(&SYS)->ctrl ? sc->pos :
		edit->mouse_position;

	vec3_t diff = vec3_sub(sc->pos, pivot);
	/* float radius = vec3_len(diff); */

	float cosy = cosf(inc_y);
	float siny = sinf(inc_y);
	float cosx = cosf(inc_x);
	float sinx = sinf(inc_x);

	float oldx = sc->rot.x;

	c_spacial_rotate_X(sc, -oldx);
	c_spacial_rotate_Y(sc, inc_y);

	c_spacial_lock(sc);
	sc->pos = vec3_add(pivot, vec3_rotate(diff, c_spacial_upwards(sc), cosy, siny));

	diff = vec3_sub(sc->pos, pivot);

	c_spacial_set_pos(sc, vec3_add(pivot,
				vec3_rotate(diff, c_spacial_forward(sc), cosx, sinx)));

	c_spacial_rotate_X(sc, oldx + inc_x);
	c_spacial_unlock(sc);

	return STOP;
}

REG()
{
	ct_t *ct = ct_new("editLook", sizeof(c_editlook_t),
			c_editlook_init, NULL, 1, ref("spacial"));

	ct_listener(ct, WORLD, sig("mouse_wheel"), c_editlook_mouse_wheel);

	ct_listener(ct, WORLD | 100, sig("mouse_move"), c_editlook_mouse_move);

	ct_listener(ct, WORLD, sig("window_resize"), c_editlook_window_resize);

	ct_listener(ct, WORLD, sig("mouse_press"), c_editlook_mouse_press);

	ct_listener(ct, WORLD, sig("mouse_release"), c_editlook_mouse_release);

}

