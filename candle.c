#include "candle.h"
#include "file.h"

#ifndef WIN32
#include <unistd.h>
#endif
#include <dirent.h>

DEC_SIG(world_update);
DEC_SIG(world_draw);

DEC_SIG(events_begin);
DEC_SIG(event_handle);
DEC_SIG(events_end);

DEC_SIG(global_menu);
DEC_SIG(component_menu);

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

	entity_signal(self->ecm->none, events_begin, NULL);
	while(SDL_PollEvent(&event))
	{
		if(event.type == SDL_QUIT)
		{
			self->exit = 1;
			return;
		}
		if(!self->pressing)
		{
			if(entity_signal(self->ecm->none, event_handle, &event) == 0)
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
						&(mouse_move_data){event.motion.xrel, event.motion.yrel,
						event.motion.x, event.motion.y});
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
	entity_signal(self->ecm->none, events_end, NULL);
}

static int render_loop(candle_t *self)
{
	int last = SDL_GetTicks();
	int fps = 0;

	//SDL_GL_MakeCurrent(state->renderer->window, state->renderer->context); 


	while(!self->exit)
	{
		candle_handle_events(self);
		loader_update(self->loader);

		/* if(state->gameStarted) */
		{
			/* candle_handle_events(self); */
			entity_signal(self->ecm->none, world_draw, NULL);


			/* GUI */
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
		/* SDL_Delay(16); */
		/* SDL_Delay(1); */
	}
	return 1;
}

void candle_register(ecm_t *ecm)
{
	ecm_register_signal(ecm, &world_update, sizeof(float));
	ecm_register_signal(ecm, &world_draw, sizeof(void*));
	ecm_register_signal(ecm, &event_handle, sizeof(void*));
	ecm_register_signal(ecm, &events_end, sizeof(void*));
	ecm_register_signal(ecm, &events_begin, sizeof(void*));
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
	ulong i = self->templates_size++;
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
	ulong i = self->resources.materials_size++;
	self->resources.materials = realloc(self->resources.materials,
			sizeof(*self->resources.materials) * self->resources.materials_size);
	self->resources.materials[i] = material;
	if(material->name != name) strncpy(material->name, name, sizeof(material->name));
}

material_t *candle_material_get(candle_t *self, const char *name)
{
	material_t *material;
	ulong i;
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
	ulong i = self->resources.meshes_size++;
	self->resources.meshes = realloc(self->resources.meshes,
			sizeof(*self->resources.meshes) * self->resources.meshes_size);
	self->resources.meshes[i] = mesh;
	strncpy(mesh->name, name, sizeof(mesh->name));
}

mesh_t *candle_mesh_get(candle_t *self, const char *name)
{
	mesh_t *mesh;
	ulong i;
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
	ulong i;
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
	ulong i = self->resources.textures_size++;
	self->resources.textures = realloc(self->resources.textures,
			sizeof(*self->resources.textures) * self->resources.textures_size);
	self->resources.textures[i] = texture;
	strncpy(texture->name, name, sizeof(texture->name));
}

candle_t *candle_new(int comps_size, ...)
{
	candle_t *self = calloc(1, sizeof *self);
	candle = self;

	self->ecm = ecm_new();

	self->firstDir = SDL_GetBasePath();
	candle_reset_dir(self);

	shaders_reg();


	int i;
	for(i = 0; i < 3; i++)
	{
		candle_register(self->ecm);

		keyboard_register(self->ecm);
		mouse_register(self->ecm);

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

		c_physics_register(self->ecm);
		c_window_register(self->ecm);
		c_renderer_register(self->ecm);
		c_editmode_register(self->ecm);
		c_camera_register(self->ecm);


		va_list comps;
		va_start(comps, comps_size);
		int i;
		for(i = 0; i < comps_size; i++)
		{
			c_reg_cb cb = va_arg(comps, c_reg_cb);
			cb(self->ecm);
		}
		va_end(comps);
	}

	self->loader = loader_new(0);

	self->systems = entity_new(self->ecm, 2, c_window_new(0, 0), c_physics_new());

	/* candle_import_dir(self, self->ecm->none, "./"); */

	return self;
}

candle_t *candle;
