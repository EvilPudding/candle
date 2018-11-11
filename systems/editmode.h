#ifndef EDITMODE_H
#define EDITMODE_H

#include <ecs/ecm.h>
#include <utils/mesh.h>
#include <utils/renderer.h>
#include <utils/nk.h>

/* struct tool_event */
/* { */
/* 	void *ctx; */
/* 	entity_t selected; */
/* }; */

typedef struct
{
	c_t super; /* extends c_t */

	int control;
	int visible;
	int dragging;
	int pressing;
	int activated;
	int tool;
	/* int outside; */

	vec3_t mouse_position;
	int menu_x;
	int menu_y;

	/* TOOL VARIABLES */
	vec4_t start_prop;
	vec3_t drag_diff;
	float start_radius;
	vec3_t start_obj_pos;
	int last_edge;
	vec2_t start_screen;
	float tool_fade;
	int translate_mode;
	float radius;

	vec3_t mouse_screen_pos;

	void *nk;
	vec2_t spawn_pos;
	entity_t selected;
	entity_t over;
	entity_t context;
	int selected_poly;
	int over_poly;
	vec3_t tool_start;

	entity_t open_entities[16];
	int open_entities_count;

	texture_t *open_textures[16];
	int open_textures_count;

	void *open_vil;

	enum {
		EDIT_VERT,
		EDIT_EDGE,
		EDIT_FACE,
		EDIT_OBJECT
	} mode;

	struct {
		int ct;
		int distance;
	} ct_list[10];
	int ct_list_size;
	char ct_search_bak[32];
	char ct_search[32];
	char shell[512];

	renderer_t *backup_renderer;
	entity_t camera;
	entity_t auxiliar;
	entity_t arrows, X, Y, Z, W, RX, RY, RZ, SX, SY, SZ;
} c_editmode_t;

DEF_CASTER("editmode", c_editmode, c_editmode_t)

c_editmode_t *c_editmode_new(void);
void c_editmode_activate(c_editmode_t *self);
void c_editmode_register(void);
void c_editmode_update_mouse(c_editmode_t *self, float x, float y);
void c_editmode_open_texture(c_editmode_t *self, texture_t *tex);
void c_editmode_select(c_editmode_t *self, entity_t select);

#endif /* !EDITMODE_H */
