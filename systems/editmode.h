#ifndef EDITMODE_H
#define EDITMODE_H

#include <ecm.h>

typedef struct
{
	c_t super; /* extends c_t */

	int control;
	int visible;
	int outside;

	void *nkctx;
	entity_t selected;
	entity_t over;

} c_editmode_t;

DEF_CASTER(ct_editmode, c_editmode, c_editmode_t)

DEF_SIG(global_menu);
DEF_SIG(component_menu);

c_editmode_t *c_editmode_new(void);
void c_editmode_register(ecm_t *ecm);

#endif /* !EDITMODE_H */
