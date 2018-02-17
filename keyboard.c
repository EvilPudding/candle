#include <ecm.h>
#include "keyboard.h"
#include <SDL2/SDL.h>

DEC_SIG(key_up);
DEC_SIG(key_down);

void keyboard_register()
{
	ecm_register_signal(&key_up, sizeof(char));
	ecm_register_signal(&key_down, sizeof(char));
}

