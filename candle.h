#ifndef CREST_H
#define CREST_H

#include <glutil.h>

#include <loader.h>
#include <systems/sauces.h>

#include <texture.h>

#include <keyboard.h>
#include <mouse.h>

DEF_SIG(world_update);
DEF_SIG(world_draw);
DEF_SIG(event_handle);
DEF_SIG(events_begin);
DEF_SIG(events_end);

void candle_register(void);

typedef struct candle_t candle_t;
typedef entity_t(*template_cb)(entity_t root, FILE *fd, candle_t *candle);

typedef struct
{
	char key[32];
	template_cb cb;
} template_t;

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

	template_t *templates;
	uint templates_size;

	void *render_thr;
	void *candle_thr;
	void *ticker_thr;
	int fps;
	SDL_threadID render_id;
	SDL_sem *sem;

} candle_t;

void candle_wait(candle_t *self);
void candle_reset_dir(candle_t *self);

void candle_register_template(candle_t *self, const char *key, template_cb cb);
int candle_import(candle_t *self, entity_t root, const char *map_name);
int candle_import_dir(candle_t *self, entity_t root, const char *dir_name);

/* TODO send this to mouse.h */
void candle_grab_mouse(candle_t *self, entity_t ent, int visibility);
void candle_release_mouse(candle_t *self, entity_t ent, int reset);

extern candle_t *candle;

#endif /* !CREST_H */
