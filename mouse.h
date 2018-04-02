#ifndef MOUSE_H
#define MOUSE_H

#include <ecm.h>

typedef struct
{
	void *pedantic; /* Fix missing member warning */
} mouse_t;

typedef struct
{
	float x, y;
} mouse_position_data;

typedef struct
{
	float x, y, direction;
	int button;
} mouse_button_data;

typedef struct
{
	float sx, sy;
	float x, y;
} mouse_move_data;

typedef struct c_mouse_t
{
	c_t super;
	/* currently, mouse has no options */
} c_mouse_t;

DEF_CASTER("mouse", c_mouse, c_mouse_t)
c_mouse_t *c_mouse_new(void);

#endif /* !MOUSE_H */
