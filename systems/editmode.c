#include "editmode.h"
#include <components/editlook.h>
#include <components/name.h>
#include <components/node.h>
#include <components/camera.h>
#include <components/axis.h>
#include <components/attach.h>
#include <components/model.h>
#include <systems/window.h>
#include <systems/keyboard.h>
#include <candle.h>
#include <utils/mesh.h>
#include <vil/vil.h>
#include <stdlib.h>
#include <utils/renderer.h>

static int32_t c_editmode_activate_loader(c_editmode_t *self);
static void c_editmode_open_entity(c_editmode_t *self, entity_t ent);
static void c_editmode_selected_delete(c_editmode_t *self);

#define TRANSLATE	0
#define ROTATE		1
#define SCALE		2
#define POLYPEN		3

mat_t *g_sel_mat = NULL;

int32_t translate_init(struct edit_translate *self, c_editmode_t *ec)
{
	self->dragging = 0;

	if(!entity_exists(self->arrows))
	{
		self->arrows = entity_new(c_node_new());
		c_node(&self->arrows)->inherit_scale = 0;
		c_node(&self->arrows)->ghost = 1;

		self->X = entity_new(c_axis_new(0, VEC3(1.0f, 0.0f, 0.0f)));
		self->Y = entity_new(c_axis_new(0, VEC3(0.0f, 1.0f, 0.0f)));
		self->Z = entity_new(c_axis_new(0, VEC3(0.0f, 0.0f, 1.0f)));

#ifdef MESH4
		self->W = entity_new(c_name_new("W"), c_axis_new(0, vec4(0.0f, 0.0f, 0.0f, 1.0f)));
		c_node_add(c_node(&self->arrows), 1, self->W);
#endif

		c_node_add(c_node(&self->arrows), 3, self->X, self->Y, self->Z);
	}

	c_node_add(c_node(&ec->selected), 1, self->arrows);

	c_model_set_visible(c_model(&self->X), 1);
	c_model_set_visible(c_model(&self->Y), 1);
	c_model_set_visible(c_model(&self->Z), 1);
#ifdef MESH4
	c_model_set_visible(c_model(&self->W), 1);
#endif
	return CONTINUE;
}

int32_t translate_end(struct edit_translate *self)
{
	self->dragging = 0;
	c_model_set_visible(c_model(&self->X), 0);
	c_model_set_visible(c_model(&self->Y), 0);
	c_model_set_visible(c_model(&self->Z), 0);
#ifdef MESH4
	c_model_set_visible(c_model(&self->W), 0);
#endif
	c_node_unparent(c_node(&self->arrows), 0);
	return CONTINUE;
}

vec2_t bind_mouse_pos(struct edit_rotate *self)
{
	return self->p;
}
float bind_start_radius(struct edit_rotate *self)
{
	return self->start_radius;
}
float bind_tool_fade(struct edit_rotate *self)
{
	return self->tool_fade;
}
vec3_t bind_obj_pos(struct edit_rotate *self)
{
	return self->obj_pos;
}



int32_t rotate_init(struct edit_rotate *self, c_editmode_t *ec)
{
	self->dragging = 0;

	if(!entity_exists(self->arrows))
	{
		self->arrows = entity_new(c_node_new());
		c_node(&self->arrows)->inherit_scale = 0;
		c_node(&self->arrows)->ghost = 1;

		self->X = entity_new(c_axis_new(1, VEC3(1.0f, 0.0f, 0.0f)));
		self->Z = entity_new(c_axis_new(1, VEC3(0.0f, 0.0f, 1.0f)));
		self->Y = entity_new(c_axis_new(1, VEC3(0.0f, 1.0f, 0.0f)));

		c_spatial_rotate_Z(c_spatial(&self->X), -M_PI / 2.0f);
		c_spatial_rotate_X(c_spatial(&self->Z), M_PI / 2.0f);

		c_node_add(c_node(&self->arrows), 3, self->X, self->Y, self->Z);

		renderer_t *renderer = c_camera(&ec->camera)->renderer;

		renderer_add_pass(renderer, "tool", "editmode", ref("quad"),
				ADD, renderer_tex(renderer, ref("final")), NULL, 0,
			(bind_t[]){
				{VEC2, "mouse_pos", (getter_cb)bind_mouse_pos, self},
				{NUM, "start_radius", (getter_cb)bind_start_radius, self},
				{NUM, "tool_fade", (getter_cb)bind_tool_fade, self},
				{NUM, "tool_fade", (getter_cb)bind_tool_fade, self},
				{VEC3, "selected_pos", (getter_cb)bind_obj_pos, self},
				{NONE}
			}
		);

	}
	c_node_add(c_node(&ec->selected), 1, self->arrows);

	c_model_set_visible(c_model(&self->X), 1);
	c_model_set_visible(c_model(&self->Y), 1);
	c_model_set_visible(c_model(&self->Z), 1);
#ifdef MESH4
	c_model_set_visible(c_model(&self->W), 1);
#endif

	return CONTINUE;
}
/* TODO POLYPEN */
		/* mat_t *mat = mat_new("aux"); */
		/* mat->emissive.color = vec4(0.1f, 0.8f, 1.0f, 0.8f); */
		/* mat->albedo.color = vec4(1, 1, 1, 1.0f); */
		/* self->auxiliar = entity_new(c_node_new(), c_model_new(mesh_new(), mat, 1, 1)); */
		/* c_node(&self->auxiliar)->ghost = 1; */
		/* c_model_set_xray(c_model(&self->auxiliar), 1); */

int32_t rotate_update(struct edit_rotate *self, float dt)
{
	if(self->tool_fade > 0.0f)
	{
		self->tool_fade -= dt * 5;
		if(self->tool_fade < 0) self->tool_fade = 0.0f;
	}
	return CONTINUE;
}

int32_t rotate_end(struct edit_rotate *self)
{
	self->dragging = 0;

	c_model_set_visible(c_model(&self->X), 0);
	c_model_set_visible(c_model(&self->Y), 0);
	c_model_set_visible(c_model(&self->Z), 0);
	c_node_unparent(c_node(&self->arrows), 0);
	return CONTINUE;
}

int32_t scale_init(struct edit_scale *self, c_editmode_t *ec)
{
	self->dragging = 0;

	if(!entity_exists(self->arrows))
	{
		self->arrows = entity_new(c_node_new());
		c_node(&self->arrows)->inherit_scale = 0;
		c_node(&self->arrows)->ghost = 1;

		self->X = entity_new(c_axis_new(2, VEC3(1.0f, 0.0f, 0.0f)));
		self->Y = entity_new(c_axis_new(2, VEC3(0.0f, 1.0f, 0.0f)));
		self->Z = entity_new(c_axis_new(2, VEC3(0.0f, 0.0f, 1.0f)));

		c_node_add(c_node(&self->arrows), 3, self->X, self->Y, self->Z);
	}

	c_node_add(c_node(&ec->selected), 1, self->arrows);

	c_model_set_visible(c_model(&self->X), 1);
	c_model_set_visible(c_model(&self->Y), 1);
	c_model_set_visible(c_model(&self->Z), 1);
	return CONTINUE;
}

int32_t scale_end(struct edit_scale *self)
{
	self->dragging = 0;
	c_model_set_visible(c_model(&self->X), 0);
	c_model_set_visible(c_model(&self->Y), 0);
	c_model_set_visible(c_model(&self->Z), 0);
	c_node_unparent(c_node(&self->arrows), 0);
	return CONTINUE;
}

int32_t scale_drag(struct edit_scale *self, vec3_t p, int32_t button, c_editmode_t *ec)
{
	if(button != SDL_BUTTON_LEFT) return CONTINUE;
	c_camera_t *cam = c_camera(&ec->camera);
	c_spatial_t *sc = c_spatial(&ec->selected);
	c_node_t *ns = c_node(sc);

	vec3_t obj_pos = c_node_pos_to_global(ns, Z3);

	float dist = -mat4_mul_vec4(cam->renderer->glvars[0].inv_model,
			vec4(_vec3(obj_pos), 1.0f)).z;
	dist = c_camera_unlinearize(cam, dist);
	vec3_t proj = c_camera_real_pos(cam, dist, p.xy);
	float radius = vec3_dist(proj, obj_pos);

	if(!self->dragging)
	{
		self->start_scale = sc->scale;
		self->dragging = 1;
		self->start_radius = radius;
		signal_init(ref("transform_start"), sizeof(void*));
		entity_signal_same(ec->selected, ref("transform_start"), NULL, NULL);
	}


	c_spatial_set_scale(sc, vec3_scale(self->start_scale,
				radius / self->start_radius));
	return STOP;
}

int32_t rotate_release(struct edit_rotate *self, vec3_t p, int32_t button,
		c_editmode_t *ec)
{
	if(self->dragging && button == SDL_BUTTON_LEFT)
	{
		self->dragging = 0;
		entity_signal_same(ec->selected, ref("transform_stop"), NULL, NULL);
		c_camera_t *cam = c_camera(&ec->camera);
		renderer_toggle_pass(cam->renderer, ref("tool"), 0);
		return STOP;
	}
	return CONTINUE;
}

int32_t scale_release(struct edit_scale *self, vec3_t p, int32_t button,
                      c_editmode_t *ec)
{
	if(self->dragging && button == SDL_BUTTON_LEFT)
	{
		self->dragging = 0;
		entity_signal_same(ec->selected, ref("transform_stop"), NULL, NULL);
		return STOP;
	}
	return CONTINUE;
}

int32_t translate_release(struct edit_translate *self, vec3_t p, int32_t button,
                      c_editmode_t *ec)
{
	if(self->dragging && button == SDL_BUTTON_LEFT)
	{
		self->dragging = 0;
		entity_signal_same(ec->selected, ref("transform_stop"), NULL, NULL);
		return STOP;
	}
	return CONTINUE;
}

int32_t translate_drag(struct edit_translate *self, vec3_t p, int32_t button, c_editmode_t *ec)
{
	if(button != SDL_BUTTON_LEFT) return CONTINUE;
	c_spatial_t *sc = c_spatial(&ec->selected);
	c_node_t *ns = c_node(sc);
	c_node_t *parent = entity_exists(ns->parent) ? c_node(&ns->parent) : NULL;
	c_camera_t *cam = c_camera(&ec->camera);
	vec3_t obj_pos = c_node_pos_to_global(ns, Z3);

	if(!self->dragging)
	{
		self->start_pos = c_node_pos_to_global(ns, Z3);
		self->dragging = 1;
		entity_signal_same(ec->selected, ref("transform_start"), NULL, NULL);

		if(parent)
		{
			vec3_t local_pos = c_node_pos_to_local(parent,
					ec->mouse_position);
			self->drag_diff = vec3_sub(sc->pos, local_pos);
		}
		else
		{
			self->drag_diff = vec3_sub(sc->pos, ec->mouse_position);
		}
	}

	if(self->mode == 0)
	{
		vec3_t pos = c_camera_real_pos(cam, p.z, p.xy);

		if(parent)
		{
			pos = c_node_pos_to_local(parent, pos);
		}

		pos = vec3_add(self->drag_diff, pos);
		c_spatial_set_pos(sc, pos);
	}
	else
	{
		c_node_t *nc = c_node(cam);
		vec3_t cam_pos = c_node_pos_to_global(nc, Z3);
		c_spatial_set_pos(sc, vec3_mix(obj_pos, cam_pos,
					(self->start_screen.y - p.y) * 10.0f));
	}

	return STOP;
}

int32_t rotate_drag(struct edit_rotate *self, vec3_t p, int32_t button, c_editmode_t *ec)
{
	if(button != SDL_BUTTON_LEFT) return CONTINUE;

	c_spatial_t *sc = c_spatial(&ec->selected);
	c_node_t *ns = c_node(sc);
	c_node_t *parent = entity_exists(ns->parent) ? c_node(&ns->parent) : NULL;
	c_camera_t *cam = c_camera(&ec->camera);

	self->obj_pos = c_node_pos_to_global(ns, Z3);
	self->p = p.xy;

	float dist = -mat4_mul_vec4(cam->renderer->glvars[0].inv_model,
			vec4(_vec3(self->obj_pos), 1.0f)).z;
	dist = c_camera_unlinearize(cam, dist);
	vec3_t proj = c_camera_real_pos(cam, dist, p.xy);
	float radius = vec3_dist(proj, self->obj_pos);

	if(!self->dragging)
	{
		self->tool_fade = 1.0f;
		self->start_screen = p.xy;
		self->dragging = 1;
		entity_signal_same(ec->selected, ref("transform_start"), NULL, NULL);
		self->start_radius = radius;

		self->start_quat = sc->rot_quat;
		renderer_toggle_pass(cam->renderer, ref("tool"), 1);
	}

	c_spatial_lock(sc);

	vec2_t sp = c_camera_screen_pos(cam, self->obj_pos).xy;
	vec2_t dif = vec2_sub(p.xy, sp);

	float angle1 = atan2f(self->start_screen.y - sp.y, self->start_screen.x - sp.x);
	float angle2 = atan2f(dif.y, dif.x);
	float angle = angle1 - angle2;

	float d = radius - self->start_radius;
	if(d > 0.0f)	d = fmax(d - 0.3f, 0.0f);
	else			d = fmin(d + 0.3f, 0.0f);

	c_node_t *nc = c_node(cam);
	vec3_t cam_pos = c_node_pos_to_global(nc, Z3);
	vec3_t to_cam = vec3_sub(self->obj_pos, cam_pos);
	vec3_t axis1 = to_cam;
	if(parent) axis1 = c_node_dir_to_local(parent, to_cam);

	vec4_t rot = quat_rotate(vec3_norm(axis1), angle);
	rot = quat_mul(rot, self->start_quat);

	/* printf("%f\n", d); */
	vec3_t to_mouse = vec3_sub(self->obj_pos, proj);
	vec3_t axis2 = vec3_cross(to_cam, to_mouse);
	if(parent)
	{
		axis2 = c_node_dir_to_local(parent, axis2);
	}
	vec4_t rot2 = quat_rotate(vec3_norm(axis2), (d / (self->start_radius - 0.3))*M_PI);
	rot = quat_mul(rot2, rot);

	sc->rot_quat = rot;
	sc->update_id++;
	sc->modified = 1;
	c_spatial_unlock(sc);
	return STOP;
}

void c_editmode_add_tool(c_editmode_t *self, char key, const char *name,
                         void *init, void *mmove, void *mdrag, void *mpress,
                         void *mrelease, void *update, void *end, void *usrptr,
                         uint32_t require_component)
{
	uint32_t i;
	struct mouse_tool *tool = NULL;
	for(i = 0; i < self->tools_num; i++)
	{
		struct mouse_tool *t = &self->tools[i];
		if(t->key == key)
		{
			tool = t;
			break;
		}
	}

	if (!tool)
	{
		tool = &self->tools[self->tools_num++];
	}

	tool->key = key;
	tool->init = init;
	tool->mmove = mmove;
	tool->mpress = mpress;
	tool->mrelease = mrelease;
	tool->mdrag = mdrag;
	tool->update = update;
	tool->end = end;
	tool->usrptr = usrptr;
	tool->require_component = require_component;
}


void c_editmode_init(c_editmode_t *self)
{
	self->tool = -1;
	self->spawn_pos = vec2(10, 10);
	self->mode = EDIT_OBJECT;
	self->context = c_entity(self);
	c_node(self)->unpacked = 1;

	c_editmode_add_tool(self, 't', "translate", translate_init, NULL,
			translate_drag, NULL, translate_release, NULL, translate_end,
			calloc(1, sizeof(struct edit_translate)), ref("spacial"));

	c_editmode_add_tool(self, 'r', "rotate", rotate_init, NULL,
			rotate_drag, NULL, rotate_release, rotate_update, rotate_end,
			calloc(1, sizeof(struct edit_rotate)), ref("spacial"));

	c_editmode_add_tool(self, 's', "scale", scale_init, NULL,
			scale_drag, NULL, scale_release, NULL, scale_end,
			calloc(1, sizeof(struct edit_scale)), ref("spacial"));

	if(!g_sel_mat)
	{
		g_sel_mat = mat_new("sel_mat", "default");
		/* g_sel_mat->albedo.color = vec4(0, 0.1, 0.4, 1); */
		mat4f(g_sel_mat, ref("albedo.color"), vec4(0.0, 0.0, 0.0, 0.0));
	}
}

int32_t c_editmode_bind_mode(c_editmode_t *self)
{
	return self->mode;
}

vec2_t c_editmode_bind_over(c_editmode_t *self)
{
	return entity_to_vec2(self->over);
}
vec2_t c_editmode_bind_over_poly(c_editmode_t *self)
{
	return entity_to_vec2(self->over_poly);
}
vec2_t c_editmode_bind_mouse_position(c_editmode_t *self)
{
	return vec2(_vec2(self->mouse_iposition));
}


vec3_t c_editmode_bind_selected_pos(c_editmode_t *self)
{
	if(!entity_exists(self->selected)) return Z3;
	c_node_t *nc = c_node(&self->selected);
	if(!nc) return Z3;
	return c_node_pos_to_global(nc, Z3);
}

vec2_t c_editmode_bind_context(c_editmode_t *self)
{
	return entity_to_vec2(self->context);
}
vec3_t c_editmode_bind_context_pos(c_editmode_t *self)
{
	if (!entity_exists(self->context))
		return vec3(0.0f);
	c_node_t *node = c_node(&self->context);
	if (!node)
		return vec3(0.0f);
	return c_node_pos_to_global(node, vec3(0.0f));
}
float c_editmode_bind_context_phase(c_editmode_t *self)
{
	return self->context_enter_phase;
}
vec2_t c_editmode_bind_selected(c_editmode_t *self)
{
	return entity_to_vec2(self->selected);
}

vec2_t c_editmode_bind_sel(c_editmode_t *self)
{
	return entity_to_vec2(self->selected);
}

/* entity_t p; */
c_editmode_t *c_editmode_new()
{
	c_editmode_t *self = component_new("editmode");
	return self;
}

static renderer_t *editmode_renderer_new(c_editmode_t *self)
{
	renderer_t *renderer = renderer_new(1.00f);
	renderer_default_pipeline(renderer);

	self->mouse_depth = texture_new_2D(1, 1, 0,
		buffer_new("color",	false, 4));

	texture_t *tmp = texture_new_2D(0, 0, TEX_INTERPOLATE,
		buffer_new("color",	true, 4));
	renderer_add_tex(renderer, "tmp", 1.0f, tmp);

	renderer_add_pass(renderer, "mouse_depth", "extract_depth", ref("quad"),
			0, self->mouse_depth, NULL, 0,
		(bind_t[]){
			{CLEAR_COLOR, .vec4 = vec4(0.0f)},
			{TEX, "gbuffer", .buffer = renderer_tex(renderer, ref("gbuffer"))},
			{VEC2, "position", (getter_cb)c_editmode_bind_mouse_position, self},
			{NONE}
		}
	);

	renderer_add_pass(renderer, "context", "context", ref("quad"),
			0, renderer_tex(renderer, ref("final")), NULL, 0,
		(bind_t[]){
			{TEX, "gbuffer", .buffer = renderer_tex(renderer, ref("gbuffer"))},
			{TEX, "sbuffer", .buffer = renderer_tex(renderer, ref("selectable"))},
			{TEX, "ssao", .buffer = renderer_tex(renderer, ref("ssao"))},
			{VEC2, "over_id", (getter_cb)c_editmode_bind_over, self},
			{VEC2, "over_poly_id", (getter_cb)c_editmode_bind_over_poly, self},
			{VEC2, "context_id", (getter_cb)c_editmode_bind_context, self},
			{VEC2, "sel_id", (getter_cb)c_editmode_bind_sel, self},
			{VEC3, "context_pos", (getter_cb)c_editmode_bind_context_pos, self},
			{NUM, "context_phase", (getter_cb)c_editmode_bind_context_phase, self},
			{NONE}
		}
	);

	renderer_add_pass(renderer, "highlight", "highlight", ref("quad"),
			ADD, renderer_tex(renderer, ref("final")), NULL, 0,
		(bind_t[]){
			{TEX, "sbuffer", .buffer = renderer_tex(renderer, ref("selectable"))},
			{INT, "mode", (getter_cb)c_editmode_bind_mode, self},
			{VEC2, "over_id", (getter_cb)c_editmode_bind_over, self},
			{VEC2, "over_poly_id", (getter_cb)c_editmode_bind_over_poly, self},
			{VEC2, "context_id", (getter_cb)c_editmode_bind_context, self},
			{VEC2, "sel_id", (getter_cb)c_editmode_bind_sel, self},
			{NONE}
		}
	);


	renderer_add_pass(renderer, "highlights_0", "border", ref("quad"),
			0, tmp, NULL, 0,
		(bind_t[]){
			{CLEAR_COLOR, .vec4 = vec4(0.0f)},
			{TEX, "sbuffer", .buffer = renderer_tex(renderer, ref("selectable"))},
			{INT, "mode", (getter_cb)c_editmode_bind_mode, self},
			{VEC2, "over_id", (getter_cb)c_editmode_bind_over, self},
			{VEC2, "over_poly_id", (getter_cb)c_editmode_bind_over_poly, self},
			{VEC2, "sel_id", (getter_cb)c_editmode_bind_sel, self},
			{INT, "horizontal", .integer = 1},
			{NONE}
		}
	);

	renderer_add_pass(renderer, "highlights_1", "border", ref("quad"),
			ADD, renderer_tex(renderer, ref("final")), NULL, 0,
		(bind_t[]){
			{TEX, "sbuffer", .buffer = renderer_tex(renderer, ref("selectable"))},
			{TEX, "tmp", .buffer = tmp},
			{INT, "mode", (getter_cb)c_editmode_bind_mode, self},
			{VEC2, "over_id", (getter_cb)c_editmode_bind_over, self},
			{VEC2, "over_poly_id", (getter_cb)c_editmode_bind_over_poly, self},
			{VEC2, "sel_id", (getter_cb)c_editmode_bind_sel, self},
			{INT, "horizontal", .integer = 0},
			{NONE}
		}
	);
	renderer->ready = 0;

	return renderer;
}

void c_editmode_activate(c_editmode_t *self)
{
	c_editmode_open_entity(self, c_entity(self));

	self->activated = 1;
	self->control = 1;

	self->backup_renderer = c_window(self)->renderer;

	if(!entity_exists(self->camera))
	{
		self->camera = entity_new(
			c_name_new("Edit Camera"), c_editlook_new(), c_node_new(),
			c_camera_new(70, 0.1, 100.0, true, true, true, editmode_renderer_new(self))
		);
		c_spatial_t *sc = c_spatial(&self->camera);
		c_spatial_lock(sc);
		c_spatial_set_pos(sc, vec3(6, 6, 6));
		c_spatial_rotate_Y(sc, M_PI / 2);
		/* c_spatial_rotate_X(sc, -M_PI * 0.05); */
		c_spatial_unlock(sc);
	}

	c_camera_assign(c_camera(&self->camera));

	loader_push_wait(g_candle->loader, (loader_cb)c_editmode_activate_loader,
			NULL, (c_t*)self);

}

static int32_t c_editmode_activate_loader(c_editmode_t *self)
{
	self->nk = nk_can_init(c_window(self)->window); 
	struct nk_font_atlas *atlas; 
	nk_can_font_stash_begin(&atlas); 
	nk_can_font_stash_end(); 

	return CONTINUE;
}

void c_editmode_update_mouse(c_editmode_t *self, float x, float y)
{
	c_camera_t *cam = c_camera(&self->camera);
	renderer_t *renderer = cam->renderer;
	float px = x / renderer->width;
	float py = 1.0f - y / renderer->height;
	self->mouse_iposition = ivec2(x, renderer->height - y - 1);
	self->mouse_screen_pos.x = px;
	self->mouse_screen_pos.y = py;
	entity_t result = renderer_entity_at_pixel(renderer,
			x, y, NULL);
	uint32_t imouse_depth = texture_get_pixel(self->mouse_depth, 0, 0, 0, NULL);
	self->mouse_screen_pos.z = ((float)imouse_depth) / 65535.0f;
	/* self->mouse_screen_pos.z = 0.988937; */

	vec3_t pos = c_camera_real_pos(cam, self->mouse_screen_pos.z, vec2(px, py));
	self->mouse_position = pos;

	if(self->mode == EDIT_OBJECT)
	{
		self->over = result;
	}
	else
	{
		return;
		if(entity_exists(self->selected) && result == self->selected)
		{
			uint32_t result = renderer_geom_at_pixel(renderer, x, y,
					&self->mouse_screen_pos.z);
			self->over_poly = result;
			/* printf("%lu\n", result); */
		}
		else
		{
			self->over_poly = 0;
		}
	}
}

int32_t c_editmode_mouse_move(c_editmode_t *self, mouse_move_data *event)
{
	if(!entity_exists(self->camera)) return CONTINUE;

	renderer_t *renderer = c_camera(&self->camera)->renderer;
	vec2_t p = vec2(event->x / renderer->width,
			1.0f - event->y / renderer->height);
	self->mouse_screen_pos.xy = p;

	if(self->control && !g_candle->pressing)
	{
		if(self->tool > -1 && entity_exists(self->selected) &&
				self->selected != SYS)
		{
			struct mouse_tool *tool = &self->tools[self->tool];
			if(tool->mmove)
			{
				int32_t res = tool->mmove(tool->usrptr,
						self->mouse_position, self);
				if(c_keyboard(self)->ctrl)
				{
					c_editmode_update_mouse(self, event->x, event->y);
				}
				if(res == STOP) return STOP;
			}
			if(self->pressing_l && tool->mdrag)
			{
				int32_t res = tool->mdrag(tool->usrptr,
						self->mouse_screen_pos, SDL_BUTTON_LEFT, self);
				if(c_keyboard(self)->ctrl)
				{
					c_editmode_update_mouse(self, event->x, event->y);
				}
				if(res == STOP) return STOP;

			}
		}
		if(self->pressing_l)
		{
			self->dragging = 1;
		}

		c_editmode_update_mouse(self, event->x, event->y);

		/* if(self->tool == POLYPEN) */
		/* { */
		/* 	mesh_t *aux = c_model(&self->auxiliar)->mesh; */
		/* 	mesh_lock(aux); */
		/* 	mesh_clear(aux); */

		/* 	int32_t aux_vert0 = mesh_add_vert(aux, VEC3(_vec3(self->tool_start))); */
		/* 	int32_t aux_vert1 = mesh_add_vert(aux, VEC3(_vec3(self->mouse_position))); */
		/* 	int32_t aux_edge0 = mesh_add_edge_s(aux, aux_vert0, -1); */
		/* 	mesh_add_edge_s(aux, aux_vert1, aux_edge0); */

		/* 	mesh_unlock(aux); */
		/* } */
	}
	return CONTINUE;
}

int32_t c_editmode_mouse_press(c_editmode_t *self, mouse_button_data *event)
{
	if(!entity_exists(self->camera)) return CONTINUE;
	renderer_t *renderer = c_camera(&self->camera)->renderer;

	entity_t ent = renderer_entity_at_pixel(renderer, event->x, event->y, NULL);
	uint32_t geom = renderer_geom_at_pixel(renderer, event->x, event->y, NULL);

	if(ent)
	{
		model_button_data ev = {
			.x = event->x, .y = event->y, .direction = event->direction,
			.depth = self->mouse_screen_pos.z, .geom = geom, .button = event->button
		};
		int32_t res = entity_signal_same(ent, ref("model_press"), &ev, NULL);
		if(res == STOP) return CONTINUE;
	}

	if(event->button == SDL_BUTTON_LEFT)
	{
		self->pressing_l = 1;
	}
	if(event->button == SDL_BUTTON_RIGHT)
	{
		self->pressing_r = 1;
	}

	if(self->tool > -1 && entity_exists(self->selected) && self->selected != SYS)
	{
		struct mouse_tool *tool = &self->tools[self->tool];
		if(tool->mpress)
		{
			int32_t res = tool->mpress(tool->usrptr, self->mouse_position,
					event->button, self);
			if(res == STOP) return STOP;
		}
	}
	return CONTINUE;
}

void c_editmode_open_texture(c_editmode_t *self, texture_t *tex)
{
	if(!tex) return;
	int32_t i;
	for(i = 0; i < self->open_textures_count; i++)
	{
		if(self->open_textures[i] == tex) return;
	}

	self->spawn_pos.x += 25;
	self->spawn_pos.y += 25;
	self->open_textures[self->open_textures_count++] = tex;
}

void c_editmode_open_entity(c_editmode_t *self, entity_t ent)
{
	if(!ent) return;
	int32_t i;
	self->open_entities_count = 0;

	for(i = 0; i < self->open_entities_count; i++)
	{
		if(self->open_entities[i] == ent) return;
	}

	self->spawn_pos.x += 25;
	self->spawn_pos.y += 25;
	self->open_entities[self->open_entities_count++] = ent;
}

void c_editmode_leave_context_effective(c_editmode_t *self)
{
	if(entity_exists(self->context))
	{
		c_node_t *nc = c_node(&self->context);
		if(entity_exists(nc->parent))
		{
			c_node_pack(nc, 0);
			self->context = self->context_queued;
			c_node_pack(c_node(&self->context), 1);

			/* c_editmode_select(self, context); */
		}
	}
}

void c_editmode_leave_context(c_editmode_t *self)
{
	entity_t context = self->context;
	if(entity_exists(context))
	{
		c_node_t *nc = c_node(&context);
		if(entity_exists(nc->parent))
		{
			self->context_queued = nc->parent;
		}
	}
}

void c_editmode_enter_context_effective(c_editmode_t *self)
{
	if(entity_exists(self->context))
	{
		c_node_t *nc = c_node(&self->context);
		c_node_pack(nc, 0);
	}

	if(entity_exists(self->context_queued))
	{
		self->context = self->context_queued;
		c_node_t *nc = c_node(&self->selected);
		if(nc)
		{
			self->context = self->selected;
			c_editmode_select(self, entity_null);
			c_node_pack(nc, 1);
		}
	}
}
void c_editmode_enter_context(c_editmode_t *self)
{
	if(entity_exists(self->selected))
	{
		self->context_queued = self->selected;
	}
}

void c_editmode_select(c_editmode_t *self, entity_t select)
{
	if(!entity_exists(select)) select = SYS;
	self->selected = select;
	struct mouse_tool *tool = self->tool > -1 ? &self->tools[self->tool] : NULL;

	if(entity_exists(self->selected))
	{
		if(tool && tool->end) tool->end(tool->usrptr, self);
		c_editmode_open_entity(self, self->selected);
		self->tool = -1;
	}
	else
	{
		c_editmode_open_entity(self, SYS);
		if(tool && tool->init) tool->init(tool->usrptr, self);
	}

}

int32_t c_editmode_mouse_release(c_editmode_t *self, mouse_button_data *event)
{
	if(!entity_exists(self->camera)) return CONTINUE;
	renderer_t *renderer = c_camera(&self->camera)->renderer;

	int32_t was_dragging = self->dragging;
	if(event->button == SDL_BUTTON_LEFT)
	{
		self->dragging = 0;
		self->pressing_l = 0;
	}
	else if(event->button == SDL_BUTTON_RIGHT)
	{
		self->pressing_r = 0;
	}
	entity_t ent = renderer_entity_at_pixel(renderer, event->x, event->y,
			NULL);
	if(ent)
	{
		uint32_t geom = renderer_geom_at_pixel(renderer, event->x,
				event->y, NULL);

		model_button_data ev = {
			.x = event->x, .y = event->y, .direction = event->direction,
			.depth = self->mouse_screen_pos.z, .geom = geom, .button = event->button
		};
		int32_t res = entity_signal_same(ent, ref("model_release"), &ev, NULL);
		if(res == STOP) return CONTINUE;
	}

	if(self->tool > -1 && entity_exists(self->selected) && self->selected != SYS)
	{
		struct mouse_tool *tool = &self->tools[self->tool];
		if(tool->mrelease)
		{
			int32_t res = tool->mrelease(tool->usrptr, self->mouse_position,
					event->button, self);
			if(res == STOP) return STOP;
		}
	}

	if(!was_dragging && event->button == SDL_BUTTON_RIGHT)
	{
		self->menu_x = event->x;
		self->menu_y = event->y;
	}
	if(event->button == SDL_BUTTON_LEFT)
	{
		/* if(self->tool == POLYPEN) */
		/* { */
		/* 	if(!entity_exists(self->selected) || self->selected == SYS) */
		/* 	{ */
		/* 		c_editmode_select(self, entity_new()); */
		/* 	} */
		/* 	c_model_t *mc = c_model(&self->selected); */
		/* 	if(!mc) */
		/* 	{ */
		/* 		entity_add_component(self->selected, */
		/* 				c_model_new(mesh_new(), NULL, 1, 1)); */
		/* 		mc = c_model(&self->selected); */
		/* 		mc->mesh->has_texcoords = 0; */
		/* 	} */
		/* 	mesh_lock(mc->mesh); */
		/* 	int32_t new_vert = mesh_add_vert(mc->mesh, */
		/* 			VEC3(_vec3(self->mouse_position))); */

		/* 	int32_t new_edge = mesh_add_edge_s(mc->mesh, new_vert, */
		/* 			self->last_edge); */
		/* 	mesh_select(mc->mesh, SEL_EDITING, MESH_EDGE, new_edge); */

		/* 	self->last_edge = new_edge; */
		/* 	mesh_unlock(mc->mesh); */
		/* } */
		if(!was_dragging && self->mode == EDIT_OBJECT)
		{
			entity_t result = renderer_entity_at_pixel(renderer,
					event->x, event->y, NULL);
			if (!entity_exists(result))
			{
				entity_t context = self->context;
				if (entity_exists(context))
				{
					c_editmode_leave_context(self);
					result = context;
				}
			}
			c_editmode_select(self, result);
		}
		else if(self->mode != EDIT_OBJECT)
		{
			entity_t result = renderer_geom_at_pixel(renderer, event->x,
						event->y, &self->mouse_screen_pos.z);

			/* TODO: select one more poly */
			self->selected_poly = result;
			mesh_t *mesh = c_model(&self->selected)->mesh;
			mesh_lock(mesh);
			mesh_select(mesh, SEL_EDITING, MESH_FACE, self->selected_poly);
			mesh_modified(mesh);
			mesh_unlock(mesh);
		}
	}
	return CONTINUE;
}

int32_t c_editmode_key_up(c_editmode_t *self, char *key)
{
	uint32_t i;
	if(*key == '`')
	{
		if(!self->control)
		{
			candle_grab_mouse(c_entity(self), 1);
			self->backup_renderer = c_window(self)->renderer;
			if(!self->activated) { c_editmode_activate(self); }
			c_camera_t *cam = c_camera(&self->camera);
			cam->active = 1;
			c_window(self)->renderer = cam->renderer;
			self->control = 1;
		}
		else
		{
			candle_release_mouse(c_entity(self), 0);
			self->over = entity_null;
			c_camera_t *cam = c_camera(&self->camera);
			cam->active = 0;
			c_window(self)->renderer = self->backup_renderer;
			self->control = 0;
			/* g_candle->exit = 1; */
		}
		entity_signal(entity_null, ref("editmode_toggle"), NULL, NULL);
	}
	else if(!self->control)
	{
		return CONTINUE;
	}

	if(!entity_exists(self->selected) || self->selected == SYS)
	{
		return CONTINUE;
	}

	for(i = 0; i < self->tools_num; i++)
	{
		struct mouse_tool *tool = &self->tools[i];
		if(tool->key == *key)
		{
			if(self->tool != i)
			{
				if(self->tool > -1)
				{
					struct mouse_tool *old_tool = &self->tools[self->tool];
					if(old_tool->end) old_tool->end(old_tool->usrptr, self);
				}
				if(tool->init)
				{
					tool->init(tool->usrptr, self);
				}
				self->tool = i;
				return STOP;
			}
			break;
		}
	}
	switch(*key)
	{
		case 127: c_editmode_selected_delete(self); break;
		case 'c':
			if(entity_exists(self->selected) && self->mode == EDIT_OBJECT)
			{
				self->mode = EDIT_FACE; 
				c_model_t *cm = c_model(&self->selected);
				if(cm && cm->mesh)
				{
					mesh_t *mesh = cm->mesh;
					mesh_lock(mesh);
					/* c_model_add_layer(cm, g_sel_mat, SEL_EDITING, 0.8); */
					mesh_modified(mesh);
					mesh_unlock(mesh);
				}
			}
			else
			{
				self->mode = EDIT_OBJECT; 
			}
			break;
		case '\r': c_editmode_enter_context(self); break;
		case 27: c_editmode_select(self, SYS); break;
		default: return CONTINUE;
	}
	return STOP;
}

static void c_editmode_selected_delete(c_editmode_t *self)
{
	entity_t prev = self->selected;
	c_editmode_select(self, entity_null);
	entity_destroy(prev);
}

int32_t c_editmode_key_down(c_editmode_t *self, char *key)
{
	/* switch(*key) */
	/* { */
	/* } */
	return CONTINUE;
}

int32_t c_editmode_texture_window(c_editmode_t *self, texture_t *tex)
{
	int32_t res;
	char buffer[64];
	sprintf(buffer, "TEX_%u", tex->bufs[0].id);
	char *title = buffer;
	if(tex->name[0])
	{
		title = tex->name;
	}
	float w = tex->width;
	float h = tex->height;
	/* if (w > 1000) */
	{
		h = 800.0f * (h / w);
		w = 800.0f;
	}

	res = nk_can_begin_titled(self->nk, buffer, title,
			nk_rect(self->spawn_pos.x, self->spawn_pos.y, w + 30,
				h + 130 + 35),
			NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|
			NK_WINDOW_CLOSABLE|NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE);
	if (res)
	{
		const char *bufs[16];
		int32_t i;
		for(i = 0; i < tex->bufs_size; i++)
		{
			bufs[i] = tex->bufs[i].name;
		}

		nk_layout_row_static(self->nk, 25, 200, 1);
		tex->prev_id = nk_combo(self->nk, bufs, tex->bufs_size,
				tex->prev_id, 25, nk_vec2(200, 200));

		nk_value_int(self->nk, "glid: ", tex->bufs[tex->prev_id].id);
		nk_value_int(self->nk, "dims: ", tex->bufs[tex->prev_id].dims);
		if (nk_button_label(self->nk, "save"))
		{
			printf("Saving texture.\n");
			char *file = NULL;
			emit(ref("pick_file_save"), "png", &file);
			if(file)
			{
				int32_t res = texture_save(tex, tex->prev_id, file);
				if(res)
				{
					printf("Texture saved.\n");
				}
				else
				{
					printf("Texture failed to save.\n");
				}
				free(file);
			}
		}
		struct nk_image im = nk_image_id(tex->bufs[tex->prev_id].id);
		/* im.handle.ptr = 1; */
		struct nk_command_buffer *canvas = nk_window_get_canvas(self->nk);
		struct nk_rect total_space = nk_window_get_content_region(self->nk);
		total_space.w = w;
		total_space.h = h;
		total_space.y += 85 + 35;
		/* total_space.h -= 85; */
		nk_draw_image_ext(canvas, total_space, &im,
		                  nk_rgba(255, 255, 255, 255), 1, 0, 0, 0);
	}
	nk_end(self->nk);
	return res;
}

/* From https://github.com/wooorm/levenshtein.c */
uint32_t levenshtein(const char *a, const char *b) {
  uint32_t length = strlen(a);
  uint32_t bLength = strlen(b);
  uint32_t *cache = calloc(length, sizeof(uint32_t));
  uint32_t index = 0;
  uint32_t bIndex = 0;
  uint32_t distance;
  uint32_t bDistance;
  uint32_t result;
  char code;

  /* Shortcut optimizations / degenerate cases. */
  if (a == b) {
    return 0;
  }

  if (length == 0) {
    return bLength;
  }

  if (bLength == 0) {
    return length;
  }

  /* initialize the vector. */
  while (index < length) {
    cache[index] = index + 1;
    index++;
  }

  /* Loop. */
  while (bIndex < bLength) {
    code = b[bIndex];
    result = distance = bIndex++;
    index = -1;

    while (++index < length) {
      bDistance = code == a[index] ? distance : distance + 1;
      distance = cache[index];

      cache[index] = result = distance > result
        ? bDistance > result
          ? result + 1
          : bDistance
        : bDistance > distance
          ? distance + 1
          : bDistance;
    }
  }

  free(cache);

  return result;
}

static void insert_ct(c_editmode_t *self, int32_t ct, int32_t dist)
{
	int32_t i;
	for(i = 0; i < self->ct_list_size && i < 9; i++)
	{
		if(self->ct_list[i].distance > dist)
		{
			memmove(&self->ct_list[i + 1], &self->ct_list[i], (9 - i) *
					sizeof(self->ct_list[0]));
			self->ct_list[i].distance = dist;
			self->ct_list[i].ct = ct;
			if(self->ct_list_size < 9)
			{
				self->ct_list_size++;
			}
			return;
		}
	}
	if(self->ct_list_size < 9)
	{
		self->ct_list[self->ct_list_size].distance = dist;
		self->ct_list[self->ct_list_size].ct = ct;
		self->ct_list_size++;
		return;
	}
}

void c_editmode_shell(c_editmode_t *self)
{
	nk_layout_row_dynamic(self->nk, 25, 1);
	if(nk_edit_string_zero_terminated(self->nk, NK_EDIT_FIELD |
				NK_EDIT_SIG_ENTER |
				NK_EDIT_ALWAYS_INSERT_MODE,
				self->shell, sizeof(self->shell),
				nk_filter_ascii) & NK_EDIT_COMMITED)
	{
		entity_t instance = candle_run_command(entity_null, self->shell);
		self->shell[0] = '\0';
		if(instance)
		{
			c_editmode_select(self, instance);
		}
		self->menu_x = -1;
	}
}

int32_t c_editmode_component_menu(c_editmode_t *self, void *ctx)
{
	return CONTINUE;
}

int32_t c_editmode_commands(c_editmode_t *self)
{
	int32_t res = nk_begin(self->nk, "tools",
			nk_rect(self->menu_x, self->menu_y, 2, 2),
			NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND);
	if (res)
	{
		struct nk_rect bounds = nk_window_get_bounds(self->nk);
		if(nk_contextual_begin(self->nk, 0, nk_vec2(150, 300), bounds))
		{
			c_editmode_shell(self);

			int32_t close = 0;
			if(entity_exists(self->selected))
			{
				if(nk_button_label(self->nk, "delete"))
				{
					c_editmode_selected_delete(self);
					nk_contextual_close(self->nk);
				}
				/* TODO clone tool should be universal */
				if(c_model(&self->selected))
				if(nk_button_label(self->nk, "clone"))
				{
					mesh_t *mesh = c_model(&self->selected)->mesh;
					c_editmode_select(self, entity_new(c_model_new(mesh_clone(mesh),
									c_model(&self->selected)->mat, 1, 1)));
					close = 1;
				}
				close |= entity_signal_same(self->selected, ref("component_tool"), self->nk, NULL) == STOP;
			}
			else
			{
				if(nk_button_label(self->nk, "empty"))
				{
					c_editmode_select(self, entity_new(c_node_new()));
					close = 1;
				}
				close |= entity_signal_same(c_entity(self), ref("component_tool"), self->nk, NULL) == STOP;
			}

			if(close) nk_contextual_close(self->nk);

			nk_contextual_end(self->nk);
		}
		else
		{
			self->menu_x = -1;
		}
	}
	nk_end(self->nk);
	return res;
}

int32_t c_editmode_entity_window(c_editmode_t *self, entity_t ent)
{
	c_name_t *name = c_name(&ent);
	int32_t res;
	char buffer[64];
#ifdef WIN32
	sprintf(buffer, "ENT_%I64u", ent);
#else
	sprintf(buffer, "ENT_%lu", ent);
#endif
	char *title = buffer;
	if(name)
	{
		title = name->name;
	}

	struct nk_context *ctx = self->nk;
	struct nk_style *s = &ctx->style;
	c_window_t *window = c_window(self);
	texture_t *back = NULL;
	if(window && window->renderer && window->renderer->output)
	{
		back = renderer_tex(window->renderer, ref("refr"));
		struct nk_image background = nk_image_id(back->bufs[back->prev_id].id);
		background.w = window->width;
		background.h = window->height;
		background.region[0] = 0;
		background.region[1] = 0;
		background.region[2] = window->width;
		background.region[3] = window->height;
		s->window.fixed_background = nk_style_item_image(background);
	}

	res = nk_can_begin_titled(self->nk, "entity", title,
			nk_rect(10, 10, 230, 680),
			NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|
			NK_WINDOW_CLOSABLE|NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE);
	if (res)
	{
		/* struct nk_image im = nk_image_id(back->bufs[back->prev_id].id); */

		/* struct nk_command_buffer *canvas = nk_window_get_canvas(self->nk); */
		/* struct nk_rect total_space; */
		/* total_space.x = 0; */
		/* total_space.y = 0; */
		/* total_space.w = window->width; */
		/* total_space.h = window->height; */
		/* nk_draw_image_ext(canvas, total_space, &im, nk_rgba(255, 255, 255, 255), 1); */

		/* c_editmode_shell(self); */
		int32_t i;

		signal_t *sig = ecm_get_signal(ref("component_menu"));

		/* for(i = 0; i < sig->cts_size; i++) */
		for(i = vector_count(sig->listener_types) - 1; i >= 0; i--)
		{
			listener_t *lis = vector_get(sig->listener_types, i);
			ct_t *ct = ecm_get(lis->target);
			c_t *comp = ct_get(ct, &ent);

			if(comp && !comp->ghost)
			{
				if(nk_tree_push_id(self->nk, NK_TREE_TAB, ct->name,
							NK_MAXIMIZED, i))
				{
					component_signal(comp, ct, ref("component_menu"), self->nk, NULL);
					nk_tree_pop(self->nk);
				}
			}
		}
		struct nk_rect bounds = nk_window_get_bounds(self->nk);
		if(nk_contextual_begin(self->nk, 0, nk_vec2(150, 300), bounds))
		{

			nk_layout_row_dynamic(self->nk, 25, 1);
			if(nk_contextual_item_label(self->nk, "delete", NK_TEXT_CENTERED))
			{
				entity_destroy(ent);
			}

			int32_t active = nk_edit_string_zero_terminated(self->nk, NK_EDIT_FIELD
					| NK_EDIT_SIG_ENTER, self->ct_search,
					sizeof(self->ct_search), nk_filter_ascii) &
				NK_EDIT_COMMITED;

			if(strncmp(self->ct_search_bak, self->ct_search, sizeof(self->ct_search_bak)))
			{
				strncpy(self->ct_search_bak, self->ct_search, sizeof(self->ct_search_bak));


				self->ct_list_size = 0;
				ct_t *ct;
				ecm_foreach_ct(ct,
				{
					if(ct_get(ct, &ent)) continue;

					uint32_t dist = levenshtein(ct->name, self->ct_search);
					insert_ct(self, ct->id, dist);
				});
					
			}
			if(active)
			{
				ct_t *ct = ecm_get(self->ct_list[0].ct);
				entity_add_component(ent, component_new_from_ref(ct->id));
				nk_contextual_close(self->nk);
			}
			else
			{
				for(i = 0; i < self->ct_list_size; i++)
				{
					ct_t *ct = ecm_get(self->ct_list[i].ct);
					if (nk_contextual_item_label(self->nk, ct->name, NK_TEXT_CENTERED))
					{
						self->ct_search_bak[0] = '\0';
						self->ct_search[0] = '\0';
						self->ct_list_size = 0;
						entity_add_component(ent,
								component_new_from_ref(ct->id));
					}
				}
			}
			nk_contextual_end(self->nk);
		}
	}
	nk_end(self->nk);
	return res;
}


int32_t c_editmode_update(c_editmode_t *self, float *dt)
{
	if (self->context != self->context_queued
	    && entity_exists(self->context) && self->context != SYS
	    && self->context_enter_phase > 0.0f)
	{
		self->context_enter_phase = fmax(self->context_enter_phase - *dt, 0.0);
		if (self->context_enter_phase == 0.0f
			&& (!entity_exists(self->context_queued) || self->context_queued == SYS))
		{
			c_editmode_leave_context_effective(self);
		}
	}
	else if (self->context_enter_phase < 1.0f && (entity_exists(self->context_queued) && self->context_queued != SYS))
	{
		if (self->context_enter_phase == 0.0f)
		{
			c_editmode_enter_context_effective(self);
		}
		self->context_enter_phase = fmin(self->context_enter_phase + *dt, 1.0);
	}

	if(self->tool > -1)
	{
		struct mouse_tool *tool = &self->tools[self->tool];
		if(tool->update)
		{
			tool->update(tool->usrptr, *dt, self);
		}
	}
	return CONTINUE;
}

int32_t c_editmode_draw(c_editmode_t *self)
{
	if(self->nk && (self->visible || self->control))
	{
		if(self->menu_x >= 0)
		{
			c_editmode_commands(self);
		}
		int32_t e;
		if(self->open_vil)
		{
			if (!vifunc_gui(self->open_vil, self->nk))
			{
				self->open_vil = NULL;
			}
		}

		for(e = 0; e < self->open_textures_count; e++)
		{
			if(!c_editmode_texture_window(self, self->open_textures[e]))
			{
				self->open_textures_count--;
				self->open_textures[e] =
					self->open_textures[self->open_textures_count];
				e--;
			}
		}
		for(e = 0; e < self->open_entities_count; e++)
		{
			if(!c_editmode_entity_window(self, self->open_entities[e]))
			{
				/* self->open_entities_count--; */
				/* self->open_entities[e] = */
					/* self->open_entities[self->open_entities_count]; */
				self->open_entities[e] = SYS;
				/* e--; */
				c_editmode_open_entity(self, c_entity(self));
			}
		}

		nk_can_render(NK_ANTI_ALIASING_ON);
	}
	return CONTINUE;
}

int32_t c_editmode_events_begin(c_editmode_t *self)
{
	if(self->nk) nk_input_begin(self->nk);
	return CONTINUE;
}

int32_t c_editmode_events_end(c_editmode_t *self)
{
	if(self->nk) nk_input_end(self->nk);
	return CONTINUE;
}

int32_t c_editmode_event(c_editmode_t *self, SDL_Event *event)
{
	if(self->nk && !self->dragging)
	{
		nk_can_handle_event(event);
		if(nk_window_is_any_hovered(self->nk))
		{
			self->over = entity_null;
			return STOP;
		}
		if(nk_item_is_any_active(self->nk))
		{
			return STOP;
		}
	}

	return CONTINUE;
}

int32_t c_editmode_pick_load(c_editmode_t *self, const char *filter, char **output)
{
	/* TODO: prompt for a path in nuklear */
	printf("Warning: File picking only works with the filepicker module loaded. "
	       "A gui alternative is planned but not implemented yet.\n");
	*output = strdup("unnamed");
	return STOP;
}

int32_t c_editmode_pick_save(c_editmode_t *self, const char *filter, char **output)
{
	/* TODO: prompt for a path in nuklear */
	*output = strdup("unnamed");
	return STOP;
}

REG()
{
	ct_t *ct = ct_new("editmode", sizeof(c_editmode_t), c_editmode_init,
			NULL, 1, ref("node"));

	signal_init(ref("component_menu"), sizeof(struct nk_context*));
	signal_init(ref("component_tool"), sizeof(void*));
	signal_init(ref("editmode_toggle"), sizeof(void*));
	signal_init(ref("pick_file_save"), sizeof(void*));
	signal_init(ref("pick_file_load"), sizeof(void*));
	signal_init(ref("transform_start"), sizeof(void*));
	signal_init(ref("transform_stop"), sizeof(void*));

	ct_listener(ct, WORLD | 10, ref("key_up"), c_editmode_key_up);

	ct_listener(ct, WORLD | 10, ref("key_down"), c_editmode_key_down);

	ct_listener(ct, WORLD, ref("pick_file_save"), c_editmode_pick_save);
	ct_listener(ct, WORLD, ref("pick_file_load"), c_editmode_pick_load);

	ct_listener(ct, WORLD, ref("mouse_move"), c_editmode_mouse_move);

	ct_listener(ct, WORLD, ref("world_draw"), c_editmode_draw);

	ct_listener(ct, WORLD, ref("world_update"), c_editmode_update);

	ct_listener(ct, WORLD | 50, ref("component_menu"), c_editmode_component_menu);

	ct_listener(ct, WORLD, ref("mouse_press"), c_editmode_mouse_press);

	ct_listener(ct, WORLD, ref("mouse_release"), c_editmode_mouse_release);

	ct_listener(ct, WORLD, ref("event_handle"), c_editmode_event);

	ct_listener(ct, WORLD, ref("events_begin"), c_editmode_events_begin);

	ct_listener(ct, WORLD, ref("events_end"), c_editmode_events_end);

	/* ct_listener(ct, WORLD, ref("window_resize"), c_editmode_resize); */

}

