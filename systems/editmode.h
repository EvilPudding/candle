#ifndef EDITMODE_H
#define EDITMODE_H

#include <ecm.h>
#include <mesh.h>
#include <nk.h>

typedef struct
{
	c_t super; /* extends c_t */

	int control;
	int visible;
	int dragging;
	int pressing;
	int activated;
	entity_t camera;
	entity_t backup_camera;
	/* int outside; */

	vec3_t mouse_position;

	vec3_t drag_diff;

	float mouse_depth;

	void *nk;
	vec2_t spawn_pos;
	entity_t selected;
	entity_t over;

	entity_t open_entities[16];
	int open_entities_count;

	texture_t *open_textures[16];
	int open_textures_count;

	geom_t mode;

} c_editmode_t;

DEF_CASTER(ct_editmode, c_editmode, c_editmode_t)

DEF_SIG(global_menu);
DEF_SIG(component_menu);

c_editmode_t *c_editmode_new(void);
void c_editmode_activate(c_editmode_t *self);
void c_editmode_register(ecm_t *ecm);
void c_editmode_update_mouse(c_editmode_t *self, float x, float y);
void c_editmode_open_texture(c_editmode_t *self, texture_t *tex);

#endif /* !EDITMODE_H */
