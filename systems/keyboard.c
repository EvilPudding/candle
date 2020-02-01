#include <events.h>
#include "keyboard.h"

int32_t c_keyboard_event(c_keyboard_t *self, const candle_event_t *event)
{
	candle_key_e key;
	const entity_t target = c_entity(self) == SYS ? entity_null : c_entity(self);
	switch (event->type)
	{
		case CANDLE_KEYUP:
			key = event->key;
			if (key == CANDLE_KEY_LEFT_CONTROL || key == CANDLE_KEY_RIGHT_CONTROL)
			{
				self->ctrl = 0;
			}
			if (key == CANDLE_KEY_LEFT_SHIFT || key == CANDLE_KEY_RIGHT_SHIFT)
			{
				self->shift = 0;
			}
			if (target == entity_null)
			{
				entity_signal(target, sig("key_up"), &key, NULL);
			}
			else
			{
				entity_signal_same(target, sig("key_up"), &key, NULL);
			}
			return STOP;
		case CANDLE_KEYDOWN:
			key = event->key;
			if (key == CANDLE_KEY_LEFT_CONTROL || key == CANDLE_KEY_RIGHT_CONTROL)
			{
				self->ctrl = true;
			}
			if (key == CANDLE_KEY_LEFT_SHIFT || key == CANDLE_KEY_RIGHT_SHIFT)
			{
				self->shift = true;
			}
			if (target == entity_null)
			{
				entity_signal(target, sig("key_down"), &key, NULL);
			}
			else
			{
				entity_signal_same(target, sig("key_down"), &key, NULL);
			}
			return STOP;
		default:
			return CONTINUE;
	}
	return CONTINUE;
}

void ct_keyboard(ct_t *self)
{
	ct_init(self, "keyboard", sizeof(c_keyboard_t));

	ct_add_listener(self, WORLD, -1, sig("event_handle"), c_keyboard_event);
}

c_keyboard_t *c_keyboard_new()
{
	return component_new(ct_keyboard);
}

