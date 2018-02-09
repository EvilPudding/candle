#include "candle.h"
#include "file.h"
#include <nk.h>

#ifndef WIN32
#include <unistd.h>
#endif
#include <dirent.h>

unsigned long world_update;
unsigned long world_draw;
unsigned long global_menu;
unsigned long component_menu;

#define MAX_VERTEX_MEMORY 512 * 1024
#define MAX_ELEMENT_MEMORY 128 * 1024

void candle_reset_dir(candle_t *self)
{
#ifdef WIN32
    _chdir(self->firstDir);
#else
	if(chdir(self->firstDir)) printf("Chdir failed.\n");
#endif
}

static void candle_handle_events(candle_t *self)
{
	SDL_Event event;
	/* SDL_WaitEvent(&event); */
	/* keySpec(state->key_state, state); */
	char key;
	if(self->nkctx) nk_input_begin(self->nkctx);
	while(SDL_PollEvent(&event))
	{
		if(!self->pressing)
		{
			if(self->nkctx && nk_sdl_handle_event(&event) &&
					nk_item_is_any_active(self->nkctx))
			{
				continue;
			}
		}
		switch(event.type)
		{
			case SDL_MOUSEWHEEL:
				entity_signal(self->ecm->none, mouse_wheel,
						&(mouse_button_data){event.wheel.x, event.wheel.y,
						event.wheel.direction, SDL_BUTTON_MIDDLE});
				break;
			case SDL_MOUSEBUTTONUP:
				self->pressing = 0;
				entity_signal(self->ecm->none, mouse_release,
						&(mouse_button_data){event.button.x, event.button.y,
						0, event.button.button});
				break;
			case SDL_MOUSEBUTTONDOWN:
				self->pressing = 1;
				entity_signal(self->ecm->none, mouse_press,
						&(mouse_button_data){event.button.x, event.button.y,
						0, event.button.button});
				break;
			case SDL_MOUSEMOTION:
				entity_signal(self->ecm->none, mouse_move,
						&(mouse_move_data){event.motion.xrel, event.motion.yrel});
				break;
			case SDL_QUIT:
				self->exit = 1;
				break;
			case SDL_KEYUP:
				key = event.key.keysym.sym;
				entity_signal(self->ecm->none, key_up, &key);
				break;
			case SDL_KEYDOWN:
				key = event.key.keysym.sym;
				entity_signal(self->ecm->none, key_down, &key);
				break;
			case SDL_WINDOWEVENT:
				switch(event.window.event)
				{
					case SDL_WINDOWEVENT_RESIZED:

						printf("window resize: %dx%d\n", event.window.data1,
								event.window.data2);
						entity_signal(self->ecm->none, window_resize,
								&(window_resize_data){
								.width = event.window.data1,
								.height = event.window.data2});
						break; 
				}
				break;
		}
		/* break; */
	}
	if(self->nkctx) nk_input_end(self->nkctx);
}

void node_node(candle_t *self, c_node_t *node)
{
	char buffer[64];
	char *final_name = buffer;
	const entity_t entity = c_entity(node);
	c_name_t *name = c_name(entity);
	if(name)
	{
		final_name = name->name;
	}
	else
	{
		sprintf(buffer, "%ld", entity.id);
	}
	if(nk_tree_push_id(self->nkctx, NK_TREE_NODE, final_name, NK_MINIMIZED,
				entity.id))
	{
		int i;
		for(i = 0; i < node->children_size; i++)
		{
			node_node(self, c_node(node->children[i]));
		}
		nk_tree_pop(self->nkctx);
	}
}

void node_tree(candle_t *self)
{
	int i;
	ct_t *nodes = ecm_get(self->ecm, ct_node);

	if(nk_tree_push(self->nkctx, NK_TREE_TAB, "nodes", NK_MINIMIZED))
	{
		for(i = 0; i < nodes->components_size; i++)
		{
			c_node_t *node = (c_node_t*)ct_get_at(nodes, i);
			if(node->parent.id != -1) continue;
			node_node(self, node);
		}
		nk_tree_pop(self->nkctx);
	}
}

static int render_loop(candle_t *self)
{
	int last = SDL_GetTicks();
	int fps = 0;

	//SDL_GL_MakeCurrent(state->renderer->window, state->renderer->context); 


	while(!self->exit)
	{
		if(!self->nkctx)
		{
			self->nkctx = nk_sdl_init(c_window(self->systems)->window); 

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
				/* nk_style_load_all_cursors(self->nkctx, atlas->cursors); */
				/* nk_style_set_font(self->nkctx, &roboto->handle); */
			} 
		}

		candle_handle_events(self);
		loader_update(self->loader);

		/* if(state->gameStarted) */
		{
			/* candle_handle_events(self); */
			entity_signal(self->ecm->none, world_draw, NULL);


			/* GUI */
			if(self->nkctx)
			{
				if (nk_begin(self->nkctx, "clidian", nk_rect(50, 50, 230, 180),
							NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|
							NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE))
				{

					nk_layout_row_static(self->nkctx, 30, 110, 1);
					if (nk_button_label(self->nkctx, "redraw probes"))
					{
						c_renderer_scene_changed(c_renderer(self->systems), NULL);
					}
					nk_layout_row_static(self->nkctx, 30, 110, 1);
					if (nk_button_label(self->nkctx, "pick"))
					{
						c_renderer_get_pixel(c_renderer(self->systems), 0, 7,
								window_width / 2, window_height / 2);
					}
					entity_signal(self->ecm->none, global_menu, self->nkctx);

					node_tree(self);
				}
				nk_end(self->nkctx);
				nk_sdl_render(NK_ANTI_ALIASING_ON, MAX_VERTEX_MEMORY, MAX_ELEMENT_MEMORY);

			}
			if(!entity_is_null(self->selected))
			{
				char buffer[64];
				char *final_name = buffer;
				c_name_t *name = c_name(self->selected);
				if(name)
				{
					final_name = name->name;
				}
				else
				{
					sprintf(buffer, "%ld", self->selected.id);
				}
				if (nk_begin(self->nkctx, final_name, nk_rect(300, 50, 230, 180),
							NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|
							NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE))
				{
					entity_signal_same(self->selected, component_menu, self->nkctx);
				}
				nk_end(self->nkctx);
			}
			c_window_draw(c_window(self->systems));

			fps++;
			/* candle_handle_events(self); */

		}

		int current = SDL_GetTicks();
		if(current - last > 1000)
		{
			printf("FPS: %d\n", fps);
			fps = 0;
			last = current;
		}
		glerr();
		SDL_Delay(16);
		/* SDL_Delay(1); */
	}
	return 1;
}

void candle_register(ecm_t *ecm)
{
	world_update = ecm_register_signal(ecm, sizeof(float));
	world_draw = ecm_register_signal(ecm, sizeof(void*));
	global_menu = ecm_register_signal(ecm, 0);
	component_menu = ecm_register_signal(ecm, 0);
}

static void updateWorld(candle_t *self)
{
	int current = SDL_GetTicks();
	float dt = (current - self->last_update) / 1000.0;
	entity_signal(self->ecm->none, world_update, &dt);
	self->last_update = current;
}

static int candle_loop(candle_t *candle)
{
	do
	{
		/* candle_handle_events(candle); */
		updateWorld(candle);
		SDL_Delay(16);
	}
	while(!candle->exit);
	return 1;
}

void candle_init(candle_t *candle)
{
	SDL_CreateThread((int(*)(void*))candle_loop, "candle_loop", candle);
	render_loop(candle);
}

void candle_register_template(candle_t *self, const char *key,
		template_cb cb)
{
	unsigned long i = self->templates_size++;
	self->templates = realloc(self->templates,
			sizeof(*self->templates) * self->templates_size);
	template_t *template = &self->templates[i];
	template->cb = cb;
	strncpy(template->key, key, sizeof(template->key) - 1);
}

int candle_import(candle_t *self, entity_t root, const char *map_name)
{
	printf("Importing '%s'\n", map_name);

	FILE *file = fopen(map_name, "r");

	if(file == NULL) return 0;

	entity_t pass = root;

	while(!feof(file))
	{
		char name[32];
		if(fscanf(file, "%s ", name) == -1) continue;
		template_t *template;

		for(template = self->templates; template->key; template++)
		{
			if(!strcmp(name, template->key))
			{
				pass = template->cb(pass, file, self);
				break;
			}
		}
		if(entity_is_null(pass)) pass = root;
	}

	fclose(file);

	return 1;
}

static inline int is_dir(const char *f)
{
	DIR *dir = opendir(f);
	if(dir == NULL) return 0;
	closedir(dir);
	return 1;
}

extern char g_materials_path[256];



int candle_get_materials_at(candle_t *self, const char *dir_name)
{
	char dir_buffer[2048];
	strncpy(dir_buffer, g_materials_path, sizeof(dir_buffer));

	path_join(dir_buffer, sizeof(dir_buffer), path_relative(dir_name, g_materials_path));

	DIR *dir = opendir(dir_buffer);
	if(dir == NULL) return 0;

	struct dirent *ent;
	while((ent = readdir(dir)) != NULL)
	{
		char buffer[512];

		if(ent->d_name[0] == '.') continue;

		strncpy(buffer, dir_buffer, sizeof(buffer));
		path_join(buffer, sizeof(buffer), ent->d_name);

		char *ext = strrchr(ent->d_name, '.');
		if(ext && ext != ent->d_name && !strcmp(ext, ".mat"))
		{
			ext = strrchr(buffer, '.');
			*ext = '\0';
			material_from_file(buffer, self);
			continue;
		}

		if(is_dir(buffer))
		{
			if(!material_from_file(buffer, self))
			{
				candle_get_materials_at(self, buffer);
			}
			continue;
		}



	}
	closedir(dir);

	return 1;
}

int candle_import_dir(candle_t *self, entity_t root, const char *dir_name)
{
	DIR *dir = opendir(dir_name);
	if(dir != NULL)
	{
		struct dirent *ent;
		while((ent = readdir(dir)) != NULL)
		{
			candle_import(self, root, ent->d_name);
		}
		closedir(dir);
	}
	else
	{
		return 0;
	}
	return 1;
}

void candle_material_reg(candle_t *self, const char *name, material_t *material)
{
	unsigned long i = self->resources.materials_size++;
	self->resources.materials = realloc(self->resources.materials,
			sizeof(*self->resources.materials) * self->resources.materials_size);
	self->resources.materials[i] = material;
	if(material->name != name) strncpy(material->name, name, sizeof(material->name));
}

material_t *candle_material_get(candle_t *self, const char *name)
{
	material_t *material;
	unsigned long i;
	for(i = 0; i < self->resources.materials_size; i++)
	{
		material = self->resources.materials[i];
		if(!strcmp(material->name, name)) return material;
	}
	material = material_from_file(name, self);
	if(material) candle_material_reg(self, name, material);
	return material;
}

void candle_mesh_reg(candle_t *self, const char *name, mesh_t *mesh)
{
	unsigned long i = self->resources.meshes_size++;
	self->resources.meshes = realloc(self->resources.meshes,
			sizeof(*self->resources.meshes) * self->resources.meshes_size);
	self->resources.meshes[i] = mesh;
	strncpy(mesh->name, name, sizeof(mesh->name));
}

mesh_t *candle_mesh_get(candle_t *self, const char *name)
{
	mesh_t *mesh;
	unsigned long i;
	for(i = 0; i < self->resources.meshes_size; i++)
	{
		mesh = self->resources.meshes[i];
		if(!strcmp(mesh->name, name)) return mesh;
	}
	mesh = mesh_from_file(name);
	candle_mesh_reg(self, name, mesh);
	return mesh;
}

texture_t *candle_texture_get(candle_t *self, const char *name)
{
	texture_t *texture;
	unsigned long i;
	for(i = 0; i < self->resources.textures_size; i++)
	{
		texture = self->resources.textures[i];
		if(!strcmp(texture->name, name)) return texture;
	}
	texture = texture_from_file(name);
	candle_texture_reg(self, name, texture);

	return texture;
}

void candle_texture_reg(candle_t *self, const char *name, texture_t *texture)
{
	if(!texture) return;
	unsigned long i = self->resources.textures_size++;
	self->resources.textures = realloc(self->resources.textures,
			sizeof(*self->resources.textures) * self->resources.textures_size);
	self->resources.textures[i] = texture;
	strncpy(texture->name, name, sizeof(texture->name));
}

candle_t *candle_new(int comps_size, ...)
{
	candle_t *self = calloc(1, sizeof *self);
	candle = self;

	self->selected = entity_null();
	self->ecm = ecm_new();

	self->firstDir = SDL_GetBasePath();
	candle_reset_dir(self);

	shaders_reg();

	candle_register(self->ecm);
	c_spacial_register(self->ecm);
	c_node_register(self->ecm);
	c_velocity_register(self->ecm);
	c_force_register(self->ecm);
	c_freemove_register(self->ecm);
	c_freelook_register(self->ecm);
	c_model_register(self->ecm);
	c_rigid_body_register(self->ecm);
	c_aabb_register(self->ecm);
	c_probe_register(self->ecm);
	c_light_register(self->ecm);
	c_ambient_register(self->ecm);
	c_name_register(self->ecm);

	/* OpenGL mesh plugin */
	c_mesh_gl_register(self->ecm);

	keyboard_register(self->ecm);
	mouse_register(self->ecm);

	c_physics_register(self->ecm);
	c_window_register(self->ecm);
	c_renderer_register(self->ecm);
	c_camera_register(self->ecm);

	self->loader = loader_new(0);

	va_list comps;
	va_start(comps, comps_size);
	while(comps_size--) (va_arg(comps, c_reg_cb))(self->ecm);
	va_end(comps);

	self->systems = entity_new(self->ecm, 2, c_window_new(0, 0), c_physics_new());

	/* candle_import_dir(self, self->ecm->none, "./"); */

	return self;
}

candle_t *candle;
