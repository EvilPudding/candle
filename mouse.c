#include "mouse.h"

DEC_SIG(mouse_move);
DEC_SIG(mouse_press);
DEC_SIG(mouse_release);
DEC_SIG(mouse_wheel);

void mouse_register()
{
	ecm_register_signal(&mouse_move, sizeof(mouse_move_data));
	ecm_register_signal(&mouse_press, sizeof(mouse_button_data));
	ecm_register_signal(&mouse_release, sizeof(mouse_button_data));
	ecm_register_signal(&mouse_wheel, sizeof(mouse_button_data));
}
