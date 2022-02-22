#include "candle.h"
#include "utils/str.h"

#include <string.h>
#include <stdlib.h>
#include "systems/window.h"
#include "systems/render_device.h"
#include "systems/sauces.h"
#include "systems/nodegraph.h"
#include "components/name.h"
#include "components/node.h"
#include "utils/file.h"

#define GLFW_INCLUDE_NONE
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#define _GLFW_WIN32 
#else
#define _GLFW_X11
#endif

#include "third_party/glfw/include/GLFW/glfw3.h"

#ifdef _WIN32
#include "third_party/glfw/include/GLFW/glfw3native.h"
#endif

#ifndef _WIN32
#include <unistd.h>
#define candle_getcwd getcwd
#else
#include <direct.h>
#define candle_getcwd _getcwd
#endif

#ifdef THREADED
#include "third_party/tinycthread/source/tinycthread.h"
#endif

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
	entity_signal(entity_null, ref("events_begin"), NULL, NULL);
	glfwPollEvents();
	entity_signal(entity_null, ref("events_end"), NULL, NULL);
}

static int skip = 0;
void candle_skip_frame(int frames)
{
	skip += frames;
}

bool_t is_render_thread()
{
#if defined(THREADED)
	return thrd_equal(thrd_current(), *(thrd_t*)g_candle->render_thr);
#else
	return true;
#endif
}

static void cursor_position_callback(GLFWwindow *window, double xpos, double ypos)
{
	candle_event_t event;
	event.type = CANDLE_MOUSEMOTION;
	event.mouse.x = (int)xpos;
	event.mouse.y = (int)ypos;
	entity_signal(entity_null, ref("event_handle"), &event, NULL);
}

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
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
	entity_signal(entity_null, ref("event_handle"), &event, NULL);
}

void window_resize_callback(GLFWwindow *window, int width, int height)
{
	candle_event_t event;
	event.type = CANDLE_WINDOW_RESIZE;
	event.window.width = width;
	event.window.height = height;
	entity_signal(entity_null, ref("event_handle"), &event, NULL);
}

void joystick_callback(int jid, int event)
{
	if (event == GLFW_CONNECTED)
	{
		printf("joystick connected\n");
	}
	else if (event == GLFW_DISCONNECTED)
	{
		printf("joystick disconnected\n");
	}
}

void mouse_button_callback(GLFWwindow *window, int button, int action, int mods)
{
	candle_event_t event;
	if (action == GLFW_PRESS)
	{
		event.type = CANDLE_MOUSEBUTTONDOWN;
	}
	else if (action == GLFW_RELEASE)
	{
		event.type = CANDLE_MOUSEBUTTONUP;
	}

	if (button == GLFW_MOUSE_BUTTON_LEFT)
	{
		event.key = CANDLE_MOUSE_BUTTON_LEFT;
	}
	else if (button == GLFW_MOUSE_BUTTON_RIGHT)
	{
		event.key = CANDLE_MOUSE_BUTTON_RIGHT;
	}
	else if (button == GLFW_MOUSE_BUTTON_MIDDLE)
	{
		event.key = CANDLE_MOUSE_BUTTON_MIDDLE;
	}

	entity_signal(entity_null, ref("event_handle"), &event, NULL);
}

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset)
{
	candle_event_t event;
	event.type = CANDLE_MOUSEWHEEL;
	event.key = CANDLE_MOUSE_BUTTON_MIDDLE;
	event.wheel.x = xoffset;
	event.wheel.y = yoffset;
	entity_signal(entity_null, ref("event_handle"), &event, NULL);
}

static void render_loop_init(void)
{
	c_window_t *window;
	g_candle->last_fps_tick = glfwGetTime();
	g_candle->fps_count = 0;
	entity_add_component(SYS, c_window_new(0, 0));
	entity_add_component(SYS, c_render_device_new());

	window = c_window(&SYS);

	glfwSetMouseButtonCallback(window->window, mouse_button_callback);
	glfwSetScrollCallback(window->window, scroll_callback);
	glfwSetCursorPosCallback(window->window, cursor_position_callback);
	glfwSetKeyCallback(window->window, key_callback);
	glfwSetWindowSizeCallback(window->window, window_resize_callback);
	glfwSetJoystickCallback(joystick_callback);
}

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else

/* FAKE EMSCRIPTEN */
/* #define __EMSCRIPTEN__ */
/* void emscripten_set_main_loop(void(*cb)(void), int fps, bool_t somfin) */
/* { */
/* 	while(!g_candle->exit) */
/* 	{ */
/* 		cb(); */
/* 	} */
/* } */

#endif

#ifndef __EMSCRIPTEN__
bool_t first_frame = true;
#endif

static void render_loop_tick(void)
{
	double current;
#ifndef __EMSCRIPTEN__
	if (first_frame)
	{
		c_window_lock_fps(c_window(&SYS), 1);
		first_frame = false;
	}
#endif

#ifdef __EMSCRIPTEN__ /* HACK: emscripten does not update events on glfwPollEvents*/
	entity_signal(entity_null, ref("events_end"), NULL, NULL);
#else
	candle_handle_events();
#endif

	loader_update(g_candle->loader);

	{
		entity_signal(entity_null, ref("world_draw"), NULL, NULL);
		entity_signal(entity_null, ref("world_draw_end"), NULL, NULL);

		g_candle->fps_count++;
	}

	current = glfwGetTime();
	if(current - g_candle->last_fps_tick > 1.0)
	{
		g_candle->fps = g_candle->fps_count;
		g_candle->fps_count = 0;
		/* printf("%d\n", g_candle->fps); */
		g_candle->last_fps_tick = current;
	}

#ifdef __EMSCRIPTEN__ /* HACK: emscripten does not update events on glfwPollEvents*/
	entity_signal(entity_null, ref("events_begin"), NULL, NULL);
#endif

	if (glfwWindowShouldClose(c_window(&SYS)->window))
	{
		g_candle->exit = true;
	}
}

#if defined(THREADED) && !defined(__EMSCRIPTEN__)
static int render_loop(void *data)
{
	render_loop_init();
	mtx_unlock(g_candle->mtx);

	while(!g_candle->exit)
	{
		render_loop_tick();
		ecm_clean(0);
	}
	printf("Exiting render loop\n");

	loader_update(g_candle->loader);

	return 0;
}
#endif

static void ticker_loop_tick(bool_t delay)
{
	double current;
	float dt = 16.0f / 1000.0f;
	/* float dt = (current - g_candle->last_update) / 1000.0; */

	entity_signal(entity_null, ref("world_update"), &dt, NULL);

	current = glfwGetTime();
	if (delay && current - g_candle->last_update < 0.016)
	{
#ifdef THREADED
		struct timespec remaining = { 0, 0 };
		remaining.tv_nsec = (0.016 - (current - g_candle->last_update)) * 1000000000;
		thrd_sleep(&remaining, NULL);
		g_candle->last_update += 0.016;
#endif
	}
	else
	{
		g_candle->last_update = current;
	}
}

#ifdef THREADED
static int ticker_loop(void *data)
{
	do
	{
		ticker_loop_tick(true);
		ecm_clean(0);
	}
	while(!g_candle->exit);

	printf("Exiting ticker loop\n");

	return 0;
}
#endif

#ifdef __EMSCRIPTEN__
static void em_ticker_loop(void)
{
#ifndef THREADED
	ticker_loop_tick(false);
	ticker_loop_tick(false);
#endif
	render_loop_tick();
	ecm_clean(0);
}
#endif

void candle_wait(void)
{
#ifdef __EMSCRIPTEN__
	emscripten_set_main_loop(em_ticker_loop, 0, true);
#elif !defined(THREADED)
	do
	{
		ticker_loop_tick(false);
		render_loop_tick();
		ecm_clean(0);
	}
	while(!g_candle->exit);
#endif

#ifdef THREADED
#ifndef __EMSCRIPTEN__
	thrd_join(*(thrd_t*)g_candle->render_thr, NULL);
#endif
	thrd_join(*(thrd_t*)g_candle->ticker_thr, NULL);
#endif

	ecm_clean(1);
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

int candle_run_from_memory(entity_t root, const char *bytes, size_t bytes_num)
{
	char *line;
	sfile_t *file = sopen(bytes, bytes_num);

	if (file == NULL) return 0;

	while ((line = sgets(file)))
	{
		entity_t entity;
		entity = candle_run_command(root, line);
		if(entity_exists(root) && c_node(&root) && entity != root)
		{
			c_node_add(c_node(&root), 1, entity);
		}
	}
	sclose(file);
	return 1;
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

#if defined(_WIN32)
static bool_t get_archive(unsigned char **bytes, size_t *bytes_num)
{
	HRSRC hRes = FindResource(0, MAKEINTRESOURCE(1), RT_RCDATA);
	HGLOBAL hMem = LoadResource(0, hRes);
	DWORD size = SizeofResource(0, hRes);
	*bytes = (unsigned char*)LockResource(hMem);
	*bytes_num = size;
	return true;
}
#elif defined(__EMSCRIPTEN__)
#else
extern uint8_t _binary_candle_build_data_zip_start[];
extern uint8_t _binary_candle_build_data_zip_end[];
static bool_t get_archive(unsigned char **bytes, size_t *bytes_num)
{
	*bytes = (unsigned char*)_binary_candle_build_data_zip_start;
	*bytes_num = (size_t)(  _binary_candle_build_data_zip_end
	                      - _binary_candle_build_data_zip_start);
	return true;
}
#endif

void candle_init(const char *path)
{
	size_t sauces_size;
	unsigned char *sauces_bytes;
	g_candle = calloc(1, sizeof *g_candle);
	g_candle->loader = loader_new();

	loader_start(g_candle->loader);

	{
		char *slash;
		strcpy(g_candle->firstDir, path);
		slash = strrchr(g_candle->firstDir, '/');
		if (slash)
		{
			slash[0] = '\0';
			candle_reset_dir();
		}
		else
		{
			g_candle->firstDir[0] = '\0';
		}
	}

	materials_init_vil();

	ecm_init();

	entity_new({});

	g_candle->cmds = kh_init(cmd);
	candle_reg_cmd("c_new", (cmd_cb)_c_new);

	shaders_reg();

	draw_groups_init();

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
	if (get_archive(&sauces_bytes, &sauces_size))
	{
		c_sauces_me_a_zip(c_sauces(&SYS), sauces_bytes, sauces_size);
	}
#else
	c_sauces_index_dir(c_sauces(&SYS), "resauces");
#endif

#ifdef THREADED
	g_candle->mtx = malloc(sizeof(mtx_t));
	g_candle->render_thr = malloc(sizeof(thrd_t));
	g_candle->ticker_thr = malloc(sizeof(thrd_t));

	thrd_create(g_candle->ticker_thr, ticker_loop, NULL);


#ifndef __EMSCRIPTEN__
	mtx_init(g_candle->mtx, mtx_plain);
	mtx_lock(g_candle->mtx);
	thrd_create(g_candle->render_thr, render_loop, NULL);
	mtx_lock(g_candle->mtx);
#else
	*(thrd_t*)g_candle->render_thr = thrd_current();
	render_loop_init();
#endif


#else
	render_loop_init();
#endif
}
