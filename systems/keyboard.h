#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <ecs/ecm.h>

typedef struct c_keyboard_t
{
	c_t super;
	int shift;
	int ctrl;
} c_keyboard_t;

void ct_keyboard(ct_t *self);
DEF_CASTER(ct_keyboard, c_keyboard, c_keyboard_t)
c_keyboard_t *c_keyboard_new(void);

#endif /* !KEYBOARD_H */
