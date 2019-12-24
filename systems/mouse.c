#include <SDL2/SDL.h>
#include <systems/window.h>
#include "mouse.h"

entity_t g_active_mouse = 0x0000000100000001ul;

void c_mouse_init(c_mouse_t *self)
{
	self->visible = true;
	self->previous_active = entity_null;
	if (!entity_exists(g_active_mouse))
	{
		c_mouse_activate(self);
	}
}

c_mouse_t *c_mouse_new()
{
	return component_new(ct_mouse);
}

int32_t c_mouse_event(c_mouse_t *self, const SDL_Event *event)
{
	mouse_button_data bdata;
	mouse_move_data mdata;
	entity_t target;
	if (event->type == SDL_MOUSEBUTTONUP || event->type == SDL_MOUSEBUTTONDOWN)
	{
		if (event->button.button == SDL_BUTTON_RIGHT)
		{
			self->right = event->type == SDL_MOUSEBUTTONDOWN;
		}
		if (event->button.button == SDL_BUTTON_LEFT)
		{
			self->left = event->type == SDL_MOUSEBUTTONDOWN;
		}
	}

	if (g_active_mouse != c_entity(self))
		return CONTINUE;
	target = c_entity(self) == SYS ? entity_null : c_entity(self);
	switch(event->type)
	{
		case SDL_MOUSEWHEEL:
			bdata.x = event->wheel.x;
			bdata.y = event->wheel.y;
			bdata.direction = event->wheel.direction;
			bdata.button = SDL_BUTTON_MIDDLE;
			bdata.clicks = event->button.clicks;
			if (target == entity_null)
			{
				return entity_signal(target, sig("mouse_wheel"), &bdata, NULL);
			}
			else
			{
				return entity_signal_same(target, sig("mouse_wheel"), &bdata, NULL);
			}
		case SDL_MOUSEBUTTONUP:
			bdata.x = event->button.x;
			bdata.y = event->button.y;
			bdata.direction = 0.0f;
			bdata.button = event->button.button;
			bdata.clicks = event->button.clicks;
			if (target == entity_null)
			{
				return entity_signal(target, sig("mouse_release"), &bdata, NULL);
			}
			else
			{
				return entity_signal_same(target, sig("mouse_release"), &bdata, NULL);
			}
		case SDL_MOUSEBUTTONDOWN:
			bdata.x = event->button.x;
			bdata.y = event->button.y;
			bdata.direction = 0.0f;
			bdata.button = event->button.button;
			bdata.clicks = event->button.clicks;
			if (target == entity_null)
			{
				return entity_signal(target, sig("mouse_press"), &bdata, NULL);
			}
			else
			{
				return entity_signal_same(target, sig("mouse_press"), &bdata, NULL);
			}
		case SDL_MOUSEMOTION:
			self->x = event->motion.x;
			self->y = event->motion.y;
			mdata.sx = event->motion.xrel;
			mdata.sy = event->motion.yrel;
			mdata.x = event->motion.x;
			mdata.y = event->motion.y;
			if (target == entity_null)
			{
				return entity_signal(target, sig("mouse_move"), &mdata, NULL);
			}
			else
			{
				return entity_signal_same(target, sig("mouse_move"), &mdata, NULL);
			}
	}
	return CONTINUE;
}

bool_t c_mouse_active(const c_mouse_t *self)
{
	return g_active_mouse == c_entity(self);
}

void c_mouse_deactivate(c_mouse_t *self)
{
	c_mouse_t *active_mouse;
	if (g_active_mouse != c_entity(self))
	{
		return;
	}
	g_active_mouse = self->previous_active;
	active_mouse = c_mouse(&g_active_mouse);
	if (self->visible != active_mouse->visible) {
		SDL_ShowCursor(active_mouse->visible); 
		SDL_SetRelativeMouseMode(!active_mouse->visible);
		SDL_WarpMouseInWindow(c_window(&SYS)->window,
		                      active_mouse->x, active_mouse->y);
	}
}

void c_mouse_activate(c_mouse_t *self)
{
	if (g_active_mouse != c_entity(self))
	{
		self->right = c_mouse(&g_active_mouse)->right;
		self->left = c_mouse(&g_active_mouse)->left;
		self->previous_active = g_active_mouse;
	}

	if (self->visible != c_mouse(&g_active_mouse)->visible) {
		SDL_ShowCursor(self->visible); 
		SDL_SetRelativeMouseMode(!self->visible);
		SDL_WarpMouseInWindow(c_window(&SYS)->window, self->x, self->y);
	}
	g_active_mouse = c_entity(self);
}

void c_mouse_visible(c_mouse_t *self, bool_t visible)
{
	if (self->visible != visible)
	{
		self->visible = visible;
		if (g_active_mouse == c_entity(self))
		{
			SDL_ShowCursor(self->visible); 
			SDL_SetRelativeMouseMode(!self->visible);
		}
	}
}

void ct_mouse(ct_t *self)
{
	ct_init(self, "mouse", sizeof(c_mouse_t));
	ct_set_init(self, (init_cb)c_mouse_init);
	signal_init(ref("mouse_move"), sizeof(mouse_move_data));
	signal_init(ref("mouse_press"), sizeof(mouse_button_data));
	signal_init(ref("mouse_release"), sizeof(mouse_button_data));
	signal_init(ref("mouse_wheel"), sizeof(mouse_button_data));
	ct_listener(self, WORLD, -1, ref("event_handle"), c_mouse_event);
}
