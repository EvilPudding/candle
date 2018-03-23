#include <ecm.h>
#include "keyboard.h"
#include <SDL2/SDL.h>

DEC_SIG(key_up);
DEC_SIG(key_down);

DEC_CT(keyboard)
{
	signal_init(&key_up, sizeof(char));
	signal_init(&key_down, sizeof(char));
}

