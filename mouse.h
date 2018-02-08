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
} mouse_move_data;

void mouse_register(ecm_t *ecm);

extern unsigned long mouse_move;
extern unsigned long mouse_press;
extern unsigned long mouse_release;
extern unsigned long mouse_wheel;

#endif /* !MOUSE_H */
