#include "vil.h"
#include <utils/nk.h>
#include <utils/khash.h>

struct vil_node; /* vil_type with display data */
struct vil_type; /* vil_type with display data */

typedef void(*vil_type_gui_cb)(struct vil_node *node, void *ctx);
static struct vil_type *add_type(const char *name,
		vil_type_gui_cb builtin_gui, int builtin_size);
static void func_add_node(struct vil_type *func, unsigned int type,
		const char *name, struct nk_vec2 pos, int is_input, int is_output);
static void func_link(struct vil_type *type, int in_id, int in_slot,
		int out_id, int out_slot);

struct vil_link
{
	int input_id;
	int input_slot;
	int output_id;
	int output_slot;
	struct nk_vec2 in;
	struct nk_vec2 out;
};

struct node_linking
{
	int active;
	struct vil_node *node;
	int input_id;
	int input_slot;
};

struct vil_node
{
	struct vil_type *type;

	int id;
	char name[32];
	struct vil_node *next;
	struct vil_node *prev;

	int is_input;
	int is_output;

	struct nk_rect bounds;

	void *builtin_data;
};

struct vil_type
{
	char name[32];

	struct vil_node node_buf[32];
	struct vil_link links[64];
	struct vil_node *begin;
	struct vil_node *end;
	int node_count;
	int link_count;

	unsigned int		builtin_size;
	vil_type_gui_cb		builtin_gui;
	/* vil_type_load_cb	builtin_load; */
	/* vil_type_save_cb	builtin_save; */
};

KHASH_MAP_INIT_INT(vil_type, struct vil_type)

static struct node_linking linking;
static struct nk_vec2 scrolling;
static int show_grid;
static struct vil_node *selected;

static khash_t(vil_type) *g_types;

void number_gui(struct vil_node *node, void *ctx) 
{
	int *num = node->builtin_data;
	if(*num < 29) *num = 29;
	nk_layout_row_dynamic(ctx, *num, 1);
	*num = (nk_byte)nk_propertyi(ctx, "#N:", 0, *num, 255, 1, 1);
}

REG() {
	g_types = kh_init(vil_type);

	show_grid = nk_true;

	add_type("number", (vil_type_gui_cb)number_gui, sizeof(float));

	struct vil_type *func = add_type("test", NULL, 0);

	func_add_node(func, ref("number"), "A", nk_vec2(40, 10), 1, 0);
	func_add_node(func, ref("number"), "B", nk_vec2(40, 260), 1, 0);
	func_add_node(func, ref("number"), "C", nk_vec2(200, 260), 0, 1);

	func = add_type("parent", NULL, 0);

	func_add_node(func, ref("number"), "num", nk_vec2(40, 10), 0, 0);
	func_add_node(func, ref("test"), "sum", nk_vec2(40, 260), 0, 0);
}

static struct vil_type *add_type(const char *name,
		vil_type_gui_cb builtin_gui, int builtin_size)
{
	int ret;
	khiter_t k = kh_put(vil_type, g_types, ref(name), &ret);
	struct vil_type *type = &kh_value(g_types, k);
	strncpy(type->name, name, sizeof(type->name));
	type->builtin_gui = builtin_gui;
	type->builtin_size = builtin_size;
	type->begin = NULL;
	type->end = NULL;

	/* func_link(type, 0, 0, 2, 0); */
	/* func_link(type, 1, 0, 2, 1); */

	return type;
}

static void func_push(struct vil_type *type, struct vil_node *node)
{
	if (!type->begin) {
		node->next = NULL;
		node->prev = NULL;
		type->begin = node;
		type->end = node;
	} else {
		node->prev = type->end;
		if (type->end)
			type->end->next = node;
		node->next = NULL;
		type->end = node;
	}
}

static void func_pop(struct vil_type *type, struct vil_node *node)
{
	if (node->next)
		node->next->prev = node->prev;
	if (node->prev)
		node->prev->next = node->next;
	if (type->end == node)
		type->end = node->prev;
	if (type->begin == node)
		type->begin = node->next;
	node->next = NULL;
	node->prev = NULL;
}

static struct vil_node *func_find(struct vil_type *type, int id)
{
	struct vil_node *iter = type->begin;
	while (iter) {
		if (iter->id == id)
			return iter;
		iter = iter->next;
	}
	return NULL;
}

/* static int type_count_outputs(struct vil_type *type) */
/* { */
/* 	int count = 0; */
/* 	struct vil_node *it = type->begin; */
/* 	if(type->builtin_gui) count++; */
/* 	while (it) */
/* 	{ */
/* 		if(it->is_output) count++; */
/* 		it = it->next; */
/* 	} */
/* 	return count; */
/* } */

/* static int type_count_inputs(struct vil_type *type) */
/* { */
/* 	int count = 0; */
/* 	struct vil_node *it = type->begin; */
/* 	if(type->builtin_gui) count++; */
/* 	while (it) */
/* 	{ */
/* 		if(it->is_input) count++; */
/* 		it = it->next; */
/* 	} */
/* 	return count; */
/* } */

static struct vil_type *get_type(uint ref)
{
	khiter_t k = kh_get(vil_type, g_types, ref);
	if(k == kh_end(g_types))
	{
		printf("Type does not exist\n");
		return NULL;
	}

	return &kh_value(g_types, k);
}


static void func_add_node(struct vil_type *func, unsigned int type,
		const char *name, struct nk_vec2 pos, int is_input, int is_output)
{
	struct vil_node *node;
	/* NK_ASSERT((nk_size)func->node_count < NK_LEN(func->node_buf)); */
	int i = func->node_count++;
	node = &func->node_buf[i];
	node->id = i;
	node->bounds.x = pos.x;
	node->bounds.y = pos.y;
	node->bounds.w = 180;
	node->bounds.h = 29;
	node->type = get_type(type);
	node->builtin_data = calloc(1, node->type->builtin_size);
	node->is_input = is_input;
	node->is_output = is_output;
	/* node->input_count = type_count_inputs(node->type); */
	/* node->output_count = 0; */
	/* node->output_count = type_count_outputs(node->type); */

	strcpy(node->name, name);
	func_push(func, node);
}

static void func_link(struct vil_type *func, int in_id, int in_slot,
		int out_id, int out_slot)
{
	struct vil_link *link;
	/* NK_ASSERT((nk_size)func->link_count < NK_LEN(func->links)); */
	link = &func->links[func->link_count++];
	link->input_id = in_id;
	link->input_slot = in_slot;
	link->output_id = out_id;
	link->output_slot = out_slot;
}

static float node_inputs(struct vil_type *root, struct vil_node *node,
		float x, float y, struct nk_context *ctx)
{
	const struct nk_input *in = &ctx->input;
	struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);

	int n;
	if(node->type->builtin_gui)
	{
		struct nk_rect circle;
		circle.x = x - 4;
		circle.y = 29 + y + node->bounds.h / 2.0f;
		circle.w = 8; circle.h = 8;
		nk_fill_circle(canvas, circle, nk_rgb(100, 100, 100));
		if (nk_input_is_mouse_released(in, NK_BUTTON_LEFT) &&
				nk_input_is_mouse_hovering_rect(in, circle) &&
				linking.active && linking.node != node)
		{
			linking.active = nk_false;
			func_link(root, linking.input_id,
					linking.input_slot, node->id, n);
		}
		y += node->bounds.h;
	}
	struct vil_node *it = node->type->begin;
	while (it)
	{
		if(it->is_input)
		{
			y = node_inputs(root, it, x, y, ctx);
		}
		it = it->next;
	}
	return y;
}

static float node_gui(struct vil_node *node, void *ctx)
{
	node->bounds.h = 0.0f;
	if(node->type->builtin_gui)
	{
		node->type->builtin_gui(node, ctx);
		struct nk_rect rect = nk_layout_widget_bounds(ctx);
		node->bounds.h += rect.h;
	}
	struct vil_node *it = node->type->begin;
	while (it)
	{
		if(it->is_input)
		{
			node->bounds.h += node_gui(it, ctx);
		}
		it = it->next;
	}
	return node->bounds.h;
}

int node_editorf(uint type_ref, void *nk)
{
	struct nk_context *ctx = nk;
	int n = 0;
	struct nk_rect total_space;
	const struct nk_input *in = &ctx->input;
	struct nk_command_buffer *canvas;
	struct vil_node *updated = 0;
	struct vil_type *type = get_type(type_ref);


	if (nk_begin(ctx, type->name, nk_rect(0, 0, 800, 600),
				NK_WINDOW_BORDER|NK_WINDOW_NO_SCROLLBAR|NK_WINDOW_MOVABLE|NK_WINDOW_CLOSABLE))
	{
		/* allocate complete window space */
		canvas = nk_window_get_canvas(ctx);
		total_space = nk_window_get_content_region(ctx);
		nk_layout_space_begin(ctx, NK_STATIC, total_space.h, type->node_count);
		{
			struct vil_node *it = type->begin;
			/* struct nk_rect size = nk_layout_space_bounds(ctx); */
			struct nk_panel *node = 0;

			/* execute each node as a movable group */
			while (it)
			{
				/* calculate scrolled node window position and size */
				nk_layout_space_push(ctx, nk_rect(it->bounds.x - scrolling.x,
							it->bounds.y - scrolling.y, it->bounds.w, 35 + it->bounds.h));

				/* execute node window */
				if (nk_group_begin(ctx, it->name,
							NK_WINDOW_MOVABLE|
							NK_WINDOW_NO_SCROLLBAR|
							NK_WINDOW_BORDER|
							NK_WINDOW_TITLE))
				{
					/* always have last selected node on top */

					node = nk_window_get_panel(ctx);
					if (nk_input_mouse_clicked(in, NK_BUTTON_LEFT, node->bounds) &&
							(!(it->prev && nk_input_mouse_clicked(in, NK_BUTTON_LEFT,
							  nk_layout_space_rect_to_screen(ctx, node->bounds)))) &&
							type->end != it)
					{
						updated = it;
					}

					node_gui(it, nk);
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
					/* node connector and linking */
					struct nk_rect bounds;
					bounds = nk_layout_space_rect_to_local(ctx, node->bounds);
					bounds.x += scrolling.x;
					bounds.y += scrolling.y;
					bounds.h = it->bounds.h;
					it->bounds = bounds;

					/* output connector */
					/* for (n = 0; n < it->output_count; ++n) */
					/* { */
					/* 	struct nk_rect circle; */
					/* 	circle.x = node->bounds.x + node->bounds.w-4; */
					/* 	/1* circle.y = node->bounds.y + space * (float)(n+1); *1/ */
					/* 	circle.y = 24.0f/2 + node->bounds.y + 29.0f * (float)(n+1); */
					/* 	circle.w = 8; circle.h = 8; */
					/* 	nk_fill_circle(canvas, circle, nk_rgb(100, 100, 100)); */

					/* 	/1* start linking process *1/ */
					/* 	if (nk_input_has_mouse_click_down_in_rect(in, */
					/* 				NK_BUTTON_LEFT, circle, nk_true)) */
					/* 	{ */
					/* 		linking.active = nk_true; */
					/* 		linking.node = it; */
					/* 		linking.input_id = it->id; */
					/* 		linking.input_slot = n; */
					/* 	} */

					/* 	/1* draw curve from linked node slot to mouse position *1/ */
					/* 	if (linking.active && linking.node == it && */
					/* 			linking.input_slot == n) */
					/* 	{ */
					/* 		struct nk_vec2 l0 = nk_vec2(circle.x + 3, circle.y + 3); */
					/* 		struct nk_vec2 l1 = in->mouse.pos; */
					/* 		nk_stroke_curve(canvas, l0.x, l0.y, l0.x + 50.0f, l0.y, */
					/* 				l1.x - 50.0f, l1.y, l1.x, l1.y, 1.0f, nk_rgb(100, 100, 100)); */
					/* 	} */
					/* } */

					/* input connector */
					node_inputs(type, it, node->bounds.x, node->bounds.y, ctx);
				}
				it = it->next;
			}

			/* reset linking connection */
			if (linking.active && nk_input_is_mouse_released(in, NK_BUTTON_LEFT))
			{
				linking.active = nk_false;
				linking.node = NULL;
				fprintf(stdout, "linking failed\n");
			}

			/* draw each link */
			for (n = 0; n < type->link_count; ++n)
			{
				struct vil_link *link = &type->links[n];
				struct vil_node *ni = func_find(type, link->input_id);
				struct vil_node *no = func_find(type, link->output_id);
				struct nk_vec2 l0 = nk_layout_space_to_screen(ctx,
						nk_vec2(ni->bounds.x + ni->bounds.w, 24.0f/2 + 3.0f + ni->bounds.y +
							29.0f * (float)(link->input_slot+1)));
				struct nk_vec2 l1 = nk_layout_space_to_screen(ctx,
						nk_vec2(no->bounds.x, 24.0f/2 + 3.0f + no->bounds.y + 29.0f *
							(float)(link->output_slot+1)));

				l0.x -= scrolling.x;
				l0.y -= scrolling.y;
				l1.x -= scrolling.x;
				l1.y -= scrolling.y;
				nk_stroke_curve(canvas, l0.x, l0.y, l0.x + 50.0f, l0.y,
						l1.x - 50.0f, l1.y, l1.x, l1.y, 1.0f, nk_rgb(100, 100, 100));
			}

			if (updated)
			{
				/* reshuffle nodes to have least recently selected node on top */
				func_pop(type, updated);
				func_push(type, updated);
			}

			/* node selection */
			if (nk_input_mouse_clicked(in, NK_BUTTON_LEFT, nk_layout_space_bounds(ctx)))
			{
				it = type->begin;
				selected = NULL;
				/* type->bounds = nk_rect(in->mouse.pos.x, in->mouse.pos.y, 100, 200); */
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
			/* 		func_add_node(type, "New", nk_vec2(400, 260), */
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
	return !nk_window_is_closed(ctx, type->name);
}

