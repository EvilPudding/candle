#include <utils/glutil.h>
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#define NK_INCLUDE_COMMAND_USERDATA
#define NK_SDL_GL3_IMPLEMENTATION
#include "nuklear/nuklear.h"
#include "nuklear/demo/sdl_opengl3/nuklear_sdl_gl3.h"

#undef NK_SHADER_VERSION 
#define NK_SHADER_VERSION "#version 400\n"

NK_API void
nk_candle_device_create(void)
{
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
    static const GLchar *fragment_shader =
        NK_SHADER_VERSION
        "precision mediump float;\n"
        "uniform sampler2D Texture;\n"
        "in vec2 Frag_UV;\n"
        "in vec4 Frag_Color;\n"
        "out vec4 Out_Color;\n"
        "void main(){\n"
        "   Out_Color = Frag_Color * textureLod(Texture, Frag_UV.st, 0);\n"
        "}\n";

    struct nk_sdl_device *dev = &sdl.ogl;
    nk_buffer_init_default(&dev->cmds);
    dev->prog = glCreateProgram();
    dev->vert_shdr = glCreateShader(GL_VERTEX_SHADER);
    dev->frag_shdr = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(dev->vert_shdr, 1, &vertex_shader, 0);
    glShaderSource(dev->frag_shdr, 1, &fragment_shader, 0);
    glCompileShader(dev->vert_shdr);
    glCompileShader(dev->frag_shdr);
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
    dev->uniform_proj = glGetUniformLocation(dev->prog, "ProjMtx");
    dev->attrib_pos = glGetAttribLocation(dev->prog, "Position");
    dev->attrib_uv = glGetAttribLocation(dev->prog, "TexCoord");
    dev->attrib_col = glGetAttribLocation(dev->prog, "Color");

    {
        /* buffer setup */
        GLsizei vs = sizeof(struct nk_sdl_vertex);
        size_t vp = offsetof(struct nk_sdl_vertex, position);
        size_t vt = offsetof(struct nk_sdl_vertex, uv);
        size_t vc = offsetof(struct nk_sdl_vertex, col);

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

NK_API struct nk_context*
nk_candle_init(SDL_Window *win)
{
    sdl.win = win;
    nk_init_default(&sdl.ctx, 0);
    sdl.ctx.clip.copy = nk_sdl_clipbard_copy;
    sdl.ctx.clip.paste = nk_sdl_clipbard_paste;
    sdl.ctx.clip.userdata = nk_handle_ptr(0);
	sdl.ctx.style.tab.sym_minimize       = NK_SYMBOL_PLUS;
	sdl.ctx.style.tab.sym_maximize       = NK_SYMBOL_MINUS;
    nk_candle_device_create();
    return &sdl.ctx;
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
    nk_uint *state = 0;

    /* retrieve tree state from internal widget state tables */
    if (!hash) {
        title_len = (int)nk_strlen(title);
        tree_hash = nk_murmur_hash(title, (int)title_len, (nk_hash)line);
    } else tree_hash = nk_murmur_hash(hash, len, (nk_hash)line);
    state = nk_find_value(win, tree_hash);
    if (!state) {
        state = nk_add_value(ctx, win, tree_hash, 0);
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

void nk_candle_render(enum nk_anti_aliasing AA, int max_vertex_buffer,
		int max_element_buffer)
{
    struct nk_sdl_device *dev = &sdl.ogl;
    int width, height;
    int display_width, display_height;
    struct nk_vec2 scale;
    GLfloat ortho[4][4] = {
        {2.0f, 0.0f, 0.0f, 0.0f},
        {0.0f,-2.0f, 0.0f, 0.0f},
        {0.0f, 0.0f,-1.0f, 0.0f},
        {-1.0f,1.0f, 0.0f, 1.0f},
    };
    SDL_GetWindowSize(sdl.win, &width, &height);
    SDL_GL_GetDrawableSize(sdl.win, &display_width, &display_height);
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
    glActiveTexture(GL_TEXTURE0);

    /* setup program */
    glUseProgram(dev->prog);
    glUniform1i(dev->uniform_tex, 0);
    glUniformMatrix4fv(dev->uniform_proj, 1, GL_FALSE, &ortho[0][0]);
    {
        /* convert from command queue into draw list and draw to screen */
        const struct nk_draw_command *cmd;
        void *vertices, *elements;
        const nk_draw_index *offset = NULL;
        struct nk_buffer vbuf, ebuf;

        /* allocate vertex and element buffer */
        glBindVertexArray(dev->vao);
        glBindBuffer(GL_ARRAY_BUFFER, dev->vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, dev->ebo);

        glBufferData(GL_ARRAY_BUFFER, max_vertex_buffer, NULL, GL_STREAM_DRAW);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, max_element_buffer, NULL, GL_STREAM_DRAW);

        /* load vertices/elements directly into vertex/element buffer */
        vertices = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
        elements = glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);
        {
            /* fill convert configuration */
            struct nk_convert_config config;
            static const struct nk_draw_vertex_layout_element vertex_layout[] = {
                {NK_VERTEX_POSITION, NK_FORMAT_FLOAT, NK_OFFSETOF(struct nk_sdl_vertex, position)},
                {NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT, NK_OFFSETOF(struct nk_sdl_vertex, uv)},
                {NK_VERTEX_COLOR, NK_FORMAT_R8G8B8A8, NK_OFFSETOF(struct nk_sdl_vertex, col)},
                {NK_VERTEX_LAYOUT_END}
            };
            NK_MEMSET(&config, 0, sizeof(config));
            config.vertex_layout = vertex_layout;
            config.vertex_size = sizeof(struct nk_sdl_vertex);
            config.vertex_alignment = NK_ALIGNOF(struct nk_sdl_vertex);
            config.null = dev->null;
            config.circle_segment_count = 22;
            config.curve_segment_count = 22;
            config.arc_segment_count = 22;
            config.global_alpha = 1.0f;
            config.shape_AA = AA;
            config.line_AA = AA;

            /* setup buffers to load vertices and elements */
            nk_buffer_init_fixed(&vbuf, vertices, (nk_size)max_vertex_buffer);
            nk_buffer_init_fixed(&ebuf, elements, (nk_size)max_element_buffer);
            nk_convert(&sdl.ctx, &dev->cmds, &vbuf, &ebuf, &config);
        }
        glUnmapBuffer(GL_ARRAY_BUFFER);
        glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);

        /* iterate over and execute each draw command */
        nk_draw_foreach(cmd, &sdl.ctx, &dev->cmds) {
            if (!cmd->elem_count) continue;
			if(cmd->userdata.ptr == (void*)(unsigned long)1) glDisable(GL_BLEND);
            glBindTexture(GL_TEXTURE_2D, (GLuint)cmd->texture.id);
            glScissor((GLint)(cmd->clip_rect.x * scale.x),
                (GLint)((height - (GLint)(cmd->clip_rect.y + cmd->clip_rect.h)) * scale.y),
                (GLint)(cmd->clip_rect.w * scale.x),
                (GLint)(cmd->clip_rect.h * scale.y));
            glDrawElements(GL_TRIANGLES, (GLsizei)cmd->elem_count, GL_UNSIGNED_SHORT, offset);
            offset += cmd->elem_count;
			if(cmd->userdata.ptr == (void*)(unsigned long)1) glEnable(GL_BLEND);
        }
        nk_clear(&sdl.ctx);
    }

    glUseProgram(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
}

void nk_draw_image_ext(struct nk_command_buffer *b, struct nk_rect r,
    const struct nk_image *img, struct nk_color col, int no_blending)
{
    struct nk_command_image *cmd;
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
    cmd->header.userdata.ptr = (void*)(unsigned long)no_blending;
}

