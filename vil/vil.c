#include "vil.h"
#include <assert.h>
#include <systems/editmode.h>
#include <utils/khash.h>
#include <utils/mafs.h>
#include <candle.h>

static slot_t linking;
vicall_t *g_dragging;
vicall_t *g_renaming;
char g_renaming_buffer[64];
bool_t g_create_dialogue;
vifunc_t *g_create_template;
char g_create_buffer[64];

vicall_t *g_over_context;
slot_t g_over_sub_context;

vicall_t *g_open_context;
slot_t g_open_sub_context;

bool_t g_open_global_context;

static vec2_t g_context_pos;
static uint32_t linking_type;
static slot_t g_call_link;
static struct nk_vec2 scrolling;

static void vicall_propagate_changed(vicall_t *self, bool_t data_only);
bool_t vicall_is_linked(vifunc_t *root, slot_t slot);
void propagate_data(vicall_t *root, vicall_t *call, slot_t parent_slot,
                    uint8_t *parent_data);
void vicall_copy_defaults(vicall_t *call);

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

static bool_t slot_equal(slot_t a, slot_t b)
{
	if (a.depth != b.depth) return false;
	for (uint32_t i = 0; i < a.depth; i++)
	{
		if (a.calls[i] != b.calls[i]) return false;
	}
	return true;
}

struct vil_ret *vicall_get_ret(vicall_t *call, slot_t slot)
{
	for (uint32_t i = 0; i < 256; i++)
	{
		if (   slot_equal(call->output_args[i].from, slot)
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
	for (uint32_t i = 0; i < 256; i++)
	{
		if (   slot_equal(call->input_args[i].into, slot)
		    || call->input_args[i].into.depth == 0)
		{
			call->input_args[i].into = slot;
			return &call->input_args[i];
		}
	}
	assert(false);
}

static vicall_t *vifunc_get_call(vifunc_t *type, const slot_t slot,
                                 uint32_t depth, uint32_t max_depth)
{
	if (slot.depth == 0 || max_depth == 0) return NULL;
	for (vicall_t *iter = type->begin; iter; iter = iter->next)
	{
		if (iter->id == slot.calls[depth])
		{
			depth++;
			if (depth == max_depth)
			{
				return iter;
			}
			else 
			{
				return vifunc_get_call(iter->type, slot, depth, max_depth);
			}
		}
	}
	return NULL;
}

slot_t vifunc_slot_from_name(vifunc_t *func, char *buffer,
                             const char *separator)
{
	slot_t slot = {0};
	char *name;
	char *rest = buffer;
	while ((name = strtok_r(rest, separator, &rest)))
	{
		uint32_t id = ~0;
		for(vicall_t *it = func->begin; it; it = it->next)
		{
			if (!strncmp(it->name, name, sizeof(it->name)))
			{
				id = it->id;
				func = it->type;
				break;
			}
		}
		if (id == ~0)
		{
			return slot;
		}

		slot.calls[slot.depth++] = id;
	}
	return slot;
}

void vifunc_slot_to_name(vifunc_t *func, slot_t slot, char *buffer,
                         const char *separator, const char *append)
{
	buffer[0] = '\0';
	vicall_t *call = NULL;
	for (uint32_t i = 0; i < slot.depth; i++)
	{
		call = vifunc_get_call(func, slot, i, i + 1);
		if (!call)
		{
			printf("call %u does not exist in '%s'\n", slot.calls[i],
			       func->name);
			exit(1);
		}
		strcat(buffer, call->name);
		if (i == 0 && append) strcat(buffer, append);
		if (i < slot.depth - 1) strcat(buffer, separator);
		func = call->type;
	}
}

void _vicall_save(vicall_t *self, FILE *fp)
{
	fprintf(fp, "%s %s %f %f %x %d %d %d %d %d %d\n", 
	        self->name, self->type->name, self->bounds.x, self->bounds.y,
	        self->color, self->is_input, self->is_output,
	        self->is_linked, self->is_hidden, self->is_locked, self->data_offset);
}

bool_t _vicall_load(vifunc_t *parent, const char *name, FILE *fp)
{
	char type[64];
	vec2_t pos;
	uint32_t color, data_offset;
	bool_t is_input, is_output, is_linked, is_hidden, is_locked;

	uint32_t ret = fscanf(fp, "%s %f %f %x %d %d %d %d %d %d\n", 
	                      type, &pos.x, &pos.y,
	                      &color, &is_input, &is_output,
	                      &is_linked, &is_hidden, &is_locked, &data_offset);
	if (ret < 0) return false;

	uint32_t flags =   is_linked * V_LINKED | is_hidden * V_HID
	                 | is_output * V_OUT | is_input * V_IN;

	vicall_t *self = vicall_new(parent, vil_get(parent->ctx, ref(type)),
	                            name, pos, data_offset, flags);
	self->color = color;

	return true;
}

void vifunc_save(vifunc_t *self, const char *filename)
{
	FILE *fp = fopen(filename, "w");
	if (!fp) return;

	for (uint32_t i = 0; i < self->call_count; i++)
	{
		vicall_t *call = &self->call_buf[i];
		_vicall_save(call, fp);
	}

	for (uint32_t i = 0; i < self->call_count; i++)
	{
		vicall_t *call = &self->call_buf[i];
		if (call->is_input)
		{
			for (uint32_t a = 0; a < 256; a++)
			{
				char buffer[256];
				struct vil_arg *arg = &call->input_args[a];
				if (arg->into.depth == 0) break;
				vicall_t *child = vifunc_get_call(self, arg->into, 0,
												  arg->into.depth);

				if (!vicall_is_linked(call->type, arg->into))
				if (child->type->builtin_save && arg->initialized)
				{
					vifunc_slot_to_name(self, arg->into, buffer, ".", NULL);
					fprintf(fp, "%s = ", buffer);
					child->type->builtin_save(child, arg->data, fp);
					fprintf(fp, "\n");
				}
			}
		}
	}
	for (uint32_t i = 0; i < self->link_count; i++)
	{
		struct vil_link *link = &self->links[i];

		char into[256];
		char from[256];

		vifunc_slot_to_name(self, link->into, into, ".", NULL);
		vifunc_slot_to_name(self, link->from, from, ".", NULL);
		fprintf(fp ,"%s -> %s\n", from, into);
	}
}

slot_t vifunc_ref_to_slot(vifunc_t *self, uint32_t ref)
{
	for(vicall_t *it = self->begin; it; it = it->next)
	{
		for (uint32_t i = 0; i < 256; i++)
		{
			char buffer[256];
			struct vil_arg *arg = &it->input_args[i];
			if (!arg->into.depth) break;
			vifunc_slot_to_name(it->parent, arg->into, buffer, ".", NULL);
			uint32_t arg_ref = ref(buffer);
			if (ref == arg_ref)
			{
				return arg->into;
			}
		}
		for (uint32_t i = 0; i < 256; i++)
		{
			char buffer[256];
			struct vil_ret *ret = &it->output_args[i];
			/* if (!ret->from.depth) break; */
			vifunc_slot_to_name(it->parent, ret->from, buffer, ".", NULL);
			uint32_t ret_ref = ref(buffer);
			if (ref == ret_ref)
			{
				return ret->from;
			}
		}
	}
	return (slot_t){ 0 };
}

int arg_changed(vicall_t *call)
{
	vicall_propagate_changed(call, true);
	return 1;
}

void vicall_update_initialized(vicall_t *call)
{
	for (uint32_t i = 0; i < 256; i++)
	{
		struct vil_arg *arg = &call->input_args[i];
		assert(!arg->data_size || arg->previous_data);
		if (arg->data_size && memcmp(arg->previous_data, arg->data,
		                             arg->data_size))
		{
			memcpy(arg->previous_data, arg->data, call->type->builtin_size);
			arg->initialized = true;
		}
	}
}

void vicall_set_arg(vicall_t *call, uint32_t ref, void *value)
{
	for (uint32_t i = 0; i < 256; i++)
	{
		char buffer[256];
		struct vil_arg *arg = &call->input_args[i];
		if (arg->into.depth == 0) break;
		vifunc_slot_to_name(call->type, slot_pop(arg->into), buffer, ".", NULL);
		uint32_t arg_ref = ref(buffer);
		if (ref == arg_ref || (ref == ~0 && buffer[0] == '\0'))
		{
			memcpy(arg->data, value, arg->data_size);
			break;
		}
	}
	vicall_update_initialized(call);
	loader_push(g_candle->loader, (loader_cb)arg_changed, call, NULL);
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
			slt.calls[i - 1] = slt.calls[i];
		slt.depth--;
		slt.calls[slt.depth] = 0;

		for (uint32_t i = 0; i < root->link_count; i++) {
			if (slot_equal(root->links[i].into, slt)) {
				return true;
			}
		}

		root = vifunc_get_call(root, slt, 0, 1)->type;
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
		slot.calls[slot.depth++] = ref(iter->name);

		/* if (   (iter->is_input  && !allow_input) */
		    /* || (iter->is_output && !allow_output)) continue; */
		if (   (!iter->is_input  || !allow_input)
		    && (!iter->is_output || !allow_output)) continue;

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

static void vicall_propagate_changed_aux0(vicall_t *self, bool_t data_only);
static void vicall_propagate_changed_aux1(vicall_t *self, bool_t *iterated,
                                          bool_t data_only)
{

	iterated[self->id] = true;
	if (self->watcher && !self->parent->locked)
	{
		self->watcher(self, (slot_t){.depth = 1, .calls={self->id}},
		              self->watcher_usrptr);
	}
	if (!data_only && self->parent->watcher && !self->parent->locked)
	{
		self->parent->watcher(self->parent, self->parent->watcher_usrptr);
	}
	for (uint32_t i = 0; i < 256; i++)
	{
		struct vil_ret *output = &self->output_args[i];
		if (output->into.depth == 0) continue;
		vicall_t *input = vifunc_get_call(self->parent, output->into, 0, 1);
		if (!input) break;

		if (!iterated[input->id])
		{
			vicall_propagate_changed_aux1(input, iterated, data_only);
		}
	}
}

static void vicall_propagate_changed_aux0(vicall_t *self, bool_t data_only)
{
	bool_t *iterated = alloca(sizeof(bool_t) * self->parent->call_count);
	memset(iterated, false, sizeof(bool_t) * self->parent->call_count);
	vicall_propagate_changed_aux1(self, iterated, data_only);
}

static void vicall_propagate_changed(vicall_t *self, bool_t data_only)
{
	vil_t *ctx = self->type->ctx;
	vicall_propagate_changed_aux0(self, data_only);

	for(khiter_t k = kh_begin(ctx->funcs); k != kh_end(ctx->funcs); ++k)
	{
		if(!kh_exist(ctx->funcs, k)) continue;
		vifunc_t *func = kh_value(ctx->funcs, k);
		for(vicall_t *it = func->begin; it; it = it->next)
		{
			if (it->type == self->parent)
			{
				vicall_propagate_changed(it, data_only);
			}
		}
	}
}

static void _vifunc_link(vifunc_t *self, slot_t from_slot, slot_t into_slot)
{
	struct vil_link *link = &self->links[self->link_count++];

	g_call_link = from_slot;

	link->from = from_slot;
	link->into = into_slot;

	vicall_t *into = vifunc_get_call(self, into_slot, 0, 1);
	vicall_t *from = vifunc_get_call(self, from_slot, 0, 1);

	/* char buffer_to[256]; */
	/* char buffer_from[256]; */
	/* vifunc_slot_to_name(self, into_slot, buffer_to, ".", "_in"); */
	/* vifunc_slot_to_name(self, from_slot, buffer_from, ".", "_out"); */
	/* printf("linking %s to %s\n", buffer_from, buffer_to); */

	struct vil_arg *arg = vicall_get_arg(into, into_slot);
	struct vil_ret *ret = vicall_get_ret(from, from_slot);
	arg->from = from_slot;
	arg->into = into_slot;

	ret->into = into_slot;
	ret->from = from_slot;

	vicall_propagate_changed(into, false);
}

void vifunc_link(vifunc_t *self, uint32_t from, uint32_t into)
{
	slot_t from_slot = vifunc_ref_to_slot(self, from);
	slot_t into_slot = vifunc_ref_to_slot(self, into);
	_vifunc_link(self, from_slot, into_slot);
}

bool_t vifunc_load(vifunc_t *self, const char *filename)
{
	if (!self) return false;

	/* TODO: clear self */

	FILE *fp = fopen(filename, "r");
	if (!fp) return false;

	self->locked++;
	while (!feof(fp))
	{
		char field[64];
		int ret = fscanf(fp, "%64s ", field);
		if (ret < 0) goto fail;
		slot_t slot = vifunc_slot_from_name(self, field, ".");
		vicall_t *root = vifunc_get_call(self, slot, 0, 1);

		if (!root)
		{
			if (slot.depth <= 1)
			{
				_vicall_load(self, field, fp);
			}
			else
			{
				printf("Error loading '%s'\n", filename);
			}
		}
		else
		{
			vicall_t *call = vifunc_get_call(root->type, slot, 1, slot.depth);
			char oper[8];
			ret = fscanf(fp, "%8s ", oper);
			if (ret < 0) goto fail;
			if (!strncmp(oper, "=", 8))
			{
				struct vil_arg *arg = vicall_get_arg(root, slot);
				if (call->type->builtin_load)
				{
					call->type->builtin_load(call, arg->data, fp);
					ret = fscanf(fp, "\n");
					if (ret < 0) goto fail;
				}
				else
				{
					printf("Type '%s' cannot be assigned to.\n",
					       call->type->name);
				}
			}
			else if (!strncmp(oper, "->", 8))
			{
				char into[256];
				ret = fscanf(fp, "%256s\n", into);
				if (ret < 0) goto fail;
				slot_t into_slot = vifunc_slot_from_name(self, into, ".");
				_vifunc_link(self, slot, into_slot);
			}
		}
	}
	for (vicall_t *it = self->begin; it; it = it->next)
	{
		if (it->is_linked || !it->is_input) continue;
		vicall_update_initialized(it);
	}

	self->locked--;

	fclose(fp);
	return true;

fail:
	self->locked--;
	fclose(fp);
	return false;
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

void vicall_destroy(vicall_t *self)
{
	for (uint32_t a = 0; a < 256; a++)
	{
		struct vil_arg *it_arg = &self->input_args[a];
		if (it_arg->into.depth == 0) break;
		if (it_arg->data && it_arg->alloc_head)
		{
			free(it_arg->data);
		}
		if (it_arg->previous_data)
		{
			free(it_arg->previous_data);
		}
	}
}

void vifunc_destroy(vifunc_t *self)
{
	for (vicall_t *it = self->begin; it; it = it->next)
	{
		vicall_destroy(it);
	}
	khiter_t k = kh_get(vifunc, self->ctx->funcs, self->id);
	kh_del(vifunc, self->ctx->funcs, k);
}

vifunc_t *vifunc_new(vil_t *ctx, const char *name,
		vifunc_gui_cb builtin_gui,
		uint32_t builtin_size, bool_t is_assignable)
{
	if (!ctx->funcs) return NULL;
	int32_t ret = 0;
	uint32_t id = ref(name);
	khiter_t k = kh_put(vifunc, ctx->funcs, id, &ret);
	if(ret == -1) exit(1);
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

void vifunc_slot_print(vifunc_t *func, slot_t slot)
{
	char buffer[256];
	vifunc_slot_to_name(func, slot, buffer, ".", NULL);
	printf("%s\n", buffer);
}

void slot_print(slot_t slt)
{
	for (uint32_t i = 0; i < 16; i++)
		if (i < slt.depth)
			printf("%u ", slt.calls[i]);
		else
			printf("- ");
	puts("");
}

slot_t slot_insert_start(slot_t slt, uint32_t start)
{
	for (uint32_t i = 15; i >= 1; i--)
		slt.calls[i] = slt.calls[i - 1];
	slt.calls[0] = start;
	slt.depth += 1;
	return slt;
}

slot_t slot_pop(slot_t slt)
{
	for (uint32_t i = 1; i < 16; i++)
		slt.calls[i - 1] = slt.calls[i];
	slt.depth--;
	slt.calls[slt.depth] = 0;
	return slt;
}

void propagate_output(vicall_t *root, vicall_t *call, slot_t parent_slot)
{
	slot_t slot = parent_slot;
	slot.calls[slot.depth++] = call->id;
	vicall_get_ret(root, slot);

	for (vicall_t *it = call->type->begin; it; it = it->next)
	{
		if (!it->is_output) continue;
		propagate_output(root, it, slot);
	}
}

void propagate_data(vicall_t *root, vicall_t *call, slot_t parent_slot,
                    uint8_t *parent_data)
{
	slot_t slot = parent_slot;
	slot.calls[slot.depth++] = call->id;
	struct vil_arg *arg = vicall_get_arg(root, slot);

	if (call->data_offset != ~0 && parent_data)
	{
		arg->data = parent_data + call->data_offset;
	}
	if (!arg->data && call->type->builtin_size)
	{
		arg->data = calloc(1, call->type->builtin_size);
		arg->alloc_head = true;
		arg->initialized = false;
	}
	if (!arg->previous_data && call->type->builtin_size)
	{
		arg->previous_data = calloc(1, call->type->builtin_size);
	}
	arg->data_size = call->type->builtin_size;

	for (vicall_t *it = call->type->begin; it; it = it->next)
	{
		if (!it->is_input || it->is_linked) continue;
		propagate_data(root, it, slot, arg->data);
	}
}

void vicall_copy_defaults(vicall_t *call)
{
	for (vicall_t *it = call->type->begin; it; it = it->next)
	{
		for (uint32_t a = 0; a < 256; a++)
		{
			struct vil_arg *it_arg = &it->input_args[a];
			if (it_arg->into.depth == 0) break;
			slot_t slot = slot_insert_start(it_arg->into, call->id);
			struct vil_arg *call_arg = vicall_get_arg(call, slot);

			if (call_arg->data_size < it_arg->data_size)
			{
				call_arg->data_size = it_arg->data_size;
				call_arg->data = calloc(1, it_arg->data_size);
				call_arg->previous_data = calloc(1, it_arg->data_size);
			}

			assert(!call_arg->data_size || call_arg->data);
			assert(it_arg->data_size == call_arg->data_size);

			if (it_arg->data_size && !call_arg->initialized)
			{
				/* printf("%f\n", *((float*)it_arg->data)); */
				memcpy(call_arg->data, it_arg->data, it_arg->data_size);
				memcpy(call_arg->previous_data, it_arg->data, it_arg->data_size);
			}
		}
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
	call->bounds.z = 120;
	call->bounds.w = 20;
	call->type = type;
	call->parent = parent;
	call->color = nk_color_u32(nk_rgb(10, 10, 10));

	call->data_offset = data_offset;
	call->is_input = !!(flags & V_IN);
	call->is_hidden = !!(flags & V_HID);
	call->is_output = !!(flags & V_OUT);
	call->is_linked = !!(flags & V_LINKED);

	strncpy(call->name, name, sizeof(call->name));
	type_push(parent, call);
	if (call->is_linked || !call->is_input) return call;

	propagate_data(call, call, (slot_t){0}, NULL);
	propagate_output(call, call, (slot_t){0});
	vicall_copy_defaults(call);

	vicall_propagate_changed(call, false);

	return call;
}

static void vifunc_unlink(vifunc_t *self, slot_t into_slot)
{
	uint32_t i;

	struct vil_link *link = NULL;
	for(i = 0; i < self->link_count; i++)
	{
		link = &self->links[i];
		if (slot_equal(link->into, into_slot)) break;
	}
	if (!link || !slot_equal(link->into, into_slot)) return;

	vicall_t *from = vifunc_get_call(self, link->from, 0, 1);
	vicall_t *into = vifunc_get_call(self, link->into, 0, 1);
	printf("unlinking %s -> %s\n", from->name, into->name);

	struct vil_ret *ret = vicall_get_ret(from, link->from);
	struct vil_arg *arg = vicall_get_arg(into, into_slot);

	arg->from.depth = 0;
	ret->into.depth = 0;

	self->link_count--;
	if(self->link_count > 0)
	{
		*link = self->links[self->link_count];
	}
	vicall_propagate_changed(into, false);
}

static void output_options(vicall_t *root,
		vicall_t *call, slot_t parent_slot, struct nk_context *nk)
{
	vicall_t *it;
	for (it = call->type->begin; it; it = it->next)
	{
		slot_t slot = parent_slot;
		slot.calls[slot.depth++] = it->id;
		struct vil_ret *ret = vicall_get_ret(root, slot);
		if(it != root && it->type->builtin_size && it->type->is_assignable)
		{
			if(!ret->into.depth) /* is not linked */
			{
				if (nk_contextual_item_label(nk, it->name, NK_TEXT_CENTERED))
				{
					ret->into.depth = ~0;
					linking = slot;
					printf("linking "); vifunc_slot_print(root->parent, slot);
					linking_type = it->type->id;
				}
			}
		}
		if(it->is_output)
		{
			output_options(root, it, slot, nk);
		}
	}
}

static float call_outputs(vicall_t *root,
		vicall_t *call, slot_t parent_slot, float x, float y,
		struct nk_context *nk, bool_t exposed)
{

	vicall_t *it;
	slot_t slot = parent_slot;
	slot.calls[slot.depth++] = call->id;

	struct vil_ret *ret = vicall_get_ret(root, slot);

	if(!linking.depth && ret->into.depth == (unsigned char)-1)
	{
		ret->into.depth = 0;
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
		ret->y = y;
		struct nk_rect circle;
		circle.x = x + call->bounds.z + 2;
		/* circle.y = call->bounds.y + space * (float)(n+1); */
		circle.y = y + 20.0f / 2.0f;
		circle.w = 8; circle.h = 8;
		circle.x -= scrolling.x;
		circle.y -= scrolling.y;
		nk_fill_circle(canvas, circle, nk_rgba_u32(call->color));

		struct nk_rect bound = nk_rect(circle.x-8, circle.y-8,
				circle.w+16, circle.h+16);

		/* start linking process */
		if (g_dragging == NULL)
		{
			if (nk_input_is_mouse_click_down_in_rect(in,
						NK_BUTTON_LEFT, bound, nk_true))
			{
				linking = slot;
				linking_type = call->type->id;
			}
		}

		/* draw curve from linked call slot to mouse position */
		if (linking.depth && slot_equal(linking, slot))
		{
			struct nk_vec2 l0 = nk_vec2(circle.x + 4, circle.y + 4);
			struct nk_vec2 l1 = in->mouse.pos;

			nk_stroke_curve(canvas, l0.x, l0.y, l0.x + 50.0f, l0.y,
					l1.x - 50.0f, l1.y, l1.x, l1.y, 2.0f, nk_rgb(10, 10, 10));
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
			g_over_sub_context = slot;
		}
		exposed = false;
	}

	for (it = call->type->begin; it; it = it->next)
	{
		if(it->is_output)
		{
			y = call_outputs(root, it, slot, x, y, nk, exposed);
		}
	}
	return y;
}

static struct nk_vec2 get_output_position(vifunc_t *type, slot_t search)
{
	vicall_t *call = vifunc_get_call(type, search, 0, 1);
	if (!call) return nk_vec2(0.0f, 0.0f);
	struct vil_ret *ret = vicall_get_ret(call, search);
	if (!ret) return nk_vec2(0.0f, 0.0f);
	return nk_vec2(call->bounds.x + call->bounds.z, ret->y);
}

static struct nk_vec2 get_input_position(vifunc_t *type, slot_t search)
{
	vicall_t *call = vifunc_get_call(type, search, 0, 1);
	if (!call) return nk_vec2(0.0f, 0.0f);
	struct vil_arg *arg = vicall_get_arg(call, search);
	if (!arg) return nk_vec2(0.0f, 0.0f);

	return nk_vec2(call->bounds.x, arg->y);
}

static void call_inputs(vicall_t *root,
		vicall_t *call, slot_t parent_slot, float x, float y,
		struct nk_context *nk, bool_t inherit_hidden)
{


	slot_t slot = parent_slot;
	slot.calls[slot.depth++] = call->id;

	struct vil_arg *arg = vicall_get_arg(root, slot);

	uint32_t is_linking = linking.depth && linking.calls[0] != root->id;
	uint32_t linking_allowed = is_linking && linking_type == call->type->id;

	uint32_t is_linked =  arg->from.depth;

	inherit_hidden |= call->is_hidden;
	if((linking_allowed || is_linked) && !inherit_hidden)
	{
		const struct nk_input *in = &nk->input;

		struct nk_command_buffer *canvas = nk_window_get_canvas(nk);

		struct nk_rect circle = nk_rect(x + 2 - scrolling.x,
				arg->y + y - scrolling.y, 8, 8);
		nk_fill_circle(canvas, circle, nk_rgba_u32(call->color));

		struct nk_rect bound = nk_rect(circle.x-8, circle.y-8,
				circle.w+16, circle.h+16);

		if(is_linked)
		{
			if (nk_input_is_mouse_click_down_in_rect(in,
						NK_BUTTON_LEFT, bound, nk_true))
			{
				linking = arg->from;
				linking_type = call->type->id;
				vifunc_unlink(root->parent, slot);
				is_linked = false;
				return;
			}
		}

		if (g_dragging == NULL)
		{
			if (linking_allowed &&
					nk_input_is_mouse_released(in, NK_BUTTON_LEFT) &&
					nk_input_is_mouse_hovering_rect(in, bound))
			{
				if(is_linked)
				{
					vifunc_unlink(root->parent, slot);
				}
				_vifunc_link(root->parent, linking, slot);
				linking.depth = 0;
			}
		}
		if (is_linked) return;
	}
	if (root != call && vicall_is_linked(root->type, slot)) return;

	if (!call->type->builtin_gui)
	{
		for (vicall_t *it = call->type->begin; it; it = it->next)
		{
			if (!it->is_input) continue;
			call_inputs(root, it, slot, x, y, nk, inherit_hidden);
		}
	}
}

void _vicall_iterate_dependencies(vicall_t *self, bool_t *iterated,
                                  vil_link_cb link,
                                  vil_call_cb call,
                                  void *usrptr)
{
	iterated[self->id] = true;
	for (uint32_t i = 0; i < 256; i++)
	{
		struct vil_arg *input = &self->input_args[i];
		if (input->from.depth == 0) continue;
		vicall_t *output = vifunc_get_call(self->parent, input->from, 0, 1);
		if (!iterated[output->id])
		{
			_vicall_iterate_dependencies(output, iterated, link, call, usrptr);
		}
		if (link) link(self->parent, input->into, input->from, usrptr);
	}
	if (call) call(self, (slot_t){.depth = 1, .calls = {self->id}}, usrptr);
}

void vicall_iterate_dependencies(vicall_t *self, vil_link_cb link,
                                 vil_call_cb call, void *usrptr)
{
	bool_t *iterated = alloca(sizeof(bool_t) * self->parent->call_count);
	memset(iterated, false, sizeof(bool_t) * self->parent->call_count);

	_vicall_iterate_dependencies(self, iterated, link, call, usrptr);
}

void vifunc_iterate_dependencies(vifunc_t *self, vil_link_cb link,
                                 vil_call_cb call, void *usrptr)
{
	bool_t *iterated = alloca(sizeof(bool_t) * self->call_count);
	memset(iterated, false, sizeof(bool_t) * self->call_count);

	for (vicall_t *it = self->begin; it; it = it->next)
	{
		if (iterated[it->id]) continue;
		_vicall_iterate_dependencies(it, iterated, link, call, usrptr);
	}
}

void _vicall_foreach_unlinked_input(vicall_t *root,
                                     slot_t parent_slot,
									 uint8_t *parent_data, vicall_t *call,
									 vil_foreach_input_cb cb,
									 void *usrptr)
{

	slot_t slot = parent_slot;
	slot.calls[slot.depth++] = call->id;

	uint8_t *data = parent_data;
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
                        uint8_t *data, void *usrptr)
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
                  vicall_t *call, void *nk,
                  bool_t inherit_hidden, float *y,
                  bool_t *return_has_children,
                  bool_t collapsible,
                  bool_t *modified)
{

	float call_h = 0.0f;

	slot_t slot = parent_slot;
	slot.calls[slot.depth++] = call->id;
	struct vil_arg *arg = vicall_get_arg(root, slot);
	arg->y = *y;
	float start_y = *y;

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

	inherit_hidden |= call->is_hidden;
	if (call->type->builtin_gui)
	{
		if (!inherit_hidden)
		{
			call_h = call->type->builtin_gui(call, arg->data, nk);
			/* call_h += 2.0f; */
			arg->y = *y + call_h / 2.f;
			*y += call_h;
			if (return_has_children)
			{
				(*return_has_children) = true;
			}
		}
	}
	else
	{
		bool_t coll = false;
		bool_t coll_open = false;
		if (call != root && !inherit_hidden && arg->has_children)
		{
			if (collapsible)
			{
				coll = true;
				coll_open = (nk_tree_push_id(nk, NK_TREE_TAB, call->name,
							NK_MINIMIZED, arg - root->input_args));
			}
			else
			{
				label_gui(call, nk);
				call_h = 24.0f;
				arg->y = start_y + call_h / 2.0f;
				*y += call_h;
			}
		}
		if (!coll || coll_open)
		{
			arg->has_children = false;
			for (vicall_t *it = call->type->begin; it; it = it->next)
			{
				if (!it->is_input) continue;
				float children_h = _vicall_gui(root, slot, it, nk,
									  inherit_hidden, y, &arg->has_children,
									  collapsible, modified);
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
		if (coll_open)
		{
			nk_tree_pop(nk);
		}
	}

	if (call->type->builtin_size && memcmp(arg->previous_data, arg->data,
	                                       call->type->builtin_size))
	{
		memcpy(arg->previous_data, arg->data, call->type->builtin_size);
		arg->initialized = true;
		(*modified) = true;
	}
	if (call == root) call_h = fmax(25.0f, call_h);

	return call_h;
}

float vicall_gui(vicall_t *call, void *nk, bool_t collapsible, float yb)
{
	if (!call) return 0.0f;
	slot_t sl = {0};
	float y = yb + 83.0f;
	bool_t modified = false;

	propagate_data(call, call, sl, NULL);
	vicall_copy_defaults(call);

	float h = _vicall_gui(call, sl, call, nk, false, &y, NULL, collapsible,
	                      &modified);
	if (modified)
	{
		vicall_propagate_changed(call, true);
	}
	return h;
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

void vifunc_copy(vifunc_t *dst, vifunc_t *src)
{
	for (vicall_t *it = src->begin; it; it = it->next)
	{
		uint32_t flags =   it->is_linked * V_LINKED | it->is_hidden * V_HID
						 | it->is_output * V_OUT | it->is_input * V_IN;
		vicall_t *copy = vicall_new(dst, it->type,
				it->name, it->bounds.xy, it->data_offset, flags);
		copy->color = it->color;
		for (uint32_t i = 0; i < 256; i++)
		{
			struct vil_arg *it_arg = &it->input_args[i];
			struct vil_arg *cp_arg = &copy->input_args[i];
			if (it_arg->into.depth == 0) break;
			if (it_arg->data)
			{
				assert(cp_arg->data_size == it_arg->data_size);
				memcpy(cp_arg->data, it_arg->data, it_arg->data_size);
				memcpy(cp_arg->previous_data, it_arg->data, it_arg->data_size);
				cp_arg->initialized = it_arg->initialized;
			}
		}
		vicall_update_initialized(copy);
	}
	for (uint32_t i = 0; i < src->link_count; i++)
	{
		struct vil_link *link = &src->links[i];

		char into[256];
		char from[256];

		vifunc_slot_to_name(src, link->into, into, ".", NULL);
		vifunc_slot_to_name(src, link->from, from, ".", NULL);
		vifunc_link(dst, ref(from), ref(into));
	}
}

void _create_func(vil_t *ctx)
{
	char buffer[64] = "_";
	strncat(buffer, g_create_buffer, sizeof(buffer) - 1);
	vifunc_t *new_func = vifunc_new(ctx, buffer, NULL, 0, false);
	if (g_create_template)
	{
		vifunc_copy(new_func, g_create_template);
		g_create_template = NULL;
	}
	c_editmode(&SYS)->open_vil = new_func;
}

void vifunc_watch(vifunc_t *self, vil_func_cb callback, void *usrptr)
{
	if (!self) return;
	self->watcher = callback;
	self->watcher_usrptr = usrptr;
}

void vicall_unwatch(vicall_t *self)
{
	if (!self) return;
	self->watcher = NULL;
	self->watcher_usrptr = NULL;
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
	                                   nk_rect(0, 0, 1000, 800),
									   NK_WINDOW_BORDER |
									   NK_WINDOW_NO_SCROLLBAR |
									   NK_WINDOW_MOVABLE |
									   NK_WINDOW_CLOSABLE);
	if (res)
	{
		/* allocate complete window space */
		total_space = nk_window_get_content_region(nk);
		nk_layout_row_begin(nk, NK_STATIC, 20, 10);

		nk_layout_row_push(nk, 100);
		if (nk_combo_begin_label(nk, self->name, nk_vec2(100.0f, 500.0f)))
		{
			nk_layout_row_dynamic(nk, 20.0f, 1);
			for(khiter_t k = kh_begin(ctx->funcs); k != kh_end(ctx->funcs); ++k)
			{
				if(!kh_exist(ctx->funcs, k)) continue;
				vifunc_t *t = kh_value(ctx->funcs, k);
				if (t->name[0] == '_' || t->name[0] == '$') continue;
				if (nk_combo_item_label(nk, t->name, NK_TEXT_LEFT))
				{
					c_editmode(&SYS)->open_vil = t;
				}
			}
			nk_combo_end(nk);
		}

		nk_layout_row_push(nk, 140);
		if (nk_button_label(nk, "new empty"))
		{
			g_create_buffer[0] = '\0';
			g_create_dialogue = true;
			g_create_template = NULL;
		}
		for (khiter_t k = kh_begin(ctx->funcs); k != kh_end(ctx->funcs); ++k)
		{
			if(!kh_exist(ctx->funcs, k)) continue;
			vifunc_t *t = kh_value(ctx->funcs, k);
			if (t->name[0] != '$') continue;

			nk_layout_row_push(nk, 100);
			char buffer[256];
			snprintf(buffer, sizeof(buffer), "new %s", t->name + 1);
			if (nk_button_label(nk, buffer))
			{
				g_create_buffer[0] = '\0';
				g_create_dialogue = true;
				g_create_template = t;
			}
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
			g_over_sub_context.depth = 0;
			g_over_context = NULL;
			vicall_t *it;
			/* struct nk_rect size = nk_layout_space_bounds(nk); */
			struct nk_panel *call = 0;

			/* draw each link */
			struct nk_command_buffer *canvas = nk_window_get_canvas(nk);
			for (uint32_t n = 0; n < self->link_count; ++n)
			{
				struct vil_link *link = &self->links[n];

				struct nk_vec2 pi = get_input_position(self, link->into);
				struct nk_vec2 po = get_output_position(self, link->from);

				struct nk_vec2 l0 = nk_layout_space_to_screen(nk, po);
				struct nk_vec2 l1 = nk_layout_space_to_screen(nk, pi);

				l0.x -= scrolling.x;
				l0.y = po.y - scrolling.y + 15.0f;
				l1.x -= scrolling.x;
				l1.y -= scrolling.y + 55.0f;
				nk_stroke_curve(canvas, l0.x, l0.y, l0.x + 50.0f, l0.y,
						l1.x - 50.0f, l1.y, l1.x, l1.y, 2.0f, nk_rgb(10, 10, 10));
			}


			/* execute each call as a movable group */
			for (it = self->begin; it; it = it->next)
			{
				/* calculate scrolled call window position and size */
				if (it->bounds.x - scrolling.x + it->bounds.z < -55.0f) continue;
				if (it->bounds.y - scrolling.y + it->bounds.w < -55.0f) continue;

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
								NK_EDIT_SIG_ENTER | NK_EDIT_GOTO_END_ON_ACTIVATE,
								g_renaming_buffer, sizeof(g_renaming_buffer),
								nk_filter_ascii);
						if ((status & NK_EDIT_COMMITED) || (status & NK_EDIT_DEACTIVATED))
						{
							if (strlen(g_renaming_buffer) > 0)
							{
								strncpy(it->name, g_renaming_buffer,
								        sizeof(it->name));
								vicall_propagate_changed(it, false);
							}
							g_renaming = NULL;
							g_renaming_buffer[0] = '\0';
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

					float h = vicall_gui(it, nk, false, it->bounds.y);
					it->bounds.w = h;
					/* contextual menu */
					nk_group_end(nk);
				}
				nk_style_pop_color(nk);
				nk_style_pop_style_item(nk);

				if (call)
				{
					slot_t sl = {0};
					/* output connector */

					struct nk_vec2 i0 = nk_layout_space_to_screen(nk, nk_vec2(it->bounds.x, it->bounds.y));
					struct nk_vec2 o1 = nk_vec2(i0.x, i0.y - it->bounds.y);

					call_outputs(it, it, sl, i0.x - 5.0f, i0.y + 20.0f, nk, true);

					/* input connector */
					call_inputs(it, it, sl, o1.x - 5.0f, o1.y - 60.0f, nk, false);
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
					linking.depth = 0;
					fprintf(stdout, "linking failed\n");
				}
			}

			if(!g_open_global_context && !g_open_context && (   g_over_sub_context.depth
			                                                 || g_open_sub_context.depth))
			{
				if (nk_contextual_begin(nk, 0, nk_vec2(100, 520),
				                        nk_rect(0, 0, 10000, 10000)))
				{
					if (g_open_sub_context.depth == 0)
					{
						g_open_sub_context = g_over_sub_context;
					}
					nk_layout_row_dynamic(nk, 20, 1);

					vicall_t *open_context = vifunc_get_call(self,
					                                   g_open_sub_context, 0,
					                                   1);
					vicall_t *open_sub = open_context;
					if (g_open_sub_context.depth > 1)
					{
						open_sub = vifunc_get_call(open_context->type,
						                           g_open_sub_context, 1,
						                           g_open_sub_context.depth);
					}

					output_options(open_context, open_sub, g_open_sub_context,
					               nk);

					nk_contextual_end(nk);
				}
				else
				{
					g_open_sub_context.depth = 0;
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
						if(t->name[0] == '_' || t->name[0] == '$') continue;
						if(nk_contextual_item_label(nk, t->name, NK_TEXT_CENTERED))
						{
							char name[64];
							snprintf(name, sizeof(name), "%s%d", t->name,
							         self->call_count);
							g_renaming = vicall_new(self, t, name,
							                        g_context_pos, ~0, V_BOTH);
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
					_create_func(ctx);
				}
				g_create_dialogue = false;
			}
			if (nk_button_label(nk, "create"))
			{
				_create_func(ctx);
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

