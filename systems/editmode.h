#ifndef EDITMODE_H
#define EDITMODE_H

#include <ecm.h>
#include <mesh.h>

typedef struct
{
	c_t super; /* extends c_t */

	int control;
	int visible;
	int dragging;
	int pressing;
	int activated;
	/* int outside; */

	vec3_t mouse_position;

	vec3_t drag_diff;

	float mouse_depth;

	void *nkctx;
	entity_t selected;
	entity_t over;

	geom_t mode;

} c_editmode_t;

DEF_CASTER(ct_editmode, c_editmode, c_editmode_t)

DEF_SIG(global_menu);
DEF_SIG(component_menu);

c_editmode_t *c_editmode_new(void);
void c_editmode_activate(c_editmode_t *self);
void c_editmode_register(ecm_t *ecm);
void c_editmode_update_mouse(c_editmode_t *self, float x, float y);

#endif /* !EDITMODE_H */
