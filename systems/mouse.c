#include "../systems/window.h"
#include "mouse.h"

#ifdef __WIN32
#  ifndef _GLFW_WIN32 
#    define _GLFW_WIN32 
#  endif
#else
#  ifndef _GLFW_WIN32 
#    define _GLFW_X11
#  endif
#endif
#include "../third_party/glfw/include/GLFW/glfw3.h"


#define DOUBLE_CLICK_LO 0.02
#define DOUBLE_CLICK_HI 0.2

entity_t g_active_mouse = 0x0000000100000001ul;

void c_mouse_init(c_mouse_t *self)
{
	self->visible = true;
	self->previous_active = entity_null;
	if (!entity_exists(g_active_mouse))
	{
		c_mouse_activate(self, false);
	}
}

c_mouse_t *c_mouse_new()
{
	return component_new(ct_mouse);
}

int32_t c_mouse_handle_click(c_mouse_t *self, candle_key_e key)
{
	entity_t target = c_entity(self) == SYS ? entity_null : c_entity(self);
	int *count;
	mouse_button_data bdata;

	bdata.x = self->x;
	bdata.y = self->y;
	bdata.button = key;
	if (key == CANDLE_MOUSE_BUTTON_RIGHT)
	{
		count = &self->right_count;
	}
	else if (key == CANDLE_MOUSE_BUTTON_LEFT)
	{
		count = &self->left_count;
	}
	else
	{
		return CONTINUE;
	}
	if (*count >= 4)
	{
		*count = 0;
		return entity_signal_same(target, sig("mouse_click_double"), &bdata, NULL);
	}
	else
	{
		return entity_signal_same(target, sig("mouse_click"), &bdata, NULL);
	}
	return CONTINUE;
}

int32_t c_mouse_event(c_mouse_t *self, const candle_event_t *event)
{
	mouse_button_data bdata;
	mouse_move_data mdata;
	entity_t target;

	if (event->type == CANDLE_MOUSEBUTTONUP || event->type == CANDLE_MOUSEBUTTONDOWN)
	{
		const double time_now = glfwGetTime();
		if (event->key == CANDLE_MOUSE_BUTTON_RIGHT)
		{
			const double dt = time_now - self->last_right_event;
			self->right = event->type == CANDLE_MOUSEBUTTONDOWN;
			if (dt > DOUBLE_CLICK_LO && dt < DOUBLE_CLICK_HI)
			{
				self->right_count++;
			}
			else
			{
				self->right_count = 1;
			}
			self->last_right_event = time_now;
		}
		if (event->key == CANDLE_MOUSE_BUTTON_LEFT)
		{
			const double dt = time_now - self->last_left_event;
			self->left = event->type == CANDLE_MOUSEBUTTONDOWN;
			self->last_left_event = time_now;
			if (dt > DOUBLE_CLICK_LO && dt <DOUBLE_CLICK_HI)
			{
				self->left_count++;
			}
			else
			{
				self->left_count = 1;
			}
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
			return entity_signal_same(target, sig("mouse_wheel"), &bdata, NULL);
		case CANDLE_MOUSEBUTTONUP:
			bdata.x = self->x;
			bdata.y = self->y;
			bdata.direction = 0.0f;
			bdata.button = event->key;
			if (entity_signal_same(target, sig("mouse_release"), &bdata, NULL) == STOP)
				return STOP;
			return c_mouse_handle_click(self, event->key);
		case CANDLE_MOUSEBUTTONDOWN:
			bdata.x = self->x;
			bdata.y = self->y;
			bdata.direction = 0.0f;
			bdata.button = event->key;
			return entity_signal_same(target, sig("mouse_press"), &bdata, NULL);
		case CANDLE_MOUSEMOTION:
			mdata.sx = event->mouse.x - self->x;
			mdata.sy = event->mouse.y - self->y;
			self->x = event->mouse.x;
			self->y = event->mouse.y;
			mdata.x = event->mouse.x;
			mdata.y = event->mouse.y;
			return entity_signal_same(target, sig("mouse_move"), &mdata, NULL);
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

void c_mouse_activate(c_mouse_t *self, bool_t restore_position)
{
	if (g_active_mouse != c_entity(self))
	{
		if (!restore_position) {
			self->x = c_mouse(&g_active_mouse)->x;
			self->y = c_mouse(&g_active_mouse)->y;
		}
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
	ct_add_listener(self, WORLD, -1, ref("event_handle"), c_mouse_event);
}
