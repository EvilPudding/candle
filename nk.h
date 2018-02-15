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

void nk_candle_render(enum nk_anti_aliasing AA, int max_vertex_buffer,
		int max_element_buffer);
void nk_draw_image_ext(struct nk_command_buffer *b, struct nk_rect r,
    const struct nk_image *img, struct nk_color col, int no_blending);

#endif /* !NK_H */
