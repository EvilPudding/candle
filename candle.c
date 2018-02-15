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
		if(!entity_is_null(self->mouse_owners[0]))
		{
			if(entity_signal_same(self->mouse_owners[0],
						event_handle, &event) == 0)
			{
				continue;
			}
		}
		else
		{
			if(entity_signal(self->ecm->none, event_handle, &event) == 0)
			{
				continue;
			}
		}
		mouse_button_data bdata;
		mouse_move_data mdata;
		switch(event.type)
		{
			case SDL_MOUSEWHEEL:
				bdata = (mouse_button_data){event.wheel.x, event.wheel.y,
					event.wheel.direction, SDL_BUTTON_MIDDLE};
				/* if(!entity_is_null(self->mouse_owners[0])) */
				/* { */
					/* entity_signal_same(self->mouse_owners[0], mouse_wheel, &bdata); */
				/* } */
				/* else */
				{
					entity_signal(self->ecm->none, mouse_wheel, &bdata);
				}
				break;
			case SDL_MOUSEBUTTONUP:
				bdata = (mouse_button_data){event.button.x, event.button.y, 0,
					event.button.button};
				if(!entity_is_null(self->mouse_owners[0]))
				{
					entity_signal_same(self->mouse_owners[0], mouse_release, &bdata);
				}
				else
				{
					entity_signal(self->ecm->none, mouse_release, &bdata);
				}
				break;
			case SDL_MOUSEBUTTONDOWN:
				bdata = (mouse_button_data){event.button.x, event.button.y, 0,
					event.button.button};
				/* if(!entity_is_null(self->mouse_owners[0])) */
				/* { */
					/* entity_signal_same(self->mouse_owners[0], mouse_press, &bdata); */
				/* } */
				/* else */
				{
					entity_signal(self->ecm->none, mouse_press, &bdata);
				}
				break;
			case SDL_MOUSEMOTION:
				self->mx = event.motion.x; self->my = event.motion.y;
				mdata = (mouse_move_data){event.motion.xrel, event.motion.yrel,
						event.motion.x, event.motion.y};
				if(!entity_is_null(self->mouse_owners[0]))
				{
					entity_signal_same(self->mouse_owners[0], mouse_move, &mdata);
				}
				else
				{
					entity_signal(self->ecm->none, mouse_move, &mdata);
				}
				break;
			case SDL_KEYUP:
				key = event.key.keysym.sym;
				if(key == -31)
				{
					self->shift = 0;
				}
				entity_signal(self->ecm->none, key_up, &key);
				break;
			case SDL_KEYDOWN:
				key = event.key.keysym.sym;
				if(key == -31)
				{
					self->shift = 1;
				}
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
			c_window_draw(c_window(&self->systems));

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
	uint i = self->templates_size++;
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

void candle_release_mouse(candle_t *self, entity_t ent, int reset)
{
	int i;
	for(i = 0; i < 16; i++)
	{
		if(entity_equal(self->mouse_owners[i], ent))
		{
			/* // SDL_SetWindowGrab(mainWindow, SDL_FALSE); */
			SDL_SetRelativeMouseMode(SDL_FALSE);
			if(reset)
			{
				SDL_WarpMouseInWindow(c_window(&self->systems)->window, self->mo_x,
						self->mo_y);
			}
			for(; i < 15; i++)
			{
				self->mouse_owners[i] = self->mouse_owners[i + 1];
				self->mouse_visible[i] = self->mouse_visible[i + 1];

			}
		}
	}
	int vis = self->mouse_visible[0];
	SDL_ShowCursor(vis); 
	SDL_SetRelativeMouseMode(!vis);
}

void candle_grab_mouse(candle_t *self, entity_t ent, int visibility)
{
	int i;
	for(i = 15; i >= 1; i--)
	{
		self->mouse_owners[i] = self->mouse_owners[i - 1];
		self->mouse_visible[i] = self->mouse_visible[i - 1];
	}
	self->mouse_owners[0] = ent;
	self->mouse_visible[0] = visibility;
	self->mo_x = self->mx;
	self->mo_y = self->my;
	SDL_ShowCursor(visibility); 
	SDL_SetRelativeMouseMode(!visibility);
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
	for(i = 0; i < 4; i++)
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
		c_editlook_register(self->ecm);

		/* OpenGL mesh plugin */
		c_mesh_gl_register(self->ecm);

		c_physics_register(self->ecm);
		c_window_register(self->ecm);
		c_renderer_register(self->ecm);
		c_editmode_register(self->ecm);
		c_camera_register(self->ecm);
		c_sauces_register(self->ecm);

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

	self->systems = entity_new(self->ecm, 3, c_window_new(0, 0),
			c_physics_new(), c_sauces_new());

	self->mouse_owners[0] = entity_null();
	self->mouse_visible[0] = 1;

	/* candle_import_dir(self, self->ecm->none, "./"); */

	return self;
}

candle_t *candle;
