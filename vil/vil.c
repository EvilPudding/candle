#include "vil.h"
#include <assert.h>
#include <systems/editmode.h>
#include <utils/khash.h>
#include <utils/mafs.h>

static slot_t linking;
vicall_t *g_dragging;
vicall_t *g_renaming;
bool_t g_create_dialogue;
char g_create_buffer[64];

vicall_t *g_over_context;
vicall_t *g_over_sub_context;
vicall_t *g_over_sub_context_parent;

vicall_t *g_open_context;
vicall_t *g_open_sub_context;

bool_t g_open_global_context;

static vec2_t g_context_pos;
static uint32_t linking_type;
static slot_t g_call_link;
static struct nk_vec2 scrolling;

void label_gui(vicall_t *call, void *ctx) 
{
	nk_layout_row_dynamic(ctx, 20, 1);
	nk_label(ctx, call->name, NK_TEXT_LEFT);
}

void vil_init(vil_t *self)
{
	self->funcs = kh_init(vifunc);
	self->user_data = NULL;
}

struct vil_ret *vicall_get_ret(vicall_t *call, slot_t slot)
{
	for (uint32_t i = 0; i < 128; i++)
	{
		if (   call->output_args[i].from.val == slot.val
		    || call->output_args[i].from.depth == 0)
		{
			call->output_args[i].from = slot;
			return &call->output_args[i];
		}
	}
	assert(false);
}

struct vil_arg *vicall_get_arg(vicall_t *call, slot_t slot)
{
	for (uint32_t i = 0; i < 128; i++)
	{
		if (   call->input_args[i].into.val == slot.val
		    || call->input_args[i].into.depth == 0)
		{
			call->input_args[i].into = slot;
			return &call->input_args[i];
		}
	}
	assert(false);
}

void _vicall_save(vicall_t *self, FILE *fp)
{
	fwrite(&self->type->id, sizeof(self->type->id), 1, fp);
	fwrite(&self->name, sizeof(self->name), 1, fp);
	fwrite(&self->bounds.x, sizeof(self->bounds.x), 1, fp);
	fwrite(&self->bounds.y, sizeof(self->bounds.y), 1, fp);
	fwrite(&self->color, sizeof(self->color), 1, fp);
	fwrite(&self->is_input, sizeof(self->is_input), 1, fp);
	fwrite(&self->is_output, sizeof(self->is_output), 1, fp);
	fwrite(&self->is_linked, sizeof(self->is_linked), 1, fp);
	fwrite(&self->is_hidden, sizeof(self->is_hidden), 1, fp);
	fwrite(&self->data_offset, sizeof(self->data_offset), 1, fp);

	if (self->is_input)
	{
		for (uint32_t a = 0; a < 128; a++)
		{
			struct vil_arg *arg = &self->input_args[a];
			fwrite(&arg->into.val, sizeof(arg->into.val), 1, fp);
			fwrite(&arg->data_size, sizeof(arg->data_size), 1, fp);
			fwrite(&arg->y, sizeof(arg->y), 1, fp);
			if (arg->into.depth == 0) break;

			if (arg->data_size)
				fwrite(arg->data, arg->data_size, 1, fp);
		}
	}
}

bool_t _vicall_load(vifunc_t *parent, FILE *fp)
{
	uint32_t color;
	uint32_t typeid;

	int ret = fread(&typeid, sizeof(typeid), 1, fp);
	if (ret < 0) return false;

	char name[64];
	ret = fread(&name, sizeof(name), 1, fp);
	if (ret < 0) return false;

	vifunc_t *type = vil_get(parent->ctx, typeid);
	assert(type);

	vec2_t pos;
	ret = fread(&pos.x, sizeof(pos.x), 1, fp);
	if (ret < 0) return false;
	ret = fread(&pos.y, sizeof(pos.y), 1, fp);
	if (ret < 0) return false;

	uint32_t is_input, is_output, is_linked, is_hidden, data_offset;
	ret = fread(&color, sizeof(color), 1, fp);
	if (ret < 0) return false;

	ret = fread(&is_input, sizeof(is_input), 1, fp);
	if (ret < 0) return false;
	ret = fread(&is_output, sizeof(is_output), 1, fp);
	if (ret < 0) return false;
	ret = fread(&is_linked, sizeof(is_linked), 1, fp);
	if (ret < 0) return false;
	ret = fread(&is_hidden, sizeof(is_hidden), 1, fp);
	if (ret < 0) return false;
	ret = fread(&data_offset, sizeof(data_offset), 1, fp);
	if (ret < 0) return false;

	uint32_t flags =   is_linked * V_LINKED | is_hidden * V_HID
	                 | is_output * V_OUT | is_input * V_IN;

	vicall_t *self = vicall_new(parent, type, name, pos, data_offset, flags);
	self->color = color;

	if (self->is_input)
	{
		for (uint32_t a = 0; a < 128; a++)
		{
			slot_t slot;
			uint32_t data_size;
			float y;
			ret = fread(&slot.val, sizeof(slot.val), 1, fp);
			if (ret < 0) return false;
			ret = fread(&data_size, sizeof(data_size), 1, fp);
			if (ret < 0) return false;
			ret = fread(&y, sizeof(y), 1, fp);
			if (ret < 0) return false;

			if (slot.depth == 0) break;
			struct vil_arg *arg = vicall_get_arg(self, slot);
			arg->y = y;
			assert(arg->data_size == data_size);

			if (arg->data_size)
			{
				ret = fread(arg->data, arg->data_size, 1, fp);
				if (ret < 0) return false;
			}
		}
	}

	return true;
}

void vifunc_save(vifunc_t *self, const char *filename)
{
	FILE *fp = fopen(filename, "w");
	if (!fp) return;
	fwrite(&self->call_count, sizeof(self->call_count), 1, fp);
	fwrite(&self->link_count, sizeof(self->link_count), 1, fp);

	for (uint32_t i = 0; i < self->call_count; i++)
	{
		vicall_t *call = &self->call_buf[i];
		_vicall_save(call, fp);
	}

	for (uint32_t i = 0; i < self->link_count; i++)
	{
		struct vil_link *link = &self->links[i];
		fwrite(&link->from.val, sizeof(link->from.val), 1, fp);
		fwrite(&link->into.val, sizeof(link->into.val), 1, fp);
	}
}

void vicall_set_arg(vicall_t *call, uint32_t ref, void *value)
{
	for (uint32_t i = 0; i < 128; i++)
	{
		char buffer[256];
		struct vil_arg *arg = &call->input_args[i];
		vifunc_slot_to_name(call->parent, arg->into, buffer, ".", NULL);
		uint32_t arg_ref = ref(buffer);
		if (ref == arg_ref)
		{
			memcpy(arg->data, value, arg->data_size);
			break;
		}
	}
}

static void type_push(vifunc_t *type, vicall_t *call)
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

bool_t vicall_is_linked(vifunc_t *root, slot_t slot)
{
	slot_t slt = slot;
	for (uint8_t depth = 1; depth < slot.depth; depth++) {

		for (uint32_t i = 1; i < 7; i++)
			slt._[i - 1] = slt._[i];
		slt.depth--;
		slt._[slt.depth] = 0;

		for (uint32_t i = 0; i < root->link_count; i++) {
			if (root->links[i].into.val == slt.val) {
				return true;
			}
		}

		root = root->call_buf[slot._[depth]].type;
	}
	return false;
}

void _vifunc_foreach_call(vifunc_t *self, bool_t recursive,
                          slot_t parent_slot,
                          bool_t allow_input, bool_t allow_output,
                          bool_t skip_linked, vil_call_cb callback, void *usrptr)
{
	for (vicall_t *iter = self->begin; iter; iter = iter->next)
	{
		slot_t slot = parent_slot;
		slot._[slot.depth++] = iter->id;

		if (   (iter->is_input  && !allow_input)
		    || (iter->is_output && !allow_output)) continue;

		if (skip_linked)
		{
			if (   iter->is_linked
			    || (iter->is_input && vicall_is_linked(self, slot)))
			{
				return;
			}
		}

		if (!recursive || iter->type->builtin_gui)
		{
			callback(iter, slot, usrptr);
		}

		if (recursive)
		{
			vifunc_foreach_call(iter->type, recursive, allow_input,
			                    allow_output, skip_linked, callback, usrptr);
		}
	}
}

void vifunc_foreach_call(vifunc_t *self, bool_t recursive,
                         bool_t allow_input, bool_t allow_output,
                         bool_t skip_linked, vil_call_cb callback, void *usrptr)
{
	_vifunc_foreach_call(self, recursive, (slot_t){0}, allow_input, allow_output,
	                     skip_linked, callback, usrptr);
}

static vicall_t *get_call(vifunc_t *type, uint32_t id)
{
	for (vicall_t *iter = type->begin; iter; iter = iter->next)
	{
		if (iter->id == id) return iter;
	}
	return NULL;
}

static void vicall_propagate_changed_aux0(vicall_t *self);
static void vicall_propagate_changed_aux1(vicall_t *self, bool_t *iterated)
{

	iterated[self->id] = true;
	if (self->watcher && !self->parent->locked)
	{
		self->watcher(self, (slot_t){.depth = 1, ._={self->id}},
		              self->watcher_usrptr);
	}
	for (uint32_t i = 0; i < 128; i++)
	{
		struct vil_ret *output = &self->output_args[i];
		if (output->into.val == 0) continue;
		vicall_t *input = get_call(self->parent, output->into._[0]);
		if (!input) break;

		if (!iterated[input->id])
		{
			vicall_propagate_changed_aux1(input, iterated);
		}
	}
}

static void vicall_propagate_changed_aux0(vicall_t *self)
{
	bool_t *iterated = alloca(sizeof(bool_t) * self->parent->call_count);
	memset(iterated, false, sizeof(bool_t) * self->parent->call_count);
	vicall_propagate_changed_aux1(self, iterated);
}

static void vicall_propagate_changed(vicall_t *self)
{
	vil_t *ctx = self->type->ctx;
	vicall_propagate_changed_aux0(self);

	for(khiter_t k = kh_begin(ctx->funcs); k != kh_end(ctx->funcs); ++k)
	{
		if(!kh_exist(ctx->funcs, k)) continue;
		vifunc_t *func = kh_value(ctx->funcs, k);
		for(vicall_t *it = func->begin; it; it = it->next)
		{
			if (it->type == self->parent)
			{
				vicall_propagate_changed(it);
			}
		}
	}
}

void vifunc_link(vifunc_t *self, slot_t from_slot, slot_t into_slot)
{
	struct vil_link *link = &self->links[self->link_count++];

	g_call_link = from_slot;

	link->from = from_slot;
	link->into = into_slot;

	vicall_t *into = get_call(self, into_slot._[0]);
	vicall_t *from = get_call(self, from_slot._[0]);

	char buffer_to[256];
	char buffer_from[256];
	vifunc_slot_to_name(self, into_slot, buffer_to, ".", "_in");
	vifunc_slot_to_name(self, from_slot, buffer_from, ".", "_out");
	printf("lingknig %s to %s\n", buffer_from, buffer_to);

	struct vil_arg *arg = vicall_get_arg(into, into_slot);
	struct vil_ret *ret = vicall_get_ret(from, from_slot);
	arg->from = from_slot;
	arg->into = into_slot;

	ret->into = into_slot;
	ret->from = from_slot;

	vicall_propagate_changed(into);
}

bool_t vifunc_load(vifunc_t *self, const char *filename)
{
	if (!self) return false;

	FILE *fp = fopen(filename, "r");
	if (!fp) return false;

	uint32_t call_count, link_count;
	int ret = fread(&call_count, sizeof(call_count), 1, fp);
	if (ret < 0) return false;
	ret = fread(&link_count, sizeof(link_count), 1, fp);
	if (ret < 0) return false;

	self->locked++;
	for (uint32_t i = 0; i < call_count; i++)
	{
		_vicall_load(self, fp);
	}

	for (uint32_t i = 0; i < link_count; i++)
	{
		struct vil_link link;
		ret = fread(&link.from.val, sizeof(link.from.val), 1, fp);
		if (ret < 0) return false;
		ret = fread(&link.into.val, sizeof(link.into.val), 1, fp);
		if (ret < 0) return false;

		vifunc_link(self, link.from, link.into);
	}
	self->locked--;

	fclose(fp);
	return true;
}

/* void vil_load(vil_t *self, const char *filename) */
/* { */
/* 	FILE *fp = fopen(filename, "r"); */
/* 	vil_init(self); */
/* 	fclose(fp); */
/* } */

/* void vil_save(vil_t *self, const char *filename) */
/* { */
/* 	FILE *fp = fopen(filename, "w"); */

/* 	uint32_t num_types = kh_size(self->funcs); */
/* 	fprintf(fp, "%u\n", num_types); */

/* 	for (khiter_t k = kh_begin(self->funcs); k != kh_end(self->funcs); ++k) { */
/* 		if(!kh_exist(self->funcs, k)) continue; */
/* 		vifunc_save(kh_value(self->funcs, k), fp); */
/* 	} */
/* 	fclose(fp); */
/* } */

void vil_add_domain(vil_t *self, const char *name, vec4_t color)
{

}

vifunc_t *vifunc_new(vil_t *ctx, const char *name,
		vifunc_gui_cb builtin_gui,
		uint32_t builtin_size, bool_t is_assignable)
{
	if (!ctx->funcs) return NULL;
	int32_t ret = 0;
	uint32_t id = ref(name);
	khiter_t k = kh_put(vifunc, ctx->funcs, id, &ret);
	if(ret != 1) exit(1);
	vifunc_t **type = &kh_value(ctx->funcs, k);
	*type = malloc(sizeof(vifunc_t));
	**type = (vifunc_t){
		.builtin_gui = builtin_gui,
		.builtin_size = builtin_size,
		.id = id,
		.ctx = ctx,
		.is_assignable = is_assignable
	};

	strncpy((*type)->name, name, sizeof((*type)->name));

	return *type;
}

static void type_pop(vifunc_t *type, vicall_t *call)
{
	if (call->next) call->next->prev = call->prev;
	if (call->prev) call->prev->next = call->next;
	if (type->end == call) type->end = call->prev;
	if (type->begin == call) type->begin = call->next;
	call->next = NULL;
	call->prev = NULL;
}

vifunc_t *vil_get(vil_t *ctx, uint32_t ref)
{
	if (!ctx->funcs) return NULL;
	khiter_t k = kh_get(vifunc, ctx->funcs, ref);
	if(k == kh_end(ctx->funcs))
	{
		printf("Type does not exist\n");
		return NULL;
	}

	return kh_value(ctx->funcs, k);
}


void vicall_color(vicall_t *self, vec4_t color)
{
	if (!self) return;
	self->color = nk_color_u32(nk_rgba(
			color.r * 255,
			color.g * 255,
			color.b * 255,
			color.a * 255));
}

slot_t slot_id(uint32_t id)
{
	return (slot_t){.depth = 1, ._ = {id}};
}

void slot_print(slot_t slt)
{
	for (uint32_t i = 0; i < 7; i++)
		if (i < slt.depth)
			printf("%u ", slt._[i]);
		else
			printf("- ");
	puts("");
}
slot_t slot_insert_start(slot_t slt, uint32_t start)
{
	for (uint32_t i = 6; i >= 1; i--)
		slt._[i] = slt._[i - 1];
	slt._[0] = start;
	slt.depth += 1;
	return slt;
}

slot_t slot_pop(slot_t slt)
{
	for (uint32_t i = 1; i < 7; i++)
		slt._[i - 1] = slt._[i];
	slt.depth--;
	slt._[slt.depth] = 0;
	return slt;
}

void propagate_data(vicall_t *root, vicall_t *call, slot_t parent_slot,
                    char *parent_data)
{
	slot_t slot = parent_slot;
	slot._[slot.depth++] = call->id;
	struct vil_arg *arg = vicall_get_arg(root, slot);

	if (call->data_offset != ~0 && parent_data)
	{
		arg->data = parent_data + call->data_offset;
	}
	if (!arg->data && call->type->builtin_size)
	{
		arg->data = calloc(1, call->type->builtin_size);
	}
	arg->data_size = call->type->builtin_size;

	for (vicall_t *it = call->type->begin; it; it = it->next)
	{
		if (!it->is_input || it->is_linked) continue;
		propagate_data(root, it, slot, arg->data);
	}
}

vicall_t *vicall_new(vifunc_t *parent, vifunc_t *type, const char *name,
                     vec2_t pos, uint32_t data_offset, uint32_t flags)
{
	if (!parent || !type) return NULL;
	vicall_t *call;
	/* NK_ASSERT((nk_size)type->call_count < NK_LEN(type->call_buf)); */
	uint32_t i = parent->call_count++;
	call = &parent->call_buf[i];
	call->id = i;
	call->bounds.x = pos.x;
	call->bounds.y = pos.y;
	call->bounds.z = 100;
	call->bounds.w = 20;
	call->type = type;
	call->parent = parent;
	call->color = nk_color_u32(nk_rgb(50, 50, 50));

	call->data_offset = data_offset;
	call->is_input = !!(flags & V_IN);
	call->is_hidden = !!(flags & V_HID);
	call->is_output = !!(flags & V_OUT);
	call->is_linked = !!(flags & V_LINKED);
	/* call->input_count = type_count_inputs(call->type); */
	/* call->output_count = 0; */
	/* call->output_count = type_count_outputs(call->type); */

	strncpy(call->name, name, sizeof(call->name));
	type_push(parent, call);
	if (call->is_linked || !call->is_input) return call;

	propagate_data(call, call, (slot_t){0}, NULL);
	for (vicall_t *it = call->type->begin; it; it = it->next)
	{
		for (uint32_t a = 0; a < 128; a++)
		{
			struct vil_arg *it_arg = &it->input_args[a];
			if (it_arg->into.depth == 0) break;
			slot_t slot = slot_insert_start(it_arg->into, call->id);
			struct vil_arg *call_arg = vicall_get_arg(call, slot);

			assert(!call_arg->data_size || call_arg->data);
			assert(it_arg->data_size == call_arg->data_size);

			if (it_arg->data_size)
				memcpy(call_arg->data, it_arg->data, it_arg->data_size);
		}
	}

	return call;
}

static bool_t get_call_output_info(vicall_t *root, vicall_t *call, float *y,
		slot_t parent_slot, slot_t search)
{
	vicall_t *it;
	slot_t slot = parent_slot;
	slot._[slot.depth++] = call->id;

	bool_t is_primitive = root == call && call->type->is_assignable;
	bool_t is_exposed = call->parent == root->type && !call->parent->is_assignable;

	if (is_primitive)
	{
		if (y) *y -= 20.0f;
	}

	const float h = 20.0f;
	if(slot.val == search.val)
	{
		if (y) *y += h / 2.0f;
		return true;
	}

	struct vil_ret *arg = vicall_get_ret(root, slot);
	if(arg->into.depth || is_primitive || is_exposed) /* is linked */
	{
		if (y) *y += h;
	}

	for (it = call->type->begin; it; it = it->next)
	{
		if(it->is_output)
		{
			if(get_call_output_info(root, it, y, slot, search))
			{
				return true;
			}
		}
	}
	return false;
}

static void vifunc_unlink(vifunc_t *self, slot_t into_slot)
{
	uint32_t i;

	struct vil_link *link = NULL;
	for(i = 0; i < self->link_count; i++)
	{
		link = &self->links[i];
		if (link->into.val == into_slot.val) break;
	}
	if (!link || link->into.val != into_slot.val) return;

	vicall_t *from = get_call(self, link->from._[0]);
	vicall_t *into = get_call(self, link->into._[0]);
	printf("unlinking %s -> %s\n", from->name, into->name);

	struct vil_ret *ret = vicall_get_ret(from, link->from);
	struct vil_arg *arg = vicall_get_arg(into, into_slot);

	arg->from.val = 0;
	ret->into.val = 0;

	self->link_count--;
	if(self->link_count > 0)
	{
		*link = self->links[self->link_count];
	}
	vicall_propagate_changed(into);
}

static void output_options(vicall_t *root,
		vicall_t *call, slot_t parent_slot, struct nk_context *nk)
{
	slot_t slot = parent_slot;
	slot._[slot.depth++] = call->id;
	vicall_t *it;
	struct vil_ret *ret = vicall_get_ret(root, slot);
	if(call != root && call->type->builtin_size && call->type->is_assignable)
	{
		if(!ret->into.depth) /* is not linked */
		{
			if (nk_contextual_item_label(nk, call->name, NK_TEXT_CENTERED))
			{
				ret->into.depth = ~0;
				linking = slot;
				linking_type = call->type->id;
			}
		}
	}
	for (it = call->type->begin; it; it = it->next)
	{
		if(it->is_output)
		{
			output_options(root, it, slot, nk);
		}
	}
}

static float call_outputs(vicall_t *root,
		vicall_t *call, slot_t parent_slot, float y,
		struct nk_context *nk, bool_t exposed)
{

	vicall_t *it;
	slot_t slot = parent_slot;
	slot._[slot.depth++] = call->id;

	struct vil_ret *ret = vicall_get_ret(root, slot);

	if(!linking.depth && ret->into.depth == (unsigned char)-1)
	{
		ret->into.val = 0;
	}
	const struct nk_input *in = &nk->input;
	struct nk_command_buffer *canvas = nk_window_get_canvas(nk);

	bool_t is_primitive = root == call && call->type->is_assignable;
	if (is_primitive)
	{
		y -= 20.0f;
	}

	if(call->type->is_assignable && (ret->into.depth || exposed)) /* is linked */
	{
		struct nk_rect circle;
		circle.x = root->bounds.x + call->bounds.z + 2;
		/* circle.y = call->bounds.y + space * (float)(n+1); */
		circle.y = y + 20.0f / 2.0f;
		circle.w = 8; circle.h = 8;
		nk_fill_circle(canvas, circle, nk_rgba_u32(call->color));

		struct nk_rect bound = nk_rect(circle.x-8, circle.y-8,
				circle.w+16, circle.h+16);

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
					l1.x - 50.0f, l1.y, l1.x, l1.y, 2.0f, nk_rgb(40, 40, 40));
		}
		y += 20;
		circle.y -= 4.0f;
		circle.x += 10.0f;
		circle.w = 100.0f;
		circle.h = 24.0f;
		if (nk_input_is_mouse_hovering_rect(in, bound))
		{
			nk_draw_text(canvas, circle, call->name, strlen(call->name),
						 ((struct nk_context*)nk)->style.font,
						 (struct nk_color){0, 0, 0, 255},
						 (struct nk_color){255, 255, 255, 255});
			g_over_sub_context = call;
			g_over_sub_context_parent = root;
		}
		exposed = false;
	}

	for (it = call->type->begin; it; it = it->next)
	{
		if(it->is_output)
		{
			y = call_outputs(root, it, slot, y, nk, exposed);
		}
	}
	return y;
}

static struct nk_vec2 get_output_position(vifunc_t *type, slot_t search)
{
	vicall_t *call = get_call(type, search._[0]);
	if (!call) return nk_vec2(0.0f, 0.0f);
	float y = call->bounds.y + 25.0f;
	get_call_output_info(call, call, &y, (slot_t){0}, search);
	return nk_vec2(call->bounds.x + call->bounds.z, y);
}

static struct nk_vec2 get_input_position(vifunc_t *type, slot_t search)
{
	vicall_t *call = get_call(type, search._[0]);
	if (!call) return nk_vec2(0.0f, 0.0f);
	struct vil_arg *arg = vicall_get_arg(call, search);
	if (!arg) return nk_vec2(0.0f, 0.0f);

	return nk_vec2(call->bounds.x, arg->y);
}

static void call_inputs(vicall_t *root,
		vicall_t *call, slot_t parent_slot,
		struct nk_context *nk, bool_t inherit_hidden)
{


	slot_t slot = parent_slot;
	slot._[slot.depth++] = call->id;

	struct vil_arg *arg = vicall_get_arg(root, slot);

	uint32_t is_linking = linking.depth && linking._[0] != root->id;
	uint32_t linking_allowed = is_linking && linking_type == call->type->id;

	uint32_t is_linked =  arg->from.depth;

	float y = arg->y + 55.0f;

	inherit_hidden |= call->is_hidden;
	if((linking_allowed || is_linked) && !inherit_hidden)
	{
		const struct nk_input *in = &nk->input;
		struct nk_command_buffer *canvas = nk_window_get_canvas(nk);

		struct nk_rect circle = nk_rect(root->bounds.x + 2,
				y, 8, 8);
		nk_fill_circle(canvas, circle, nk_rgba_u32(call->color));

		struct nk_rect bound = nk_rect(circle.x-5, circle.y-5,
				circle.w+10, circle.h+10);

		if(is_linked)
		{
			if (nk_input_has_mouse_click_down_in_rect(in,
						NK_BUTTON_LEFT, bound, nk_true))
			{
				linking = arg->from;
				linking_type = call->type->id;
				vifunc_unlink(root->parent, slot);
				is_linked = false;
				return;
			}
		}

		if (linking_allowed &&
				nk_input_is_mouse_released(in, NK_BUTTON_LEFT) &&
				nk_input_is_mouse_hovering_rect(in, bound))
		{
			if(is_linked)
			{
				vifunc_unlink(root->parent, slot);
			}
			vifunc_link(root->parent, linking, slot);
			linking.val = 0;
		}
		if (is_linked) return;
	}
	if (root != call && vicall_is_linked(root->type, slot)) return;

	if (!call->type->builtin_gui)
	{
		for (vicall_t *it = call->type->begin; it; it = it->next)
		{
			if (!it->is_input) continue;
			call_inputs(root, it, slot, nk, inherit_hidden);
		}
	}
}

void _vicall_iterate_dependencies(vicall_t *self, bool_t *iterated,
                                  vil_link_cb link,
                                  vil_call_cb call,
                                  void *usrptr)
{
	iterated[self->id] = true;
	for (uint32_t i = 0; i < 128; i++)
	{
		struct vil_arg *input = &self->input_args[i];
		if (input->from.val == 0) continue;
		vicall_t *output = get_call(self->parent, input->from._[0]);
		if (!iterated[output->id])
		{
			_vicall_iterate_dependencies(output, iterated, link, call, usrptr);
		}
		if (link) link(self->parent, input->into, input->from, usrptr);
	}
	if (call) call(self, (slot_t){.depth = 1, ._ = {self->id}}, usrptr);
}

void vicall_iterate_dependencies(vicall_t *self, vil_link_cb link,
                                 vil_call_cb call, void *usrptr)
{
	bool_t *iterated = alloca(sizeof(bool_t) * self->parent->call_count);
	memset(iterated, false, sizeof(bool_t) * self->parent->call_count);

	_vicall_iterate_dependencies(self, iterated, link, call, usrptr);
}

void vifunc_slot_to_name(vifunc_t *func, slot_t slot, char *buffer,
                         const char *separator, const char *append)
{
	buffer[0] = '\0';
	vicall_t *call = NULL;
	for (uint32_t i = 0; i < slot.depth; i++)
	{
		call = get_call(func, slot._[i]);
		strcat(buffer, call->name);
		if (i == 0 && append) strcat(buffer, append);
		if (i < slot.depth - 1) strcat(buffer, separator);
		func = call->type;
	}
}

void _vicall_foreach_unlinked_input(vicall_t *root,
                                     slot_t parent_slot,
									 char *parent_data, vicall_t *call,
									 vil_foreach_input_cb cb,
									 void *usrptr)
{

	slot_t slot = parent_slot;
	slot._[slot.depth++] = call->id;

	char *data = parent_data;
	struct vil_arg *arg = vicall_get_arg(root, slot);
	
	/* if (call->is_input) */
	/* { */
		if (arg->from.depth || call->is_linked)
			/* input is linked */
		{
			return;
		}
		if (root != call && vicall_is_linked(root->type, slot)) return;

		data += (call->data_offset != ~0 ? call->data_offset : 0);
		if ((call->type->builtin_size && call->data_offset == ~0) || !parent_data)
		{
			if (arg->data == NULL)
			{
				arg->data = calloc(1, call->type->builtin_size);
			}
			data = arg->data;
		}
	/* } */

	if (call->type->builtin_gui)
	{
		if (call->type->builtin_size)
			cb(root, call, slot, data, usrptr);
	}
	else
	{
		for (vicall_t *it = call->type->begin; it; it = it->next)
		{
			if (!it->is_input) continue;
			_vicall_foreach_unlinked_input(root, slot, data, it, cb, usrptr);
		}
	}
}

void vicall_foreach_unlinked_input(vicall_t *call, vil_foreach_input_cb cb,
                                    void *usrptr)
{
	if (!call) return;
	slot_t sl = {0};
	_vicall_foreach_unlinked_input(call, sl, NULL, call, cb, usrptr);
}

void vifunc_foreach_unlinked_input(vifunc_t *self, vil_foreach_input_cb cb,
                                   void *usrptr)
{
	for (vicall_t *it = self->begin; it; it = it->next)
		vicall_foreach_unlinked_input(it, cb, usrptr);
}

static void _count_size(vicall_t *root, vicall_t *call, slot_t slot,
                        char *data, void *usrptr)
{
	uint32_t *size = usrptr;
	(*size) += call->type->builtin_size;
}

uint32_t vicall_get_size(vicall_t *call)
{
	uint32_t size = 0;
	vicall_foreach_unlinked_input(call, _count_size, &size);
	return size;
}

float _vicall_gui(vicall_t *root, slot_t parent_slot,
                  char *parent_data, vicall_t *call, void *nk,
                  bool_t inherit_hidden, float *y,
                  bool_t *return_has_children)
{
	if (root == call)
		*y = call->bounds.y + 28.0f;

	float call_h = 0.0f;

	slot_t slot = parent_slot;
	slot._[slot.depth++] = call->id;
	struct vil_arg *arg = vicall_get_arg(root, slot);
	arg->y = *y;
	float start_y = *y;

	char *data = parent_data;
	/* if (call->is_input) */
	/* { */
		if (arg->from.depth || call->is_linked)
			/* input is linked */
		{
			if (root != call)
			{
				label_gui(call, nk);
				call_h = 24.0f;
				arg->y = *y + call_h / 2.0f;
				*y += call_h;
			}
			return call_h;
		}
		if (root != call && vicall_is_linked(root->type, slot))
		{
			return 0.0f;
		}

		data += (call->data_offset != ~0 ? call->data_offset : 0);
		if ((call->type->builtin_size && call->data_offset == ~0) || !parent_data)
		{
			if (arg->data == NULL)
			{
				arg->data = calloc(1, call->type->builtin_size);
			}
			data = arg->data;
		}
	/* } */

	char *previous = NULL;
	if (call->type->builtin_size)
	{
		previous = alloca(call->type->builtin_size);
		memcpy(previous, data, call->type->builtin_size);
	}
	inherit_hidden |= call->is_hidden;
	if (call->type->builtin_gui)
	{
		if (!inherit_hidden)
		{
			call_h = call->type->builtin_gui(call, data, nk);
			/* call_h += 2.0f; */
			arg->y = *y + call_h / 2.f;
			*y += call_h;
			if (return_has_children)
			{
				(*return_has_children) = true;
			}
		}
		/* struct nk_rect rect = nk_layout_widget_bounds(nk); */
		/* call_h = rect.h; */

	}
	else
	{
		if (call != root && !inherit_hidden && arg->has_children)
		{
			label_gui(call, nk);
			call_h = 24.0f;
			arg->y = start_y + call_h / 2.0f;
			*y += call_h;
		}
		arg->has_children = false;
		for (vicall_t *it = call->type->begin; it; it = it->next)
		{
			if (!it->is_input) continue;
			float children_h = _vicall_gui(root, slot, data, it, nk,
			                      inherit_hidden, y, &arg->has_children);
			if (return_has_children)
			{
				(*return_has_children) = arg->has_children;
			}
			if (!inherit_hidden)
			{
				call_h += children_h;
			}
		}
		if (call == root)
		{
			arg->y = start_y + call_h / 2.0f;
		}
	}
	if (previous && memcmp(previous, data, call->type->builtin_size))
	{
		/* vicall_propagate_changed(call); */
		/* vicall_propagate_changed(get_call(root->type, parent_slot._[1])); */
		vicall_propagate_changed(root);
	}
	if (call == root) call_h = fmax(25.0f, call_h);

	return call_h;
}

float vicall_gui(vicall_t *call, void *nk)
{
	if (!call) return 0.0f;
	slot_t sl = {0};
	float y = 0.0f;
	return _vicall_gui(call, sl, NULL, call, nk, false, &y, NULL);
}

const char *vicall_name(const vicall_t *self)
{
	return self->name;
}

vicall_t *vifunc_get(vifunc_t *self, const char *name)
{
	if (!self) return NULL;
	for (vicall_t *it = self->begin; it; it = it->next)
	{
		if (!strncmp(it->name, name, sizeof(it->name))) return it;
	}
	return NULL;
}

void _vifunc_foreach_func(vifunc_t *self, vil_func_cb callback, void *usrptr)
{
	if (self->tmp) return;
	self->tmp = true;
	for (vicall_t *it = self->begin; it; it = it->next)
	{
		_vifunc_foreach_func(it->type, callback, usrptr);
	}
	callback(self, usrptr);
}

void vil_foreach_func(vil_t *self, vil_func_cb callback, void *usrptr)
{
	for(khiter_t k = kh_begin(self->funcs); k != kh_end(self->funcs); ++k)
	{
		if(!kh_exist(self->funcs, k)) continue;
		vifunc_t *func = kh_value(self->funcs, k);
		func->tmp = false;
	}
	for(khiter_t k = kh_begin(self->funcs); k != kh_end(self->funcs); ++k)
	{
		if(!kh_exist(self->funcs, k)) continue;
		vifunc_t *func = kh_value(self->funcs, k);
		_vifunc_foreach_func(func, callback, usrptr);
	}
}

void vicall_watch(vicall_t *self, vil_call_cb callback, void *usrptr)
{
	if (!self) return;
	self->watcher = callback;
	self->watcher_usrptr = usrptr;
}

uint32_t vifunc_gui(vifunc_t *self, void *nk)
{
	struct nk_rect total_space;
	const struct nk_input *in = &(((struct nk_context *)nk)->input);
	vicall_t *dragging = NULL;
	vil_t *ctx = self->ctx;

	struct nk_style *s = &(((struct nk_context *)nk)->style);

	uint32_t res = nk_can_begin_titled(nk, self->name, self->name,
	                                   nk_rect(0, 0, 800, 600),
									   NK_WINDOW_BORDER |
									   NK_WINDOW_NO_SCROLLBAR |
									   NK_WINDOW_MOVABLE |
									   NK_WINDOW_CLOSABLE);
	if (res)
	{
		/* allocate complete window space */
		total_space = nk_window_get_content_region(nk);
		nk_layout_row_begin(nk, NK_STATIC, 20, 4);

		nk_layout_row_push(nk, 100);
		if (nk_combo_begin_label(nk, self->name, nk_vec2(100.0f, 500.0f)))
		{
			nk_layout_row_dynamic(nk, 20.0f, 1);
			for(khiter_t k = kh_begin(ctx->funcs); k != kh_end(ctx->funcs); ++k)
			{
				if(!kh_exist(ctx->funcs, k)) continue;
				vifunc_t *t = kh_value(ctx->funcs, k);
				if (t->name[0] == '_') continue;
				if (nk_combo_item_label(nk, t->name, NK_TEXT_LEFT))
				{
					c_editmode(&SYS)->open_vil = t;
				}
			}
			nk_combo_end(nk);
		}

		nk_layout_row_push(nk, 100);
		if (nk_button_label(nk, "create"))
		{
			g_create_buffer[0] = '\0';
			g_create_dialogue = true;
		}
		nk_layout_row_push(nk, 100);
		if (nk_button_label(nk, "save"))
		{
			char *output = NULL;
			entity_signal(entity_null, ref("pick_file_save"), "vil", &output);
			if (output)
			{
				vifunc_save(self, output);
				free(output);
			}
		}
		nk_layout_row_end(nk);

		nk_layout_space_begin(nk, NK_STATIC, total_space.h, self->call_count);
		{
			g_over_sub_context = NULL;
			g_over_context = NULL;
			vicall_t *it;
			/* struct nk_rect size = nk_layout_space_bounds(nk); */
			struct nk_panel *call = 0;

			/* draw each link */
			uint32_t n;
			struct nk_command_buffer *canvas = nk_window_get_canvas(nk);
			for (n = 0; n < self->link_count; ++n)
			{
				struct vil_link *link = &self->links[n];

				struct nk_vec2 pi = get_input_position(self, link->into);
				struct nk_vec2 po = get_output_position(self, link->from);

				struct nk_vec2 l0 = nk_layout_space_to_screen(nk, po);
				struct nk_vec2 l1 = nk_layout_space_to_screen(nk, pi);

				l0.x -= scrolling.x;
				l0.y -= scrolling.y;
				l1.x -= scrolling.x;
				l1.y -= scrolling.y;
				nk_stroke_curve(canvas, l0.x, l0.y, l0.x + 50.0f, l0.y,
						l1.x - 50.0f, l1.y, l1.x, l1.y, 2.0f, nk_rgb(50, 50, 50));
			}


			/* execute each call as a movable group */
			for (it = self->begin; it; it = it->next)
			{
				/* calculate scrolled call window position and size */
				nk_layout_space_push(nk, nk_rect(it->bounds.x - scrolling.x,
							it->bounds.y - scrolling.y, it->bounds.z, 35 + it->bounds.w));

				/* if(!it->is_linked) */
				/* { */
				/* 	nk_style_push_color(nk, &s->window.background, nk_rgba(0,0,0,255)); */
				/* 	nk_style_push_style_item(nk, &s->window.fixed_background, */
				/* 			nk_style_item_color(nk_rgba(0,0,0,255))); */
				/* } */
				/* else */
				/* { */
					nk_style_push_color(nk, &s->window.background, nk_rgba(0,0,0,255));
					nk_style_push_style_item(nk, &s->window.fixed_background,
							nk_style_item_color(nk_rgba(20,20,20,100)));
				/* } */
				/* execute call window */
				if (nk_group_begin(nk, it->name,
							NK_WINDOW_NO_SCROLLBAR|
							NK_WINDOW_BORDER))
				{
					/* always have last selected call on top */

					call = nk_window_get_panel(nk);
					if(!call) continue;

					nk_layout_row_dynamic(nk, 20, 1);
					if (g_renaming == it)
					{
						nk_edit_focus(nk, NK_EDIT_ALWAYS_INSERT_MODE);
						int32_t status = nk_edit_string_zero_terminated(nk,
								NK_EDIT_SIG_ENTER | NK_EDIT_GOTO_END_ON_ACTIVATE, it->name,
								sizeof(it->name), nk_filter_ascii);
						if ((status & NK_EDIT_COMMITED) || (status & NK_EDIT_DEACTIVATED))
						{
							if (strlen(it->name) == 0)
							{
								snprintf(it->name, sizeof(it->name), "%s%d",
								         it->type->name, it->id);

							}
							g_renaming = NULL;
						}
					}
					else
					{
						nk_label(nk, it->name, NK_TEXT_CENTERED);
					}


					struct nk_rect bd = call->bounds;
					struct nk_rect header = nk_rect(bd.x, bd.y, bd.w, 20);
					if (!g_dragging && nk_input_is_mouse_hovering_rect(in, header))
					{
						if(!linking.depth && nk_input_is_mouse_pressed(in, NK_BUTTON_LEFT))
						{
							dragging = it;
						}
						g_over_context = it;
					}

					float h = vicall_gui(it, nk);
					it->bounds.w = h;
					/* contextual menu */
					nk_group_end(nk);
				}
				nk_style_pop_color(nk);
				nk_style_pop_style_item(nk);
				{
					/* call connector and linking */
					struct nk_rect bounds;
					bounds = nk_layout_space_rect_to_local(nk, call->bounds);
					bounds.x += scrolling.x;
					bounds.y += scrolling.y;
					bounds.h = it->bounds.w;
					it->bounds.x = bounds.x;
					it->bounds.y = bounds.y;
					it->bounds.z = bounds.w;
					it->bounds.w = bounds.h;


					slot_t sl = {.val=0};
					/* output connector */
					call_outputs(it, it, sl, call->bounds.y + 20, nk, true);
					/* input connector */
					call_inputs(it, it, sl, nk, false);
				}
				if(g_dragging)
				{
					if(g_dragging == it)
					{
						if(nk_input_is_mouse_released(in, NK_BUTTON_LEFT))
						{
							g_dragging = NULL;
						}
						it->bounds.x += in->mouse.delta.x;
						it->bounds.y += in->mouse.delta.y;
					}
				}
			}
			/* reset linking connection */
			if(linking.depth)
			{
				dragging = NULL;
				g_dragging = NULL;
				if (nk_input_is_mouse_released(in, NK_BUTTON_LEFT))
				{
					linking.val = 0;
					fprintf(stdout, "linking failed\n");
				}
			}

			if(!g_open_global_context && !g_open_context && (g_over_sub_context || g_open_sub_context))
			{
				if (nk_contextual_begin(nk, 0, nk_vec2(100, 520),
				                        nk_rect(0, 0, 10000, 10000)))
				{
					if (!g_open_sub_context) g_open_sub_context = g_over_sub_context;
					nk_layout_row_dynamic(nk, 20, 1);
					slot_t sl = {0};
					if (g_over_sub_context_parent != g_open_sub_context)
					{
						sl = (slot_t){.depth=1, ._ = {g_over_sub_context_parent->id}};
					}
					output_options(g_open_sub_context, g_open_sub_context, sl, nk);
					nk_contextual_end(nk);
				}
				else
				{
					g_open_sub_context = NULL;
				}
			}
			else if(!g_open_global_context && (g_over_context || g_open_context))
			{
				if (nk_contextual_begin(nk, 0, nk_vec2(100, 520),
				                        nk_rect(0, 0, 10000, 10000)))
				{
					if (!g_open_context) g_open_context = g_over_context;
					nk_layout_row_dynamic(nk, 20, 1);
					if (nk_contextual_item_label(nk, "rename", NK_TEXT_CENTERED))
					{
						g_renaming = g_open_context;
					}
					if (nk_contextual_item_label(nk, "open", NK_TEXT_CENTERED))
					{
						c_editmode(&SYS)->open_vil = g_open_context->type;
					}
					if (nk_contextual_item_label(nk, "remove", NK_TEXT_CENTERED))
					{
					}
					nk_contextual_end(nk);
				}
				else
				{
					g_open_context = NULL;
				}
			}
			else
			{
				if (nk_contextual_begin(nk, 0, nk_vec2(150, 500), nk_window_get_bounds(nk)))
				{
					if (!g_open_global_context)
					{
						g_open_global_context = true;
						g_context_pos = vec2(in->mouse.pos.x, in->mouse.pos.y);
					}
					nk_layout_row_dynamic(nk, 20, 1);
					khiter_t k;
					for(k = kh_begin(ctx->funcs); k != kh_end(ctx->funcs); ++k)
					{
						if(!kh_exist(ctx->funcs, k)) continue;
						vifunc_t *t = kh_value(ctx->funcs, k);
						if (t == self) continue;
						if(t->name[0] == '_') continue;
						if(nk_contextual_item_label(nk, t->name, NK_TEXT_CENTERED))
						{
							g_renaming = vicall_new(self, t, "", g_context_pos,
													~0, V_BOTH);
						}

					}
					nk_contextual_end(nk);
				}
				else
				{
					g_open_global_context = false;
				}
			}


			if(dragging)
			{
				g_dragging = dragging;
				if (dragging->next)
				{
					/* reshuffle calls to have least recently selected call on top */
					type_pop(self, g_dragging);
					type_push(self, g_dragging);
				}
			}

		}
		nk_layout_space_end(nk);

		/* window content scrolling */
		if (nk_input_is_mouse_hovering_rect(in, nk_window_get_bounds(nk)) &&
				nk_input_is_mouse_down(in, NK_BUTTON_MIDDLE))
		{
			scrolling.x -= in->mouse.delta.x;
			scrolling.y -= in->mouse.delta.y;
		}
	}
	nk_end(nk);
	if (g_create_dialogue)
	{
		if (nk_can_begin_titled(nk, "vifunccreate", "create a new function",
										    nk_rect(400, 300, 150, 60),
										    NK_WINDOW_BORDER |
										    NK_WINDOW_NO_SCROLLBAR |
										    NK_WINDOW_MOVABLE |
										    NK_WINDOW_CLOSABLE))
		{
			nk_layout_row_dynamic(nk, 20, 2);

			nk_edit_focus(nk, NK_EDIT_ALWAYS_INSERT_MODE);
			int32_t status = nk_edit_string_zero_terminated(nk,
					NK_EDIT_SIG_ENTER | NK_EDIT_GOTO_END_ON_ACTIVATE, g_create_buffer,
					sizeof(g_create_buffer), nk_filter_ascii);
			if (status & NK_EDIT_COMMITED)
			{
				if (strlen(g_create_buffer) > 0)
				{
					c_editmode(&SYS)->open_vil = vifunc_new(ctx, g_create_buffer, NULL, 0, false);
				}
				g_create_dialogue = false;
			}
			if (nk_button_label(nk, "create"))
			{
				c_editmode(&SYS)->open_vil = vifunc_new(ctx, g_create_buffer, NULL, 0, false);
				g_create_dialogue = false;
				
			}
		}
		else
		{
			g_create_dialogue = false;
		}
		nk_end(nk);
	}
	return res;
}

