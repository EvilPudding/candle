#ifndef CREST_H
#define CREST_H

#include <glutil.h>

#include <loader.h>
#include <systems/sauces.h>

#include <texture.h>

#include <keyboard.h>
#include <mouse.h>
#include <khash.h>

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
	entity_t systems;
	loader_t *loader;

	int events[2];

	char *firstDir;

	int exit;
	int last_update;
	int pressing;
	int shift;

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
	SDL_threadID render_id;
	SDL_sem *sem;

} candle_t;

#define SYS ((entity_t){1})
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

extern candle_t *g_candle;

#endif /* !CREST_H */
