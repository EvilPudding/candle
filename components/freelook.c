#include "freelook.h"
#include "../systems/window.h"
#include "../components/spatial.h"
#include "../components/node.h"
#include "../systems/mouse.h"
#include <math.h>


static void c_freelook_init(c_freelook_t *self)
{
	self->win_min_side = 1080;

	self->x_control = entity_null;
	self->y_control = entity_null;
	self->force_down = entity_null;
}

void c_freelook_set_controls(c_freelook_t *self,
		entity_t x_control, entity_t y_control)
{
	self->x_control = x_control;
	self->y_control = y_control;
}

static int c_freelook_window_resize(c_freelook_t *self,
		window_resize_data *event)
{
	self->win_min_side = (event->width < event->height) ?
		event->width : event->height;
	return CONTINUE;
}

static void c_freelook_update(c_freelook_t *self)
{
	c_spatial_t *sc;
	float rot;
	const float max_up = M_PI / 2.0 - 0.01;
	const float max_down = -M_PI / 2.0 + 0.01;

	if(!entity_exists(self->x_control) || !entity_exists(self->y_control))
	{
		return;
	}

	sc = c_spatial(self);

	rot = sc->rot.x;
	if(rot > max_up) rot = max_up;
	if(rot < max_down) rot = max_down;
	c_spatial_set_rot(sc, 1, 0, 0, rot);

	/* c_spatial_update_model_matrix(c_spatial(self->x_control)); */
	/* c_spatial_update_model_matrix(c_spatial(self->y_control)); */
}

static int c_freelook_mouse_move(c_freelook_t *self, mouse_move_data *event)
{
	float frac = self->sensitivity / self->win_min_side;
	float new_rot;
	c_spatial_t *sc_y, *sc_x;

	sc_x = c_spatial(&self->x_control);
	new_rot = sc_x->rot.y - event->sx * frac;
	c_spatial_set_rot(sc_x, 0, 1, 0, new_rot);


	sc_y = c_spatial(&self->y_control);
	new_rot = sc_y->rot.x - event->sy * frac;
	c_spatial_set_rot(sc_y, 1, 0, 0, new_rot);

	c_freelook_update(self);

	return CONTINUE;
}

void ct_freelook(ct_t *self)
{
	ct_init(self, "freelook", sizeof(c_freelook_t));
	ct_set_init(self, (init_cb)c_freelook_init);
	ct_add_dependency(self, ct_node);
	ct_add_listener(self, WORLD, 0, sig("mouse_move"), c_freelook_mouse_move);
	ct_add_listener(self, ENTITY, 0, sig("entity_created"), c_freelook_update);
	ct_add_listener(self, WORLD, 0, sig("window_resize"), c_freelook_window_resize);
}

c_freelook_t *c_freelook_new(entity_t force_down, float sensitivity)
{
	c_freelook_t *self = component_new(ct_freelook);
	self->sensitivity = sensitivity;

	self->force_down = force_down;

	return self;
}

