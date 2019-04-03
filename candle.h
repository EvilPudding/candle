#ifndef CREST_H
#define CREST_H

#include <utils/loader.h>
#include <systems/sauces.h>

#include <utils/texture.h>

#include <systems/keyboard.h>
#include <systems/mouse.h>
#include <utils/khash.h>

void candle_register(void);

typedef struct candle_t candle_t;
typedef entity_t(*cmd_cb)(entity_t root, int argc, char **argv);

typedef struct
{
	char key[32];
	cmd_cb cb;
} cmd_t;

KHASH_MAP_INIT_INT(cmd, cmd_t)

typedef struct candle_t
{
	loader_t *loader;

	int events[2];

	char *firstDir;

	int exit;
	int last_update;
	int pressing;

	/* TODO move this to mouse.h */
	int mx, my;
	entity_t mouse_owners[16];
	int mouse_visible[16];
	int mo_x, mo_y;
	/* ------------------------- */

	khash_t(cmd) *cmds;

	void *render_thr;
	void *candle_thr;
	void *ticker_thr;
	int fps;
#ifndef __EMSCRIPTEN__
	uint64_t render_id;
	void *sem;
#endif
	int fps_count;
	uint64_t last_tick;

} candle_t;

void candle_wait(void);
void candle_reset_dir(void);

void candle_reg_cmd(const char *key, cmd_cb cb);
int candle_import(entity_t root, const char *map_name);
int candle_run(entity_t root, const char *map_name);
entity_t candle_run_command(entity_t root,
		const char *command);

/* TODO send this to mouse.h */
void candle_grab_mouse(entity_t ent, int visibility);
void candle_release_mouse(entity_t ent, int reset);
void candle_skip_frame(int frames);

extern candle_t *g_candle;

#endif /* !CREST_H */
