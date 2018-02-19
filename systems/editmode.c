#include "editmode.h"
#include <components/editlook.h>
#include <candle.h>
#include <keyboard.h>
#include <stdlib.h>
#include "renderer.h"

void nk_candle_render(enum nk_anti_aliasing AA, int max_vertex_buffer,
		int max_element_buffer);
static int c_editmode_activate_loader(c_editmode_t *self);

DEC_CT(ct_editmode);

DEC_SIG(global_menu);
DEC_SIG(component_menu);

#define MAX_VERTEX_MEMORY 512 * 1024
#define MAX_ELEMENT_MEMORY 128 * 1024


void c_editmode_init(c_editmode_t *self)
{
	self->control = 0;
	self->visible = 0;
	self->dragging = 0;
	self->pressing = 0;
	self->activated = 0;
	self->open_entities_count = 0;
	self->open_textures_count = 0;
	self->mouse_position = vec3(0.0f);
	/* self->outside = 0; */
	self->nk = NULL;
	self->spawn_pos = vec2(25, 25);
	self->selected = entity_null;
	self->over = entity_null;
	self->camera = entity_null;
}

vec3_t c_editmode_bind_selected(entity_t caller)
{
	/* if(!c_editmode(&caller)) return vec3(0.0f); */
	vec3_t id_color = int_to_vec3(c_editmode(&caller)->over);

	return id_color;
}


/* entity_t p; */
c_editmode_t *c_editmode_new()
{
	c_editmode_t *self = component_new(ct_editmode);

	/* mesh_t *cube = sauces_mesh("cube.ply"); */
	/* p = entity_new( */
			/* c_model_paint(c_model_new(cube, 1), 0, sauces_mat("pack1/white"))); */
	/* c_spacial_scale(c_spacial(&p), vec3(0.2)); */

	return self;
}

void c_editmode_activate(c_editmode_t *self)
{
	loader_push(candle->loader, (loader_cb)c_editmode_activate_loader, NULL,
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
	self->nk = nk_sdl_init(c_window(self)->window); 

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

	c_renderer_add_pass(c_renderer(self), "hightlight",
		PASS_SCREEN_SCALE,
		1.0f, 1.0f, "highlight",
		(bind_t[]){
			{BIND_GBUFFER, "gbuffer", .gbuffer.name = "opaque"},
			{BIND_VEC3, "id_color", .getter = (ptr_getter)c_editmode_bind_selected,
			.entity = c_entity(self)},
			{BIND_NONE}
		}
	);

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

	self->over = result;
}

int c_editmode_mouse_move(c_editmode_t *self, mouse_move_data *event)
{
	if(self->control && !candle->pressing)
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
	return 1;
}

int c_editmode_mouse_press(c_editmode_t *self, mouse_button_data *event)
{
	if(event->button == SDL_BUTTON_LEFT)
	{
		self->pressing = 1;

		entity_t result = c_renderer_entity_at_pixel(c_renderer(self),
				event->x, event->y, NULL);
		self->selected = result;
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
		self->selected = entity_null;
	}
	else
	{
		if(self->pressing)
		{
			c_editmode_open_entity(self, self->selected);
		}
	}
	self->pressing = 0;
	return 1;
}


int c_editmode_key_up(c_editmode_t *self, char *key)
{
	switch(*key)
	{
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

void node_node(c_editmode_t *self, c_node_t *node)
{
	char buffer[64];
	char *final_name = buffer;
	const entity_t entity = c_entity(node);
	c_name_t *name = c_name(node);
	if(name)
	{
		final_name = name->name;
	}
	else
	{
		sprintf(buffer, "NODE_%ld", entity);
	}
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
			node_node(self, c_node(&node->children[i]));
		}
		nk_tree_pop(self->nk);
	}
}

void node_tree(c_editmode_t *self)
{
	int i;
	ct_t *nodes = ecm_get(ct_node);

	if(nk_tree_push(self->nk, NK_TREE_TAB, "nodes", NK_MINIMIZED))
	{
		int p;
		for(p = 0; p < nodes->pages_size; p++)
		for(i = 0; i < nodes->pages[p].components_size; i++)
		{
			c_node_t *node = (c_node_t*)ct_get_at(nodes, p, i);
			if(node->parent != entity_null) continue;
			node_node(self, node);
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

		signal_t *sig = &g_ecm->signals[component_menu];

		for(i = 0; i < sig->cts_size; i++)
		{
			ct_t *ct = ecm_get(sig->cts[i]);
			c_t *comp = ct_get(ct, ent);
			if(comp && !ct->is_interaction)
			{
				if(nk_tree_push_id(self->nk, NK_TREE_TAB, ct->name,
							NK_MINIMIZED, i))
				{
					component_signal(comp, ct, component_menu, self->nk);
					nk_tree_pop(self->nk);
				}
				int j;
				for(j = 0; j < ct->depends_size; j++)
				{
					if(ct->depends[j].is_interaction)
					{
						c_t *inter = ct_get(ct, ent);
						ct_t *inter_ct = ecm_get(ct->depends[j].ct);
						component_signal(inter, inter_ct,
								component_menu, self->nk);
					}
				}
			}
		}
	}
	nk_end(self->nk);
	return res;
}


int c_editmode_draw(c_editmode_t *self)
{
	if(self->nk && (self->visible || self->control))
	{
		if (nk_begin(self->nk, "clidian",
					nk_rect(self->spawn_pos.x, self->spawn_pos.y, 230, 180),
					NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|
					NK_WINDOW_SCALABLE|NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE))
		{


			nk_layout_row_static(self->nk, 30, 110, 1);
			if (nk_button_label(self->nk, "systems"))
			{
				c_editmode_open_entity(self, c_entity(self));
			}

			entity_signal(c_entity(self), global_menu, self->nk);

			node_tree(self);
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

void c_editmode_register()
{
	ct_t *ct = ecm_register("EditMode", &ct_editmode,
			sizeof(c_editmode_t), (init_cb)c_editmode_init, 0);

	ecm_register_signal(&global_menu, sizeof(struct nk_context*));
	ecm_register_signal(&component_menu, sizeof(struct nk_context*));

	ct_register_listener(ct, WORLD, key_up, (signal_cb)c_editmode_key_up);

	ct_register_listener(ct, WORLD, key_down, (signal_cb)c_editmode_key_down);

	/* ct_register_listener(ct, WORLD, world_update, */
	/*		 (signal_cb)c_editmode_update); */

	ct_register_listener(ct, WORLD, mouse_move,
			(signal_cb)c_editmode_mouse_move);

	ct_register_listener(ct, WORLD|RENDER_THREAD, world_draw,
			(signal_cb)c_editmode_draw);

	ct_register_listener(ct, WORLD, mouse_press,
			(signal_cb)c_editmode_mouse_press);

	ct_register_listener(ct, WORLD, mouse_release,
			(signal_cb)c_editmode_mouse_release);

	ct_register_listener(ct, WORLD|RENDER_THREAD, event_handle,
			(signal_cb)c_editmode_event);

	ct_register_listener(ct, WORLD|RENDER_THREAD, events_begin,
			(signal_cb)c_editmode_events_begin);

	ct_register_listener(ct, WORLD|RENDER_THREAD, events_end,
			(signal_cb)c_editmode_events_end);
}

