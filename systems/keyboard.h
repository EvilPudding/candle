#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <ecs/ecm.h>

typedef struct c_keyboard_t
{
	c_t super;
	int shift;
	int ctrl;
	/* currently, keyboard has no options */
} c_keyboard_t;

DEF_CASTER("keyboard", c_keyboard, c_keyboard_t)
c_keyboard_t *c_keyboard_new(void);

#endif /* !KEYBOARD_H */
