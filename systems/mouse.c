#include <systems/window.h>
#include "mouse.h"
#include <GLFW/glfw3.h>

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

int32_t c_mouse_event(c_mouse_t *self, const candle_event_t *event)
{
	mouse_button_data bdata;
	mouse_move_data mdata;
	entity_t target;
	if (event->type == CANDLE_MOUSEBUTTONUP || event->type == CANDLE_MOUSEBUTTONDOWN)
	{
		if (event->key == CANDLE_MOUSE_BUTTON_RIGHT)
		{
			self->right = event->type == CANDLE_MOUSEBUTTONDOWN;
		}
		if (event->key == CANDLE_MOUSE_BUTTON_LEFT)
		{
			self->left = event->type == CANDLE_MOUSEBUTTONDOWN;
		}
	}

	if (g_active_mouse != c_entity(self))
		return CONTINUE;
	target = c_entity(self) == SYS ? entity_null : c_entity(self);
	switch(event->type)
	{
		case CANDLE_MOUSEWHEEL:
			bdata.x = event->wheel.x;
			bdata.y = event->wheel.y;
			bdata.direction = event->wheel.direction;
			bdata.button = CANDLE_MOUSE_BUTTON_MIDDLE;
			if (target == entity_null)
			{
				return entity_signal(target, sig("mouse_wheel"), &bdata, NULL);
			}
			else
			{
				return entity_signal_same(target, sig("mouse_wheel"), &bdata, NULL);
			}
		case CANDLE_MOUSEBUTTONUP:
			bdata.x = self->x;
			bdata.y = self->y;
			bdata.direction = 0.0f;
			bdata.button = event->key;
			if (target == entity_null)
			{
				return entity_signal(target, sig("mouse_release"), &bdata, NULL);
			}
			else
			{
				return entity_signal_same(target, sig("mouse_release"), &bdata, NULL);
			}
		case CANDLE_MOUSEBUTTONDOWN:
			bdata.x = self->x;
			bdata.y = self->y;
			bdata.direction = 0.0f;
			bdata.button = event->key;
			if (target == entity_null)
			{
				return entity_signal(target, sig("mouse_press"), &bdata, NULL);
			}
			else
			{
				return entity_signal_same(target, sig("mouse_press"), &bdata, NULL);
			}
		case CANDLE_MOUSEMOTION:
			mdata.sx = event->mouse.x - self->x;
			mdata.sy = event->mouse.y - self->y;
			self->x = event->mouse.x;
			self->y = event->mouse.y;
			mdata.x = event->mouse.x;
			mdata.y = event->mouse.y;
			if (target == entity_null)
			{
				return entity_signal(target, sig("mouse_move"), &mdata, NULL);
			}
			else
			{
				return entity_signal_same(target, sig("mouse_move"), &mdata, NULL);
			}
		default:
			return CONTINUE;
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
	if (self->visible != active_mouse->visible)
	{
		c_window_t *window = c_window(&SYS);
		glfwSetInputMode(window->window, GLFW_CURSOR, active_mouse->visible ?
		                 GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
		glfwSetCursorPos(window->window, active_mouse->x, active_mouse->y);
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
		c_window_t *window = c_window(&SYS);

		glfwSetInputMode(window->window, GLFW_CURSOR, self->visible ?
		                 GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
		glfwSetCursorPos(window->window, self->x, self->y);
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
			c_window_t *window = c_window(&SYS);
			glfwSetInputMode(window->window, GLFW_CURSOR, self->visible ?
			                 GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
			/* sdl_ShowCursor(self->visible); */ 
			/* sdl_SetRelativeMouseMode(!self->visible); */
		}
	}
}

void ct_mouse(ct_t *self)
{
	ct_init(self, "mouse", sizeof(c_mouse_t));
	ct_set_init(self, (init_cb)c_mouse_init);
	ct_listener(self, WORLD, -1, ref("event_handle"), c_mouse_event);
}
