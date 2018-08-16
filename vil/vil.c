#include "vil.h"
#include <utils/nk.h>
#include <utils/khash.h>
#include <utils/mafs.h>

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
	vitype_t *type;
	int expanded;
};

struct vil_ret
{
	/* float height = 29; */
	slot_t link;
	vitype_t *type;
};

typedef struct vicall_t
{
	vitype_t *type;
	vitype_t *parent;

	unsigned int id;
	char name[32];
	vicall_t *next;
	vicall_t *prev;

	int collapsable;
	int is_input;
	int is_output;

	struct nk_color color;

	struct nk_rect bounds;

	struct vil_arg input_args[32];
	struct vil_ret output_args[32];

	void *builtin_data;
} vicall_t;

typedef struct vitype_t
{
	char name[32];
	unsigned int id;

	vicall_t call_buf[32];
	struct vil_link links[64];
	vicall_t *begin;
	vicall_t *end;
	unsigned int call_count;
	unsigned int link_count;

	unsigned int		builtin_size;
	vitype_gui_cb		builtin_gui;
	/* vitype_load_cb	builtin_load; */
	/* vitype_save_cb	builtin_save; */
	vil_t *ctx;
} vitype_t;

static slot_t linking;
vicall_t *g_dragging;
vicall_t *g_hovered;
static unsigned int linking_type;
static slot_t g_call_unlink;
static slot_t g_call_link;
static struct nk_vec2 scrolling;


void label_gui(vicall_t *call, void *ctx) 
{
	nk_layout_row_dynamic(ctx, 29, 1);
	nk_label(ctx, call->name, NK_TEXT_LEFT);
}

void vil_context_init(vil_t *self)
{
	self->types = kh_init(vitype);
}

void vil_add_domain(vil_t *self, const char *name, vec4_t color)
{

}

vitype_t *vil_add_type(vil_t *ctx, const char *name,
		vitype_gui_cb builtin_gui, int builtin_size)
{
	int ret = 0;
	int id = ref(name);
	khiter_t k = kh_put(vitype, ctx->types, id, &ret);
	if(ret != 1) exit(1);
	vitype_t **type = &kh_value(ctx->types, k);
	*type = malloc(sizeof(vitype_t));
	**type = (vitype_t){
		.builtin_gui = builtin_gui,
		.builtin_size = builtin_size,
		.id = id,
		.ctx = ctx
	};

	strncpy((*type)->name, name, sizeof((*type)->name));
	/* vicall_link(type, 0, 0, 2, 0); */
	/* vicall_link(type, 1, 0, 2, 1); */

	return *type;
}

static void type_push(vitype_t *type, vicall_t *call)
{
	if (!type->begin) {
		call->next = NULL;
		call->prev = NULL;
		type->begin = call;
		type->end = call;
	} else {
		call->prev = type->end;
		type->end->next = call;
		call->next = NULL;
		type->end = call;
	}
}

static void type_pop(vitype_t *type, vicall_t *call)
{
	if (call->next) call->next->prev = call->prev;
	if (call->prev) call->prev->next = call->next;
	if (type->end == call) type->end = call->prev;
	if (type->begin == call) type->begin = call->next;
	call->next = NULL;
	call->prev = NULL;
}

static vicall_t *get_call(vitype_t *type, int id)
{
	vicall_t *iter = type->begin;
	while (iter) {
		if (iter->id == id)
			return iter;
		iter = iter->next;
	}
	return NULL;
}

vitype_t *vil_get_type(vil_t *ctx, uint ref)
{
	khiter_t k = kh_get(vitype, ctx->types, ref);
	if(k == kh_end(ctx->types))
	{
		printf("Type does not exist\n");
		return NULL;
	}

	return kh_value(ctx->types, k);
}


void vicall_color(vicall_t *self, vec4_t color)
{
	self->color = nk_rgba(
			color.r * 255,
			color.g * 255,
			color.b * 255,
			color.a * 255);
}

vicall_t *vitype_add(vitype_t *parent, vitype_t *type,
		const char *name, vec2_t pos, int flags)
{
	vicall_t *call;
	/* NK_ASSERT((nk_size)type->call_count < NK_LEN(type->call_buf)); */
	unsigned int i = parent->call_count++;
	call = &parent->call_buf[i];
	call->id = i;
	call->bounds.x = pos.x;
	call->bounds.y = pos.y;
	call->bounds.w = 180;
	call->bounds.h = 29;
	call->type = type;
	call->parent = parent;
	call->color = nk_rgb(100, 100, 100);

	if(call->type->builtin_size)
	{
		call->builtin_data = calloc(1, call->type->builtin_size);
	}
	call->collapsable = !!(flags & V_COL);
	call->is_input = !!(flags & V_IN);
	call->is_output = !!(flags & V_OUT);
	/* call->input_count = type_count_inputs(call->type); */
	/* call->output_count = 0; */
	/* call->output_count = type_count_outputs(call->type); */

	strncpy(call->name, name, sizeof(call->name));
	type_push(parent, call);
	return call;
}

static void type_unlink(vicall_t *root, slot_t out_slot,
		int field)
{
	int i;
	vitype_t *type = root->parent;

	if(type->link_count > 1)
	{
		for(i = 0; i < type->link_count; i++)
		{
			struct vil_link *link = &type->links[i];
			if(link->output.val == out_slot.val)
			{
				*link = type->links[type->link_count - 1];
				break;
			}
		}
	}
	type->link_count--;
	root->input_args[field].link.val = 0;
	root->output_args[field].link.val = 0;
}

void vicall_link(vicall_t *root, slot_t in_slot, slot_t out_slot,
		int field)
{
	struct vil_link *link = &root->parent->links[root->parent->link_count++];

	g_call_link = in_slot;

	link->input = in_slot;
	link->output = out_slot;
	root->input_args[field].link = in_slot;
}

static void output_options(vicall_t *root, int *field,
		vicall_t *call, slot_t parent_slot, struct nk_context *nk)
{
	slot_t slot = parent_slot;
	slot._[slot.depth++] = call->id;
	vicall_t *it;
	int call_field = (*field)++;
	if(call->type->builtin_size)
	{
		if(!root->output_args[call_field].link.depth) /* is not linked */
		{
			if (nk_contextual_item_label(nk, call->name, NK_TEXT_CENTERED))
			{
				root->output_args[call_field].link.depth = (unsigned char)-1;
				linking = slot;
				linking_type = call->type->id;
			}
		}
	}
	for (it = call->type->begin; it; it = it->next)
	{
		if(it->is_output)
		{
			output_options(root, field, it, slot, nk);
		}
	}
}

static float call_outputs(vicall_t *root, int *field,
		vicall_t *call, slot_t parent_slot, float y,
		struct nk_context *nk)
{
	int call_field = (*field)++;
	vicall_t *it;
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
	const struct nk_input *in = &nk->input;
	struct nk_command_buffer *canvas = nk_window_get_canvas(nk);

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
			linking_type = call->type->id;
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

	for (it = call->type->begin; it; it = it->next)
	{
		if(it->is_output)
		{
			y = call_outputs(root, field, it, slot, y, nk);
		}
	}
	return y;
}

static int get_call_output_y(vicall_t *root, vicall_t *call, float *y,
		slot_t parent_slot, slot_t search)
{
	vicall_t *it;
	slot_t slot = parent_slot;
	slot._[slot.depth++] = call->id;

	float h = 29.0f;
	if(slot.val == search.val)
	{
		*y += h / 2.0f;
		return 1;
	}

	for (it =  call->type->begin; it; it = it->next)
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
static int get_call_input_y(vicall_t *root, int *field,
		vicall_t *call, float *y, slot_t parent_slot, slot_t search)
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

	if(call->type->builtin_gui)
	{
		*y += h;
		return 0;
	}

	vicall_t *it;

	for(it = call->type->begin; it; it = it->next)
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

struct nk_vec2 get_output_position(vitype_t *type, slot_t search)
{
	vicall_t *call = get_call(type, search._[0]);
	float y = call->bounds.y + 33;
	get_call_output_y(call, call, &y, (slot_t){0}, search);
	return nk_vec2(call->bounds.x + call->bounds.w, y);
}

struct nk_vec2 get_input_position(vitype_t *type, slot_t search)
{
	int field = 0;
	vicall_t *call = get_call(type, search._[0]);
	float y = call->bounds.y + 33;
	get_call_input_y(call, &field, call, &y, (slot_t){0}, search);

	return nk_vec2(call->bounds.x, y);
}

static void call_inputs(vicall_t *root, int *field,
		vicall_t *call, float *y, slot_t parent_slot,
		struct nk_context *nk)
{
	int call_field = (*field)++;

	slot_t slot = parent_slot;
	slot._[slot.depth++] = call->id;

	struct vil_arg *arg = &root->input_args[call_field];

	int is_linking = linking.depth && linking._[0] != root->id;
	int linking_allowed = is_linking && linking_type == call->type->id;
	int is_linked = arg->link.depth;

	float call_h = arg->height;

	if(linking_allowed || is_linked)
	{
		const struct nk_input *in = &nk->input;
		struct nk_command_buffer *canvas = nk_window_get_canvas(nk);

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
				linking_type = call->type->id;
				type_unlink(root, slot, call_field);
				g_call_unlink = arg->link;
			}
		}

		if (linking_allowed &&
				nk_input_is_mouse_released(in, NK_BUTTON_LEFT) &&
				nk_input_is_mouse_hovering_rect(in, bound))
		{
			if(is_linked)
			{
				type_unlink(root, slot, call_field);
				g_call_unlink = arg->link;
			}
			vicall_link(root, linking, slot, call_field);
			linking.val = 0;
		}
	}
	vicall_t *it;
	for (it = call->type->begin; it; it = it->next)
	{
		if(it->is_input)
		{
			call_inputs(root, field, it, y, slot, nk);
		}
	}
	if(call->type->builtin_gui) *y += call_h;
}

static float call_gui(vicall_t *root, int *field,
		char **inherited_data, vicall_t *call, void *nk)
{
	vicall_t *it;
	float call_h = 0.0f;
	int call_field = (*field)++;
	char **data = (char**)&root->input_args[call_field].data;

	if(root->input_args[call_field].link.depth) /* input is linked */
	{
		label_gui(call, nk);
		struct nk_rect rect = nk_layout_widget_bounds(nk);
		call_h = rect.h;
		return root->input_args[call_field].height = call_h;
	}
	if(*data == NULL)
	{
		if(*inherited_data)
		{
			*data = *inherited_data;
			*inherited_data += call->type->builtin_size;
		}
		else if(call->type->builtin_size)
		{
			*data = calloc(1, call->type->builtin_size);
		}
	}

	char *current_data = *data;

	if(call->type->builtin_gui)
	{
		call->type->builtin_gui(call, *data, nk);
		struct nk_rect rect = nk_layout_widget_bounds(nk);
		call_h = rect.h;
	}
	else
	{
		for (it = call->type->begin; it; it = it->next)
		{
			if(it->is_input)
			{
				if(*data)
				{
					if(current_data >= (*data) + call->type->builtin_size)
					{
						current_data = *data;
					}
				}

				call_h += call_gui(root, field, &current_data, it, nk);
			}
		}
	}
	return root->input_args[call_field].height = call_h;
}

const char *vicall_name(const vicall_t *self)
{
	return self->name;
}

int vitype_gui(vitype_t *type, void *nk)
{
	struct nk_rect total_space;
	const struct nk_input *in = &(((struct nk_context *)nk)->input);
	vicall_t *hovered = NULL;
	vicall_t *dragging = NULL;
	vil_t *ctx = type->ctx;

	if (nk_begin(nk, type->name, nk_rect(0, 0, 800, 600),
				NK_WINDOW_BORDER |
				NK_WINDOW_NO_SCROLLBAR |
				NK_WINDOW_MOVABLE |
				NK_WINDOW_CLOSABLE))
	{
		/* allocate complete window space */
		total_space = nk_window_get_content_region(nk);
		nk_layout_space_begin(nk, NK_STATIC, total_space.h, type->call_count);
		{
			vicall_t *it;
			/* struct nk_rect size = nk_layout_space_bounds(nk); */
			struct nk_panel *call = 0;

			/* execute each call as a movable group */
			for (it = type->begin; it; it = it->next)
			{
				/* calculate scrolled call window position and size */
				nk_layout_space_push(nk, nk_rect(it->bounds.x - scrolling.x,
							it->bounds.y - scrolling.y, it->bounds.w, 35 + it->bounds.h));

				/* execute call window */
				if (nk_group_begin(nk, it->name,
							NK_WINDOW_NO_SCROLLBAR|
							NK_WINDOW_BORDER|
							NK_WINDOW_TITLE))
				{
					/* always have last selected call on top */

					call = nk_window_get_panel(nk);
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
					/* nk_layout_row_begin(nk, NK_DYNAMIC, 25, 2); */
					/* nk_layout_row_push(nk, 0.50); */
					/* nk_label(nk, "input", NK_TEXT_LEFT); */
					/* nk_layout_row_push(nk, 0.50); */
					/* nk_label(nk, "output", NK_TEXT_RIGHT); */
					/* nk_layout_row_end(nk); */

					/* nk_layout_row_begin(nk, NK_DYNAMIC, 25, 2); */
					/* nk_layout_row_push(nk, 0.50); */
					/* nk_label(nk, "input", NK_TEXT_LEFT); */
					/* nk_layout_row_push(nk, 0.50); */
					/* nk_label(nk, "output", NK_TEXT_RIGHT); */
					/* nk_layout_row_end(nk); */
					/* contextual menu */
					if(g_hovered == it)
					{
						if (nk_contextual_begin(nk, 0, nk_vec2(100, 220), bd))
						{
							nk_layout_row_dynamic(nk, 25, 1);
							int field = 0;
							slot_t sl = {0};
							output_options(it, &field, it, sl, nk);
							nk_contextual_end(nk);
						}
					}
					nk_group_end(nk);
				}
				{
					/* call connector and linking */
					struct nk_rect bounds;
					bounds = nk_layout_space_rect_to_local(nk, call->bounds);
					bounds.x += scrolling.x;
					bounds.y += scrolling.y;
					bounds.h = it->bounds.h;
					it->bounds = bounds;


					slot_t sl = {0};
					int field = 0;
					/* output connector */
					call_outputs(it, &field, it, sl, call->bounds.y + 29, nk);
					/* input connector */
					field = 0;
					float y = call->bounds.y + 29;
					call_inputs(it, &field, it, &y, sl, nk);
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
			struct nk_command_buffer *canvas = nk_window_get_canvas(nk);
			for (n = 0; n < type->link_count; ++n)
			{
				struct vil_link *link = &type->links[n];

				struct nk_vec2 pi = get_input_position(type, link->output);
				struct nk_vec2 po = get_output_position(type, link->input);

				struct nk_vec2 l0 = nk_layout_space_to_screen(nk, po);
				struct nk_vec2 l1 = nk_layout_space_to_screen(nk, pi);

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
			else
			{
			}

			if(dragging)
			{
				g_dragging = dragging;
				if (dragging->next)
				{
					/* reshuffle calls to have least recently selected call on top */
					type_pop(type, g_dragging);
					type_push(type, g_dragging);
				}
			}

		}
		nk_layout_space_end(nk);

		if(!hovered)
		{
			if (nk_contextual_begin(nk, 0, nk_vec2(150, 300), nk_window_get_bounds(nk)))
			{
				nk_layout_row_dynamic(nk, 25, 1);
				khiter_t k;
				for(k = kh_begin(ctx->types); k != kh_end(ctx->types); ++k)
				{
					if(!kh_exist(ctx->types, k)) continue;
					vitype_t *t = kh_value(ctx->types, k);
					if(t->name[0] == '_') continue;
					if(nk_contextual_item_label(nk, t->name, NK_TEXT_CENTERED))
					{
						vitype_add(type, t, "anon", vec2(in->mouse.pos.x -
									scrolling.x, in->mouse.pos.y -
									scrolling.y), 0);
					}

				}
				nk_contextual_end(nk);
			}
		}

		/* window content scrolling */
		if (nk_input_is_mouse_hovering_rect(in, nk_window_get_bounds(nk)) &&
				nk_input_is_mouse_down(in, NK_BUTTON_MIDDLE))
		{
			scrolling.x += in->mouse.delta.x;
			scrolling.y += in->mouse.delta.y;
		}
	}
	nk_end(nk);
	return !nk_window_is_closed(nk, type->name);
}

