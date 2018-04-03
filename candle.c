#include "candle.h"
#include "file.h"

#include <string.h>
#include <stdlib.h>

#include <systems/renderer.h>
#include <systems/physics.h>
#include <systems/window.h>
#include <systems/sauces.h>

#include <components/node.h>

#ifndef WIN32
#include <unistd.h>
#endif
#include <dirent.h>
#include <fcntl.h>

void candle_reset_dir(candle_t *self)
{
#ifdef WIN32
    _chdir(self->firstDir);
#else
	if(chdir(self->firstDir)) printf("Chdir failed.\n");
#endif
}

int handle_event(candle_t *self, SDL_Event event)
{
	char key;
	if(self->mouse_owners[0] != entity_null)
	{
		if(entity_signal_same(self->mouse_owners[0], sig("event_handle"),
					&event) == 0)
		{
			return 1;
		}
	}
	else
	{
		if(entity_signal(entity_null, sig("event_handle"), &event) == 0)
		{
			return 1;
		}
	}
	mouse_button_data bdata;
	mouse_move_data mdata;
	switch(event.type)
	{
		case SDL_MOUSEWHEEL:
			bdata = (mouse_button_data){event.wheel.x, event.wheel.y,
				event.wheel.direction, SDL_BUTTON_MIDDLE};
			entity_signal(entity_null, sig("mouse_wheel"), &bdata);
			break;
		case SDL_MOUSEBUTTONUP:
			bdata = (mouse_button_data){event.button.x, event.button.y, 0,
				event.button.button};
			if(self->mouse_owners[0])
			{
				entity_signal_same(self->mouse_owners[0], sig("mouse_release"),
						&bdata);
			}
			else
			{
				entity_signal(entity_null, sig("mouse_release"), &bdata);
			}
			break;
		case SDL_MOUSEBUTTONDOWN:
			bdata = (mouse_button_data){event.button.x, event.button.y, 0,
				event.button.button};
			if(self->mouse_owners[0])
			{
				entity_signal_same(self->mouse_owners[0], sig("mouse_press"),
						&bdata);
			}
			else
			{
				entity_signal(entity_null, sig("mouse_press"), &bdata);
			}
			break;
		case SDL_MOUSEMOTION:
			self->mx = event.motion.x; self->my = event.motion.y;
			mdata = (mouse_move_data){event.motion.xrel, event.motion.yrel,
				event.motion.x, event.motion.y};

			if(self->mouse_owners[0])
			{
				entity_signal_same(self->mouse_owners[0], sig("mouse_move"),
						&mdata);
			}
			else
			{
				entity_signal(entity_null, sig("mouse_move"), &mdata);
			}
			break;
		case SDL_KEYUP:
			key = event.key.keysym.sym;
			if(key == -31)
			{
				self->shift = 0;
			}
			entity_signal(entity_null, sig("key_up"), &key);
			break;
		case SDL_KEYDOWN:
			key = event.key.keysym.sym;
			if(key == -31)
			{
				self->shift = 1;
			}
			entity_signal(entity_null, sig("key_down"), &key);
			break;
		case SDL_WINDOWEVENT:
			switch(event.window.event)
			{
				case SDL_WINDOWEVENT_RESIZED:
				c_window_handle_resize(c_window(&self->systems), event);
					break; 
			}
			break;
	}
	/* break; */

	return 0;
}
static void candle_handle_events(candle_t *self)
{
	SDL_Event event;
	/* SDL_WaitEvent(&event); */
	/* keySpec(state->key_state, state); */
	int has_events = 0;
	entity_signal(entity_null, sig("events_begin"), NULL);
	while(SDL_PollEvent(&event))
	{
		/* if(!) */
		{
			/* has_events = 1; */
			if(event.type == SDL_QUIT)
			{
				//close(candle->events[0]);
				//close(candle->events[1]);
				self->exit = 1;
				return;
			}
			/* int res = write(candle->events[1], &event, sizeof(event)); */
			/* if(res == -1) exit(1); */
			handle_event(self, event);
		}
	}
	entity_signal(entity_null, sig("events_end"), NULL);
	if(has_events)
	{
		/* event.type = SDL_USEREVENT; */
		/* write(candle->events[1], &event, sizeof(event)); */
	}
}

static int render_loop(candle_t *self)
{
	self->loader = loader_new();

	int last = SDL_GetTicks();
	int fps = 0;
	self->render_id = SDL_ThreadID();
	//SDL_GL_MakeCurrent(state->renderer->window, state->renderer->context); 
	/* SDL_LockMutex(self->mut); */
	entity_add_component(self->systems, c_window_new(0, 0));
	/* printf("unlock 2\n"); */
	SDL_SemPost(self->sem);

	while(!self->exit)
	{
		candle_handle_events(self);
		loader_update(self->loader);

		/* if(state->gameStarted) */
		{
			/* candle_handle_events(self); */
			/* printf("\t%ld\n", self->render_id); */
			entity_signal(entity_null, sig("world_draw"), NULL);
			entity_signal(entity_null, sig("ui_draw"), NULL);

			ecm_clean();

			c_window_draw(c_window(&self->systems));

			fps++;
			/* candle_handle_events(self); */

		}

		int current = SDL_GetTicks();
		if(current - last > 1000)
		{
			self->fps = fps;
			fps = 0;
			last = current;
		}
		glerr();
		/* SDL_Delay(16); */
		/* SDL_Delay(1); */
	}
	return 1;
}

void candle_register()
{
	signal_init(sig("world_update"), sizeof(float));
	signal_init(sig("world_draw"), sizeof(void*));
	signal_init(sig("event_handle"), sizeof(void*));
	signal_init(sig("events_end"), sizeof(void*));
	signal_init(sig("events_begin"), sizeof(void*));

	/* TODO remove ui_draw in favour of sorted by priority listeners */
	signal_init(sig("ui_draw"), sizeof(void*));
}

static int ticker_loop(candle_t *self)
{
	do
	{
		int current = SDL_GetTicks();
		float dt = (current - self->last_update) / 1000.0;
		entity_signal(entity_null, sig("world_update"), &dt);
		ecm_clean();
		self->last_update = current;
		SDL_Delay(16);
	}
	while(!self->exit);
	return 1;
}

/* static int candle_loop(candle_t *self) */
/* { */
/* 	SDL_Event event; */

/* 	ssize_t s; */
/* 	while((s = read(candle->events[0], &event, sizeof(event))) > 0) */
/* 	{ */
/* 		/1* if(event.type == SDL_USEREVENT) break; *1/ */
/* 		handle_event(self, event); */
/* 	} */
/* 	return 1; */
/* } */

void candle_wait(candle_t *candle)
{
	/* SDL_WaitThread(candle->candle_thr, NULL); */
	SDL_WaitThread(candle->render_thr, NULL);
	SDL_WaitThread(candle->ticker_thr, NULL);
}

void candle_reg_prefab(candle_t *self, const char *key, prefab_cb cb)
{
	uint i = self->prefabs_size++;
	self->prefabs = realloc(self->prefabs,
			sizeof(*self->prefabs) * self->prefabs_size);
	prefab_t *prefab = &self->prefabs[i];
	prefab->cb = cb;
	strncpy(prefab->key, key, sizeof(prefab->key) - 1);
}

entity_t candle_run_command(candle_t *self, entity_t root, char *command)
{
	/* TODO: optimize this function */
	int i;

	prefab_t *prefab;

	entity_t instance = root;

	char separators[] = " ";

	char *p = strtok(command, separators);
	char *argv[32];
	int argc = 0;

	argv[argc++] = strdup(p);

	while((p = strtok(NULL, separators)) != NULL)
	{
		argv[argc++] = strdup(p);
	}

	for(prefab = self->prefabs; prefab->key; prefab++)
	{
		if(!strcmp(argv[0], prefab->key))
		{
			instance = prefab->cb(instance, argc, argv);
			break;
		}
	}
	for(i = 0; i < argc; i++)
	{
		free(argv[i]);
	}
	if(instance == entity_null) instance = root;
	return instance;
}

int candle_run(candle_t *self, entity_t root, const char *map_name)
{
	FILE *file = fopen(map_name, "r");

	if(file == NULL) return 0;

	char *line = NULL;
	size_t n = 0;
	while(1)
	{
		ssize_t read = getline(&line, &n, file);
		if(read == -1) break;
		if(read == 0) continue;
		entity_t entity = candle_run_command(self, root, line);

		if(root && c_node(&root) && entity != root)
		{
			c_node_add(c_node(&root), 1, entity);
		}
	}
	free(line);

	fclose(file);

	return 1;
}

/* int candle_import_dir(candle_t *self, entity_t root, const char *dir_name) */
/* { */
/* 	DIR *dir = opendir(dir_name); */
/* 	if(dir != NULL) */
/* 	{ */
/* 		struct dirent *ent; */
/* 		while((ent = readdir(dir)) != NULL) */
/* 		{ */
/* 			candle_import(self, root, ent->d_name); */
/* 		} */
/* 		closedir(dir); */
/* 	} */
/* 	else */
/* 	{ */
/* 		return 0; */
/* 	} */
/* 	return 1; */
/* } */

void candle_release_mouse(candle_t *self, entity_t ent, int reset)
{
	int i;
	for(i = 0; i < 16; i++)
	{
		if(self->mouse_owners[i] == ent)
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

__attribute__((constructor (CONSTR_BEFORE_REG)))
void candle_init(void)
{
	candle_t *self = calloc(1, sizeof *self);
	candle = self;

	ecm_init();

	self->firstDir = SDL_GetBasePath();
	candle_reset_dir(self);

	shaders_reg();

	candle_register();

}

__attribute__((constructor (CONSTR_AFTER_REG)))
void candle_init2(void)
{
	if(candle->sem) return;

	candle->mouse_owners[0] = entity_null;
	candle->mouse_visible[0] = 1;

	candle->systems = entity_new(c_mouse_new(), c_keyboard_new(),
			c_physics_new(), c_sauces_new());

	//int res = pipe(candle->events);
	//if(res == -1) exit(1);

	/* candle->candle_thr = SDL_CreateThread((int(*)(void*))candle_loop, "candle_loop", candle); */
	candle->sem = SDL_CreateSemaphore(0);
	candle->render_thr = SDL_CreateThread((int(*)(void*))render_loop, "render_loop", candle);
	candle->ticker_thr = SDL_CreateThread((int(*)(void*))ticker_loop, "ticker_loop", candle);
	SDL_SemWait(candle->sem);
	/* SDL_Delay(500); */

	/* candle_import_dir(candle, entity_null, "./"); */
}

candle_t *candle;
