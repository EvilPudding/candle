#include "keyboard.h"
#include <SDL2/SDL.h>


c_keyboard_t *c_keyboard_new()
{
	c_keyboard_t *self = component_new("keyboard");

	return self;
}

REG()
{
	ct_new("keyboard", sizeof(c_keyboard_t), NULL, NULL, 0);

	signal_init(sig("key_up"), sizeof(char));
	signal_init(sig("key_down"), sizeof(char));
}

