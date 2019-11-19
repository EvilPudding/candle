#include <SDL2/SDL.h>
#include <systems/window.h>
#include "mouse.h"

entity_t g_active_mouse = SYS;

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
	return component_new("mouse");
}

int32_t c_mouse_event(c_mouse_t *self, const SDL_Event *event)
{
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

	mouse_button_data bdata;
	mouse_move_data mdata;
	if (g_active_mouse != c_entity(self))
		return CONTINUE;
	const entity_t target = c_entity(self) == SYS ? entity_null : c_entity(self);
	switch(event->type)
	{
		case SDL_MOUSEWHEEL:
			bdata = (mouse_button_data){event->wheel.x, event->wheel.y,
				event->wheel.direction, SDL_BUTTON_MIDDLE, event->button.clicks};
			if (target == entity_null)
			{
				return entity_signal(target, sig("mouse_wheel"), &bdata, NULL);
			}
			else
			{
				return entity_signal_same(target, sig("mouse_wheel"), &bdata, NULL);
			}
		case SDL_MOUSEBUTTONUP:
			bdata = (mouse_button_data){event->button.x, event->button.y, 0,
				event->button.button, event->button.clicks};
			if (target == entity_null)
			{
				return entity_signal(target, sig("mouse_release"), &bdata, NULL);
			}
			else
			{
				return entity_signal_same(target, sig("mouse_release"), &bdata, NULL);
			}
		case SDL_MOUSEBUTTONDOWN:
			bdata = (mouse_button_data){event->button.x, event->button.y, 0,
				event->button.button, event->button.clicks};
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
			mdata = (mouse_move_data){event->motion.xrel, event->motion.yrel,
				event->motion.x, event->motion.y};
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
	if (g_active_mouse != c_entity(self))
	{
		return;
	}
	g_active_mouse = self->previous_active;
	c_mouse_t *active_mouse = c_mouse(&g_active_mouse);
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

REG()
{
	ct_t *ct = ct_new("mouse", sizeof(c_mouse_t), c_mouse_init, NULL, 0);

	signal_init(sig("mouse_move"), sizeof(mouse_move_data));
	signal_init(sig("mouse_press"), sizeof(mouse_button_data));
	signal_init(sig("mouse_release"), sizeof(mouse_button_data));
	signal_init(sig("mouse_wheel"), sizeof(mouse_button_data));
	ct_listener(ct, WORLD, -1, sig("event_handle"), c_mouse_event);
}
