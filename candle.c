#include "candle.h"
#include <utils/file.h>

#include <string.h>
#include <stdlib.h>

#include <systems/renderer.h>
#include <systems/physics.h>
#include <systems/window.h>
#include <systems/sauces.h>
#include <systems/nodegraph.h>

#include <components/node.h>
#include <components/name.h>

#ifndef WIN32
#include <unistd.h>
#endif
#include <dirent.h>
#include <fcntl.h>

candle_t *g_candle;

void candle_reset_dir()
{
#ifdef WIN32
    _chdir(g_candle->firstDir);
#else
	if(chdir(g_candle->firstDir)) printf("Chdir failed.\n");
#endif
}

int handle_event(SDL_Event event)
{
	char key;
	if(entity_exists(g_candle->mouse_owners[0]))
	{
		if(entity_signal_same(g_candle->mouse_owners[0], sig("event_handle"),
					&event) == STOP)
		{
			return CONTINUE;
		}
	}
	else
	{
		if(entity_signal(entity_null, sig("event_handle"), &event) == STOP)
		{
			return CONTINUE;
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
			if(entity_exists(g_candle->mouse_owners[0]))
			{
				entity_signal_same(g_candle->mouse_owners[0], sig("mouse_release"),
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
			if(entity_exists(g_candle->mouse_owners[0]))
			{
				entity_signal(g_candle->mouse_owners[0], sig("mouse_press"),
						&bdata);
			}
			else
			{
				entity_signal(entity_null, sig("mouse_press"), &bdata);
			}
			break;
		case SDL_MOUSEMOTION:
			g_candle->mx = event.motion.x; g_candle->my = event.motion.y;
			mdata = (mouse_move_data){event.motion.xrel, event.motion.yrel,
				event.motion.x, event.motion.y};

			if(entity_exists(g_candle->mouse_owners[0]))
			{
				entity_signal_same(g_candle->mouse_owners[0], sig("mouse_move"),
						&mdata);
			}
			else
			{
				entity_signal(entity_null, sig("mouse_move"), &mdata);
			}
			break;
		case SDL_KEYUP:
			key = event.key.keysym.sym;
			if(key == -32)
			{
				c_keyboard(&SYS)->ctrl = 0;
			}
			if(key == -31)
			{
				c_keyboard(&SYS)->shift = 0;
			}
			entity_signal(entity_null, sig("key_up"), &key);
			break;
		case SDL_KEYDOWN:
			key = event.key.keysym.sym;
			if(key == -32)
			{
				c_keyboard(&SYS)->ctrl = 1;
			}
			if(key == -31)
			{
				c_keyboard(&SYS)->shift = 1;
			}
			entity_signal(entity_null, sig("key_down"), &key);
			break;
		case SDL_WINDOWEVENT:
			switch(event.window.event)
			{
				case SDL_WINDOWEVENT_RESIZED:
				c_window_handle_resize(c_window(&SYS), event);
					break; 
			}
			break;
	}
	/* break; */

	return STOP;
}
static void candle_handle_events(void)
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
				g_candle->exit = 1;
				return;
			}
			/* int res = write(candle->events[1], &event, sizeof(event)); */
			/* if(res == -1) exit(1); */
			handle_event(event);
		}
	}
	entity_signal(entity_null, sig("events_end"), NULL);
	if(has_events)
	{
		/* event.type = SDL_USEREVENT; */
		/* write(candle->events[1], &event, sizeof(event)); */
	}
}

static int render_loop(void)
{
	g_candle->loader = loader_new();

	int last = SDL_GetTicks();
	int fps = 0;
	g_candle->render_id = SDL_ThreadID();
	//SDL_GL_MakeCurrent(state->renderer->window, state->renderer->context); 
	/* SDL_LockMutex(g_candle->mut); */
	entity_add_component(SYS, c_window_new(0, 0));
	/* printf("unlock 2\n"); */
	SDL_SemPost(g_candle->sem);

	while(!g_candle->exit)
	{
		candle_handle_events();
		loader_update(g_candle->loader);

		/* if(state->gameStarted) */
		{
			/* candle_handle_events(self); */
			/* printf("\t%ld\n", self->render_id); */
			entity_signal(entity_null, sig("world_draw"), NULL);

			ecm_clean(0);

			c_window_draw(c_window(&SYS));

			fps++;
			/* candle_handle_events(self); */

		}

		int current = SDL_GetTicks();
		if(current - last > 1000)
		{
			g_candle->fps = fps;
			fps = 0;
			last = current;
		}
		glerr();
		/* SDL_Delay(16); */
		/* SDL_Delay(1); */
	}

	ecm_clean(1);
	loader_update(g_candle->loader);

	return 1;
}

void candle_register()
{
	signal_init(sig("world_update"), sizeof(float));
	signal_init(sig("world_draw"), sizeof(void*));
	signal_init(sig("event_handle"), sizeof(void*));
	signal_init(sig("events_end"), sizeof(void*));
	signal_init(sig("events_begin"), sizeof(void*));
}

static int ticker_loop(void)
{
	do
	{
		int current = SDL_GetTicks();
		float dt = (current - g_candle->last_update) / 1000.0;
		entity_signal(entity_null, sig("world_update"), &dt);
		ecm_clean(0);
		g_candle->last_update = current;
		SDL_Delay(16);
	}
	while(!g_candle->exit);

	ecm_clean(1);

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

void candle_wait(void)
{
	/* SDL_WaitThread(candle->candle_thr, NULL); */
	SDL_WaitThread(g_candle->render_thr, NULL);
	SDL_WaitThread(g_candle->ticker_thr, NULL);
}

void candle_reg_cmd(const char *key, cmd_cb cb)
{
	int ret;
	uint hash = ref(key);
	khiter_t k = kh_put(cmd, g_candle->cmds, hash, &ret);
	cmd_t *cmd = &kh_value(g_candle->cmds, k);

	cmd->cb = cb;
	strncpy(cmd->key, key, sizeof(cmd->key) - 1);
}


entity_t candle_run_command(entity_t root, const char *command)
{
	if(command[0] == '\0') return root;
	char *copy = strdup(command);
	/* TODO: optimize this function */
	int i;

	entity_t instance = root;

	char separators[] = " ";

	char *p = strtok(copy, separators);
	char *argv[32];
	int argc = 0;

	argv[argc++] = strdup(p);

	while((p = strtok(NULL, separators)) != NULL)
	{
		argv[argc++] = strdup(p);
	}

	uint hash = ref(argv[0]);
	khiter_t k = kh_get(cmd, g_candle->cmds, hash);
	if(k != kh_end(g_candle->cmds))
	{
		cmd_t *cmd = &kh_value(g_candle->cmds, k);

		instance = cmd->cb(instance, argc, argv);

	}
	for(i = 0; i < argc; i++) free(argv[i]);
	free(copy);

	if(!entity_exists(instance)) instance = root;
	return instance;
}

int candle_run(entity_t root, const char *map_name)
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
		entity_t entity = candle_run_command(root, line);

		if(entity_exists(root) && c_node(&root) && entity != root)
		{
			c_node_add(c_node(&root), 1, entity);
		}
	}
	free(line);

	fclose(file);

	return 1;
}

void candle_release_mouse(entity_t ent, int reset)
{
	int i;
	for(i = 0; i < 16; i++)
	{
		if(g_candle->mouse_owners[i] == ent)
		{
			/* // SDL_SetWindowGrab(mainWindow, SDL_FALSE); */
			SDL_SetRelativeMouseMode(SDL_FALSE);
			if(reset)
			{
				SDL_WarpMouseInWindow(c_window(&SYS)->window, g_candle->mo_x,
						g_candle->mo_y);
			}
			for(; i < 15; i++)
			{
				g_candle->mouse_owners[i] = g_candle->mouse_owners[i + 1];
				g_candle->mouse_visible[i] = g_candle->mouse_visible[i + 1];

			}
		}
	}
	int vis = g_candle->mouse_visible[0];
	SDL_ShowCursor(vis); 
	SDL_SetRelativeMouseMode(!vis);
}

void candle_grab_mouse(entity_t ent, int visibility)
{
	int i;
	if(g_candle->mouse_owners[0] == ent) return;
	for(i = 15; i >= 1; i--)
	{
		g_candle->mouse_owners[i] = g_candle->mouse_owners[i - 1];
		g_candle->mouse_visible[i] = g_candle->mouse_visible[i - 1];
	}
	g_candle->mouse_owners[0] = ent;
	g_candle->mouse_visible[0] = visibility;
	g_candle->mo_x = g_candle->mx;
	g_candle->mo_y = g_candle->my;
	SDL_ShowCursor(visibility); 
	SDL_SetRelativeMouseMode(!visibility);
}

__attribute__((constructor (CONSTR_BEFORE_REG)))
void candle_init(void)
{
	g_candle = calloc(1, sizeof *g_candle);

	ecm_init();

	entity_new();

	g_candle->cmds = kh_init(cmd);

	g_candle->firstDir = SDL_GetBasePath();
	candle_reset_dir();

	shaders_reg();

	candle_register();

}

__attribute__((constructor (CONSTR_AFTER_REG)))
void candle_init2(void)
{
	if(g_candle->sem) return;

	g_candle->mouse_owners[0] = entity_null;
	g_candle->mouse_visible[0] = 1;

	entity_add_component(SYS, c_name_new("Candle"));
	entity_add_component(SYS, c_nodegraph_new());
	entity_add_component(SYS, c_mouse_new());
	entity_add_component(SYS, c_keyboard_new());
	entity_add_component(SYS, c_physics_new());
	entity_add_component(SYS, c_sauces_new());
	/* entity_add_component(SYS, c_model_new(NULL, NULL, 0, 0)); */

	//int res = pipe(candle->events);
	//if(res == -1) exit(1);

	/* candle->candle_thr = SDL_CreateThread((int(*)(void*))candle_loop, "candle_loop", candle); */
	g_candle->sem = SDL_CreateSemaphore(0);
	g_candle->render_thr = SDL_CreateThread((int(*)(void*))render_loop, "render_loop", NULL);
	g_candle->ticker_thr = SDL_CreateThread((int(*)(void*))ticker_loop, "ticker_loop", NULL);
	SDL_SemWait(g_candle->sem);
	/* SDL_Delay(500); */

	/* candle_import_dir(candle, entity_null, "./"); */
}

