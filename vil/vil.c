#include "vil.h"
#include <utils/nk.h>
#include <utils/khash.h>
#include <utils/mafs.h>

struct vil_call;
struct vil_func;

typedef union
{
	struct { unsigned char depth; unsigned char _[7]; };
	uint64_t val;
} slot_t;

typedef void(*vil_func_gui_cb)(struct vil_call *call, void *data, void *ctx);
static struct vil_func *add_func(const char *name,
		vil_func_gui_cb builtin_gui, int builtin_size);
static struct vil_call *add_call(struct vil_func *root, unsigned int func,
		const char *name, struct nk_color color, struct nk_vec2 pos,
		int is_input, int is_output, int collapsable);
static void func_link(struct vil_call *root, slot_t in_slot, slot_t out_slot,
		int field);

struct vil_link
{
	slot_t input;
	slot_t output;
};

struct vil_arg
{
	float height;
	slot_t link;
	void *data;
	struct vil_func *type;
	int expanded;
};

struct vil_ret
{
	/* float height = 29; */
	slot_t link;
	struct vil_func *type;
};

struct vil_call
{
	struct vil_func *func;
	struct vil_func *parent;

	unsigned int id;
	char name[32];
	struct vil_call *next;
	struct vil_call *prev;

	int collapsable;
	int is_input;
	int is_output;

	struct nk_color color;

	struct nk_rect bounds;

	struct vil_arg input_args[32];
	struct vil_ret output_args[32];

	void *builtin_data;
};

struct vil_func
{
	char name[32];
	unsigned int id;

	struct vil_call call_buf[32];
	struct vil_link links[64];
	struct vil_call *begin;
	struct vil_call *end;
	unsigned int call_count;
	unsigned int link_count;

	unsigned int		builtin_size;
	vil_func_gui_cb		builtin_gui;
	/* vil_func_load_cb	builtin_load; */
	/* vil_func_save_cb	builtin_save; */
};

KHASH_MAP_INIT_INT(vil_func, struct vil_func *)

static slot_t linking;
struct vil_call *g_dragging;
struct vil_call *g_hovered;
static unsigned int linking_type;
static slot_t g_call_unlink;
static slot_t g_call_link;
static struct nk_vec2 scrolling;
static int show_grid;

static khash_t(vil_func) *g_funcs;

void label_gui(struct vil_call *call, void *ctx) 
{
	nk_layout_row_dynamic(ctx, 29, 1);
	nk_label(ctx, call->name, NK_TEXT_LEFT);
}

void color_gui(struct vil_call *call, struct nk_colorf *color, void *ctx) 
{
	nk_layout_row_dynamic(ctx, 29, 1);
	if (nk_combo_begin_color(ctx, nk_rgb_cf(*color), nk_vec2(200,400))) {
		nk_layout_row_dynamic(ctx, 120, 1);
		*color = nk_color_picker(ctx, *color, NK_RGBA);

		nk_combo_end(ctx);
	}
}

void number_gui(struct vil_call *call, float *num, void *ctx) 
{
	/* if(*num < 29) *num = 29; */
	/* nk_layout_row_begin(ctx, NK_DYNAMIC, *num, 2); */
	nk_layout_row_begin(ctx, NK_DYNAMIC, 29, 2);
	nk_layout_row_push(ctx, 0.20);
	nk_label(ctx, call->name, NK_TEXT_LEFT);
	nk_layout_row_push(ctx, 0.80);
	*num = nk_propertyf(ctx, "#", 0, *num, 1, 0.01, 0.01);
	nk_layout_row_end(ctx);
	/* nk_slider_int(ctx, 29, num, 200, 1); */
}

REG() {
	g_funcs = kh_init(vil_func);
	struct vil_func *func;

	show_grid = nk_true;

	func = add_func("number", (vil_func_gui_cb)number_gui, sizeof(float));
	func = add_func("_color_picker", (vil_func_gui_cb)color_gui, sizeof(vec4_t));

	func = add_func("test", NULL, 0);
		add_call(func, ref("number"), "A", nk_rgb(100, 100, 100), nk_vec2(40, 10), 1, 0, 0);
		add_call(func, ref("number"), "B", nk_rgb(100, 100, 100), nk_vec2(40, 260), 1, 0, 0);
		add_call(func, ref("number"), "C", nk_rgb(100, 100, 100), nk_vec2(200, 260), 0, 1, 0);

	func = add_func("rgba", NULL, sizeof(vec4_t));
		add_call(func, ref("_color_picker"), "color", nk_rgb(100, 100, 100), nk_vec2(100, 10), 1, 0, 0);
		add_call(func, ref("number"), "r", nk_rgb(255, 0, 0), nk_vec2(100, 10), 1, 1, 1);
		add_call(func, ref("number"), "g", nk_rgb(0, 255, 0), nk_vec2(200, 10), 1, 1, 1);
		add_call(func, ref("number"), "b", nk_rgb(0, 0, 255), nk_vec2(300, 10), 1, 1, 1);
		add_call(func, ref("number"), "a", nk_rgb(255, 255, 255), nk_vec2(300, 10), 1, 1, 1);


	func = add_func("parent", NULL, 0);
		add_call(func, ref("number"),	"num",	nk_rgb(100, 100, 100), nk_vec2(40, 10), 0, 0, 0);
		add_call(func, ref("number"),	"num2",	nk_rgb(100, 100, 100), nk_vec2(149, 13), 0, 0, 0);
		add_call(func, ref("test"),		"sum",	nk_rgb(100, 100, 100), nk_vec2(40, 260), 0, 0, 0);
		add_call(func, ref("rgba"),	"color1", nk_rgb(100, 100, 100), nk_vec2(0, 0), 0, 0, 0);
		add_call(func, ref("rgba"),	"color2", nk_rgb(100, 100, 100), nk_vec2(0, 0), 0, 0, 0);
}

static struct vil_func *add_func(const char *name,
		vil_func_gui_cb builtin_gui, int builtin_size)
{
	int ret = 0;
	int id = ref(name);
	khiter_t k = kh_put(vil_func, g_funcs, id, &ret);
	if(ret != 1) exit(1);
	struct vil_func **func = &kh_value(g_funcs, k);
	*func = malloc(sizeof(struct vil_func));
	**func = (struct vil_func){
		.builtin_gui = builtin_gui,
		.builtin_size = builtin_size,
		.id = id
	};

	strncpy((*func)->name, name, sizeof((*func)->name));
	/* func_link(func, 0, 0, 2, 0); */
	/* func_link(func, 1, 0, 2, 1); */

	return *func;
}

static void func_push(struct vil_func *func, struct vil_call *call)
{
	if (!func->begin) {
		call->next = NULL;
		call->prev = NULL;
		func->begin = call;
		func->end = call;
	} else {
		call->prev = func->end;
		func->end->next = call;
		call->next = NULL;
		func->end = call;
	}
}

static void func_pop(struct vil_func *func, struct vil_call *call)
{
	if (call->next) call->next->prev = call->prev;
	if (call->prev) call->prev->next = call->next;
	if (func->end == call) func->end = call->prev;
	if (func->begin == call) func->begin = call->next;
	call->next = NULL;
	call->prev = NULL;
}

static struct vil_call *get_call(struct vil_func *func, int id)
{
	struct vil_call *iter = func->begin;
	while (iter) {
		if (iter->id == id)
			return iter;
		iter = iter->next;
	}
	return NULL;
}

static struct vil_func *get_func(uint ref)
{
	khiter_t k = kh_get(vil_func, g_funcs, ref);
	if(k == kh_end(g_funcs))
	{
		printf("Type does not exist\n");
		return NULL;
	}

	return kh_value(g_funcs, k);
}


static struct vil_call *add_call(struct vil_func *root, unsigned int func,
		const char *name, struct nk_color color, struct nk_vec2 pos,
		int is_input, int is_output, int collapsable)
{
	struct vil_call *call;
	/* NK_ASSERT((nk_size)func->call_count < NK_LEN(func->call_buf)); */
	unsigned int i = root->call_count++;
	call = &root->call_buf[i];
	call->id = i;
	call->bounds.x = pos.x;
	call->bounds.y = pos.y;
	call->bounds.w = 180;
	call->bounds.h = 29;
	call->func = get_func(func);
	call->parent = root;
	call->color = color;
	if(call->func->builtin_size)
	{
		call->builtin_data = calloc(1, call->func->builtin_size);
	}
	call->is_input = is_input;
	call->collapsable = collapsable;
	call->is_output = is_output;
	/* call->input_count = func_count_inputs(call->func); */
	/* call->output_count = 0; */
	/* call->output_count = func_count_outputs(call->func); */

	strncpy(call->name, name, sizeof(call->name));
	func_push(root, call);
	return call;
}

static void func_unlink(struct vil_call *root, slot_t out_slot,
		int field)
{
	int i;
	struct vil_func *func = root->parent;

	if(func->link_count > 1)
	{
		for(i = 0; i < func->link_count; i++)
		{
			struct vil_link *link = &func->links[i];
			if(link->output.val == out_slot.val)
			{
				*link = func->links[func->link_count - 1];
				break;
			}
		}
	}
	func->link_count--;
	root->input_args[field].link.val = 0;
	root->output_args[field].link.val = 0;
}

static void func_link(struct vil_call *root, slot_t in_slot, slot_t out_slot,
		int field)
{
	struct vil_link *link = &root->parent->links[root->parent->link_count++];

	g_call_link = in_slot;

	link->input = in_slot;
	link->output = out_slot;
	root->input_args[field].link = in_slot;
}

static void output_options(struct vil_call *root, int *field,
		struct vil_call *call, slot_t parent_slot, struct nk_context *ctx)
{
	slot_t slot = parent_slot;
	slot._[slot.depth++] = call->id;
	struct vil_call *it;
	int call_field = (*field)++;
	if(call->func->builtin_size)
	{
		if(!root->output_args[call_field].link.depth) /* is not linked */
		{
			if (nk_contextual_item_label(ctx, call->name, NK_TEXT_CENTERED))
			{
				root->output_args[call_field].link.depth = (unsigned char)-1;
				linking = slot;
				linking_type = call->func->id;
			}
		}
	}
	for (it = call->func->begin; it; it = it->next)
	{
		if(it->is_output)
		{
			output_options(root, field, it, slot, ctx);
		}
	}
}

static float call_outputs(struct vil_call *root, int *field,
		struct vil_call *call, slot_t parent_slot, float y,
		struct nk_context *ctx)
{
	int call_field = (*field)++;
	struct vil_call *it;
	slot_t slot = parent_slot;
	slot._[slot.depth++] = call->id;

	struct vil_ret *arg = &root->output_args[call_field];

	if(!linking.depth && arg->link.depth == (unsigned char)-1)
	{
		arg->link.val = 0;
	}
	if(g_call_unlink.depth && g_call_unlink.val == slot.val)
	{
		arg->link.val = 0;
		g_call_unlink.val = 0;
	}
	if(g_call_link.depth && g_call_link.val == slot.val)
	{
		arg->link = g_call_link;
		g_call_link.val = 0;
	}
	const struct nk_input *in = &ctx->input;
	struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);

	if(arg->link.depth) /* is linked */
	{
		struct nk_rect circle;
		circle.x = root->bounds.x + call->bounds.w + 2;
		/* circle.y = call->bounds.y + space * (float)(n+1); */
		circle.y = y + 29.0f / 2.0f;
		circle.w = 8; circle.h = 8;
		nk_fill_circle(canvas, circle, call->color);

		struct nk_rect bound = nk_rect(circle.x-5, circle.y-5,
				circle.w+10, circle.h+10);

		/* start linking process */
		if (nk_input_has_mouse_click_down_in_rect(in,
					NK_BUTTON_LEFT, bound, nk_true))
		{
			linking = slot;
			linking_type = call->func->id;
		}

		/* draw curve from linked call slot to mouse position */
		if (linking.depth && linking.val == slot.val)
		{
			struct nk_vec2 l0 = nk_vec2(circle.x + 3, circle.y + 3);
			struct nk_vec2 l1 = in->mouse.pos;
			nk_stroke_curve(canvas, l0.x, l0.y, l0.x + 50.0f, l0.y,
					l1.x - 50.0f, l1.y, l1.x, l1.y, 1.0f, nk_rgb(100, 100, 100));
		}
		y += 29;
	}

	for (it = call->func->begin; it; it = it->next)
	{
		if(it->is_output)
		{
			y = call_outputs(root, field, it, slot, y, ctx);
		}
	}
	return y;
}

static int get_call_output_y(struct vil_call *root, struct vil_call *call, float *y,
		slot_t parent_slot, slot_t search)
{
	struct vil_call *it;
	slot_t slot = parent_slot;
	slot._[slot.depth++] = call->id;

	float h = 29.0f;
	if(slot.val == search.val)
	{
		*y += h / 2.0f;
		return 1;
	}

	for (it =  call->func->begin; it; it = it->next)
	{
		if(it->is_output)
		{
			if(get_call_output_y(root, it, y, slot, search))
			{
				return 1;
			}
		}
	}
	return 0;
}
static int get_call_input_y(struct vil_call *root, int *field,
		struct vil_call *call, float *y, slot_t parent_slot, slot_t search)
{
	slot_t slot = parent_slot;
	slot._[slot.depth++] = call->id;

	int call_field = (*field)++;

	float h = root->input_args[call_field].height;
	if(slot.val == search.val)
	{
		*y += h / 2.0f;
		return 1;
	}

	if(call->func->builtin_gui)
	{
		*y += h;
		return 0;
	}

	struct vil_call *it;

	for(it = call->func->begin; it; it = it->next)
	{
		if(it->is_input)
		{
			if(get_call_input_y(root, field, it, y, slot, search))
			{
				return 1;
			}
		}
	}
	return 0;
}

struct nk_vec2 get_output_position(struct vil_func *func, slot_t search)
{
	struct vil_call *call = get_call(func, search._[0]);
	float y = call->bounds.y + 33;
	get_call_output_y(call, call, &y, (slot_t){0}, search);
	return nk_vec2(call->bounds.x + call->bounds.w, y);
}

struct nk_vec2 get_input_position(struct vil_func *func, slot_t search)
{
	int field = 0;
	struct vil_call *call = get_call(func, search._[0]);
	float y = call->bounds.y + 33;
	get_call_input_y(call, &field, call, &y, (slot_t){0}, search);

	return nk_vec2(call->bounds.x, y);
}

static void call_inputs(struct vil_call *root, int *field,
		struct vil_call *call, float *y, slot_t parent_slot,
		struct nk_context *ctx)
{
	int call_field = (*field)++;

	slot_t slot = parent_slot;
	slot._[slot.depth++] = call->id;

	struct vil_arg *arg = &root->input_args[call_field];

	int is_linking = linking.depth && linking._[0] != root->id;
	int linking_allowed = is_linking && linking_type == call->func->id;
	int is_linked = arg->link.depth;

	float call_h = arg->height;

	if(linking_allowed || is_linked)
	{
		const struct nk_input *in = &ctx->input;
		struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);

		struct nk_rect circle = nk_rect(root->bounds.x + 2,
				*y + call_h / 2.0f, 8, 8);
		nk_fill_circle(canvas, circle, call->color);

		struct nk_rect bound = nk_rect(circle.x-5, circle.y-5,
				circle.w+10, circle.h+10);

		if(is_linked)
		{
			if (nk_input_has_mouse_click_down_in_rect(in,
						NK_BUTTON_LEFT, bound, nk_true))
			{
				linking = arg->link;
				linking_type = call->func->id;
				func_unlink(root, slot, call_field);
				g_call_unlink = arg->link;
			}
		}

		if (linking_allowed &&
				nk_input_is_mouse_released(in, NK_BUTTON_LEFT) &&
				nk_input_is_mouse_hovering_rect(in, bound))
		{
			if(is_linked)
			{
				func_unlink(root, slot, call_field);
				g_call_unlink = arg->link;
			}
			func_link(root, linking, slot, call_field);
			linking.val = 0;
		}
	}
	struct vil_call *it;
	for (it = call->func->begin; it; it = it->next)
	{
		if(it->is_input)
		{
			call_inputs(root, field, it, y, slot, ctx);
		}
	}
	if(call->func->builtin_gui) *y += call_h;
}

static float call_gui(struct vil_call *root, int *field,
		char **inherited_data, struct vil_call *call, void *ctx)
{
	struct vil_call *it;
	float call_h = 0.0f;
	int call_field = (*field)++;
	char **data = (char**)&root->input_args[call_field].data;

	if(root->input_args[call_field].link.depth) /* input is linked */
	{
		label_gui(call, ctx);
		struct nk_rect rect = nk_layout_widget_bounds(ctx);
		call_h = rect.h;
		return root->input_args[call_field].height = call_h;
	}
	if(*data == NULL)
	{
		if(*inherited_data)
		{
			*data = *inherited_data;
			*inherited_data += call->func->builtin_size;
		}
		else if(call->func->builtin_size)
		{
			*data = calloc(1, call->func->builtin_size);
		}
	}

	char *current_data = *data;

	if(call->func->builtin_gui)
	{
		call->func->builtin_gui(call, *data, ctx);
		struct nk_rect rect = nk_layout_widget_bounds(ctx);
		call_h = rect.h;
	}
	else
	{
		for (it = call->func->begin; it; it = it->next)
		{
			if(it->is_input)
			{
				if(*data)
				{
					if(current_data >= (*data) + call->func->builtin_size)
					{
						current_data = *data;
					}
				}

				call_h += call_gui(root, field, &current_data, it, ctx);
			}
		}
	}
	return root->input_args[call_field].height = call_h;
}

int func_gui(uint func_ref, void *nk)
{
	struct nk_context *ctx = nk;
	struct nk_rect total_space;
	const struct nk_input *in = &ctx->input;
	struct vil_func *func = get_func(func_ref);

	if (nk_begin(ctx, func->name, nk_rect(0, 0, 800, 600),
				NK_WINDOW_BORDER |
				NK_WINDOW_NO_SCROLLBAR |
				NK_WINDOW_MOVABLE |
				NK_WINDOW_CLOSABLE))
	{
		/* allocate complete window space */
		total_space = nk_window_get_content_region(ctx);
		nk_layout_space_begin(ctx, NK_STATIC, total_space.h, func->call_count);
		{
			struct vil_call *hovered = NULL;
			struct vil_call *dragging = NULL;
			struct vil_call *it;
			/* struct nk_rect size = nk_layout_space_bounds(ctx); */
			struct nk_panel *call = 0;

			/* execute each call as a movable group */
			for (it = func->begin; it; it = it->next)
			{
				/* calculate scrolled call window position and size */
				nk_layout_space_push(ctx, nk_rect(it->bounds.x - scrolling.x,
							it->bounds.y - scrolling.y, it->bounds.w, 35 + it->bounds.h));

				/* execute call window */
				if (nk_group_begin(ctx, it->name,
							NK_WINDOW_NO_SCROLLBAR|
							NK_WINDOW_BORDER|
							NK_WINDOW_TITLE))
				{
					/* always have last selected call on top */

					call = nk_window_get_panel(ctx);
					struct nk_rect bd = call->bounds;
					struct nk_rect header = nk_rect(bd.x, bd.y - 29, bd.w, 29);
					bd.y -= 29;
					bd.h += 29;

					if(g_dragging)
					{
						if(g_dragging == it)
						{
							if(nk_input_is_mouse_released(in, NK_BUTTON_LEFT))
							{
								g_dragging = NULL;
							}
							call->bounds.x += in->mouse.delta.x;
							call->bounds.y += in->mouse.delta.y;
						}
					}
					else if (nk_input_is_mouse_hovering_rect(in, header))
					{
						if(!linking.depth && nk_input_is_mouse_pressed(in, NK_BUTTON_LEFT))
						{
							dragging = it;
						}
						hovered = it;
					}

					int field = 0;
					char *inherited_data = NULL;
					float h = call_gui(it, &field, &inherited_data, it, nk);
					it->bounds.h = h;
					/* nk_layout_row_begin(ctx, NK_DYNAMIC, 25, 2); */
					/* nk_layout_row_push(ctx, 0.50); */
					/* nk_label(ctx, "input", NK_TEXT_LEFT); */
					/* nk_layout_row_push(ctx, 0.50); */
					/* nk_label(ctx, "output", NK_TEXT_RIGHT); */
					/* nk_layout_row_end(ctx); */

					/* nk_layout_row_begin(ctx, NK_DYNAMIC, 25, 2); */
					/* nk_layout_row_push(ctx, 0.50); */
					/* nk_label(ctx, "input", NK_TEXT_LEFT); */
					/* nk_layout_row_push(ctx, 0.50); */
					/* nk_label(ctx, "output", NK_TEXT_RIGHT); */
					/* nk_layout_row_end(ctx); */
					/* contextual menu */
					if(g_hovered == it)
					{
						if (nk_contextual_begin(ctx, 0, nk_vec2(100, 220), bd))
						{
							nk_layout_row_dynamic(ctx, 25, 1);
							int field = 0;
							slot_t sl = {0};
							output_options(it, &field, it, sl, ctx);
							nk_contextual_end(ctx);
						}
					}
					nk_group_end(ctx);
				}
				{
					/* call connector and linking */
					struct nk_rect bounds;
					bounds = nk_layout_space_rect_to_local(ctx, call->bounds);
					bounds.x += scrolling.x;
					bounds.y += scrolling.y;
					bounds.h = it->bounds.h;
					it->bounds = bounds;


					slot_t sl = {0};
					int field = 0;
					/* output connector */
					call_outputs(it, &field, it, sl, call->bounds.y + 29, ctx);
					/* input connector */
					field = 0;
					float y = call->bounds.y + 29;
					call_inputs(it, &field, it, &y, sl, ctx);
				}
			}

			/* reset linking connection */
			if(linking.depth)
			{
				dragging = NULL;
				g_dragging = NULL;
				if (nk_input_is_mouse_released(in, NK_BUTTON_LEFT))
				{
					g_call_unlink = linking;
					linking.val = 0;
					fprintf(stdout, "linking failed\n");
				}
			}

			/* draw each link */
			int n;
			struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
			for (n = 0; n < func->link_count; ++n)
			{
				struct vil_link *link = &func->links[n];

				struct nk_vec2 pi = get_input_position(func, link->output);
				struct nk_vec2 po = get_output_position(func, link->input);

				struct nk_vec2 l0 = nk_layout_space_to_screen(ctx, po);
				struct nk_vec2 l1 = nk_layout_space_to_screen(ctx, pi);

				l0.x -= scrolling.x;
				l0.y -= scrolling.y;
				l1.x -= scrolling.x;
				l1.y -= scrolling.y;
				nk_stroke_curve(canvas, l0.x, l0.y, l0.x + 50.0f, l0.y,
						l1.x - 50.0f, l1.y, l1.x, l1.y, 1.0f, nk_rgb(100, 100, 100));
			}

			if(hovered)
			{
				if(g_hovered && g_hovered != hovered)
				/* Don't transition directly so contextual has chance to close */
				{
					g_hovered = NULL;
				}
				else
				{
					g_hovered = hovered;
				}
			}

			if(dragging)
			{
				g_dragging = dragging;
				if (dragging->next)
				{
					/* reshuffle calls to have least recently selected call on top */
					func_pop(func, g_dragging);
					func_push(func, g_dragging);
				}
			}

		}
		nk_layout_space_end(ctx);

		/* window content scrolling */
		if (nk_input_is_mouse_hovering_rect(in, nk_window_get_bounds(ctx)) &&
				nk_input_is_mouse_down(in, NK_BUTTON_MIDDLE))
		{
			scrolling.x += in->mouse.delta.x;
			scrolling.y += in->mouse.delta.y;
		}
	}
	nk_end(ctx);
	return !nk_window_is_closed(ctx, func->name);
}

