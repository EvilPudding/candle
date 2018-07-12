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
#include <stdlib.h>
#include "renderer.h"

static int c_editmode_activate_loader(c_editmode_t *self);
void c_editmode_open_entity(c_editmode_t *self, entity_t ent);
static void c_editmode_update_axis(c_editmode_t *self);
static void c_editmode_selected_delete(c_editmode_t *self);

#define MAX_VERTEX_MEMORY (512 * 1024) / 4
#define MAX_ELEMENT_MEMORY (128 * 1024) / 4

mat_t *g_sel_mat = NULL;

entity_t arrows, X, Y, Z, W, RX, RY, RZ;
void c_editmode_init(c_editmode_t *self)
{
	self->spawn_pos = vec2(10, 10);
	self->mode = EDIT_OBJECT;
	self->context = c_entity(self);
	c_node(self)->unpacked = 1;
	if(!g_sel_mat)
	{
		g_sel_mat = mat_new("sel_mat");
		/* g_sel_mat->albedo.color = vec4(0, 0.1, 0.4, 1); */
		g_sel_mat->albedo.color = vec4(0.0, 0.0, 0.0, 0.0);
		g_sel_mat->transparency.color = vec4(0.6, 0.3, 0.1, 0.0f);
				/* sauces_mat("white"); */
	}
}

int c_editmode_bind_mode(c_editmode_t *self)
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

vec2_t c_editmode_bind_context(c_editmode_t *self)
{
	return entity_to_vec2(self->context);
}
vec2_t c_editmode_bind_selected(c_editmode_t *self)
{
	return entity_to_vec2(self->selected);
}

vec2_t c_editmode_bind_sel(c_editmode_t *self)
{
	return entity_to_vec2(self->selected);
}
void c_editmode_coords(c_editmode_t *self)
{
	if(!entity_exists(arrows))
	{
		arrows = entity_new(c_name_new("Coord System"), c_node_new());
		c_node(&arrows)->inherit_scale = 0;
		c_node(&arrows)->ghost = 1;

		X = entity_new(c_name_new("X"), c_axis_new(0, vec4(1.0f, 0.0f, 0.0f, 0.0f)));
		Z = entity_new(c_name_new("Z"), c_axis_new(0, vec4(0.0f, 0.0f, 1.0f, 0.0f)));
		Y = entity_new(c_name_new("Y"), c_axis_new(0, vec4(0.0f, 1.0f, 0.0f, 0.0f)));

		RX = entity_new(c_name_new("RX"), c_axis_new(1, vec4(1.0f, 0.0f, 0.0f, 0.0f)));
		RZ = entity_new(c_name_new("RZ"), c_axis_new(1, vec4(0.0f, 0.0f, 1.0f, 0.0f)));
		RY = entity_new(c_name_new("RY"), c_axis_new(1, vec4(0.0f, 1.0f, 0.0f, 0.0f)));

		c_node_add(c_node(&arrows), 6, X, Y, Z, RX, RY, RZ);

		c_spacial_rotate_Z(c_spacial(&RX), -M_PI / 2.0f);
		c_spacial_rotate_X(c_spacial(&RZ), M_PI / 2.0f);

#ifdef MESH4
		W = entity_new(c_name_new("W"), c_axis_new(0, vec4(0.0f, 0.0f, 0.0f, 1.0f)));
		c_node_add(c_node(&arrows), 1, W);
#endif
	}
	if(entity_exists(self->selected))
	{
		c_node_t *nc = c_node(&self->selected);
		/* c_attach_target(c_attach(&arrows), self->selected); */
		if(!nc)
		{
			entity_add_component(self->selected, c_node_new());
			nc = c_node(&self->selected);
		}
		c_node_add(nc, 1, arrows);
	}
	else
	{
		c_node_unparent(c_node(&arrows), 0);
	}
	c_editmode_update_axis(self);
}


/* entity_t p; */
c_editmode_t *c_editmode_new()
{
	c_editmode_t *self = component_new("editmode");
	return self;
}

void c_editmode_activate(c_editmode_t *self)
{
	c_editmode_open_entity(self, c_entity(self));

	loader_push_wait(g_candle->loader, (loader_cb)c_editmode_activate_loader,
			NULL, (c_t*)self);

	self->activated = 1;
	self->control = 1;

	self->backup_camera = c_renderer(self)->camera;

	if(!entity_exists(self->camera))
	{
		self->camera = entity_new(
			c_name_new("Edit Camera"), c_editlook_new(), c_node_new(),
			c_camera_new(70, 0.1, 100.0)
		);
		c_spacial_t *sc = c_spacial(&self->camera);
		c_spacial_lock(sc);
		c_spacial_set_pos(sc, vec3(4, 1.5, 0));
		c_spacial_rotate_Y(sc, M_PI / 2);
		c_spacial_rotate_X(sc, -M_PI * 0.05);
		c_spacial_unlock(sc);
	}
	c_camera_activate(c_camera(&self->camera));
	c_camera_update_view(c_camera(&self->camera));
	c_renderer(self)->camera = self->camera;

}

static int c_editmode_activate_loader(c_editmode_t *self)
{
	self->nk = nk_can_init(c_window(self)->window); 


	{ 
		struct nk_font_atlas *atlas; 
		nk_can_font_stash_begin(&atlas); 
		/* struct nk_font *droid = nk_font_atlas_add_from_file(atlas, "../../../extra_font/DroidSans.ttf", 14, 0); */
		/* struct nk_font *roboto = nk_font_atlas_add_from_file(atlas, "../../../extra_font/Roboto-Regular.ttf", 16, 0); */
		/* struct nk_font *future = nk_font_atlas_add_from_file(atlas, "../../../extra_font/kenvector_future_thin.ttf", 13, 0); */
		/* struct nk_font *clean = nk_font_atlas_add_from_file(atlas, "../../../extra_font/ProggyClean.ttf", 12, 0); */
		/* struct nk_font *tiny = nk_font_atlas_add_from_file(atlas, "../../../extra_font/ProggyTiny.ttf", 10, 0); */
		/* struct nk_font *cousine = nk_font_atlas_add_from_file(atlas, "../../../extra_font/Cousine-Regular.ttf", 13, 0); */
		nk_can_font_stash_end(); 
		/* nk_style_load_all_cursors(self->nk, atlas->cursors); */
		/* nk_style_set_font(self->nk, &roboto->handle); */
	} 

	c_renderer_t *renderer = c_renderer(self);
	c_renderer_add_pass(renderer, "highlights", "highlight", sig("render_quad"),
			PASS_MULTIPLY,
		(bind_t[]){
			{BIND_OUT, .buffer = c_renderer_tex(renderer, ref("final"))},
			{BIND_TEX, "sbuffer", .buffer = c_renderer_tex(renderer, ref("selectable"))},
			{BIND_INT, "mode", (getter_cb)c_editmode_bind_mode, self},
			{BIND_VEC2, "over_id", (getter_cb)c_editmode_bind_over, self},
			{BIND_VEC2, "over_poly_id", (getter_cb)c_editmode_bind_over_poly, self},
			{BIND_VEC2, "context_id", (getter_cb)c_editmode_bind_context, self},
			{BIND_VEC2, "sel_id", (getter_cb)c_editmode_bind_sel, self},
			{BIND_NONE}
		}
	);

	c_renderer_add_pass(renderer, "highlights_0", "border", sig("render_quad"),
			PASS_CLEAR_COLOR,
		(bind_t[]){
			{BIND_OUT, .buffer = c_renderer_tex(renderer, ref("tmp"))},
			{BIND_TEX, "sbuffer", .buffer = c_renderer_tex(renderer, ref("selectable"))},
			{BIND_INT, "mode", (getter_cb)c_editmode_bind_mode, self},
			{BIND_VEC2, "over_id", (getter_cb)c_editmode_bind_over, self},
			{BIND_VEC2, "over_poly_id", (getter_cb)c_editmode_bind_over_poly, self},
			{BIND_VEC2, "sel_id", (getter_cb)c_editmode_bind_sel, self},
			{BIND_INT, "horizontal", .integer = 1},
			{BIND_NONE}
		}
	);

	c_renderer_add_pass(renderer, "highlights_1", "border", sig("render_quad"),
			PASS_ADDITIVE,
		(bind_t[]){
			{BIND_OUT, .buffer = c_renderer_tex(renderer, ref("final"))},
			{BIND_TEX, "sbuffer", .buffer = c_renderer_tex(renderer, ref("selectable"))},
			{BIND_TEX, "tmp", .buffer = c_renderer_tex(renderer, ref("tmp"))},
			{BIND_INT, "mode", (getter_cb)c_editmode_bind_mode, self},
			{BIND_VEC2, "over_id", (getter_cb)c_editmode_bind_over, self},
			{BIND_VEC2, "over_poly_id", (getter_cb)c_editmode_bind_over_poly, self},
			{BIND_VEC2, "sel_id", (getter_cb)c_editmode_bind_sel, self},
			{BIND_INT, "horizontal", .integer = 0},
			{BIND_NONE}
		}
	);

	return CONTINUE;
}

void c_editmode_update_mouse(c_editmode_t *self, float x, float y)
{
	c_renderer_t *renderer = c_renderer(self);
	c_camera_t *cam = c_camera(&renderer->camera);
	float px = x / renderer->width;
	float py = 1.0f - y / renderer->height;
	entity_t result = c_renderer_entity_at_pixel(renderer,
			x, y, &self->mouse_depth);

	vec3_t pos = c_camera_real_pos(cam, self->mouse_depth, vec2(px, py));
	self->mouse_position = pos;

	if(self->mode == EDIT_OBJECT)
	{
		self->over = result;
	}
	else
	{
		if(entity_exists(self->selected) && result == self->selected)
		{
			entity_t result = c_renderer_geom_at_pixel(renderer, x, y,
					&self->mouse_depth);
			self->over_poly = result;
			/* printf("%lu\n", result); */
		}
		else
		{
			self->over_poly = 0;
		}
	}
}

int c_editmode_mouse_move(c_editmode_t *self, mouse_move_data *event)
{
	if(self->control && !g_candle->pressing)
	{
		if(self->mode == EDIT_OBJECT)
		{
			if(self->pressing && entity_exists(self->selected))
			{
				c_renderer_t *renderer = c_renderer(self);
				c_camera_t *cam = c_camera(&renderer->camera);

				c_spacial_t *sc = c_spacial(&self->selected);
				entity_t parent = c_node(&self->selected)->parent;
				if(!self->dragging)
				{
					self->dragging = 1;

					if(entity_exists(parent))
					{
						c_node_t *nc = c_node(&parent);
						vec3_t local_pos = c_node_global_to_local(nc, self->mouse_position);
						self->drag_diff = vec3_sub(sc->pos, local_pos);
					}
					else
					{
						self->drag_diff = vec3_sub(sc->pos, self->mouse_position);
					}
				}
				float px = event->x / renderer->width;
				float py = 1.0f - event->y / renderer->height;

				vec3_t pos = c_camera_real_pos(cam, self->mouse_depth, vec2(px, py));
				if(entity_exists(parent))
				{
					c_node_t *nc = c_node(&parent);
					pos = c_node_global_to_local(nc, pos);
				}

				c_spacial_set_pos(sc, vec3_add(self->drag_diff, pos));
			}
			else
			{
				c_editmode_update_mouse(self, event->x, event->y);
			}
		}
		else
		{
			c_editmode_update_mouse(self, event->x, event->y);
		}
	}
	return CONTINUE;
}

int c_editmode_mouse_press(c_editmode_t *self, mouse_button_data *event)
{
	if(event->button == SDL_BUTTON_LEFT)
	{
		self->pressing = 1;
	}
	return CONTINUE;
}

void c_editmode_open_texture(c_editmode_t *self, texture_t *tex)
{
	if(!tex) return;
	int i;
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
	int i;
	self->open_entities_count = 0;

	for(i = 0; i < self->open_entities_count; i++)
	{
		if(self->open_entities[i] == ent) return;
	}

	self->spawn_pos.x += 25;
	self->spawn_pos.y += 25;
	self->open_entities[self->open_entities_count++] = ent;
}

void c_editmode_leave_context(c_editmode_t *self)
{
	entity_t context = self->context;
	if(entity_exists(context))
	{
		c_node_t *nc = c_node(&context);
		if(entity_exists(nc->parent))
		{
			c_node_pack(nc, 0);

			self->context = nc->parent;
			c_node_pack(c_node(&self->context), 1);

			/* c_editmode_select(self, context); */
		}
	}
}
void c_editmode_enter_context(c_editmode_t *self)
{
	if(entity_exists(self->context))
	{
		c_node_t *nc = c_node(&self->context);
		c_node_pack(nc, 0);
	}

	if(entity_exists(self->selected))
	{
		c_node_t *nc = c_node(&self->selected);
		if(nc)
		{
			printf("unpacking %ld\n", self->selected);
			c_node_pack(nc, 1);
			self->context = self->selected;
			c_editmode_select(self, entity_null);
		}
	}
}

void c_editmode_select(c_editmode_t *self, entity_t select)
{
	self->selected = select;
	c_editmode_coords(self);

	if(entity_exists(self->selected))
	{
		c_editmode_open_entity(self, self->selected);
	}
	else
	{
		c_editmode_open_entity(self, SYS);
	}

}

int c_editmode_mouse_release(c_editmode_t *self, mouse_button_data *event)
{
	if(self->dragging)
	{
		self->dragging = 0;
		/* self->selected = entity_null; */
	}
	else
	{
		if(event->button == SDL_BUTTON_RIGHT)
		{
			self->menu_x = event->x;
			self->menu_y = event->y;
		}
		if(self->pressing)
		{
			entity_t result = c_renderer_entity_at_pixel(c_renderer(self),
					event->x, event->y, NULL);

			if(self->mode == EDIT_OBJECT)
			{
				c_editmode_select(self, result);
			}
			else// if(result == self->selected)
			{
				entity_t result =
					c_renderer_geom_at_pixel(c_renderer(self), event->x,
							event->y, &self->mouse_depth);

				/* TODO: select one more poly */
				self->selected_poly = result;
				mesh_t *mesh = c_model(&self->selected)->mesh;
				mesh_lock(mesh);
				mesh_select(mesh, SEL_EDITING, MESH_FACE, self->selected_poly);
				mesh_modified(mesh);
				mesh_unlock(mesh);
			}
		}
	}
	self->pressing = 0;
	return CONTINUE;
}


static void c_editmode_update_axis(c_editmode_t *self)
{
	if(entity_exists(arrows))
	{
		c_model(&RX)->visible = self->selected && self->tool;
		c_model(&RY)->visible = self->selected && self->tool;
		c_model(&RZ)->visible = self->selected && self->tool;
		c_model(&X)->visible = self->selected && !self->tool;
		c_model(&Y)->visible = self->selected && !self->tool;
		c_model(&Z)->visible = self->selected && !self->tool;
#ifdef MESH4
		c_model(&W)->visible = self->selected && !self->tool;
#endif
	}
}

int c_editmode_key_up(c_editmode_t *self, char *key)
{
	switch(*key)
	{
		case 'c':
			if(self->control)
			{
				if(entity_exists(self->selected) && self->mode == EDIT_OBJECT)
				{
					self->mode = EDIT_FACE; 
					c_model_t *cm = c_model(&self->selected);
					if(cm && cm->mesh)
					{
						mesh_t *mesh = cm->mesh;
						mesh_lock(mesh);
						c_model_add_layer(cm, g_sel_mat, SEL_EDITING, 0.8);
						mesh_modified(mesh);
						mesh_unlock(mesh);
					}
				}
				else
				{
					self->mode = EDIT_OBJECT; 
				}
			}
			break;
		case 't':
			self->tool = 0;
			c_editmode_update_axis(self);
			break;
		case 'r':
			self->tool = 1;
			c_editmode_update_axis(self);
			break;
		case '`':
			{
				if(!self->control)
				{
					candle_grab_mouse(c_entity(self), 1);
					self->backup_camera = c_renderer(self)->camera;
					if(!self->activated) { c_editmode_activate(self); }
					c_renderer(self)->camera = self->camera;
					self->control = 1;
				}
				else
				{
					candle_release_mouse(c_entity(self), 0);
					self->over = entity_null;
					c_renderer(self)->camera = self->backup_camera;
					self->control = 0;
				}
				break;
			}
		case 127:
			c_editmode_selected_delete(self);
			break;
		case 27:
			if(entity_exists(self->selected))
			{
				self->menu_x = -1;
				c_editmode_select(self, entity_null);
			}
			else
			{
				c_editmode_leave_context(self);
			}
			break;
		case '\r':
			c_editmode_enter_context(self);
			break;
	}
	return CONTINUE;
}

static void c_editmode_selected_delete(c_editmode_t *self)
{
	entity_t prev = self->selected;
	c_editmode_select(self, entity_null);
	entity_destroy(prev);
}

int c_editmode_key_down(c_editmode_t *self, char *key)
{
	/* switch(*key) */
	/* { */
	/* } */
	return CONTINUE;
}

void tools_gui(c_editmode_t *self)
{
	/* unsigned long i; */

}

int c_editmode_texture_window(c_editmode_t *self, texture_t *tex)
{
	int res;
	char buffer[64];
	sprintf(buffer, "TEX_%u", tex->bufs[0].id);
	char *title = buffer;
	if(tex->name[0])
	{
		title = tex->name;
	}

	res = nk_begin_titled(self->nk, buffer, title,
			nk_rect(self->spawn_pos.x, self->spawn_pos.y, tex->width + 30,
				tex->height + 130),
			NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|
			NK_WINDOW_CLOSABLE|NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE);
	if (res)
	{
		const char *bufs[16];
		int i;
		for(i = 0; i < tex->bufs_size; i++)
		{
			bufs[i] = tex->bufs[i].name;
		}

		nk_layout_row_static(self->nk, 25, 200, 1);
		tex->prev_id = nk_combo(self->nk, bufs, tex->bufs_size,
				tex->prev_id, 25, nk_vec2(200, 200));


		/* slider color combobox */


		nk_value_int(self->nk, "glid: ", tex->bufs[tex->prev_id].id);
		nk_value_int(self->nk, "dims: ", tex->bufs[tex->prev_id].dims);
		struct nk_image im = nk_image_id(tex->bufs[tex->prev_id].id);
		/* im.handle.ptr = 1; */
		struct nk_command_buffer *canvas = nk_window_get_canvas(self->nk);
		struct nk_rect total_space = nk_window_get_content_region(self->nk);
		total_space.w = tex->width;
		total_space.h = tex->height;

		total_space.y += 85;
		/* total_space.h -= 85; */
		nk_draw_image_ext(canvas, total_space, &im, nk_rgba(255, 255, 255, 255), 1);
	}
	nk_end(self->nk);
	return res;
}

/* From https://github.com/wooorm/levenshtein.c */
unsigned int levenshtein(const char *a, const char *b) {
  unsigned int length = strlen(a);
  unsigned int bLength = strlen(b);
  unsigned int *cache = calloc(length, sizeof(unsigned int));
  unsigned int index = 0;
  unsigned int bIndex = 0;
  unsigned int distance;
  unsigned int bDistance;
  unsigned int result;
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

static void insert_ct(c_editmode_t *self, int ct, int dist)
{
	int i;
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

int c_editmode_component_menu(c_editmode_t *self, void *ctx)
{
	return CONTINUE;
}

int c_editmode_commands(c_editmode_t *self)
{
	int res = nk_begin(self->nk, "tools",
			nk_rect(self->menu_x, self->menu_y, 2, 2),
			NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND);
	if (res)
	{
		struct nk_rect bounds = nk_window_get_bounds(self->nk);
		if(nk_contextual_begin(self->nk, 0, nk_vec2(150, 300), bounds))
		{
			c_editmode_shell(self);
			if(entity_exists(self->selected))
			{
				if(nk_button_label(self->nk, "delete"))
				{
					c_editmode_selected_delete(self);
					nk_contextual_close(self->nk);
				}
				if(c_model(&self->selected))
				{
					if(nk_button_label(self->nk, "subdivide"))
					{
						c_model_edit(c_model(&self->selected), MESH_SUBDIVIDE, MESH_FACE);
						nk_contextual_close(self->nk);
					}
					if(nk_button_label(self->nk, "spherize"))
					{
						c_model_edit(c_model(&self->selected), MESH_SPHERIZE, MESH_FACE);
						nk_contextual_close(self->nk);
					}
					if(nk_button_label(self->nk, "extrude"))
					{
						c_model_edit(c_model(&self->selected), MESH_EXTRUDE, MESH_FACE);
						nk_contextual_close(self->nk);
					}
					if(nk_button_label(self->nk, "clone"))
					{
						mesh_t *mesh = c_model(&self->selected)->mesh;
						c_editmode_select(self,
								entity_new(c_model_new(mesh_clone(mesh), mat_new("k"),
										1, 1)));
						nk_contextual_close(self->nk);
					}
				}
			}
			else
			{
				if(nk_button_label(self->nk, "cube"))
				{
					c_editmode_select(self, entity_new(c_model_new(NULL, mat_new("k"), 1, 1)));
					c_model_edit(c_model(&self->selected), MESH_CUBE, MESH_FACE);
					nk_contextual_close(self->nk);
				}
				if(nk_button_label(self->nk, "torus"))
				{
					c_editmode_select(self, entity_new(c_model_new(NULL, mat_new("k"), 1, 1)));
					c_model_edit(c_model(&self->selected), MESH_TORUS, MESH_FACE);
					nk_contextual_close(self->nk);
				}
				if(nk_button_label(self->nk, "icosphere"))
				{
					c_editmode_select(self, entity_new(c_model_new(NULL, mat_new("k"), 1, 1)));
					c_model_edit(c_model(&self->selected), MESH_ICOSPHERE, MESH_FACE);
					nk_contextual_close(self->nk);
				}
				if(nk_button_label(self->nk, "circle"))
				{
					c_editmode_select(self, entity_new(c_model_new(NULL, mat_new("k"), 1, 1)));
					c_model_edit(c_model(&self->selected), MESH_CIRCLE, MESH_FACE);
					nk_contextual_close(self->nk);
				}
			}
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

int c_editmode_entity_window(c_editmode_t *self, entity_t ent)
{
	c_name_t *name = c_name(&ent);
	int res;
	char buffer[64];
	sprintf(buffer, "ENT_%ld", ent);
	char *title = buffer;
	if(name)
	{
		title = name->name;
	}
	res = nk_begin_titled(self->nk, "entity", title,
			nk_rect(10, 10, 230, 680),
			NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|
			NK_WINDOW_CLOSABLE|NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE);
	if (res)
	{
		/* c_editmode_shell(self); */
		int i;

		signal_t *sig = ecm_get_signal(sig("component_menu"));

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
					component_signal(comp, ct, sig("component_menu"), self->nk);
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

			int active = nk_edit_string_zero_terminated(self->nk, NK_EDIT_FIELD
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

					uint dist = levenshtein(ct->name, self->ct_search);
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


int c_editmode_draw(c_editmode_t *self)
{

	if(self->nk && (self->visible || self->control))
	{
		if(self->menu_x >= 0)
		{
			c_editmode_commands(self);
		}
		int e;
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


		nk_can_render(NK_ANTI_ALIASING_ON, MAX_VERTEX_MEMORY, MAX_ELEMENT_MEMORY);
	}
	return CONTINUE;
}

int c_editmode_events_begin(c_editmode_t *self)
{
	if(self->nk) nk_input_begin(self->nk);
	return CONTINUE;
}

int c_editmode_events_end(c_editmode_t *self)
{
	if(self->nk) nk_input_end(self->nk);
	return CONTINUE;
}

int c_editmode_event(c_editmode_t *self, SDL_Event *event)
{
	if(self->nk)
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

REG()
{
	ct_t *ct = ct_new("editmode", sizeof(c_editmode_t), c_editmode_init,
			NULL, 1, ref("node"));

	signal_init(sig("component_menu"), sizeof(struct nk_context*));

	ct_listener(ct, WORLD, sig("key_up"), c_editmode_key_up);

	ct_listener(ct, WORLD, sig("key_down"), c_editmode_key_down);

	ct_listener(ct, WORLD, sig("mouse_move"), c_editmode_mouse_move);

	ct_listener(ct, WORLD, sig("world_draw"), c_editmode_draw);

	ct_listener(ct, WORLD | 50, sig("component_menu"), c_editmode_component_menu);

	ct_listener(ct, WORLD, sig("mouse_press"), c_editmode_mouse_press);

	ct_listener(ct, WORLD, sig("mouse_release"), c_editmode_mouse_release);

	ct_listener(ct, WORLD, sig("event_handle"), c_editmode_event);

	ct_listener(ct, WORLD, sig("events_begin"), c_editmode_events_begin);

	ct_listener(ct, WORLD, sig("events_end"), c_editmode_events_end);

	/* ct_listener(ct, WORLD, sig("window_resize"), c_editmode_resize); */

}

