#include "candle.h"
#include <utils/file.h>
#include <utils/glutil.h>
#include <utils/str.h>

#include <string.h>
#include <stdlib.h>

#include <systems/window.h>
#include <systems/render_device.h>
#include <systems/sauces.h>
#include <systems/nodegraph.h>
#include <components/name.h>
#include <components/node.h>

#include <GLFW/glfw3.h>

#ifndef WIN32
#include <unistd.h>
#define candle_getcwd getcwd
#else
#include <direct.h>
#define candle_getcwd _getcwd
#endif

#include <tinycthread.h>

candle_t *g_candle;


entity_t candle_run_command_args(entity_t root, int argc, char **argv,
                                 bool_t *handled)
{
	entity_t instance = root;
	uint32_t hash = ref(argv[0]);
	khiter_t k = kh_get(cmd, g_candle->cmds, hash);
	if(k != kh_end(g_candle->cmds))
	{
		cmd_t *cmd = &kh_value(g_candle->cmds, k);

		instance = cmd->cb(instance, argc, argv);
		if (handled) *handled = true;
	}
	return instance;
}

bool_t candle_utility(int argc, char **argv)
{
	bool_t handled = false;
	argc--;
	argv++;
	if (argc > 0)
	{
		candle_run_command_args(entity_null, argc, argv, &handled);
	}
	return handled;
}

void candle_reset_dir()
{
#ifdef WIN32
    _chdir(g_candle->firstDir);
#else
	if(chdir(g_candle->firstDir)) printf("Chdir failed.\n");
#endif
}

static void candle_handle_events(void)
{
	entity_signal(entity_null, sig("events_begin"), NULL, NULL);
	glfwPollEvents();

	/* entity_signal(entity_null, sig("event_handle"), &event, NULL); */

	entity_signal(entity_null, sig("events_end"), NULL, NULL);
}

static int skip = 0;
void candle_skip_frame(int frames)
{
	skip += frames;
}

bool_t is_render_thread()
{
#ifndef __EMSCRIPTEN__
	return thrd_equal(thrd_current(), *(thrd_t*)g_candle->render_thr);
#else
	return true;
#endif
}

static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
{
	candle_event_t event;
	event.type = CANDLE_MOUSEMOTION;
	event.mouse.x = (int)xpos;
	event.mouse.y = (int)ypos;
	entity_signal(entity_null, sig("event_handle"), &event, NULL);
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	candle_event_t event;
	if (action == GLFW_PRESS)
	{
		event.type = CANDLE_KEYDOWN;
	}
	else if (action == GLFW_RELEASE)
	{
		event.type = CANDLE_KEYUP;
	}
	else
	{
		return;
	}

	if (mods & GLFW_MOD_SUPER)
	{
		return;
	}

	if (!(mods & GLFW_MOD_SHIFT) && key >= 'A' && key <= 'Z')
	{
		key += 'a' - 'A';
	}
	event.key = key;
	entity_signal(entity_null, sig("event_handle"), &event, NULL);
}

void window_resize_callback(GLFWwindow* window, int width, int height)
{
	candle_event_t event;
	event.type = CANDLE_WINDOW_RESIZE;
	event.window.width = width;
	event.window.height = height;
	entity_signal(entity_null, sig("event_handle"), &event, NULL);
}

void joystick_callback(int jid, int event)
{
	printf("joy event %d %d\n", jid, event);
	if (event == GLFW_CONNECTED)
	{
		printf("joystick connected\n");
	}
	else if (event == GLFW_DISCONNECTED)
	{
		printf("joystick disconnected\n");
	}
}

static void render_loop_init(void)
{
	c_window_t *window;
	g_candle->last_tick = glfwGetTime();
	g_candle->fps_count = 0;
	entity_add_component(SYS, c_window_new(0, 0));
	entity_add_component(SYS, c_render_device_new());

	window = c_window(&SYS);

	glfwSetCursorPosCallback(window->window, cursor_position_callback);
	glfwSetKeyCallback(window->window, key_callback);
	glfwSetWindowSizeCallback(window->window, window_resize_callback);
	glfwSetJoystickCallback(joystick_callback);
}

static void render_loop_tick(void)
{
	double current;
	candle_handle_events();

	loader_update(g_candle->loader);
	glerr();

	/* if(state->gameStarted) */
	{
		/* candle_handle_events(self); */
		/* printf("\t%ld\n", self->render_id); */
		entity_signal(entity_null, sig("world_draw"), NULL, NULL);

		ecm_clean(0);

		glerr();
		g_candle->fps_count++;
		/* candle_handle_events(self); */

	}

	current = glfwGetTime();
	if(current - g_candle->last_tick > 1.0)
	{
		g_candle->fps = g_candle->fps_count;
		g_candle->fps_count = 0;
		printf("%d\n", g_candle->fps);
		g_candle->last_tick = current;
	}

	if (glfwWindowShouldClose(c_window(&SYS)->window))
	{
		g_candle->exit = true;
	}

	{
		struct timespec remaining = { 0, 8000000 };
		thrd_sleep(&remaining, NULL);
	}
	glerr();
}

#ifndef __EMSCRIPTEN__
static int render_loop(void)
{
	render_loop_init();
	mtx_unlock(g_candle->mtx);

	while(!g_candle->exit)
	{
		render_loop_tick();
	}

	ecm_clean(1);
	loader_update(g_candle->loader);

	return 1;
}
#endif

void candle_register()
{
	signal_init(sig("world_update"), sizeof(float));
	signal_init(sig("world_draw"), sizeof(void*));
	signal_init(sig("event_handle"), sizeof(void*));
	signal_init(sig("events_end"), sizeof(void*));
	signal_init(sig("events_begin"), sizeof(void*));
	signal_init(sig("load"), sizeof(void*));
	signal_init(sig("expr_load"), sizeof(void*));
	signal_init(sig("expr_eval"), sizeof(void*));
	signal_init(sig("expr_var"), sizeof(void*));
	signal_init(sig("expr_del"), sizeof(void*));

	draw_groups_init();
}

static void ticker_loop_tick(void)
{
	double current = glfwGetTime();
	float dt = 16.0f / 1000.0f;
	/* float dt = (current - g_candle->last_update) / 1000.0; */

	entity_signal(entity_null, sig("world_update"), &dt, NULL);

	ecm_clean(0);
	g_candle->last_update = current;
#ifndef __EMSCRIPTEN__
	{
		struct timespec remaining = { 0, 16000000 };
		thrd_sleep(&remaining, NULL);
	}
#endif
}
static void ticker_loop(void)
{
#ifndef __EMSCRIPTEN__
	do
	{
#endif
		ticker_loop_tick();
#ifdef __EMSCRIPTEN__
		render_loop_tick();
#else
	}
	while(!g_candle->exit);

	ecm_clean(1);
#endif
}

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
void candle_wait(void)
{
#ifdef __EMSCRIPTEN__
	emscripten_set_main_loop(ticker_loop, 0, true);
#else
	thrd_join(*(thrd_t*)g_candle->render_thr, NULL);
	thrd_join(*(thrd_t*)g_candle->ticker_thr, NULL);
#endif
}

void candle_reg_cmd(const char *key, cmd_cb cb)
{
	int ret;
	uint32_t hash = ref(key);
	khiter_t k = kh_put(cmd, g_candle->cmds, hash, &ret);
	cmd_t *cmd = &kh_value(g_candle->cmds, k);

	cmd->cb = cb;
	strncpy(cmd->key, key, sizeof(cmd->key) - 1);
}

entity_t candle_run_command(entity_t root, const char *command)
{
	entity_t instance;
	char *copy;
	char separators[] = " ";
	char *p;
	char *argv[64];
	int argc;

	if(command[0] == '\0') return root;

	copy = malloc(strlen(command) + 1);
	strcpy(copy, command);

	instance = root;

	p = strtok(copy, separators);
	argc = 0;

	argv[argc++] = p;

	while((p = strtok(NULL, separators)) != NULL)
	{
		argv[argc++] = p;
	}

	instance = candle_run_command_args(root, argc, argv, NULL);

	free(copy);
	if(!entity_exists(instance)) instance = root;
	return instance;
}

int candle_run(entity_t root, const char *map_name)
{
	char *line;
	FILE *file = fopen(map_name, "r");

	if (file == NULL) return 0;

	line = NULL;
	while (true)
	{
		entity_t entity;
		line = str_readline(file);
		if (str_len(line) == 0)
		{
			str_free(line);
			break;
		}

		entity = candle_run_command(root, line);

		if(entity_exists(root) && c_node(&root) && entity != root)
		{
			c_node_add(c_node(&root), 1, entity);
		}
		str_free(line);
	}

	fclose(file);
	return 1;
}

static
entity_t _c_new(entity_t root, int argc, char **argv)
{
	printf("CREATING NEW COMPONENT %s\n", argv[1]);
	return entity_null;
}

void candle_init2(void);
void candle_init(const char *path)
{
	g_candle = calloc(1, sizeof *g_candle);
	g_candle->loader = loader_new();

	loader_start(g_candle->loader);

	{
		strcpy(g_candle->firstDir, path);
		strrchr(g_candle->firstDir, '/')[0] = '\0';
		printf("%s\n", g_candle->firstDir);
		candle_reset_dir();
	}

	materials_init_vil();

	ecm_init();

	mat_new("default_material", "default");
	entity_new({});

	g_candle->cmds = kh_init(cmd);
	candle_reg_cmd("c_new", (cmd_cb)_c_new);

	shaders_reg();

	candle_register();

	candle_init2();

}

void candle_init2()
{
	if(c_name(&SYS)) return;

	entity_add_component(SYS, c_name_new("Candle"));
	entity_add_component(SYS, c_nodegraph_new());
	entity_add_component(SYS, c_mouse_new());
	entity_add_component(SYS, c_keyboard_new());
#ifndef __EMSCRIPTEN__
	entity_add_component(SYS, c_controllers_new());
#endif
	entity_add_component(SYS, c_sauces_new());
	entity_add_component(SYS, c_node_new());

	textures_reg();
	meshes_reg();
	materials_reg();

#ifndef __EMSCRIPTEN__

	g_candle->mtx = malloc(sizeof(mtx_t));
	mtx_init(g_candle->mtx, mtx_plain);
	mtx_lock(g_candle->mtx);

	g_candle->render_thr = malloc(sizeof(thrd_t));
	g_candle->ticker_thr = malloc(sizeof(thrd_t));

	thrd_create(g_candle->render_thr, (thrd_start_t)render_loop, NULL);
	thrd_create(g_candle->ticker_thr, (thrd_start_t)ticker_loop, NULL);
	mtx_lock(g_candle->mtx);
#else
	render_loop_init();
#endif

	/* candle_import_dir(candle, entity_null, "./"); */
}
