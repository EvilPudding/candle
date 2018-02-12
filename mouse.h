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

void mouse_register(ecm_t *ecm);

DEF_SIG(mouse_move);
DEF_SIG(mouse_press);
DEF_SIG(mouse_release);
DEF_SIG(mouse_wheel);

#endif /* !MOUSE_H */
