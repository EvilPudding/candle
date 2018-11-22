#ifndef EDITMODE_H
#define EDITMODE_H

#include <ecs/ecm.h>
#include <utils/mesh.h>
#include <utils/renderer.h>
#include <utils/nk.h>

struct edit_scale
{
	int dragging;
	vec3_t start_scale;

	float start_radius;
	entity_t arrows, X, Y, Z;
};

struct edit_rotate
{
	int dragging;
	vec4_t start_quat;

	vec3_t obj_pos;
	vec2_t start_screen;
	vec2_t p;
	float start_radius;
	float tool_fade;
	entity_t arrows, X, Y, Z;
};

struct edit_translate
{
	int dragging;
	vec3_t start_pos;

	vec3_t drag_diff;
	float start_radius;
	vec2_t start_screen;
	int mode;
	entity_t arrows, X, Y, Z;
#ifdef MESH4
	entity_t W;
#endif
};

struct edit_poly
{
	int last_edge;
};

typedef struct c_editmode_t c_editmode_t;

struct mouse_tool
{
	char key;
	char name[64];
	int (*update)(void *usrptr, float dt, c_editmode_t *ec);
	int (*init)(void *usrptr, c_editmode_t *ec);
	int (*end)(void *usrptr, c_editmode_t *ec);
	int (*mmove)(void *usrptr, vec3_t p, c_editmode_t *ec);
	int (*mpress)(void *usrptr, vec3_t p, int button, c_editmode_t *ec);
	int (*mrelease)(void *usrptr, vec3_t p, int button, c_editmode_t *ec);
	int (*mdrag)(void *usrptr, vec3_t p, int button, c_editmode_t *ec);
	void *usrptr;
};

typedef struct c_editmode_t
{
	c_t super; /* extends c_t */

	int control;
	int visible;
	int dragging;
	int pressing_l;
	int pressing_r;
	int activated;
	int tool;
	struct mouse_tool tools[16];
	int tools_num;
	/* int outside; */

	vec3_t mouse_position;
	int menu_x;
	int menu_y;

	/* TOOL VARIABLES */

	vec3_t mouse_screen_pos;

	void *nk;
	vec2_t spawn_pos;
	entity_t selected;
	entity_t over;
	entity_t context;
	int selected_poly;
	int over_poly;

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
