#include <SDL2/SDL.h>
#include "controller.h"

void c_controllers_rumble(c_controllers_t *self, uint32_t controller,
                          float strength)
{
	SDL_Haptic *haptic = self->controllers[controller].haptic;
	if (haptic == NULL) return;
	if (SDL_HapticRumbleInit(haptic) != 0) return;
	if (strength == 0.f)
	{
		SDL_HapticRumbleStop(haptic);
	}
	else
	{
		SDL_HapticRumblePlay(haptic, strength, 5000);
	}
}

float normalize_axis(int32_t val)
{
	const int deadzone = 3000;
	float value;
	if (val == 0)
	{
		value = 0.0f;
	}
	else if (val < 0)
	{
		value = ((float)val + deadzone) / (32768.0f - deadzone);
		if (value > 0.f)
			value = 0.f;
	}
	else
	{
		value = ((float)val - deadzone) / (32767.0f - deadzone);
		if (value < 0.f)
			value = 0.f;
	}
	return value;
}

int32_t c_controllers_event(c_controllers_t *self, const SDL_Event *event)
{
	float value;
	uint32_t signal;
	SDL_GameController *pad;
	controller_t *controller = &self->controllers[event->cdevice.which];
	controller_button_t *button;

	switch(event->type)
	{
		case SDL_CONTROLLERDEVICEADDED:

			pad = SDL_GameControllerOpen(event->cdevice.which);
			if (SDL_GameControllerGetAttached(pad) == 1)
			{
				if (event->cdevice.which >= self->num_controllers)
				{
					self->num_controllers = event->cdevice.which + 1;
				}
				controller->haptic = SDL_HapticOpenFromJoystick(SDL_GameControllerGetJoystick(pad));
				controller->connected = true;
			}
			else
			{
				printf("SDL_GetError() = %s\n", SDL_GetError());
			}
			return STOP;
		case SDL_CONTROLLERBUTTONDOWN:
		case SDL_CONTROLLERBUTTONUP:
			switch (event->cbutton.button)
			{
				case SDL_CONTROLLER_BUTTON_A:
					button = &controller->a;
					break;
				case SDL_CONTROLLER_BUTTON_B:
					button = &controller->b;
					break;
				case SDL_CONTROLLER_BUTTON_X:
					button = &controller->x;
					break;
				case SDL_CONTROLLER_BUTTON_Y:
					button = &controller->y;
					break;
				case SDL_CONTROLLER_BUTTON_BACK:
					button = &controller->back;
					break;
				case SDL_CONTROLLER_BUTTON_GUIDE:
					button = &controller->guide;
					break;
				case SDL_CONTROLLER_BUTTON_START:
					button = &controller->start;
					break;
				case SDL_CONTROLLER_BUTTON_LEFTSTICK:
					button = &controller->axis_left.button;
					break;
				case SDL_CONTROLLER_BUTTON_RIGHTSTICK:
					button = &controller->axis_right.button;
					break;
				case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
					button = &controller->shoulder_left;
					break;
				case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
					button = &controller->shoulder_right;
					break;
				case SDL_CONTROLLER_BUTTON_DPAD_UP:
					button = &controller->up;
					break;
				case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
					button = &controller->down;
					break;
				case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
					button = &controller->left;
					break;
				case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
					button = &controller->right;
					break;
				default:
					printf("wat\n");
					return CONTINUE;
			}
			printf("button\n");
			signal =   event->type == SDL_CONTROLLERBUTTONDOWN
			                  ? ref("controller_button_down")
			                  : ref("controller_button_up");
			button->pressed = event->type == SDL_CONTROLLERBUTTONDOWN;
			button->key = event->cbutton.button;
			entity_signal(entity_null, signal, button, NULL);
			return STOP;
		case SDL_CONTROLLERAXISMOTION:
			value = normalize_axis(event->caxis.value);
			switch (event->caxis.axis)
			{
				case SDL_CONTROLLER_AXIS_LEFTX:
					controller->axis_left.changed = true;
					controller->axis_left.x = value;
					break;
				case SDL_CONTROLLER_AXIS_LEFTY:
					controller->axis_left.changed = true;
					controller->axis_left.y = value;
					break;
				case SDL_CONTROLLER_AXIS_RIGHTX:
					controller->axis_right.changed = true;
					controller->axis_right.x = value;
					break;
				case SDL_CONTROLLER_AXIS_RIGHTY:
					controller->axis_right.changed = true;
					controller->axis_right.y = value;
					break;
				case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
					controller->trigger_left.pressed = ((float)event->caxis.value) / 32767.0f;
					break;
				case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
					controller->trigger_right.pressed = ((float)event->caxis.value) / 32767.0f;
					break;
				default:
					printf("wat? %d\n", event->caxis.axis);
			}
			return STOP;
	}
	return CONTINUE;
}

int32_t c_controllers_events_end(c_controllers_t *self)
{
	uint32_t i;
	for (i = 0; i < self->num_controllers; i++)
	{
		controller_t *controller = &self->controllers[i];
		if (!controller->connected) continue;
		if (   controller->axis_left.changed
		    || controller->axis_left.x != 0.f
		    || controller->axis_left.y != 0.f)
		{
			controller->axis_left.changed = false;
			entity_signal(entity_null, ref("controller_axis"), &controller->axis_left, NULL);
		}
		if (   controller->axis_right.changed
		    || controller->axis_right.x != 0.f
		    || controller->axis_right.y != 0.f)
		{
			controller->axis_right.changed = false;
			entity_signal(entity_null, ref("controller_axis"), &controller->axis_right, NULL);
		}
	}
		
	return CONTINUE;
}


void ct_controller(ct_t *self)
{
	ct_init(self, "controller", sizeof(c_controllers_t));

	signal_init(ref("controller_connected"), sizeof(uint32_t));
	signal_init(ref("controller_button_up"), sizeof(controller_button_t));
	signal_init(ref("controller_button_down"), sizeof(controller_button_t));
	signal_init(ref("controller_button_changed"), sizeof(controller_button_t));

	signal_init(ref("controller_axis"), sizeof(controller_axis_t));

	ct_listener(self, WORLD, 0, ref("event_handle"), c_controllers_event);
	ct_listener(self, WORLD, 0, ref("events_end"), c_controllers_events_end);
}

c_controllers_t *c_controllers_new()
{
	bool_t has_gamepads = false;
	int n_joysticks, i;
	c_controllers_t *self = component_new(ct_controller);

	if (SDL_WasInit(SDL_INIT_GAMECONTROLLER) == 1)
		exit(1);
	SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
	SDL_InitSubSystem(SDL_INIT_HAPTIC);

	n_joysticks = SDL_NumJoysticks();
	for (i = 0; i < n_joysticks; i++)
	{
		if (SDL_IsGameController(i))
		{
			SDL_GameController *pad = SDL_GameControllerOpen(i);

			if (SDL_GameControllerGetAttached(pad) == 1)
			{
				has_gamepads = true;
			}
		}
	}

	for (i = 0; i < 8; i++)
	{
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
	}

	if (has_gamepads)
	{
		SDL_GameControllerEventState(SDL_ENABLE);
	}
	return self;
}
