#ifndef NK_H
#define NK_H

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_INCLUDE_COMMAND_USERDATA
#include "nuklear/nuklear.h"
#include "nuklear/demo/sdl_opengl3/nuklear_sdl_gl3.h"

NK_API struct nk_context* nk_candle_init(SDL_Window *win);

void nk_candle_render(enum nk_anti_aliasing AA, int max_vertex_buffer,
		int max_element_buffer);
void nk_draw_image_ext(struct nk_command_buffer *b, struct nk_rect r,
    const struct nk_image *img, struct nk_color col, int no_blending);

#define nk_tree_entity_push(ctx, type, title, state, sel) nk_tree_entity_push_hashed(ctx, type, title, state, sel, NK_FILE_LINE,nk_strlen(NK_FILE_LINE),__LINE__)
#define nk_tree_entity_push_id(ctx, type, title, state, sel, chil, id) nk_tree_entity_push_hashed(ctx, type, title, state, sel, chil, NK_FILE_LINE,nk_strlen(NK_FILE_LINE),id)
NK_API int nk_tree_entity_push_hashed(struct nk_context*, enum nk_tree_type, const char *title, enum nk_collapse_states initial_state, int *selected, int has_children, const char *hash, int len, int seed);
NK_API int nk_tree_entity_image_push_hashed(struct nk_context*, enum nk_tree_type, struct nk_image, const char *title, enum nk_collapse_states initial_state, int *selected, int has_children, const char *hash, int len,int seed);
NK_API void nk_tree_entity_pop(struct nk_context*);

#endif /* !NK_H */
