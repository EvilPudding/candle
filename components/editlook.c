#include "editlook.h"
#include <candle.h>
#include <systems/window.h>
#include <systems/editmode.h>
#include <components/spatial.h>
#include <components/camera.h>
/* #include <components/camera.h> */
#include <systems/mouse.h>
#include <systems/keyboard.h>
#include <systems/controller.h>
#include <math.h>


void c_editlook_init(c_editlook_t *self)
{
	self->win_min_side = 1080;
	c_mouse(self)->visible = false;
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

	c_spatial_t *sc = c_spatial(self);

	float inc = 1.0f - (event->y * 0.1f);

	c_spatial_set_pos(sc, vec3_mix(edit->mouse_position, sc->pos, inc));

	return STOP;
}

float fake_x;
float fake_y;
int c_editlook_mouse_release(c_editlook_t *self, mouse_button_data *event)
{
	if(event->button == SDL_BUTTON_RIGHT)
	{
		self->panning = 0;
		if (c_mouse_active(c_mouse(self)))
		{
			c_mouse_deactivate(c_mouse(self));
			return STOP;
		}
	}
	return CONTINUE;
}

int c_editlook_mouse_move(c_editlook_t *self, mouse_move_data *event)
{
	c_editmode_t *edit = c_editmode(&SYS);
	if (!edit->control) return CONTINUE;

	c_spatial_t *sc = c_spatial(self);
	c_camera_t *cam = c_camera(self);
	if(!cam) return CONTINUE;

	if (!c_mouse_active(c_mouse(self)))
	{
		if (!c_mouse(edit)->right) return CONTINUE;
		c_mouse_activate(c_mouse(self));
	}

	if(c_keyboard(&SYS)->shift)
	{
		fake_x -= event->sx;
		fake_y -= event->sy;
		c_window_t *window = c_window(&SYS);
		float px = fake_x / window->width;
		float py = 1.0f - fake_y / window->height;

		if(!self->panning)
		{
			self->panning = 1;
			c_editmode_update_mouse(edit, fake_x, fake_y);

			self->pan_diff = vec3_sub(sc->pos, edit->mouse_position);
		}

		vec3_t old_pos = sc->pos;

		vec3_t new_mpos = c_camera_real_pos(cam, edit->mouse_screen_pos.z, vec2(px, py));

		vec3_t new_pos = vec3_add(self->pan_diff, new_mpos);
		c_spatial_set_pos(sc, new_pos);

		vec3_t diff = vec3_sub(new_pos, old_pos);
		self->pan_diff = vec3_sub(self->pan_diff, diff);

		return STOP;
	}


	float frac = self->sensitivity / self->win_min_side;

	float inc_y = -(event->sx * frac);
	float inc_x = -(event->sy * frac);


	const vec3_t pivot = c_keyboard(&SYS)->ctrl ? sc->pos :
		edit->mouse_position;

	vec3_t diff = vec3_sub(sc->pos, pivot);

	float cosy = cosf(inc_y);
	float siny = sinf(inc_y);
	float cosx = cosf(inc_x);
	float sinx = sinf(inc_x);

	float oldx = sc->rot.x;

	c_spatial_rotate_X(sc, -oldx);
	c_spatial_rotate_Y(sc, inc_y);

	c_spatial_lock(sc);
	sc->pos = vec3_add(pivot, vec3_rotate(diff, c_spatial_upwards(sc), cosy, siny));

	diff = vec3_sub(sc->pos, pivot);

	c_spatial_set_pos(sc, vec3_add(pivot,
				vec3_rotate(diff, c_spatial_forward(sc), cosx, sinx)));

	c_spatial_rotate_X(sc, oldx + inc_x);
	c_spatial_unlock(sc);

	return STOP;
}

int c_editlook_controller(c_editlook_t *self, controller_axis_t *event)
{
	c_spatial_t *sc = c_spatial(self);
	if (event->side == 0)
	{
		float inc_y = event->y;
		float inc_x = event->x;
		c_spatial_lock(sc);
		vec3_t pos = vec3_add(sc->pos, vec3_scale(c_spatial_sideways(sc), inc_y));
		pos = vec3_add(pos, vec3_scale(c_spatial_forward(sc), inc_x));
		c_spatial_set_pos(sc, pos);
		c_spatial_unlock(sc);

	}
	else if (event->side == 1)
	{
		float frac = self->sensitivity / 2.f;

		float inc_y = -(event->x * frac);
		float inc_x = -(event->y * frac);

		float oldx = sc->rot.x;

		c_spatial_rotate_X(sc, -oldx);
		c_spatial_rotate_Y(sc, inc_y);

		c_spatial_lock(sc);

		c_spatial_rotate_X(sc, oldx + inc_x);
		c_spatial_unlock(sc);

	}
	return CONTINUE;
}

REG()
{
	ct_t *ct = ct_new("editLook", sizeof(c_editlook_t),
			c_editlook_init, NULL, 2, ref("spatial"), ref("mouse"));

	ct_listener(ct, WORLD, 0, sig("mouse_wheel"), c_editlook_mouse_wheel);

	ct_listener(ct, WORLD, 100, sig("mouse_move"), c_editlook_mouse_move);

	ct_listener(ct, WORLD, 100, sig("controller_axis"), c_editlook_controller);

	ct_listener(ct, WORLD, 0, sig("window_resize"), c_editlook_window_resize);

	ct_listener(ct, ENTITY, 100, sig("mouse_release"), c_editlook_mouse_release);

}

