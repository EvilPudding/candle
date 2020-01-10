#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <ecs/ecm.h>

typedef struct 
{
	uint32_t controller;
	uint32_t key;
	float pressed;
} controller_button_t;

typedef struct 
{
	uint32_t controller;
	uint32_t side;
	float x, y;
	controller_button_t button;
} controller_axis_t;

typedef struct controller_t
{
	uint32_t id;
	controller_axis_t axis_right;
	controller_axis_t axis_left;
	controller_button_t up, down, left, right;
	controller_button_t a, b, x, y;
	controller_button_t trigger_left, trigger_right;
	controller_button_t shoulder_left, shoulder_right;
	controller_button_t start, back, guide;
	void *haptic;
	bool_t connected;
} controller_t;

typedef struct c_controllers_t
{
	c_t super;
	controller_t controllers[8];
	void *states;
	uint32_t num_controllers;
} c_controllers_t;

void ct_controller(ct_t *self);
DEF_CASTER(ct_controller, c_controller, c_controllers_t)
c_controllers_t *c_controllers_new(void);
void c_controllers_rumble(c_controllers_t *self, uint32_t controller,
                          float strength);

#endif /* !CONTROLLER_H */
