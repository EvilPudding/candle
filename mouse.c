#include "mouse.h"

unsigned long mouse_move;
unsigned long mouse_press;
unsigned long mouse_release;
unsigned long mouse_wheel;

void mouse_register(ecm_t *ecm)
{
	mouse_move = ecm_register_signal(ecm, sizeof(mouse_move_data));
	mouse_press = ecm_register_signal(ecm, sizeof(mouse_button_data));
	mouse_release = ecm_register_signal(ecm, sizeof(mouse_button_data));
	mouse_wheel = ecm_register_signal(ecm, sizeof(mouse_button_data));
}
