#ifndef CANDLE_H
#define CANDLE_H

#include "utils/loader.h"
#include "systems/sauces.h"

#include "utils/texture.h"

#include "systems/keyboard.h"
#include "systems/controller.h"
#include "systems/mouse.h"
#include "utils/khash.h"
#include "events.h"

void candle_register(void);

typedef entity_t(*cmd_cb)(entity_t root, int argc, char **argv);

typedef struct
{
	char key[32];
	cmd_cb cb;
} cmd_t;

KHASH_MAP_INIT_INT(cmd, cmd_t)

typedef struct
{
	loader_t *loader;

	int events[2];

	char firstDir[256];

	int exit;
	double last_update;

	entity_t input_owners[16];

	khash_t(cmd) *cmds;

	void *render_thr;
	void *ticker_thr;
	int fps;
	void *mtx;
	int fps_count;
	double last_fps_tick;

} candle_t;

void candle_wait(void);
void candle_reset_dir(void);

bool_t candle_utility(int argc, char **argv);
void candle_reg_cmd(const char *key, cmd_cb cb);
int candle_import(entity_t root, const char *map_name);
int candle_run(entity_t root, const char *map_name);
int candle_run_from_memory(entity_t root, const char *bytes, size_t bytes_num);
entity_t candle_run_command(entity_t root,
		const char *command);

void candle_skip_frame(int frames);

extern candle_t *g_candle;

void candle_init(const char *path);
bool_t is_render_thread(void);

#endif /* !CANDLE_H */
