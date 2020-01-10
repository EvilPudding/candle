#ifndef NK_H
#define NK_H

#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_INCLUDE_COMMAND_USERDATA
#include <stdarg.h>
#include "third_party/nuklear/nuklear.h"
#include <events.h>

NK_API struct nk_context* nk_can_init(void *win);

void nk_can_render(enum nk_anti_aliasing AA);
void nk_draw_image_ext(struct nk_command_buffer *b, struct nk_rect r,
    const struct nk_image *img, struct nk_color col, uint32_t no_blending,
    uint32_t tile, uint32_t width, uint32_t height);

#define nk_tree_entity_push(ctx, type, title, state, sel) nk_tree_entity_push_hashed(ctx, type, title, state, sel, NK_FILE_LINE,nk_strlen(NK_FILE_LINE),__LINE__)
#define nk_tree_entity_push_id(ctx, type, title, state, sel, chil, id) nk_tree_entity_push_hashed(ctx, type, title, state, sel, chil, NK_FILE_LINE,nk_strlen(NK_FILE_LINE),id)
NK_API int nk_tree_entity_push_hashed(struct nk_context*, enum nk_tree_type, const char *title, enum nk_collapse_states initial_state, int *selected, int has_children, const char *hash, int len, int seed);
NK_API int nk_tree_entity_image_push_hashed(struct nk_context*, enum nk_tree_type, struct nk_image, const char *title, enum nk_collapse_states initial_state, int *selected, int has_children, const char *hash, int len,int seed);
NK_API void nk_tree_entity_pop(struct nk_context*);
NK_API void nk_can_font_stash_begin(struct nk_font_atlas **atlas);
NK_API void nk_can_font_stash_end(void);
NK_API int nk_can_handle_event(struct nk_context *ctx, void *evt);
NK_API int nk_can_handle_key(struct nk_context *ctx, candle_key_e key, int down);
NK_API int nk_can_begin_titled(struct nk_context *ctx, const char *name,
		const char *title, struct nk_rect bounds, nk_flags flags);

#endif /* !NK_H */
