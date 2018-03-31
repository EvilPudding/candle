#include <ecm.h>
#include "keyboard.h"
#include <SDL2/SDL.h>


c_keyboard_t *c_keyboard_new()
{
	c_keyboard_t *self = component_new(ct_keyboard);

	return self;
}

DEC_CT(ct_keyboard)
{
	ct_new("Keyboard", &ct_keyboard, sizeof(c_keyboard_t), NULL, 0);

	signal_init(sig("key_up"), sizeof(char));
	signal_init(sig("key_down"), sizeof(char));
}

