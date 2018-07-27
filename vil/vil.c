#include "vil.h"
#include <utils/nk.h>
#include <utils/khash.h>

struct vil_call;
struct vil_type;

typedef union
{
	struct { unsigned char size; unsigned char _[7]; };
	uint64_t val;
} slot_t;

typedef void(*vil_func_gui_cb)(struct vil_call *call, void *ctx);
static struct vil_func *add_func(const char *name,
		vil_func_gui_cb builtin_gui, int builtin_size);
static void func_add_call(struct vil_func *root, unsigned int func,
		const char *name, struct nk_vec2 pos, int is_input, int is_output);
static void func_link(struct vil_call *root, slot_t in_slot, slot_t out_slot,
		int field);

struct vil_link
{
	slot_t input;
	slot_t output;
	struct nk_vec2 in;
	struct nk_vec2 out;
};

struct vil_call
{
	struct vil_func *func;
	struct vil_func *parent;

	int id;
	char name[32];
	struct vil_call *next;
	struct vil_call *prev;

	int is_input;
	int is_output;


	struct nk_rect bounds;

	float input_sizes[32];
	slot_t input_args[32];

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
	int call_count;
	int link_count;

	unsigned int		builtin_size;
	vil_func_gui_cb		builtin_gui;
	/* vil_func_load_cb	builtin_load; */
	/* vil_func_save_cb	builtin_save; */
};

KHASH_MAP_INIT_INT(vil_func, struct vil_func)

static slot_t linking;
static unsigned int linking_type;
static struct nk_vec2 scrolling;
static int show_grid;
static struct vil_call *selected;

static khash_t(vil_func) *g_funcs;

void label_gui(struct vil_call *call, void *ctx) 
{
	nk_layout_row_dynamic(ctx, 29, 1);
	nk_label(ctx, call->name, NK_TEXT_LEFT);
}
void number_gui(struct vil_call *call, void *ctx) 
{
	int *num = call->builtin_data;
	if(*num < 29) *num = 29;
	nk_layout_row_begin(ctx, NK_DYNAMIC, *num, 2);
	nk_layout_row_push(ctx, 0.20);
	nk_label(ctx, call->name, NK_TEXT_LEFT);
	nk_layout_row_push(ctx, 0.80);
	*num = (nk_byte)nk_propertyi(ctx, "#", 0, *num, 255, 1, 1);
	nk_layout_row_end(ctx);
	/* nk_slider_int(ctx, 29, num, 200, 1); */
}

REG() {
	g_funcs = kh_init(vil_func);

	show_grid = nk_true;

	add_func("number", (vil_func_gui_cb)number_gui, sizeof(float));

	struct vil_func *func = add_func("test", NULL, 0);

	func_add_call(func, ref("number"), "A", nk_vec2(40, 10), 1, 0);
	func_add_call(func, ref("number"), "B", nk_vec2(40, 260), 1, 0);
	func_add_call(func, ref("number"), "C", nk_vec2(200, 260), 0, 1);

	func = add_func("parent", NULL, 0);

	func_add_call(func, ref("number"), "num", nk_vec2(40, 10), 0, 0);
	func_add_call(func, ref("number"), "num2", nk_vec2(149, 13), 0, 0);
	func_add_call(func, ref("test"), "sum", nk_vec2(40, 260), 0, 0);
}

static struct vil_func *add_func(const char *name,
		vil_func_gui_cb builtin_gui, int builtin_size)
{
	int ret;
	int id = ref(name);
	khiter_t k = kh_put(vil_func, g_funcs, id, &ret);
	struct vil_func *func = &kh_value(g_funcs, k);
	strncpy(func->name, name, sizeof(func->name));
	func->builtin_gui = builtin_gui;
	func->builtin_size = builtin_size;
	func->begin = NULL;
	func->end = NULL;
	func->id = id;

	/* func_link(func, 0, 0, 2, 0); */
	/* func_link(func, 1, 0, 2, 1); */

	return func;
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
		if (func->end)
			func->end->next = call;
		call->next = NULL;
		func->end = call;
	}
}

static void func_pop(struct vil_func *func, struct vil_call *call)
{
	if (call->next)
		call->next->prev = call->prev;
	if (call->prev)
		call->prev->next = call->next;
	if (func->end == call)
		func->end = call->prev;
	if (func->begin == call)
		func->begin = call->next;
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

	return &kh_value(g_funcs, k);
}


static void func_add_call(struct vil_func *root, unsigned int func,
		const char *name, struct nk_vec2 pos, int is_input, int is_output)
{
	struct vil_call *call;
	/* NK_ASSERT((nk_size)func->call_count < NK_LEN(func->call_buf)); */
	int i = root->call_count++;
	call = &root->call_buf[i];
	call->id = i;
	call->bounds.x = pos.x;
	call->bounds.y = pos.y;
	call->bounds.w = 180;
	call->bounds.h = 29;
	call->func = get_func(func);
	call->parent = root;
	call->builtin_data = calloc(1, call->func->builtin_size);
	call->is_input = is_input;
	call->is_output = is_output;
	/* call->input_count = func_count_inputs(call->func); */
	/* call->output_count = 0; */
	/* call->output_count = func_count_outputs(call->func); */

	strcpy(call->name, name);
	func_push(root, call);
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
			if(link->output.val == out_slot.val || link->output.val == out_slot.val)
			{
				*link = func->links[func->link_count - 1];
				break;
			}
		}
	}
	func->link_count--;
	root->input_args[field].size = 0;
}

static void func_link(struct vil_call *root, slot_t in_slot, slot_t out_slot,
		int field)
{
	struct vil_link *link = &root->parent->links[root->parent->link_count++];
	link->input = in_slot;
	link->output = out_slot;
	root->input_args[field] = in_slot;
}

static float call_outputs(struct vil_call *root,
		struct vil_call *call, slot_t parent_slot,
		float x, float y, struct nk_context *ctx)
{
	slot_t slot = parent_slot;
	slot._[slot.size++] = call->id;

	const struct nk_input *in = &ctx->input;
	struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);

	if(call->func->builtin_gui)
	{
		struct nk_rect circle;
		circle.x = x + call->bounds.w-4;
		/* circle.y = call->bounds.y + space * (float)(n+1); */
		circle.y = y + 29.0f + 29.0f / 2.0f;
		circle.w = 8; circle.h = 8;
		nk_fill_circle(canvas, circle, nk_rgb(100, 100, 100));

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
		if (linking.size && linking.val == slot.val)
		{
			struct nk_vec2 l0 = nk_vec2(circle.x + 3, circle.y + 3);
			struct nk_vec2 l1 = in->mouse.pos;
			nk_stroke_curve(canvas, l0.x, l0.y, l0.x + 50.0f, l0.y,
					l1.x - 50.0f, l1.y, l1.x, l1.y, 1.0f, nk_rgb(100, 100, 100));
		}
		y += 29;
	}
	struct vil_call *it = call->func->begin;

	while (it)
	{
		if(it->is_output)
		{
			y = call_outputs(root, it, slot, x, y, ctx);
		}
		it = it->next;
	}
	return y;
}

static int get_call_output_y(struct vil_call *root, struct vil_call *call, float *y,
		slot_t parent_slot, slot_t search)
{
	slot_t slot = parent_slot;
	slot._[slot.size++] = call->id;

	if(call->func->builtin_gui)
	{
		float h = 29.0f;
		if(slot.val == search.val)
		{
			*y += h / 2.0f;
			return 1;
		}
		*y += h;
		return 0;
	}

	struct vil_call *it = call->func->begin;

	while (it)
	{
		if(it->is_output)
		{
			if(get_call_output_y(root, it, y, slot, search))
			{
				return 1;
			}
		}
		it = it->next;
	}
	return 0;
}
static int get_call_input_y(struct vil_call *root, int *field,
		struct vil_call *call, float *y, slot_t parent_slot, slot_t search)
{
	slot_t slot = parent_slot;
	slot._[slot.size++] = call->id;

	int call_field = (*field)++;

	float h = root->input_sizes[call_field];
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

static float call_inputs(struct vil_call *root, int *field,
		struct vil_call *call, float *y, slot_t parent_slot,
		struct nk_context *ctx)
{
	int call_field = (*field)++;

	slot_t slot = parent_slot;
	slot._[slot.size++] = call->id;

	int is_linking = linking.size && linking._[0] != root->id;
	int linking_allowed = is_linking && linking_type == call->func->id;
	int is_linked = root->input_args[call_field].size;

	float call_h = root->input_sizes[call_field];

	if(linking_allowed || is_linked)
	{
		const struct nk_input *in = &ctx->input;
		struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);

		struct nk_rect circle = nk_rect(root->bounds.x + 2,
				*y + call_h / 2.0f, 8, 8);
		nk_fill_circle(canvas, circle, nk_rgb(100, 100, 100));

		struct nk_rect bound = nk_rect(circle.x-5, circle.y-5,
				circle.w+10, circle.h+10);

		if(is_linked)
		{
			if (nk_input_has_mouse_click_down_in_rect(in,
						NK_BUTTON_LEFT, bound, nk_true))
			{
				linking = root->input_args[call_field];
				linking_type = call->func->id;
				func_unlink(root, slot, call_field);
			}
		}

		if (linking_allowed &&
				nk_input_is_mouse_released(in, NK_BUTTON_LEFT) &&
				nk_input_is_mouse_hovering_rect(in, bound))
		{
			if(is_linked) func_unlink(root, slot, call_field);
			func_link(root, linking, slot, call_field);
			linking.size = 0;
		}
	}
	else
	{
		struct vil_call *it;
		for (it = call->func->begin; it; it = it->next)
		{
			if(it->is_input)
			{
				*y += call_inputs(root, field, it, y, slot, ctx);
			}
		}
	}
	return call_h;
}

static float call_gui(struct vil_call *root, int *field,
		struct vil_call *call, void *ctx)
{
	struct vil_call *it;
	float call_h = 0.0f;
	int call_field = (*field)++;

	if(root->input_args[call_field].size) /* input is linked */
	{
		label_gui(call, ctx);
		struct nk_rect rect = nk_layout_widget_bounds(ctx);
		call_h = rect.h;
	}
	else if(call->func->builtin_gui)
	{
		call->func->builtin_gui(call, ctx);
		struct nk_rect rect = nk_layout_widget_bounds(ctx);
		call_h = rect.h;
	}
	else
	{
		for (it = call->func->begin; it; it = it->next)
		{
			if(it->is_input)
			{
				call_h += call_gui(root, field, it, ctx);
			}
		}
	}
	return root->input_sizes[call_field] = call_h;
}

int func_gui(uint func_ref, void *nk)
{
	struct nk_context *ctx = nk;
	struct nk_rect total_space;
	const struct nk_input *in = &ctx->input;
	struct vil_call *updated = 0;
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
			struct vil_call *it = func->begin;
			/* struct nk_rect size = nk_layout_space_bounds(ctx); */
			struct nk_panel *call = 0;

			/* execute each call as a movable group */
			while (it)
			{
				/* calculate scrolled call window position and size */
				nk_layout_space_push(ctx, nk_rect(it->bounds.x - scrolling.x,
							it->bounds.y - scrolling.y, it->bounds.w, 35 + it->bounds.h));

				/* execute call window */
				if (nk_group_begin(ctx, it->name,
							NK_WINDOW_MOVABLE|
							NK_WINDOW_NO_SCROLLBAR|
							NK_WINDOW_BORDER|
							NK_WINDOW_TITLE))
				{
					/* always have last selected call on top */

					call = nk_window_get_panel(ctx);
					if (nk_input_mouse_clicked(in, NK_BUTTON_LEFT, call->bounds) &&
							(!(it->prev && nk_input_mouse_clicked(in, NK_BUTTON_LEFT,
							  nk_layout_space_rect_to_screen(ctx, call->bounds)))) &&
							func->end != it)
					{
						updated = it;
					}

					int field = 0;
					float h = call_gui(it, &field, it, nk);
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
					/* output connector */
					call_outputs(it, it, sl, call->bounds.x, call->bounds.y, ctx);
					/* input connector */
					int field = 0;
					float y = call->bounds.y + 29;
					call_inputs(it, &field, it, &y, sl, ctx);
				}
				it = it->next;
			}

			/* reset linking connection */
			if (linking.size && nk_input_is_mouse_released(in, NK_BUTTON_LEFT))
			{
				linking.size = 0;
				fprintf(stdout, "linking failed\n");
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

			if (updated)
			{
				/* reshuffle calls to have least recently selected call on top */
				func_pop(func, updated);
				func_push(func, updated);
			}

			/* call selection */
			if (nk_input_mouse_clicked(in, NK_BUTTON_LEFT, nk_layout_space_bounds(ctx)))
			{
				it = func->begin;
				selected = NULL;
				/* func->bounds = nk_rect(in->mouse.pos.x, in->mouse.pos.y, 100, 200); */
				while (it)
				{
					struct nk_rect b = nk_layout_space_rect_to_screen(ctx, it->bounds);
					b.x -= scrolling.x;
					b.y -= scrolling.y;
					if (nk_input_is_mouse_hovering_rect(in, b))
						selected = it;
					it = it->next;
				}
			}

			/* contextual menu */
			/* if (nk_contextual_begin(ctx, 0, nk_vec2(100, 220), nk_window_get_bounds(ctx))) */
			/* { */
			/* 	const char *grid_option[] = {"Show Grid", "Hide Grid"}; */
			/* 	nk_layout_row_dynamic(ctx, 25, 1); */
			/* 	if (nk_contextual_item_label(ctx, "New", NK_TEXT_CENTERED)) */
			/* 		func_add_call(func, "New", nk_vec2(400, 260), */
			/* 				nk_rgb(255, 255, 255)); */
			/* 	if (nk_contextual_item_label(ctx, grid_option[show_grid],NK_TEXT_CENTERED)) */
			/* 		show_grid = !show_grid; */
			/* 	nk_contextual_end(ctx); */
			/* } */
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

