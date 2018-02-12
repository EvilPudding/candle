#include "mouse.h"

DEC_SIG(mouse_move);
DEC_SIG(mouse_press);
DEC_SIG(mouse_release);
DEC_SIG(mouse_wheel);

void mouse_register(ecm_t *ecm)
{
	ecm_register_signal(ecm, &mouse_move, sizeof(mouse_move_data));
	ecm_register_signal(ecm, &mouse_press, sizeof(mouse_button_data));
	ecm_register_signal(ecm, &mouse_release, sizeof(mouse_button_data));
	ecm_register_signal(ecm, &mouse_wheel, sizeof(mouse_button_data));
}
