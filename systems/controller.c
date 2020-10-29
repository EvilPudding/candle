#include "controller.h"
#include <candle.h>
#include <GLFW/glfw3.h>

/* void c_controllers_rumble(c_controllers_t *self, uint32_t controller, */
/*                           float strength) */
/* { */
/* 	sdl_Haptic *haptic = self->controllers[controller].haptic; */
/* 	if (haptic == NULL) return; */
/* 	if (sdl_HapticRumbleInit(haptic) != 0) return; */
/* 	if (strength == 0.f) */
/* 	{ */
/* 		sdl_HapticRumbleStop(haptic); */
/* 	} */
/* 	else */
/* 	{ */
/* 		sdl_HapticRumblePlay(haptic, strength, 5000); */
/* 	} */
/* } */

vec2_t normalize_axis(float x, float y)
{
	const float deadzone = 0.15;
	const vec2_t v = vec2(x, y);
	const vec2_t d = vec2_norm(v);
	float len = (vec2_len(v) - deadzone) / (1.0f - deadzone);
	if (len < 0.0f)
		len = 0.0f;
	return vec2_scale(d, len);
}

int32_t c_controllers_event(c_controllers_t *self, const candle_event_t *event)
{

	if (   event->type != CANDLE_CONTROLLERDEVICEADDED
	    && event->type != CANDLE_CONTROLLERBUTTONDOWN
	    && event->type != CANDLE_CONTROLLERBUTTONUP
		&& event->type != CANDLE_CONTROLLERAXISMOTION)
	{
		return CONTINUE;
	}

	/* controller = &self->controllers[event->controller.id]; */

	return CONTINUE;
}

int32_t c_controllers_events_end(c_controllers_t *self)
{
#ifndef __EMSCRIPTEN__
	uint32_t i;
	for (i = 0; i < 8; i++)
	{
		controller_t *controller = &self->controllers[i];
		GLFWgamepadstate state;
		GLFWgamepadstate *cstate = &((GLFWgamepadstate*)self->states)[i];
		if (!glfwJoystickIsGamepad(GLFW_JOYSTICK_1 + i))
		{
			continue;
		}

		if (glfwGetGamepadState(GLFW_JOYSTICK_1 + i, &state))
		{
			controller_button_t *button;
			uint32_t signal;
			if (state.buttons[GLFW_GAMEPAD_BUTTON_A] != cstate->buttons[GLFW_GAMEPAD_BUTTON_A])
			{
				cstate->buttons[GLFW_GAMEPAD_BUTTON_A] = state.buttons[GLFW_GAMEPAD_BUTTON_A];
				button = &controller->a;
				button->key = CANDLE_CONTROLLER_BUTTON_A;
				button->pressed = cstate->buttons[GLFW_GAMEPAD_BUTTON_A] == GLFW_PRESS;
				signal = button->pressed ? ref("controller_button_down")
					                     : ref("controller_button_up");
				entity_signal(entity_null, signal, button, NULL);
			}
			if (state.buttons[GLFW_GAMEPAD_BUTTON_B] != cstate->buttons[GLFW_GAMEPAD_BUTTON_B])
			{
				cstate->buttons[GLFW_GAMEPAD_BUTTON_B] = state.buttons[GLFW_GAMEPAD_BUTTON_B];
				button = &controller->b;
				button->key = CANDLE_CONTROLLER_BUTTON_B;
				button->pressed = cstate->buttons[GLFW_GAMEPAD_BUTTON_B] == GLFW_PRESS;
				signal = button->pressed ? ref("controller_button_down")
					                     : ref("controller_button_up");
				entity_signal(entity_null, signal, button, NULL);
			}
			if (state.buttons[GLFW_GAMEPAD_BUTTON_X] != cstate->buttons[GLFW_GAMEPAD_BUTTON_X])
			{
				cstate->buttons[GLFW_GAMEPAD_BUTTON_X] = state.buttons[GLFW_GAMEPAD_BUTTON_X];
				button = &controller->x;
				button->key = CANDLE_CONTROLLER_BUTTON_X;
				button->pressed = cstate->buttons[GLFW_GAMEPAD_BUTTON_X] == GLFW_PRESS;
				signal = button->pressed ? ref("controller_button_down")
					                     : ref("controller_button_up");
				entity_signal(entity_null, signal, button, NULL);
			}
			if (state.buttons[GLFW_GAMEPAD_BUTTON_Y] != cstate->buttons[GLFW_GAMEPAD_BUTTON_Y])
			{
				cstate->buttons[GLFW_GAMEPAD_BUTTON_Y] = state.buttons[GLFW_GAMEPAD_BUTTON_Y];
				button = &controller->y;
				button->key = CANDLE_CONTROLLER_BUTTON_Y;
				button->pressed = cstate->buttons[GLFW_GAMEPAD_BUTTON_Y] == GLFW_PRESS;
				signal = button->pressed ? ref("controller_button_down")
					                     : ref("controller_button_up");
				entity_signal(entity_null, signal, button, NULL);
			}

			if (state.buttons[GLFW_GAMEPAD_BUTTON_LEFT_BUMPER] != cstate->buttons[GLFW_GAMEPAD_BUTTON_LEFT_BUMPER])
			{
				cstate->buttons[GLFW_GAMEPAD_BUTTON_LEFT_BUMPER] = state.buttons[GLFW_GAMEPAD_BUTTON_LEFT_BUMPER];
				button = &controller->shoulder_left;
				button->key = CANDLE_CONTROLLER_BUTTON_LEFTSHOULDER;
				button->pressed = cstate->buttons[GLFW_GAMEPAD_BUTTON_LEFT_BUMPER] == GLFW_PRESS;
				signal = button->pressed ? ref("controller_button_down")
					                     : ref("controller_button_up");
				entity_signal(entity_null, signal, button, NULL);
			}

			if (state.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER] != cstate->buttons[GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER])
			{
				cstate->buttons[GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER] = state.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER];
				button = &controller->shoulder_right;
				button->key = CANDLE_CONTROLLER_BUTTON_RIGHTSHOULDER;
				button->pressed = cstate->buttons[GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER] == GLFW_PRESS;
				signal = button->pressed ? ref("controller_button_down")
					                     : ref("controller_button_up");
				entity_signal(entity_null, signal, button, NULL);
			}

			if (state.buttons[GLFW_GAMEPAD_BUTTON_BACK] != cstate->buttons[GLFW_GAMEPAD_BUTTON_BACK])
			{
				cstate->buttons[GLFW_GAMEPAD_BUTTON_BACK] = state.buttons[GLFW_GAMEPAD_BUTTON_BACK];
				button = &controller->back;
				button->key = CANDLE_CONTROLLER_BUTTON_BACK;
				button->pressed = cstate->buttons[GLFW_GAMEPAD_BUTTON_BACK] == GLFW_PRESS;
				signal = button->pressed ? ref("controller_button_down")
					                     : ref("controller_button_up");
				entity_signal(entity_null, signal, button, NULL);
			}

			if (state.buttons[GLFW_GAMEPAD_BUTTON_START] != cstate->buttons[GLFW_GAMEPAD_BUTTON_START])
			{
				cstate->buttons[GLFW_GAMEPAD_BUTTON_START] = state.buttons[GLFW_GAMEPAD_BUTTON_START];
				button = &controller->start;
				button->key = CANDLE_CONTROLLER_BUTTON_START;
				button->pressed = cstate->buttons[GLFW_GAMEPAD_BUTTON_START] == GLFW_PRESS;
				signal = button->pressed ? ref("controller_button_down")
					                     : ref("controller_button_up");
				entity_signal(entity_null, signal, button, NULL);
			}

			if (state.buttons[GLFW_GAMEPAD_BUTTON_GUIDE] != cstate->buttons[GLFW_GAMEPAD_BUTTON_GUIDE])
			{
				cstate->buttons[GLFW_GAMEPAD_BUTTON_GUIDE] = state.buttons[GLFW_GAMEPAD_BUTTON_GUIDE];
				button = &controller->guide;
				button->key = CANDLE_CONTROLLER_BUTTON_GUIDE;
				button->pressed = cstate->buttons[GLFW_GAMEPAD_BUTTON_GUIDE] == GLFW_PRESS;
				signal = button->pressed ? ref("controller_button_down")
					                     : ref("controller_button_up");
				entity_signal(entity_null, signal, button, NULL);
			}

			if (state.buttons[GLFW_GAMEPAD_BUTTON_LEFT_THUMB] != cstate->buttons[GLFW_GAMEPAD_BUTTON_LEFT_THUMB])
			{
				cstate->buttons[GLFW_GAMEPAD_BUTTON_LEFT_THUMB] = state.buttons[GLFW_GAMEPAD_BUTTON_LEFT_THUMB];
				button = &controller->axis_left.button;
				button->key = CANDLE_CONTROLLER_AXIS_LEFT;
				button->pressed = cstate->buttons[GLFW_GAMEPAD_BUTTON_LEFT_THUMB] == GLFW_PRESS;
				signal = button->pressed ? ref("controller_button_down")
					                     : ref("controller_button_up");
				entity_signal(entity_null, signal, button, NULL);
			}

			if (state.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_THUMB] != cstate->buttons[GLFW_GAMEPAD_BUTTON_RIGHT_THUMB])
			{
				cstate->buttons[GLFW_GAMEPAD_BUTTON_RIGHT_THUMB] = state.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_THUMB];
				button = &controller->axis_right.button;
				button->key = CANDLE_CONTROLLER_AXIS_RIGHT;
				button->pressed = cstate->buttons[GLFW_GAMEPAD_BUTTON_RIGHT_THUMB] == GLFW_PRESS;
				signal = button->pressed ? ref("controller_button_down")
					                     : ref("controller_button_up");
				entity_signal(entity_null, signal, button, NULL);
			}

			if (state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_UP] != cstate->buttons[GLFW_GAMEPAD_BUTTON_DPAD_UP])
			{
				cstate->buttons[GLFW_GAMEPAD_BUTTON_DPAD_UP] = state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_UP];
				button = &controller->up;
				button->key = CANDLE_CONTROLLER_BUTTON_UP;
				button->pressed = cstate->buttons[GLFW_GAMEPAD_BUTTON_DPAD_UP] == GLFW_PRESS;
				signal = button->pressed ? ref("controller_button_down")
					                     : ref("controller_button_up");
				entity_signal(entity_null, signal, button, NULL);
			}

			if (state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_RIGHT] != cstate->buttons[GLFW_GAMEPAD_BUTTON_DPAD_RIGHT])
			{
				cstate->buttons[GLFW_GAMEPAD_BUTTON_DPAD_RIGHT] = state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_RIGHT];
				button = &controller->right;
				button->key = CANDLE_CONTROLLER_BUTTON_RIGHT;
				button->pressed = cstate->buttons[GLFW_GAMEPAD_BUTTON_DPAD_RIGHT] == GLFW_PRESS;
				signal = button->pressed ? ref("controller_button_down")
					                     : ref("controller_button_up");
				entity_signal(entity_null, signal, button, NULL);
			}

			if (state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_DOWN] != cstate->buttons[GLFW_GAMEPAD_BUTTON_DPAD_DOWN])
			{
				cstate->buttons[GLFW_GAMEPAD_BUTTON_DPAD_DOWN] = state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_DOWN];
				button = &controller->down;
				button->key = CANDLE_CONTROLLER_BUTTON_DOWN;
				button->pressed = cstate->buttons[GLFW_GAMEPAD_BUTTON_DPAD_DOWN] == GLFW_PRESS;
				signal = button->pressed ? ref("controller_button_down")
					                     : ref("controller_button_up");
				entity_signal(entity_null, signal, button, NULL);
			}

			if (state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_LEFT ] != cstate->buttons[GLFW_GAMEPAD_BUTTON_DPAD_LEFT ])
			{
				cstate->buttons[GLFW_GAMEPAD_BUTTON_DPAD_LEFT ] = state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_LEFT ];
				button = &controller->left;
				button->key = CANDLE_CONTROLLER_BUTTON_LEFT;
				button->pressed = cstate->buttons[GLFW_GAMEPAD_BUTTON_DPAD_LEFT ] == GLFW_PRESS;
				signal = button->pressed ? ref("controller_button_down")
					                     : ref("controller_button_up");
				entity_signal(entity_null, signal, button, NULL);
			}

			if (   state.axes[GLFW_GAMEPAD_AXIS_LEFT_X] != cstate->axes[GLFW_GAMEPAD_AXIS_LEFT_X]
			    || state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y] != cstate->axes[GLFW_GAMEPAD_AXIS_LEFT_Y])
			{
				const vec2_t axis = normalize_axis(state.axes[GLFW_GAMEPAD_AXIS_LEFT_X],
				                                   state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y]);
				cstate->axes[GLFW_GAMEPAD_AXIS_LEFT_X] = state.axes[GLFW_GAMEPAD_AXIS_LEFT_X];
				cstate->axes[GLFW_GAMEPAD_AXIS_LEFT_Y] = state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y];
				controller->axis_left.x = axis.x;
				controller->axis_left.y = axis.y;
				if (controller->axis_left.x == 0.f && controller->axis_left.y == 0.f)
				{
					entity_signal(entity_null, ref("controller_axis"), &controller->axis_left, NULL);
				}
			}
			if (controller->axis_left.x != 0.f || controller->axis_left.y != 0.f)
			{
				entity_signal(entity_null, ref("controller_axis"), &controller->axis_left, NULL);
			}

			if (   state.axes[GLFW_GAMEPAD_AXIS_RIGHT_X] != cstate->axes[GLFW_GAMEPAD_AXIS_RIGHT_X]
			    || state.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y] != cstate->axes[GLFW_GAMEPAD_AXIS_RIGHT_Y])
			{
				const vec2_t axis = normalize_axis(state.axes[GLFW_GAMEPAD_AXIS_RIGHT_X],
				                                   state.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y]);
				cstate->axes[GLFW_GAMEPAD_AXIS_RIGHT_X] = state.axes[GLFW_GAMEPAD_AXIS_RIGHT_X];
				cstate->axes[GLFW_GAMEPAD_AXIS_RIGHT_Y] = state.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y];
				controller->axis_right.x = axis.x;
				controller->axis_right.y = axis.y;
				if (controller->axis_right.x == 0.f && controller->axis_right.y == 0.f)
				{
					entity_signal(entity_null, ref("controller_axis"), &controller->axis_right, NULL);
				}
			}
			if (controller->axis_right.x != 0.f || controller->axis_right.y != 0.f)
			{
				entity_signal(entity_null, ref("controller_axis"), &controller->axis_right, NULL);
			}

			if (state.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] != cstate->axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER])
			{
				cstate->buttons[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] = state.buttons[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER];
				button = &controller->trigger_right;
				button->key = CANDLE_CONTROLLER_TRIGGERLEFT;
				button->pressed = cstate->buttons[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER];
				entity_signal(entity_null, ref("controller_button_changed"), button, NULL);
			}
			if (state.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] != cstate->axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER])
			{
				cstate->buttons[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] = state.buttons[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER];
				button = &controller->trigger_right;
				button->key = CANDLE_CONTROLLER_TRIGGERRIGHT;
				button->pressed = cstate->buttons[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER];
				entity_signal(entity_null, ref("controller_button_changed"), button, NULL);
			}

		}
	}
#else
#endif
	return CONTINUE;
}


void ct_controller(ct_t *self)
{
	ct_init(self, "controller", sizeof(c_controllers_t));

	ct_add_listener(self, WORLD, 0, ref("event_handle"), c_controllers_event);
	ct_add_listener(self, WORLD, 0, ref("events_end"), c_controllers_events_end);
}

c_controllers_t *c_controllers_new()
{
	c_controllers_t *self = component_new(ct_controller);
#ifndef __EMSCRIPTEN__
	int i;
	/* bool_t has_gamepads = false; */
	/* int n_joysticks; */

	/* if (sdl_WasInit(sdl_INIT_GAMECONTROLLER) == 1) */
	/* 	exit(1); */
	/* sdl_InitSubSystem(sdl_INIT_GAMECONTROLLER); */
	/* sdl_InitSubSystem(sdl_INIT_HAPTIC); */

	/* n_joysticks = sdl_NumJoysticks(); */
	/* for (i = 0; i < n_joysticks; i++) */
	/* { */
	/* 	if (sdl_IsGameController(i)) */
	/* 	{ */
	/* 		sdl_GameController *pad = sdl_GameControllerOpen(i); */

	/* 		if (sdl_GameControllerGetAttached(pad) == 1) */
	/* 		{ */
	/* 			has_gamepads = true; */
	/* 		} */
	/* 	} */
	/* } */

	self->states = malloc(sizeof(GLFWgamepadstate) * 8);
	for (i = 0; i < 8; i++)
	{
		GLFWgamepadstate *cstate = &((GLFWgamepadstate*)self->states)[i];
		self->controllers[i].id = i;
		self->controllers[i].axis_right.controller = i;
		self->controllers[i].axis_left.controller = i;
		self->controllers[i].axis_left.side = 0;
		self->controllers[i].axis_right.side = 1;
		self->controllers[i].up.controller = i;
		self->controllers[i].down.controller = i;
		self->controllers[i].left.controller = i;
		self->controllers[i].right.controller = i;
		self->controllers[i].a.controller = i;
		self->controllers[i].b.controller = i;
		self->controllers[i].x.controller = i;
		self->controllers[i].y.controller = i;
		self->controllers[i].trigger_left.controller = i;
		self->controllers[i].trigger_right.controller = i;
		self->controllers[i].shoulder_left.controller = i;
		self->controllers[i].shoulder_right.controller = i;
		self->controllers[i].start.controller = i;
		self->controllers[i].back.controller = i;
		self->controllers[i].guide.controller = i;

		if (glfwJoystickIsGamepad(GLFW_JOYSTICK_1 + i))
		{
			glfwGetGamepadState(GLFW_JOYSTICK_1 + i, cstate);
		}
	}

	/* if (has_gamepads) */
	/* { */
	/* 	sdl_GameControllerEventState(sdl_ENABLE); */
	/* } */
#else
#endif
	return self;
}
