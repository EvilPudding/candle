#include <string.h>
#include <stdlib.h>
#include "../utils/glutil.h"
#include "../utils/texture.h"
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#define NK_INCLUDE_COMMAND_USERDATA
#include <stdarg.h>
#include "../third_party/nuklear/nuklear.h"

#ifdef __WIN32
#  ifndef _GLFW_WIN32 
#    define _GLFW_WIN32 
#  endif
#else
#  ifndef _GLFW_WIN32 
#    define _GLFW_X11
#  endif
#endif
#include "../third_party/glfw/include/GLFW/glfw3.h"
#include "../events.h"
#include "../systems/keyboard.h"

#define CAN_COLOR_MAP(NK_COLOR)\
    NK_COLOR(NK_COLOR_TEXT,                     175,175,175,255) \
    NK_COLOR(NK_COLOR_WINDOW,                   50, 50, 50, 170) \
    NK_COLOR(NK_COLOR_HEADER,                   0, 0, 0, 100) \
    NK_COLOR(NK_COLOR_BORDER,                   65, 65, 65, 0) \
    NK_COLOR(NK_COLOR_BUTTON,                   50, 50, 50, 50) \
    NK_COLOR(NK_COLOR_BUTTON_HOVER,             40, 40, 40, 200) \
    NK_COLOR(NK_COLOR_BUTTON_ACTIVE,            35, 35, 35, 255) \
    NK_COLOR(NK_COLOR_TOGGLE,                   100,100,100,255) \
    NK_COLOR(NK_COLOR_TOGGLE_HOVER,             120,120,120,255) \
    NK_COLOR(NK_COLOR_TOGGLE_CURSOR,            45, 45, 45, 255) \
    NK_COLOR(NK_COLOR_SELECT,                   45, 45, 45, 100) \
    NK_COLOR(NK_COLOR_SELECT_ACTIVE,            35, 35, 35,140) \
    NK_COLOR(NK_COLOR_SLIDER,                   38, 38, 38, 100) \
    NK_COLOR(NK_COLOR_SLIDER_CURSOR,            100,100,100,255) \
    NK_COLOR(NK_COLOR_SLIDER_CURSOR_HOVER,      120,120,120,255) \
    NK_COLOR(NK_COLOR_SLIDER_CURSOR_ACTIVE,     150,150,150,255) \
    NK_COLOR(NK_COLOR_PROPERTY,                 38, 38, 38, 100) \
    NK_COLOR(NK_COLOR_EDIT,                     38, 38, 38, 255)  \
    NK_COLOR(NK_COLOR_EDIT_CURSOR,              175,175,175,255) \
    NK_COLOR(NK_COLOR_COMBO,                    45, 45, 45, 255) \
    NK_COLOR(NK_COLOR_CHART,                    120,120,120,255) \
    NK_COLOR(NK_COLOR_CHART_COLOR,              45, 45, 45, 255) \
    NK_COLOR(NK_COLOR_CHART_COLOR_HIGHLIGHT,    255, 0,  0, 255) \
    NK_COLOR(NK_COLOR_SCROLLBAR,                40, 40, 40, 255) \
    NK_COLOR(NK_COLOR_SCROLLBAR_CURSOR,         100,100,100,255) \
    NK_COLOR(NK_COLOR_SCROLLBAR_CURSOR_HOVER,   120,120,120,255) \
    NK_COLOR(NK_COLOR_SCROLLBAR_CURSOR_ACTIVE,  150,150,150,255) \
    NK_COLOR(NK_COLOR_TAB_HEADER,               0, 0, 0, 100)

NK_GLOBAL const struct nk_color
can_default_color_style[NK_COLOR_COUNT] = {
#define NK_COLOR(a,b,c,d,e) {b,c,d,e},
    CAN_COLOR_MAP(NK_COLOR)
#undef NK_COLOR
};

#define NK_SHADER_VERSION "#version 300 es\n"

struct nk_can_device {
    struct nk_buffer cmds;
    struct nk_draw_null_texture null;
    GLuint vbo, vao, ebo;
    GLuint prog;
    GLuint vert_shdr;
    GLuint frag_shdr;
    GLint attrib_pos;
    GLint attrib_uv;
    GLint attrib_col;
    GLint uniform_tex;
    GLint uniform_indir;
    GLint uniform_scale;
    GLint uniform_size;
    GLint uniform_tile;
    GLint uniform_proj;
    GLuint font_tex;
};

struct nk_can_vertex {
    float position[2];
    float uv[2];
    nk_byte col[4];
};

struct img_info
{
	uint32_t blending;
	uint32_t tile;
	uvec2_t size;
};

static struct nk_can {
    GLFWwindow *win;
    struct nk_can_device ogl;
    struct nk_context ctx;
    struct nk_font_atlas atlas;
} can;

void nk_draw_image_ext(struct nk_command_buffer *b, struct nk_rect r,
    const struct nk_image *img, struct nk_color col, uint32_t no_blending,
    uint32_t tile, uint32_t width, uint32_t height)
{
    struct nk_command_image *cmd;
	struct img_info *info;
    NK_ASSERT(b);
    if (!b) return;
    if (b->use_clipping) {
        const struct nk_rect *c = &b->clip;
        if (c->w == 0 || c->h == 0 || !NK_INTERSECT(r.x, r.y, r.w, r.h, c->x, c->y, c->w, c->h))
            return;
    }

    cmd = (struct nk_command_image*)
        nk_command_buffer_push(b, NK_COMMAND_IMAGE, sizeof(*cmd));
    if (!cmd) return;
    cmd->x = (short)r.x;
    cmd->y = (short)r.y;
    cmd->w = (unsigned short)NK_MAX(0, r.w);
    cmd->h = (unsigned short)NK_MAX(0, r.h);
    cmd->img = *img;
    cmd->col = col;
	info = malloc(sizeof(struct img_info));
    cmd->header.userdata.ptr = info;
	info->blending = !no_blending;
	info->tile = tile;
	info->size = uvec2(width, height);
}

NK_LIB int
nk_can_panel_begin(struct nk_context *ctx, const char *title, enum nk_panel_type panel_type)
{
    struct nk_input *in;
    struct nk_window *win;
    struct nk_panel *layout;
    struct nk_command_buffer *out;
    const struct nk_style *style;
    const struct nk_user_font *font;

    struct nk_vec2 scrollbar_size;
    struct nk_vec2 panel_padding;

    NK_ASSERT(ctx);
    NK_ASSERT(ctx->current);
    NK_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout) return 0;
    nk_zero(ctx->current->layout, sizeof(*ctx->current->layout));
    if ((ctx->current->flags & NK_WINDOW_HIDDEN) || (ctx->current->flags & NK_WINDOW_CLOSED)) {
        nk_zero(ctx->current->layout, sizeof(struct nk_panel));
        ctx->current->layout->type = panel_type;
        return 0;
    }
    /* pull state into local stack */
    style = &ctx->style;
    font = style->font;
    win = ctx->current;
    layout = win->layout;
    out = &win->buffer;
    in = (win->flags & NK_WINDOW_NO_INPUT) ? 0: &ctx->input;
#ifdef NK_INCLUDE_COMMAND_USERDATA
    win->buffer.userdata = ctx->userdata;
#endif
    /* pull style configuration into local stack */
    scrollbar_size = style->window.scrollbar_size;
    panel_padding = nk_panel_get_padding(style, panel_type);

    /* window movement */
    if ((win->flags & NK_WINDOW_MOVABLE) && !(win->flags & NK_WINDOW_ROM)) {
        int left_mouse_down;
        int left_mouse_clicked;
        int left_mouse_click_in_cursor;

        /* calculate draggable window space */
        struct nk_rect header;
        header.x = win->bounds.x;
        header.y = win->bounds.y;
        header.w = win->bounds.w;
        if (nk_panel_has_header(win->flags, title)) {
            header.h = font->height + 2.0f * style->window.header.padding.y;
            header.h += 2.0f * style->window.header.label_padding.y;
        } else header.h = panel_padding.y;

        /* window movement by dragging */
        left_mouse_down = in->mouse.buttons[NK_BUTTON_LEFT].down;
        left_mouse_clicked = (int)in->mouse.buttons[NK_BUTTON_LEFT].clicked;
        left_mouse_click_in_cursor = nk_input_has_mouse_click_down_in_rect(in,
            NK_BUTTON_LEFT, header, nk_true);
        if (left_mouse_down && left_mouse_click_in_cursor && !left_mouse_clicked) {
            win->bounds.x = win->bounds.x + in->mouse.delta.x;
            win->bounds.y = win->bounds.y + in->mouse.delta.y;
            in->mouse.buttons[NK_BUTTON_LEFT].clicked_pos.x += in->mouse.delta.x;
            in->mouse.buttons[NK_BUTTON_LEFT].clicked_pos.y += in->mouse.delta.y;
            ctx->style.cursor_active = ctx->style.cursors[NK_CURSOR_MOVE];
        }
    }

    /* setup panel */
    layout->type = panel_type;
    layout->flags = win->flags;
    layout->bounds = win->bounds;
    layout->bounds.x += panel_padding.x;
    layout->bounds.w -= 2*panel_padding.x;
    if (win->flags & NK_WINDOW_BORDER) {
        layout->border = nk_panel_get_border(style, win->flags, panel_type);
        layout->bounds = nk_shrink_rect(layout->bounds, layout->border);
    } else layout->border = 0;
    layout->at_y = layout->bounds.y;
    layout->at_x = layout->bounds.x;
    layout->max_x = 0;
    layout->header_height = 0;
    layout->footer_height = 0;
    nk_layout_reset_min_row_height(ctx);
    layout->row.index = 0;
    layout->row.columns = 0;
    layout->row.ratio = 0;
    layout->row.item_width = 0;
    layout->row.tree_depth = 0;
    layout->row.height = panel_padding.y;
    layout->has_scrolling = nk_true;
    if (!(win->flags & NK_WINDOW_NO_SCROLLBAR))
        layout->bounds.w -= scrollbar_size.x;
    if (!nk_panel_is_nonblock(panel_type)) {
        layout->footer_height = 0;
        if (!(win->flags & NK_WINDOW_NO_SCROLLBAR) || win->flags & NK_WINDOW_SCALABLE)
            layout->footer_height = scrollbar_size.y;
        layout->bounds.h -= layout->footer_height;
    }

    /* draw window background */
    if (!(layout->flags & NK_WINDOW_MINIMIZED) && !(layout->flags & NK_WINDOW_DYNAMIC)) {
        struct nk_rect body;
        body.x = win->bounds.x;
        body.w = win->bounds.w;
        body.y = (win->bounds.y + layout->header_height);
        body.h = (win->bounds.h - layout->header_height);
        if (style->window.fixed_background.type == NK_STYLE_ITEM_IMAGE)
		{
			const struct nk_image *image;

			nk_push_scissor(out, win->bounds);
			image = &style->window.fixed_background.data.image;
			nk_draw_image_ext(out, nk_rect(0, 0, image->w, image->h),
			                  image, nk_white, 1, 0, 0, 0);
		}
        nk_fill_rect(out, body, 0, style->window.background);
    }

    /* panel header */
    if (nk_panel_has_header(win->flags, title))
    {
        struct nk_text text;
        struct nk_rect header;
        const struct nk_style_item *background = 0;

        /* calculate header bounds */
        header.x = win->bounds.x;
        header.y = win->bounds.y;
        header.w = win->bounds.w;
        header.h = font->height + 2.0f * style->window.header.padding.y;
        header.h += (2.0f * style->window.header.label_padding.y);

        /* shrink panel by header */
        layout->header_height = header.h;
        layout->bounds.y += header.h;
        layout->bounds.h -= header.h;
        layout->at_y += header.h;

        /* select correct header background and text color */
        if (ctx->active == win) {
            background = &style->window.header.active;
            text.text = style->window.header.label_active;
        } else if (nk_input_is_mouse_hovering_rect(&ctx->input, header)) {
            background = &style->window.header.hover;
            text.text = style->window.header.label_hover;
        } else {
            background = &style->window.header.normal;
            text.text = style->window.header.label_normal;
        }

        /* draw header background */
        header.h += 1.0f;
        if (background->type == NK_STYLE_ITEM_IMAGE) {
            text.background = nk_rgba(0,0,0,0);
            nk_draw_image(&win->buffer, header, &background->data.image, nk_white);
        } else {
            text.background = background->data.color;
            nk_fill_rect(out, header, 0, background->data.color);
        }

        /* window close button */
        {struct nk_rect button;
        button.y = header.y + style->window.header.padding.y;
        button.h = header.h - 2 * style->window.header.padding.y;
        button.w = button.h;
        if (win->flags & NK_WINDOW_CLOSABLE) {
            nk_flags ws = 0;
            if (style->window.header.align == NK_HEADER_RIGHT) {
                button.x = (header.w + header.x) - (button.w + style->window.header.padding.x);
                header.w -= button.w + style->window.header.spacing.x + style->window.header.padding.x;
            } else {
                button.x = header.x + style->window.header.padding.x;
                header.x += button.w + style->window.header.spacing.x + style->window.header.padding.x;
            }

            if (nk_do_button_symbol(&ws, &win->buffer, button,
                style->window.header.close_symbol, NK_BUTTON_DEFAULT,
                &style->window.header.close_button, in, style->font) && !(win->flags & NK_WINDOW_ROM))
            {
                layout->flags |= NK_WINDOW_HIDDEN;
                layout->flags &= (nk_flags)~NK_WINDOW_MINIMIZED;
            }
        }

        /* window minimize button */
        if (win->flags & NK_WINDOW_MINIMIZABLE) {
            nk_flags ws = 0;
            if (style->window.header.align == NK_HEADER_RIGHT) {
                button.x = (header.w + header.x) - button.w;
                if (!(win->flags & NK_WINDOW_CLOSABLE)) {
                    button.x -= style->window.header.padding.x;
                    header.w -= style->window.header.padding.x;
                }
                header.w -= button.w + style->window.header.spacing.x;
            } else {
                button.x = header.x;
                header.x += button.w + style->window.header.spacing.x + style->window.header.padding.x;
            }
            if (nk_do_button_symbol(&ws, &win->buffer, button, (layout->flags & NK_WINDOW_MINIMIZED)?
                style->window.header.maximize_symbol: style->window.header.minimize_symbol,
                NK_BUTTON_DEFAULT, &style->window.header.minimize_button, in, style->font) && !(win->flags & NK_WINDOW_ROM))
                layout->flags = (layout->flags & NK_WINDOW_MINIMIZED) ?
                    layout->flags & (nk_flags)~NK_WINDOW_MINIMIZED:
                    layout->flags | NK_WINDOW_MINIMIZED;
        }}

        {/* window header title */
        int text_len = nk_strlen(title);
        struct nk_rect label = {0,0,0,0};
        float t = font->width(font->userdata, font->height, title, text_len);
        text.padding = nk_vec2(0,0);

        label.x = header.x + style->window.header.padding.x;
        label.x += style->window.header.label_padding.x;
        label.y = header.y + style->window.header.label_padding.y;
        label.h = font->height + 2 * style->window.header.label_padding.y;
        label.w = t + 2 * style->window.header.spacing.x;
        label.w = NK_CLAMP(0, label.w, header.x + header.w - label.x);
        nk_widget_text(out, label,(const char*)title, text_len, &text, NK_TEXT_LEFT, font);}
    }

    /* set clipping rectangle */
    {struct nk_rect clip;
    layout->clip = layout->bounds;
    nk_unify(&clip, &win->buffer.clip, layout->clip.x, layout->clip.y,
        layout->clip.x + layout->clip.w, layout->clip.y + layout->clip.h);
    nk_push_scissor(out, clip);
    layout->clip = clip;}
    return !(layout->flags & NK_WINDOW_HIDDEN) && !(layout->flags & NK_WINDOW_MINIMIZED);
}

NK_API int
nk_can_begin_titled(struct nk_context *ctx, const char *name, const char *title,
    struct nk_rect bounds, nk_flags flags)
{
    struct nk_window *win;
    struct nk_style *style;
    nk_hash name_hash;
    int name_len;
    int ret = 0;

    NK_ASSERT(ctx);
    NK_ASSERT(name);
    NK_ASSERT(title);
    NK_ASSERT(ctx->style.font && ctx->style.font->width && "if this triggers you forgot to add a font");
    NK_ASSERT(!ctx->current && "if this triggers you missed a `nk_end` call");
    if (!ctx || ctx->current || !title || !name)
        return 0;

    /* find or create window */
    style = &ctx->style;
    name_len = (int)nk_strlen(name);
    name_hash = nk_murmur_hash(name, (int)name_len, NK_WINDOW_TITLE);
    win = nk_find_window(ctx, name_hash, name);
    if (!win) {
        /* create new window */
        nk_size name_length = (nk_size)name_len;
        win = (struct nk_window*)nk_create_window(ctx);
        NK_ASSERT(win);
        if (!win) return 0;

        if (flags & NK_WINDOW_BACKGROUND)
            nk_insert_window(ctx, win, NK_INSERT_FRONT);
        else nk_insert_window(ctx, win, NK_INSERT_BACK);
        nk_command_buffer_init(&win->buffer, &ctx->memory, NK_CLIPPING_ON);

        win->flags = flags;
        win->bounds = bounds;
        win->name = name_hash;
        name_length = NK_MIN(name_length, NK_WINDOW_MAX_NAME-1);
        NK_MEMCPY(win->name_string, name, name_length);
        win->name_string[name_length] = 0;
        win->popup.win = 0;
        if (!ctx->active)
            ctx->active = win;
    } else {
        /* update window */
        win->flags &= ~(nk_flags)(NK_WINDOW_PRIVATE-1);
        win->flags |= flags;
        if (!(win->flags & (NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE)))
            win->bounds = bounds;
        /* If this assert triggers you either:
         *
         * I.) Have more than one window with the same name or
         * II.) You forgot to actually draw the window.
         *      More specific you did not call `nk_clear` (nk_clear will be
         *      automatically called for you if you are using one of the
         *      provided demo backends). */
        NK_ASSERT(win->seq != ctx->seq);
        win->seq = ctx->seq;
        if (!ctx->active && !(win->flags & NK_WINDOW_HIDDEN)) {
            ctx->active = win;
            ctx->end = win;
        }
    }
    if (win->flags & NK_WINDOW_HIDDEN) {
        ctx->current = win;
        win->layout = 0;
        return 0;
    } else nk_start(ctx, win);

    /* window overlapping */
    if (!(win->flags & NK_WINDOW_HIDDEN) && !(win->flags & NK_WINDOW_NO_INPUT))
    {
        int inpanel, ishovered;
        struct nk_window *iter = win;
        float h = ctx->style.font->height + 2.0f * style->window.header.padding.y +
            (2.0f * style->window.header.label_padding.y);
        struct nk_rect win_bounds = (!(win->flags & NK_WINDOW_MINIMIZED))?
            win->bounds: nk_rect(win->bounds.x, win->bounds.y, win->bounds.w, h);

        /* activate window if hovered and no other window is overlapping this window */
        inpanel = nk_input_has_mouse_click_down_in_rect(&ctx->input, NK_BUTTON_LEFT, win_bounds, nk_true);
        inpanel = inpanel && ctx->input.mouse.buttons[NK_BUTTON_LEFT].clicked;
        ishovered = nk_input_is_mouse_hovering_rect(&ctx->input, win_bounds);
        if ((win != ctx->active) && ishovered && !ctx->input.mouse.buttons[NK_BUTTON_LEFT].down) {
            iter = win->next;
            while (iter) {
                struct nk_rect iter_bounds = (!(iter->flags & NK_WINDOW_MINIMIZED))?
                    iter->bounds: nk_rect(iter->bounds.x, iter->bounds.y, iter->bounds.w, h);
                if (NK_INTERSECT(win_bounds.x, win_bounds.y, win_bounds.w, win_bounds.h,
                    iter_bounds.x, iter_bounds.y, iter_bounds.w, iter_bounds.h) &&
                    (!(iter->flags & NK_WINDOW_HIDDEN)))
                    break;

                if (iter->popup.win && iter->popup.active && !(iter->flags & NK_WINDOW_HIDDEN) &&
                    NK_INTERSECT(win->bounds.x, win_bounds.y, win_bounds.w, win_bounds.h,
                    iter->popup.win->bounds.x, iter->popup.win->bounds.y,
                    iter->popup.win->bounds.w, iter->popup.win->bounds.h))
                    break;
                iter = iter->next;
            }
        }

        /* activate window if clicked */
        if (iter && inpanel && (win != ctx->end)) {
            iter = win->next;
            while (iter) {
                /* try to find a panel with higher priority in the same position */
                struct nk_rect iter_bounds = (!(iter->flags & NK_WINDOW_MINIMIZED))?
                iter->bounds: nk_rect(iter->bounds.x, iter->bounds.y, iter->bounds.w, h);
                if (NK_INBOX(ctx->input.mouse.pos.x, ctx->input.mouse.pos.y,
                    iter_bounds.x, iter_bounds.y, iter_bounds.w, iter_bounds.h) &&
                    !(iter->flags & NK_WINDOW_HIDDEN))
                    break;
                if (iter->popup.win && iter->popup.active && !(iter->flags & NK_WINDOW_HIDDEN) &&
                    NK_INTERSECT(win_bounds.x, win_bounds.y, win_bounds.w, win_bounds.h,
                    iter->popup.win->bounds.x, iter->popup.win->bounds.y,
                    iter->popup.win->bounds.w, iter->popup.win->bounds.h))
                    break;
                iter = iter->next;
            }
        }
        if (iter && !(win->flags & NK_WINDOW_ROM) && (win->flags & NK_WINDOW_BACKGROUND)) {
            win->flags |= (nk_flags)NK_WINDOW_ROM;
            iter->flags &= ~(nk_flags)NK_WINDOW_ROM;
            ctx->active = iter;
            if (!(iter->flags & NK_WINDOW_BACKGROUND)) {
                /* current window is active in that position so transfer to top
                 * at the highest priority in stack */
                nk_remove_window(ctx, iter);
                nk_insert_window(ctx, iter, NK_INSERT_BACK);
            }
        } else {
            if (!iter && ctx->end != win) {
                if (!(win->flags & NK_WINDOW_BACKGROUND)) {
                    /* current window is active in that position so transfer to top
                     * at the highest priority in stack */
                    nk_remove_window(ctx, win);
                    nk_insert_window(ctx, win, NK_INSERT_BACK);
                }
                win->flags &= ~(nk_flags)NK_WINDOW_ROM;
                ctx->active = win;
            }
            if (ctx->end != win && !(win->flags & NK_WINDOW_BACKGROUND))
                win->flags |= NK_WINDOW_ROM;
        }
    }
    win->layout = (struct nk_panel*)nk_create_panel(ctx);
    ctx->current = win;
    ret = nk_can_panel_begin(ctx, title, NK_PANEL_WINDOW);
    win->layout->offset_x = &win->scrollbar.x;
    win->layout->offset_y = &win->scrollbar.y;
    return ret;
}


#define MAX_VERTEX_BUFFER (512 * 1024)
#define MAX_ELEMENT_BUFFER (128 * 1024)
static void *g_vertices;
static void *g_elements;
static void checkShaderError(GLuint shader,
		const char *name, const char *code)
{
	GLint success = 0;
	GLint bufflen;

	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &bufflen);
	if (bufflen > 1)
	{
		GLchar *log_string = malloc(bufflen + 1);
		glGetShaderInfoLog(shader, bufflen, 0, log_string);
		printf("%s\n", code);
		printf("Log found for '%s':\n%s", name, log_string);
		free(log_string);
	}

	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (success != GL_TRUE)
	{ 
		printf("%s\n", name);
		exit(1);
	}
}

NK_API void
nk_can_device_create(void)
{
    struct nk_can_device *dev = &can.ogl;
    GLint status;

    static const GLchar *vertex_shader =
        NK_SHADER_VERSION
        "uniform mat4 ProjMtx;\n"
        "in vec2 Position;\n"
        "in vec2 TexCoord;\n"
        "in vec4 Color;\n"
        "out vec2 Frag_UV;\n"
        "out vec4 Frag_Color;\n"
        "void main() {\n"
        "   Frag_UV = TexCoord;\n"
        "   Frag_Color = Color;\n"
        "   gl_Position = ProjMtx * vec4(Position.xy, 0, 1);\n"
        "}\n";
    static GLchar fs[2500] = "";
    static const GLchar *fragment_shader = fs;
	strcat(fs,
        NK_SHADER_VERSION
        "precision mediump float;\n"
        "uniform sampler2D Texture;\n"
        "uniform sampler2D Indir;\n"
		"#define g_indir_w 128u\n"
		"#define g_indir_h 128u\n"
		"#define g_cache_w 64u\n"
		"#define g_cache_h 64u\n"
		"float mip_map_scalar(in vec2 texture_coordinate)\n"
		"{\n"
		"    vec2 dx_vtc = dFdx(texture_coordinate);\n"
		"    vec2 dy_vtc = dFdy(texture_coordinate);\n"
		"    return max(dot(dx_vtc, dx_vtc), dot(dy_vtc, dy_vtc));\n"
		"}\n"
		"#define MAX_MIPS 9u\n"
		"float mip_map_level(in vec2 texture_coordinate) // in texel units\n"
		"{\n"
	);
	strcat(fs,
		"	return clamp(0.5 * log2(mip_map_scalar(texture_coordinate)), 0.0, float(MAX_MIPS - 1u));\n"
		"}\n"
		"vec4 solveMip(uvec2 size, uint tile, uint mip, vec2 coords)\n"
		"{\n"
		"	uint tiles_per_row = uint(ceil(float(size.x) / 128.0));\n"
		"	uint tiles_per_col = uint(ceil(float(size.y) / 128.0));\n"
		"	uint offset = tile;\n"
		"	for (uint i = 0u; i < MAX_MIPS && i < mip; i++)\n"
		"	{\n"
		"		offset += tiles_per_row * tiles_per_col;\n"
		"		tiles_per_row = uint(ceil(0.5 * float(tiles_per_row)));\n"
		"		tiles_per_col = uint(ceil(0.5 * float(tiles_per_col)));\n"
		"	}\n"
	);
	strcat(fs,
		"	uvec2 indir_coords = uvec2(floor(coords / (pow(2.0, float(mip)) * 128.0)));\n"
		"	uint tex_tile = indir_coords.y * tiles_per_row + indir_coords.x + offset;\n"
		"	vec3 info = texelFetch(Indir, ivec2(tex_tile % g_indir_w, tex_tile / g_indir_w), 0).rgb * 255.0;\n"
		"	uint cache_tile = uint(info.r) + uint(info.g * 256.0);\n"
		"	float actual_mip = info.b;\n"
		"	uvec2 cache_coords = uvec2(cache_tile % g_cache_w, cache_tile / g_cache_w) * 129u;\n"
		"	const vec2 g_cache_size = vec2(g_cache_w * 129u, g_cache_h * 129u);\n"
	);
	strcat(fs,
		"	vec2 actual_coords = coords / pow(2.0, actual_mip);\n"
		"	vec2 intile_coords = mod(actual_coords, 128.0) + 0.5f;\n"
		"	return textureLod(Texture,\n"
		"			(vec2(cache_coords) + intile_coords) / g_cache_size, 0.0);\n"
		"}\n"
		"vec4 textureSVT(uvec2 size, uint tile, vec2 coords)\n"
		"{\n"
		"	coords.y = 1.0 - coords.y;\n"
		"	vec2 rcoords = fract(coords) * vec2(size);\n"
		"	float mip = mip_map_level(coords * vec2(size));\n"
		"	uint mip0 = uint(floor(mip));\n"
		"	uint mip1 = uint(ceil(mip));\n"
	);
	strcat(fs,
		"	return mix(solveMip(size, tile, mip0, rcoords),\n"
		"	           solveMip(size, tile, mip1, rcoords), fract(mip));\n"
		"}\n"
		"uniform vec2 Scale;\n"
		"uniform uvec2 Size;\n"
		"uniform uint Tile;\n"
        "in vec2 Frag_UV;\n"
        "in vec4 Frag_Color;\n"
        "out vec4 Out_Color;\n"
        "#line 1\n"
        "void main(){\n"
		"	vec2 uv = Frag_UV * Scale;\n"
		"	uv = fract(uv);\n"
        "   if (Tile > 0u)\n"
        "       Out_Color = Frag_Color * textureSVT(Size, Tile, uv);\n"
        "   else\n"
        "       Out_Color = Frag_Color * textureLod(Texture, uv, 3.0f);\n"
        "}\n"
	);
#ifdef __EMSCRIPTEN__
	g_vertices = malloc((size_t)MAX_VERTEX_BUFFER);
	g_elements = malloc((size_t)MAX_ELEMENT_BUFFER);
#endif

    nk_buffer_init_default(&dev->cmds);
    dev->prog = glCreateProgram();
    dev->vert_shdr = glCreateShader(GL_VERTEX_SHADER);
    dev->frag_shdr = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(dev->vert_shdr, 1, &vertex_shader, 0);
    glShaderSource(dev->frag_shdr, 1, &fragment_shader, 0);
    glCompileShader(dev->vert_shdr);
		checkShaderError(dev->vert_shdr, "kek", "code");
    glCompileShader(dev->frag_shdr);
		checkShaderError(dev->frag_shdr, "kek", "code");
    glGetShaderiv(dev->vert_shdr, GL_COMPILE_STATUS, &status);
    assert(status == GL_TRUE);
    glGetShaderiv(dev->frag_shdr, GL_COMPILE_STATUS, &status);
    assert(status == GL_TRUE);
    glAttachShader(dev->prog, dev->vert_shdr);
    glAttachShader(dev->prog, dev->frag_shdr);
    glLinkProgram(dev->prog);
    glGetProgramiv(dev->prog, GL_LINK_STATUS, &status);
    assert(status == GL_TRUE);

    dev->uniform_tex = glGetUniformLocation(dev->prog, "Texture");
    dev->uniform_indir = glGetUniformLocation(dev->prog, "Indir");
    dev->uniform_scale = glGetUniformLocation(dev->prog, "Scale");
    dev->uniform_size = glGetUniformLocation(dev->prog, "Size");
    dev->uniform_tile = glGetUniformLocation(dev->prog, "Tile");
    dev->uniform_proj = glGetUniformLocation(dev->prog, "ProjMtx");
    dev->attrib_pos = glGetAttribLocation(dev->prog, "Position");
    dev->attrib_uv = glGetAttribLocation(dev->prog, "TexCoord");
    dev->attrib_col = glGetAttribLocation(dev->prog, "Color");

    {
        /* buffer setup */
        GLsizei vs = sizeof(struct nk_can_vertex);
        size_t vp = offsetof(struct nk_can_vertex, position);
        size_t vt = offsetof(struct nk_can_vertex, uv);
        size_t vc = offsetof(struct nk_can_vertex, col);

        glGenBuffers(1, &dev->vbo);
        glGenBuffers(1, &dev->ebo);
        glGenVertexArrays(1, &dev->vao);

        glBindVertexArray(dev->vao);
        glBindBuffer(GL_ARRAY_BUFFER, dev->vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, dev->ebo);

        glEnableVertexAttribArray((GLuint)dev->attrib_pos);
        glEnableVertexAttribArray((GLuint)dev->attrib_uv);
        glEnableVertexAttribArray((GLuint)dev->attrib_col);

        glVertexAttribPointer((GLuint)dev->attrib_pos, 2, GL_FLOAT, GL_FALSE, vs, (void*)vp);
        glVertexAttribPointer((GLuint)dev->attrib_uv, 2, GL_FLOAT, GL_FALSE, vs, (void*)vt);
        glVertexAttribPointer((GLuint)dev->attrib_col, 4, GL_UNSIGNED_BYTE, GL_TRUE, vs, (void*)vc);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

NK_API void
nk_can_font_stash_begin(struct nk_font_atlas **atlas)
{
    nk_font_atlas_init_default(&can.atlas);
    nk_font_atlas_begin(&can.atlas);
    *atlas = &can.atlas;
}

NK_INTERN void
nk_can_device_upload_atlas(const void *image, int width, int height)
{
    struct nk_can_device *dev = &can.ogl;
    glGenTextures(1, &dev->font_tex);
    glBindTexture(GL_TEXTURE_2D, dev->font_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)width, (GLsizei)height, 0,
                GL_RGBA, GL_UNSIGNED_BYTE, image);
}


NK_API void
nk_can_font_stash_end(void)
{
    const void *image; int w, h;
    image = nk_font_atlas_bake(&can.atlas, &w, &h, NK_FONT_ATLAS_RGBA32);
    nk_can_device_upload_atlas(image, w, h);
    nk_font_atlas_end(&can.atlas, nk_handle_id((int)can.ogl.font_tex), &can.ogl.null);
    if (can.atlas.default_font)
        nk_style_set_font(&can.ctx, &can.atlas.default_font->handle);

}
NK_API int
nk_can_handle_key(struct nk_context *ctx, candle_key_e key, int down)
{
	c_keyboard_t *kb = c_keyboard(&SYS);
	switch(key)
	{
		case CANDLE_KEY_LEFT_SHIFT:
		case CANDLE_KEY_RIGHT_SHIFT:
			nk_input_key(ctx, NK_KEY_SHIFT, down);
			break;
		case CANDLE_KEY_DELETE:
			nk_input_key(ctx, NK_KEY_DEL, down);
			break;
		case CANDLE_KEY_ENTER:
			nk_input_key(ctx, NK_KEY_ENTER, down);
			break;
		case CANDLE_KEY_TAB:
			nk_input_key(ctx, NK_KEY_TAB, down);
			break;
		case CANDLE_KEY_BACKSPACE:
			nk_input_key(ctx, NK_KEY_BACKSPACE, down);
			break;
		case CANDLE_KEY_HOME:
			nk_input_key(ctx, NK_KEY_TEXT_START, down);
			nk_input_key(ctx, NK_KEY_SCROLL_START, down);
			break;
		case CANDLE_KEY_END:
			nk_input_key(ctx, NK_KEY_TEXT_END, down);
			nk_input_key(ctx, NK_KEY_SCROLL_END, down);
			break;
		case CANDLE_KEY_PAGE_DOWN:
			nk_input_key(ctx, NK_KEY_SCROLL_DOWN, down);
			break;
		case CANDLE_KEY_PAGE_UP:
			nk_input_key(ctx, NK_KEY_SCROLL_UP, down);
			break;
		case CANDLE_KEY_z:
			nk_input_key(ctx, NK_KEY_TEXT_UNDO, down && kb->ctrl);
			if (down)
				nk_input_char(ctx, key);
			break;
		case CANDLE_KEY_r:
			nk_input_key(ctx, NK_KEY_TEXT_REDO, down && kb->ctrl);
			if (down)
				nk_input_char(ctx, key);
			break;
		case CANDLE_KEY_c:
			nk_input_key(ctx, NK_KEY_COPY, down && kb->ctrl);
			if (down)
				nk_input_char(ctx, key);
			break;
		case CANDLE_KEY_v:
			nk_input_key(ctx, NK_KEY_PASTE, down && kb->ctrl);
			if (down)
				nk_input_char(ctx, key);
			break;
		case CANDLE_KEY_x:
			nk_input_key(ctx, NK_KEY_CUT, down && kb->ctrl);
			if (down)
				nk_input_char(ctx, key);
			break;
		case CANDLE_KEY_b:
			nk_input_key(ctx, NK_KEY_TEXT_LINE_START, down && kb->ctrl);
			if (down)
				nk_input_char(ctx, key);
			break;
		case CANDLE_KEY_e:
			nk_input_key(ctx, NK_KEY_TEXT_LINE_END, down && kb->ctrl);
			if (down)
				nk_input_char(ctx, key);
			break;
		case CANDLE_KEY_UP:
			nk_input_key(ctx, NK_KEY_UP, down);
			break;
		case CANDLE_KEY_DOWN:
			nk_input_key(ctx, NK_KEY_DOWN, down);
			break;
		case CANDLE_KEY_LEFT:
			if (kb->ctrl)
				nk_input_key(ctx, NK_KEY_TEXT_WORD_LEFT, down);
			else
				nk_input_key(ctx, NK_KEY_LEFT, down);
			break;
		case CANDLE_KEY_RIGHT:
			if (kb->ctrl)
				nk_input_key(ctx, NK_KEY_TEXT_WORD_RIGHT, down);
			else
				nk_input_key(ctx, NK_KEY_RIGHT, down);
			break;
		default:
			if (down)
				nk_input_char(ctx, key);
			return 0;
	}
	return 1;
}

/* NK_API int */
/* nk_can_handle_event(struct nk_context *ctx, candle_event_t *evt) */
/* { */
/* 	c_keyboard_t *kb = c_keyboard(&SYS); */

/*     if (evt->type == CANDLE_KEYUP || evt->type == CANDLE_KEYDOWN) { */
/*         /1* key events *1/ */
/* 		candle_key_e key = evt->key; */

/*         int down = evt->type == CANDLE_KEYDOWN; */
/*         if (key == CANDLE_KEY_LEFT_SHIFT || key == CANDLE_KEY_RIGHT_SHIFT) */
/*             nk_input_key(ctx, NK_KEY_SHIFT, down); */
/*         else if (key == CANDLE_KEY_DELETE) */
/*             nk_input_key(ctx, NK_KEY_DEL, down); */
/*         else if (key == CANDLE_KEY_ENTER) */
/*             nk_input_key(ctx, NK_KEY_ENTER, down); */
/*         else if (key == CANDLE_KEY_TAB) */
/*             nk_input_key(ctx, NK_KEY_TAB, down); */
/*         else if (key == CANDLE_KEY_BACKSPACE) */
/*             nk_input_key(ctx, NK_KEY_BACKSPACE, down); */
/*         else if (key == CANDLE_KEY_HOME) { */
/*             nk_input_key(ctx, NK_KEY_TEXT_START, down); */
/*             nk_input_key(ctx, NK_KEY_SCROLL_START, down); */
/*         } else if (key == CANDLE_KEY_END) { */
/*             nk_input_key(ctx, NK_KEY_TEXT_END, down); */
/*             nk_input_key(ctx, NK_KEY_SCROLL_END, down); */
/*         } else if (key == CANDLE_KEY_PAGE_DOWN) { */
/*             nk_input_key(ctx, NK_KEY_SCROLL_DOWN, down); */
/*         } else if (key == CANDLE_KEY_PAGE_UP) { */
/*             nk_input_key(ctx, NK_KEY_SCROLL_UP, down); */
/*         } else if (key == CANDLE_KEY_Z || key == CANDLE_KEY_z) */
/*             nk_input_key(ctx, NK_KEY_TEXT_UNDO, down && kb->ctrl); */
/*         else if (key == CANDLE_KEY_R || key == CANDLE_KEY_r) */
/*             nk_input_key(ctx, NK_KEY_TEXT_REDO, down && kb->ctrl); */
/*         else if (key == CANDLE_KEY_C || key == CANDLE_KEY_c) */
/*             nk_input_key(ctx, NK_KEY_COPY, down && kb->ctrl); */
/*         else if (key == CANDLE_KEY_V || key == CANDLE_KEY_v) */
/*             nk_input_key(ctx, NK_KEY_PASTE, down && kb->ctrl); */
/*         else if (key == CANDLE_KEY_X || key == CANDLE_KEY_x) */
/*             nk_input_key(ctx, NK_KEY_CUT, down && kb->ctrl); */
/*         else if (key == CANDLE_KEY_B || key == CANDLE_KEY_b) */
/*             nk_input_key(ctx, NK_KEY_TEXT_LINE_START, down && kb->ctrl); */
/*         else if (key == CANDLE_KEY_E || key == CANDLE_KEY_e) */
/*             nk_input_key(ctx, NK_KEY_TEXT_LINE_END, down && kb->ctrl); */
/*         else if (key == CANDLE_KEY_UP) */
/*             nk_input_key(ctx, NK_KEY_UP, down); */
/*         else if (key == CANDLE_KEY_DOWN) */
/*             nk_input_key(ctx, NK_KEY_DOWN, down); */
/*         else if (key == CANDLE_KEY_LEFT) { */
/*             if (kb->ctrl) */
/*                 nk_input_key(ctx, NK_KEY_TEXT_WORD_LEFT, down); */
/*             else nk_input_key(ctx, NK_KEY_LEFT, down); */
/*         } else if (key == CANDLE_KEY_RIGHT) { */
/*             if (kb->ctrl) */
/*                 nk_input_key(ctx, NK_KEY_TEXT_WORD_RIGHT, down); */
/*             else nk_input_key(ctx, NK_KEY_RIGHT, down); */
/*         } else return 0; */
/*         return 1; */
/*     } else if (evt->type == CANDLE_MOUSEBUTTONDOWN || evt->type == CANDLE_MOUSEBUTTONUP) { */
/*         /1* mouse button *1/ */
/*         int down = evt->type == CANDLE_MOUSEBUTTONDOWN; */
/*         const int x = evt->button.x, y = evt->button.y; */
/*         if (evt->button.button == CANDLE_MOUSE_BUTTON_LEFT) { */
/*             /1* if (evt->button.clicks > 1) *1/ */
/*                 /1* nk_input_button(ctx, NK_BUTTON_DOUBLE, x, y, down); *1/ */
/*             nk_input_button(ctx, NK_BUTTON_LEFT, x, y, down); */
/*         } else if (evt->button.button == CANDLE_MOUSE_BUTTON_MOUSE) */
/*             nk_input_button(ctx, NK_BUTTON_MIDDLE, x, y, down); */
/*         else if (evt->button.button == CANDLE_MOUSE_BUTTON_RIGHT) */
/*             nk_input_button(ctx, NK_BUTTON_RIGHT, x, y, down); */
/*         return 1; */
/*     } else if (evt->type == CANDLE_MOUSEMOTION) { */
/*         /1* mouse motion *1/ */
/*         if (ctx->input.mouse.grabbed) { */
/*             int x = (int)ctx->input.mouse.prev.x, y = (int)ctx->input.mouse.prev.y; */
/*             nk_input_motion(ctx, x + evt->motion.xrel, y + evt->motion.yrel); */
/*         } else nk_input_motion(ctx, evt->motion.x, evt->motion.y); */
/*         return 1; */
/*     } else if (evt->type == CANDLE_TEXTINPUT) { */
/*         /1* text input *1/ */
/*         nk_glyph glyph; */
/*         /1* memcpy(glyph, evt->text.text, NK_UTF_SIZE); *1/ */
/*         nk_input_glyph(ctx, glyph); */
/*         return 1; */
/*     } else if (evt->type == CANDLE_MOUSEWHEEL) { */
/*         /1* mouse wheel *1/ */
/*         nk_input_scroll(ctx,nk_vec2((float)evt->wheel.x,(float)evt->wheel.y)); */
/*         return 1; */
/*     } */
/*     return 0; */
/* } */


static void
nk_can_clipboard_paste(nk_handle usr, struct nk_text_edit *edit)
{
    /* const char *text = sdl_GetClipboardText(); */
    /* if (text) nk_textedit_paste(edit, text, nk_strlen(text)); */
    /* (void)usr; */
}


static void
nk_can_clipboard_copy(nk_handle usr, const char *text, int len)
{
    char *str = 0;
    (void)usr;
    if (!len) return;
    str = (char*)malloc((size_t)len+1);
    if (!str) return;
    memcpy(str, text, (size_t)len);
    str[len] = '\0';
    /* sdl_SetClipboardText(str); */
    free(str);
}


NK_API struct nk_context*
nk_can_init(void *win)
{
    can.win = win;
    nk_init_default(&can.ctx, 0);
    can.ctx.clip.copy = nk_can_clipboard_copy;
    can.ctx.clip.paste = nk_can_clipboard_paste;
    can.ctx.clip.userdata = nk_handle_ptr(0);
	can.ctx.style.tab.sym_minimize       = NK_SYMBOL_PLUS;
	can.ctx.style.tab.sym_maximize       = NK_SYMBOL_MINUS;
    nk_can_device_create();

	nk_style_from_table(&can.ctx, can_default_color_style);

    return &can.ctx;
}

NK_API void
nk_tree_entity_pop(struct nk_context *ctx)
{
    nk_tree_state_pop(ctx);
}


NK_INTERN int
nk_tree_entity_image_push_hashed_base(struct nk_context *ctx, enum nk_tree_type type,
    struct nk_image *img, const char *title, int title_len,
    enum nk_collapse_states *state, int *selected, int has_children)
{
    struct nk_window *win;
    struct nk_panel *layout;
    const struct nk_style *style;
    struct nk_command_buffer *out;
    const struct nk_input *in;
    const struct nk_style_button *button;
    enum nk_symbol_type symbol;
    float row_height;
    struct nk_vec2 padding;

    int text_len;
    float text_width;

    struct nk_vec2 item_spacing;
    struct nk_rect header = {0,0,0,0};
    struct nk_rect sym = {0,0,0,0};

    nk_flags ws = 0;
    enum nk_widget_layout_states widget_state;

    NK_ASSERT(ctx);
    NK_ASSERT(ctx->current);
    NK_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return 0;

    /* cache some data */
    win = ctx->current;
    layout = win->layout;
    out = &win->buffer;
    style = &ctx->style;
    item_spacing = style->window.spacing;
    padding = style->selectable.padding;

    /* calculate header bounds and draw background */
    row_height = style->font->height + 2 * style->tab.padding.y;
    nk_layout_set_min_row_height(ctx, row_height);
    nk_layout_row_dynamic(ctx, row_height, 1);
    nk_layout_reset_min_row_height(ctx);

    widget_state = nk_widget(&header, ctx);
    if (type == NK_TREE_TAB) {
        const struct nk_style_item *background = &style->tab.background;
        if (background->type == NK_STYLE_ITEM_IMAGE) {
            nk_draw_image(out, header, &background->data.image, nk_white);
        } else {
            nk_fill_rect(out, header, 0, style->tab.border_color);
            nk_fill_rect(out, nk_shrink_rect(header, style->tab.border),
                style->tab.rounding, background->data.color);
        }
    }

    in = (!(layout->flags & NK_WINDOW_ROM)) ? &ctx->input: 0;
    in = (in && widget_state == NK_WIDGET_VALID) ? &ctx->input : 0;

    /* select correct button style */
    if (*state == NK_MAXIMIZED) {
        symbol = style->tab.sym_maximize;
        if (type == NK_TREE_TAB)
            button = &style->tab.tab_maximize_button;
        else button = &style->tab.node_maximize_button;
    } else {
        symbol = style->tab.sym_minimize;
        if (type == NK_TREE_TAB)
            button = &style->tab.tab_minimize_button;
        else button = &style->tab.node_minimize_button;
    }
    {/* draw triangle button */
    sym.w = sym.h = style->font->height;
    sym.y = header.y + style->tab.padding.y;
    sym.x = header.x + style->tab.padding.x;

	if(has_children)
	{
		if (nk_do_button_symbol(&ws, &win->buffer, sym, symbol, NK_BUTTON_DEFAULT, button, in, style->font))
			*state = (*state == NK_MAXIMIZED) ? NK_MINIMIZED : NK_MAXIMIZED;}
	}

    /* draw label */
    {nk_flags dummy = 0;
    struct nk_rect label;
    /* calculate size of the text and tooltip */
    text_len = nk_strlen(title);
    text_width = style->font->width(style->font->userdata, style->font->height, title, text_len);
    text_width += (4 * padding.x);

    header.w = NK_MAX(header.w, sym.w + item_spacing.x);
    label.x = sym.x + sym.w + item_spacing.x;
    label.y = sym.y;
    label.w = NK_MIN(header.w - (sym.w + item_spacing.y + style->tab.indent), text_width);
    label.h = style->font->height;

    if (img) {
        nk_do_selectable_image(&dummy, &win->buffer, label, title, title_len, NK_TEXT_LEFT,
            selected, img, &style->selectable, in, style->font);
    } else nk_do_selectable(&dummy, &win->buffer, label, title, title_len, NK_TEXT_LEFT,
            selected, &style->selectable, in, style->font);
    }
    /* increase x-axis cursor widget position pointer */
    if (*state == NK_MAXIMIZED) {
        layout->at_x = header.x + (float)*layout->offset_x + style->tab.indent;
        layout->bounds.w = NK_MAX(layout->bounds.w, style->tab.indent);
        layout->bounds.w -= (style->tab.indent + style->window.padding.x);
        layout->row.tree_depth++;
        return nk_true;
    } else return nk_false;
}

NK_INTERN int
nk_tree_entity_base(struct nk_context *ctx, enum nk_tree_type type,
    struct nk_image *img, const char *title, enum nk_collapse_states initial_state,
    int *selected, int has_children, const char *hash, int len, int line)
{
    struct nk_window *win = ctx->current;
    int title_len = 0;
    nk_hash tree_hash = 0;
    enum nk_collapse_states *state = NULL;

    /* retrieve tree state from internal widget state tables */
    if (!hash) {
        title_len = (int)nk_strlen(title);
        tree_hash = nk_murmur_hash(title, (int)title_len, (nk_hash)line);
    } else tree_hash = nk_murmur_hash(hash, len, (nk_hash)line);
    state = (enum nk_collapse_states*)nk_find_value(win, tree_hash);
    if (!state) {
        state = (enum nk_collapse_states*)nk_add_value(ctx, win, tree_hash, 0);
        *state = initial_state;
    } return nk_tree_entity_image_push_hashed_base(ctx, type, img, title,
        nk_strlen(title), state, selected, has_children);
}

NK_API int
nk_tree_entity_push_hashed(struct nk_context *ctx, enum nk_tree_type type,
    const char *title, enum nk_collapse_states initial_state,
    int *selected, int has_children, const char *hash, int len, int seed)
{
    return nk_tree_entity_base(ctx, type, 0, title, initial_state, selected, has_children, hash, len, seed);
}
NK_API int
nk_tree_entity_image_push_hashed(struct nk_context *ctx, enum nk_tree_type type,
    struct nk_image img, const char *title, enum nk_collapse_states initial_state,
    int *selected, int has_children, const char *hash, int len,int seed)
{
    return nk_tree_entity_base(ctx, type, &img, title, initial_state, selected, has_children, hash, len, seed);
}

extern texture_t *g_cache;
extern texture_t *g_indir;
void nk_can_render(enum nk_anti_aliasing AA)
{
    struct nk_can_device *dev = &can.ogl;
    int width, height;
    int display_width, display_height;
    struct nk_vec2 scale;
    GLfloat ortho[4][4] = {
        {2.0f, 0.0f, 0.0f, 0.0f},
        {0.0f,-2.0f, 0.0f, 0.0f},
        {0.0f, 0.0f,-1.0f, 0.0f},
        {-1.0f,1.0f, 0.0f, 1.0f},
    };
	glfwGetWindowSize(can.win, &width, &height);
	glfwGetFramebufferSize(can.win, &display_width, &display_height);
	display_width = width;
	display_height = height;
    ortho[0][0] /= (GLfloat)width;
    ortho[1][1] /= (GLfloat)height;

    scale.x = (float)display_width/(float)width;
    scale.y = (float)display_height/(float)height;

    /* setup global state */
    glViewport(0,0,display_width,display_height);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, g_indir->bufs[0].id);
    glActiveTexture(GL_TEXTURE0);
    /* setup program */
    glUseProgram(dev->prog);
    glUniform1i(dev->uniform_tex, 0);
    glUniform1i(dev->uniform_indir, 1);
    glUniform2f(dev->uniform_scale, 1, 1);
    glUniformMatrix4fv(dev->uniform_proj, 1, GL_FALSE, &ortho[0][0]);
    {
        /* convert from command queue into draw list and draw to screen */
        const struct nk_draw_command *cmd;
        const nk_draw_index *offset = NULL;
        struct nk_buffer vbuf, ebuf;

        /* allocate vertex and element buffer */
        glBindVertexArray(dev->vao);
        glBindBuffer(GL_ARRAY_BUFFER, dev->vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, dev->ebo);

        glBufferData(GL_ARRAY_BUFFER, MAX_VERTEX_BUFFER, NULL, GL_STREAM_DRAW);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, MAX_ELEMENT_BUFFER, NULL, GL_STREAM_DRAW);

        /* load vertices/elements directly into vertex/element buffer */
#ifndef __EMSCRIPTEN__
        g_vertices = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
        g_elements = glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);
#endif
        {
            /* fill convert configuration */
            struct nk_convert_config config;
            static const struct nk_draw_vertex_layout_element vertex_layout[] = {
                {NK_VERTEX_POSITION, NK_FORMAT_FLOAT, NK_OFFSETOF(struct nk_can_vertex, position)},
                {NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT, NK_OFFSETOF(struct nk_can_vertex, uv)},
                {NK_VERTEX_COLOR, NK_FORMAT_R8G8B8A8, NK_OFFSETOF(struct nk_can_vertex, col)},
                {NK_VERTEX_LAYOUT_END}
            };
            NK_MEMSET(&config, 0, sizeof(config));
            config.vertex_layout = vertex_layout;
            config.vertex_size = sizeof(struct nk_can_vertex);
            config.vertex_alignment = NK_ALIGNOF(struct nk_can_vertex);
            config.null = dev->null;
            config.circle_segment_count = 22;
            config.curve_segment_count = 22;
            config.arc_segment_count = 22;
            config.global_alpha = 1.0f;
            config.shape_AA = AA;
            config.line_AA = AA;

            /* setup buffers to load vertices and elements */
            nk_buffer_init_fixed(&vbuf, g_vertices, (nk_size)MAX_VERTEX_BUFFER);
            nk_buffer_init_fixed(&ebuf, g_elements, (nk_size)MAX_ELEMENT_BUFFER);
            nk_convert(&can.ctx, &dev->cmds, &vbuf, &ebuf, &config);
        }
#ifndef __EMSCRIPTEN__
        glUnmapBuffer(GL_ARRAY_BUFFER);
        glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
#else
		glBufferSubData(GL_ARRAY_BUFFER, 0, (size_t)MAX_VERTEX_BUFFER,
		                g_vertices);
		glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, (size_t)MAX_ELEMENT_BUFFER,
		                g_elements);
#endif

        /* iterate over and execute each draw command */
        nk_draw_foreach(cmd, &can.ctx, &dev->cmds) {
			struct img_info *info = cmd->userdata.ptr;
            if (!cmd->elem_count) continue;
			if(cmd->userdata.ptr)
			{
				if (!info->blending)
				{
					glDisable(GL_BLEND);
					glUniform2f(dev->uniform_scale, 1, -1);
					glerr();
				}
				glUniform1ui(dev->uniform_tile, info->tile); glerr();
				glUniform2ui(dev->uniform_size, info->size.x, info->size.y); glerr();
			}
            glBindTexture(GL_TEXTURE_2D, (GLuint)cmd->texture.id);
            glScissor((GLint)(cmd->clip_rect.x * scale.x),
                (GLint)((height - (GLint)(cmd->clip_rect.y + cmd->clip_rect.h)) * scale.y),
                (GLint)(cmd->clip_rect.w * scale.x),
                (GLint)(cmd->clip_rect.h * scale.y));
            glDrawElements(GL_TRIANGLES, (GLsizei)cmd->elem_count, GL_UNSIGNED_SHORT, offset);
            offset += cmd->elem_count;
			if(cmd->userdata.ptr)
			{
				glEnable(GL_BLEND);
				glUniform2f(dev->uniform_scale, 1, 1);
				glUniform1ui(dev->uniform_tile, 0);
			}
			glerr();
        }
        nk_clear(&can.ctx);
    }

    glUseProgram(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
	glerr();
}

