#include "mouse.h"

DEC_SIG(mouse_move);
DEC_SIG(mouse_press);
DEC_SIG(mouse_release);
DEC_SIG(mouse_wheel);

DEC_CT(mouse)
{
	signal_init(&mouse_move, sizeof(mouse_move_data));
	signal_init(&mouse_press, sizeof(mouse_button_data));
	signal_init(&mouse_release, sizeof(mouse_button_data));
	signal_init(&mouse_wheel, sizeof(mouse_button_data));
}
