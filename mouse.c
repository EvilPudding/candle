#include "mouse.h"

c_mouse_t *c_mouse_new()
{
	return component_new("mouse");
}

REG()
{
	ct_new("mouse", sizeof(c_mouse_t), NULL, NULL, 0);

	signal_init(sig("mouse_move"), sizeof(mouse_move_data));
	signal_init(sig("mouse_press"), sizeof(mouse_button_data));
	signal_init(sig("mouse_release"), sizeof(mouse_button_data));
	signal_init(sig("mouse_wheel"), sizeof(mouse_button_data));
}
