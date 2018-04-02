#include "editmode.h"
#include <components/editlook.h>
#include <components/name.h>
#include <components/node.h>
#include <components/camera.h>
#include <components/model.h>
#include <systems/window.h>
#include <candle.h>
#include <mesh.h>
#include <keyboard.h>
#include <stdlib.h>
#include "renderer.h"

void nk_candle_render(enum nk_anti_aliasing AA, int max_vertex_buffer,
		int max_element_buffer);
static int c_editmode_activate_loader(c_editmode_t *self);

#define MAX_VERTEX_MEMORY 512 * 1024
#define MAX_ELEMENT_MEMORY 128 * 1024

mat_t *g_sel_mat = NULL;

void c_editmode_init(c_editmode_t *self)
{
	self->spawn_pos = vec2(25, 25);
	self->mode = EDIT_OBJECT;
	if(!g_sel_mat)
	{
		g_sel_mat = mat_new("sel_mat");
				/* sauces_mat("pack1/white"); */
	}
}

int c_editmode_bind_mode(c_editmode_t *self)
{
	return self->mode;
}

vec2_t c_editmode_bind_over(c_editmode_t *self)
{
	return int_to_vec2(self->over);
}
vec2_t c_editmode_bind_over_poly(c_editmode_t *self)
{
	return int_to_vec2(self->over_poly);
}

vec2_t c_editmode_bind_selected(c_editmode_t *self)
{
	return int_to_vec2(self->selected);
}


/* vec2_t c_editmode_bind_over(c_editmode_t *self) */
/* { */
/* 	return int_to_vec2(self->over); */
/* } */

/* vec2_t c_editmode_bind_selected(c_editmode_t *self) */
/* { */
/* 	return int_to_vec2(self->selected); */
/* } */


/* entity_t p; */
c_editmode_t *c_editmode_new()
{
	c_editmode_t *self = component_new("editmode");

	return self;
}

void c_editmode_activate(c_editmode_t *self)
{
	loader_push_wait(candle->loader, (loader_cb)c_editmode_activate_loader, NULL,
			(c_t*)self);

	self->activated = 1;
	self->control = 1;

	self->backup_camera = c_renderer(self)->camera;

	if(self->camera == entity_null)
	{
		self->camera = entity_new(
			c_name_new("Edit Camera"), c_editlook_new(), c_node_new(),
			c_camera_new(70, 0.1, 100.0)
		);
		c_spacial_t *sc = c_spacial(&self->camera);
		c_spacial_lock(sc);
		c_spacial_set_pos(sc, vec3(3, 3, 3));
		c_spacial_rotate_Y(sc, M_PI / 4);
		c_spacial_rotate_X(sc, -M_PI * 0.2);
		c_spacial_unlock(sc);
	}
	c_camera_activate(c_camera(&self->camera));
	c_camera_update_view(c_camera(&self->camera));
	c_renderer(self)->camera = self->camera;

}

static int c_editmode_activate_loader(c_editmode_t *self)
{
	self->nk = nk_candle_init(c_window(self)->window); 

	{ 
		struct nk_font_atlas *atlas; 
		nk_sdl_font_stash_begin(&atlas); 
		/* struct nk_font *droid = nk_font_atlas_add_from_file(atlas, "../../../extra_font/DroidSans.ttf", 14, 0); */
		/* struct nk_font *roboto = nk_font_atlas_add_from_file(atlas, "../../../extra_font/Roboto-Regular.ttf", 16, 0); */
		/* struct nk_font *future = nk_font_atlas_add_from_file(atlas, "../../../extra_font/kenvector_future_thin.ttf", 13, 0); */
		/* struct nk_font *clean = nk_font_atlas_add_from_file(atlas, "../../../extra_font/ProggyClean.ttf", 12, 0); */
		/* struct nk_font *tiny = nk_font_atlas_add_from_file(atlas, "../../../extra_font/ProggyTiny.ttf", 10, 0); */
		/* struct nk_font *cousine = nk_font_atlas_add_from_file(atlas, "../../../extra_font/Cousine-Regular.ttf", 13, 0); */
		nk_sdl_font_stash_end(); 
		/* nk_style_load_all_cursors(self->nk, atlas->cursors); */
		/* nk_style_set_font(self->nk, &roboto->handle); */
	} 

	c_renderer_add_pass(c_renderer(self), "rendered", "highlight",
			sig("render_quad"), 1.0f, PASS_DISABLE_DEPTH,
		(bind_t[]){
			{BIND_GBUFFER, "gb2"},
			{BIND_PREV_PASS_OUTPUT, "final"},
			{BIND_INTEGER, "mode", (getter_cb)c_editmode_bind_mode, self},
			{BIND_VEC2, "over_id", (getter_cb)c_editmode_bind_over, self},
			{BIND_VEC2, "over_poly_id", (getter_cb)c_editmode_bind_over_poly, self},
			{BIND_NONE}
		}
	);
	c_renderer_set_output(c_renderer(self), "rendered");

	return 1;
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
		if(result == self->selected)
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
	printf("mm\n");
	if(self->control && !candle->pressing)
	{
		if(self->mode == EDIT_OBJECT)
		{
			if(self->pressing && self->selected)
			{
				c_renderer_t *renderer = c_renderer(self);
				c_camera_t *cam = c_camera(&renderer->camera);

				c_spacial_t *sc = c_spacial(&self->selected);
				if(!self->dragging)
				{
					self->dragging = 1;
					self->drag_diff = vec3_sub(sc->pos, self->mouse_position);
				}
				float px = event->x / renderer->width;
				float py = 1.0f - event->y / renderer->height;

				vec3_t pos = c_camera_real_pos(cam, self->mouse_depth, vec2(px, py));

				vec3_t new_pos = vec3_add(self->drag_diff, pos);
				c_spacial_set_pos(sc, new_pos);
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
	return 1;
}

int c_editmode_mouse_press(c_editmode_t *self, mouse_button_data *event)
{
	if(event->button == SDL_BUTTON_LEFT)
	{
		self->pressing = 1;
	}
	return 1;
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
	for(i = 0; i < self->open_entities_count; i++)
	{
		if(self->open_entities[i] == ent) return;
	}

	self->spawn_pos.x += 25;
	self->spawn_pos.y += 25;
	self->open_entities[self->open_entities_count++] = ent;
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
		if(self->pressing)
		{
			entity_t result = c_renderer_entity_at_pixel(c_renderer(self),
					event->x, event->y, NULL);

			if(self->mode == EDIT_OBJECT)
			{
				self->selected = result;

				c_editmode_open_entity(self, self->selected);
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
	return 1;
}


int c_editmode_key_up(c_editmode_t *self, char *key)
{
	switch(*key)
	{
		case 'c':
			if(self->control)
			{
				if(self->selected && self->mode == EDIT_OBJECT)
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
		case '`':
			{
				if(!self->control)
				{
					candle_grab_mouse(candle, c_entity(self), 1);
					self->backup_camera = c_renderer(self)->camera;
					if(!self->activated) { c_editmode_activate(self); }
					c_renderer(self)->camera = self->camera;
					self->control = 1;
				}
				else
				{
					candle_release_mouse(candle, c_entity(self), 0);
					self->over = entity_null;
					c_renderer(self)->camera = self->backup_camera;
					self->control = 0;
				}
				break;
			}
	}
	return 1;
}

int c_editmode_key_down(c_editmode_t *self, char *key)
{
	/* switch(*key) */
	/* { */
	/* } */
	return 1;
}

void node_entity(c_editmode_t *self, entity_t entity)
{
	char buffer[64];
	char *final_name = buffer;
	c_name_t *name = c_name(&entity);
	if(name)
	{
		final_name = name->name;
	}
	else
	{
		sprintf(buffer, "NODE_%ld", entity);
	}
	c_node_t *node = c_node(&entity);
	if(node)
	{
		if(nk_tree_push_id(self->nk, NK_TREE_NODE, final_name, NK_MINIMIZED,
					(int)entity))
		{
			if(nk_button_label(self->nk, "select"))
			{
				c_editmode_open_entity(self, entity);
				self->selected = entity;
			}
			int i;
			for(i = 0; i < node->children_size; i++)
			{
				node_entity(self, node->children[i]);
			}
			nk_tree_pop(self->nk);
		}
	}
	else
	{
		int is_selected = self->selected == entity;
		if(nk_selectable_label(self->nk, final_name, NK_TEXT_ALIGN_LEFT, &is_selected))
		{
			c_editmode_open_entity(self, entity);
			self->selected = entity;
		}
	}
}

void tools_gui(c_editmode_t *self)
{
	/* unsigned long i; */


}

void node_tree(c_editmode_t *self)
{
	unsigned long i;

	if(nk_tree_push(self->nk, NK_TREE_TAB, "entities", NK_MINIMIZED))
	{
		for(i = 1; i < g_ecm->entities_busy_size; i++)
		{
			if(!g_ecm->entities_busy[i]) continue;
			c_node_t *node = c_node(&i);

			if(node && node->parent != entity_null) continue;
			node_entity(self, i);
		}

		nk_tree_pop(self->nk);
	}
}

int c_editmode_texture_window(c_editmode_t *self, texture_t *tex)
{
	int res;
	char buffer[64];
	sprintf(buffer, "TEX_%u", tex->id);
	char *title = buffer;
	if(tex->name[0])
	{
		title = tex->name;
	}


	res = nk_begin_titled(self->nk, buffer, title,
			nk_rect(self->spawn_pos.x, self->spawn_pos.y, 230, 280),
			NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|
			NK_WINDOW_CLOSABLE|NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE);
	if (res)
	{
		const char *bufs[16];
		int i;
		int num = tex->color_buffers_size + 1;
		for(i = 0; i < num; i++)
		{
			char *name = tex->texNames[i];
			bufs[i] = name?name:"unnamed buffer";
		}

		nk_layout_row_static(self->nk, 25, 200, 1);
		tex->prev_id = nk_combo(self->nk, bufs, num, tex->prev_id, 25,
				nk_vec2(200,200));


		/* slider color combobox */


		struct nk_image im = nk_image_id(tex->texId[tex->prev_id]);
		/* im.handle.ptr = 1; */
		struct nk_command_buffer *canvas = nk_window_get_canvas(self->nk);
		struct nk_rect total_space = nk_window_get_content_region(self->nk);
		total_space.y += 50;
		total_space.h -= 50;
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
	res = nk_begin_titled(self->nk, buffer, title,
			nk_rect(self->spawn_pos.x, self->spawn_pos.y, 230, 280),
			NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|
			NK_WINDOW_CLOSABLE|NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE);
	if (res)
	{
		int i;

		signal_t *sig = ecm_get_signal(sig("component_menu"));

		for(i = 0; i < sig->cts_size; i++)
		{
			ct_t *ct = ecm_get(sig->cts[i]);
			c_t *comp = ct_get(ct, &ent);
			if(comp && !ct->is_interaction)
			{
				if(nk_tree_push_id(self->nk, NK_TREE_TAB, ct->name,
							NK_MINIMIZED, i))
				{
					component_signal(comp, ct, sig("component_menu"), self->nk);
					nk_tree_pop(self->nk);
				}
				int j;
				for(j = 0; j < ct->depends_size; j++)
				{
					if(ct->depends[j].is_interaction)
					{
						c_t *inter = ct_get(ct, &ent);
						ct_t *inter_ct = ecm_get(ct->depends[j].ct);
						component_signal(inter, inter_ct,
								sig("component_menu"), self->nk);
					}
				}
			}
		}
		struct nk_rect bounds = nk_window_get_bounds(self->nk);
		if(nk_contextual_begin(self->nk, 0, nk_vec2(150, 300), bounds))
		{

			nk_layout_row_dynamic(self->nk, 25, 1);
			if (nk_contextual_item_label(self->nk, "delete", NK_TEXT_CENTERED))
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
					insert_ct(self, i, dist);
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
		if (nk_begin(self->nk, "Edit menu",
					nk_rect(self->spawn_pos.x, self->spawn_pos.y, 230, 380),
					NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|
					NK_WINDOW_SCALABLE|NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE))
		{


			nk_layout_row_static(self->nk, 30, 110, 1);
			if (nk_button_label(self->nk, "systems"))
			{
				c_editmode_open_entity(self, c_entity(self));
			}

			entity_signal(c_entity(self), sig("global_menu"), self->nk);

			node_tree(self);

			tools_gui(self);

			if(nk_edit_string_zero_terminated(self->nk, NK_EDIT_FIELD |
						NK_EDIT_SIG_ENTER, self->shell, sizeof(self->shell),
						nk_filter_ascii) & NK_EDIT_COMMITED)
			{
				candle_run_command(candle, entity_null, self->shell);
				self->shell[0] = '\0';
			}
		}
		nk_end(self->nk);

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
				self->open_entities_count--;
				self->open_entities[e] =
					self->open_entities[self->open_entities_count];
				e--;
			}
		}


		nk_candle_render(NK_ANTI_ALIASING_ON, MAX_VERTEX_MEMORY, MAX_ELEMENT_MEMORY);
	}
	return 1;
}

int c_editmode_events_begin(c_editmode_t *self)
{
	if(self->nk) nk_input_begin(self->nk);
	return 1;
}

int c_editmode_events_end(c_editmode_t *self)
{
	if(self->nk) nk_input_end(self->nk);
	return 1;
}

int c_editmode_event(c_editmode_t *self, SDL_Event *event)
{
	if(self->nk)
	{
		nk_sdl_handle_event(event);
		if(nk_window_is_any_hovered(self->nk))
		{
			self->over = entity_null;
			return 0;
		}
		if(nk_item_is_any_active(self->nk))
		{
			return 0;
		}
	}

	return 1;
}

REG()
{
	ct_t *ct = ct_new("editmode", sizeof(c_editmode_t),
			(init_cb)c_editmode_init, 0);

	signal_init(sig("global_menu"), sizeof(struct nk_context*));
	signal_init(sig("component_menu"), sizeof(struct nk_context*));

	ct_listener(ct, WORLD, sig("key_up"), c_editmode_key_up);

	ct_listener(ct, WORLD, sig("key_down"), c_editmode_key_down);

	ct_listener(ct, WORLD, sig("mouse_move"), c_editmode_mouse_move);

	ct_listener(ct, WORLD, sig("ui_draw"), c_editmode_draw);

	ct_listener(ct, WORLD, sig("mouse_press"), c_editmode_mouse_press);

	ct_listener(ct, WORLD, sig("mouse_release"), c_editmode_mouse_release);

	ct_listener(ct, WORLD, sig("event_handle"), c_editmode_event);

	ct_listener(ct, WORLD, sig("events_begin"), c_editmode_events_begin);

	ct_listener(ct, WORLD, sig("events_end"), c_editmode_events_end);

	/* ct_listener(ct, WORLD, sig("window_resize"), c_editmode_resize); */

}

