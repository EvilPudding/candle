#ifndef EDITMODE_H
#define EDITMODE_H

#include "../ecs/ecm.h"
#include "../utils/renderer.h"
#include "../utils/nk.h"

struct edit_scale
{
	int32_t dragging;
	vec3_t start_scale;

	float start_radius;
	entity_t arrows, X, Y, Z;
};

struct edit_rotate
{
	int32_t dragging;
	vec4_t start_quat;

	vec3_t obj_pos;
	vec2_t start_screen;
	vec2_t p;
	float start_radius;
	float tool_fade;
	entity_t arrows, X, Y, Z;
#ifdef MESH4
	entity_t W;
#endif
};

struct edit_translate
{
	int32_t dragging;
	vec3_t start_pos;

	vec3_t drag_diff;
	float start_radius;
	vec2_t start_screen;
	int32_t mode;
	entity_t arrows, X, Y, Z;
#ifdef MESH4
	entity_t W;
#endif
};

struct edit_poly
{
	int32_t last_edge;
};

struct c_editmode;

typedef int32_t (*mouse_tool_update_cb)(void *usrptr, float dt, struct c_editmode *ec);
typedef int32_t (*mouse_tool_init_cb)(void *usrptr, struct c_editmode *ec);
typedef int32_t (*mouse_tool_end_cb)(void *usrptr, struct c_editmode *ec);
typedef int32_t (*mouse_tool_move_cb)(void *usrptr, vec3_t p, struct c_editmode *ec);
typedef int32_t (*mouse_tool_press_cb)(void *usrptr, vec3_t p, int32_t button, struct c_editmode *ec);
typedef int32_t (*mouse_tool_release_cb)(void *usrptr, vec3_t p, int32_t button, struct c_editmode *ec);
typedef int32_t (*mouse_tool_drag_cb)(void *usrptr, vec3_t p, int32_t button, struct c_editmode *ec);

struct mouse_tool
{
	char key;
	char name[64];
	mouse_tool_update_cb update;
	mouse_tool_init_cb init;
	mouse_tool_end_cb end;
	mouse_tool_move_cb mmove;
	mouse_tool_press_cb mpress;
	mouse_tool_release_cb mrelease;
	mouse_tool_drag_cb mdrag;
	void *usrptr;
	uint32_t require_component;
};

typedef struct c_editmode
{
	c_t super; /* extends c_t */

	int32_t control;
	int32_t visible;
	int32_t dragging;
	int32_t tool;
	struct mouse_tool tools[16];
	int32_t tools_num;
	/* int32_t outside; */

	vec3_t mouse_position;
	int32_t menu_x;
	int32_t menu_y;
	entity_t open_entities[16];
	int32_t open_entities_count;

	texture_t *open_textures[16];
	int32_t open_textures_count;

	void *open_vil;

	vec2_t spawn_pos;

	/* TOOL VARIABLES */

	vec3_t mouse_screen_pos;
	ivec2_t mouse_iposition;
	texture_t *mouse_depth;

	void *nk;
	entity_t selected;
	entity_t over;
	entity_t context;
	entity_t context_queued;
	float context_enter_phase;
	int32_t selected_poly;
	int32_t over_poly;

	struct {
		ct_id_t ct;
		int32_t distance;
	} ct_list[10];
	int32_t ct_list_size;
	char ct_search_bak[32];
	char ct_search[32];
	char shell[512];

	renderer_t *backup_renderer;
	entity_t camera;
	entity_t auxiliar;
	entity_t arrows, X, Y, Z, W, RX, RY, RZ, SX, SY, SZ;
} c_editmode_t;

DEF_CASTER(ct_editmode, c_editmode, c_editmode_t)

c_editmode_t *c_editmode_new(void);
void c_editmode_activate(c_editmode_t *self);
void c_editmode_register(void);
void c_editmode_update_mouse(c_editmode_t *self, float x, float y);
void c_editmode_open_texture(c_editmode_t *self, texture_t *tex);
void c_editmode_select(c_editmode_t *self, entity_t select);
void c_editmode_add_tool(c_editmode_t *self, char key, const char *name,
                         mouse_tool_init_cb init,
                         mouse_tool_move_cb mmove,
                         mouse_tool_drag_cb mdrag,
                         mouse_tool_press_cb mpress,
                         mouse_tool_release_cb mrelease,
                         mouse_tool_update_cb update,
                         mouse_tool_end_cb end,
                         void *usrptr,
                         uint32_t require_component);


#endif /* !EDITMODE_H */
