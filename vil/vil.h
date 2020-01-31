#ifndef VISUAL_LOGIC_H
#define VISUAL_LOGIC_H

#include <ecs/ecm.h>
#include <utils/mafs.h>
#include <utils/khash.h>
#include <utils/macros.h>
#include <utils/macros.h>

struct vicall;
struct vifunc;

typedef float (*vifunc_gui_cb)(struct vicall *call, void *data, void *nk);
typedef void  (*vicall_save_cb)(struct vicall *call, void *data, FILE *fp);
typedef bool_t(*vicall_load_cb)(struct vicall *call, void *data, int argc, const char **argv);
typedef void  (*vifunc_save_cb)(struct vifunc *func, FILE *fp);
typedef bool_t(*vifunc_load_cb)(struct vifunc *func, FILE *fp);
typedef void  (*vil_func_cb)(struct vifunc *func, void *usrptr);

enum vicall_flags {
	V_IN = 0x001,
	V_OUT = 0x002,
	V_BOTH = V_IN | V_OUT,
	V_HID = 0x004,
	V_ALL = 0x007,
	V_LINKED = 0x008
};

KHASH_MAP_INIT_INT(vifunc, struct vifunc *)

typedef struct vil
{
	khash_t(vifunc) *funcs;
	void *user_data;
	vifunc_save_cb	builtin_save_func;
	vifunc_load_cb	builtin_load_func;
} vil_t;

typedef struct
{
	uint32_t depth;
	uint32_t calls[16];
} slot_t;

struct vil_link
{
	slot_t from;
	slot_t into;
};

struct vil_arg
{
	slot_t from;
	slot_t into;
	uint8_t *data;
	uint8_t *previous_data;
	uint32_t data_size;
	bool_t has_children;
	float y;
	bool_t initialized;
	bool_t alloc_head;
};

struct vil_ret
{
	slot_t from;
	slot_t into;
	float y;
};

typedef void(*vil_call_cb)(struct vicall *call, slot_t slot, void *usrptr);
typedef void(*vil_foreach_input_cb)(struct vicall *root, struct vicall *call,
                                    slot_t slot, uint8_t *data, void *usrptr);
typedef void(*vil_link_cb)(struct vifunc *func, slot_t to, slot_t from,
                           void *usrptr);

typedef struct vicall
{
	struct vifunc *type;
	struct vifunc *parent;

	uint32_t id;
	char name[64];
	struct vicall *next;
	struct vicall *prev;

	bool_t is_hidden;
	bool_t is_input;
	bool_t is_output;
	bool_t is_linked;
	bool_t is_locked;
	uint32_t data_offset;

	uint32_t color;

	vec4_t bounds;

	struct vil_arg *input_args;
	uint32_t input_args_num;
	struct vil_ret *output_args;
	uint32_t output_args_num;

	vil_call_cb     watcher;
	void           *watcher_usrptr;
	void           *usrptr;
} vicall_t;

typedef struct vifunc
{
	char name[32];
	uint32_t id;

	vicall_t *call_buf;
	struct vil_link *links;
	vicall_t *begin;
	vicall_t *end;
	uint32_t call_count;
	uint32_t link_count;
	uint32_t locked;

	uint32_t    	builtin_size;
	vifunc_gui_cb	builtin_gui;
	vicall_save_cb	builtin_save_call;
	vicall_load_cb	builtin_load_call;
	vil_t *ctx;

	vil_func_cb     watcher;
	void           *watcher_usrptr;

	uint32_t tmp;
	void *usrptr;
	bool_t is_assignable;
} vifunc_t;

void vil_init(vil_t *self);
vifunc_t *vil_get(vil_t *ctx, uint32_t ref);
void vil_iterate(vil_t *self, vil_func_cb callback, void *usrptr);
void vil_foreach_func(vil_t *self, vil_func_cb callback, void *usrptr);

void vicall_set_arg(vicall_t *call, uint32_t ref, void *value);
void vifunc_iterate_dependencies(vifunc_t *self,
                                 uint32_t include, uint32_t exclude, 
                                 vil_link_cb link, vil_call_cb call,
                                 void *usrptr);
vicall_t *vicall_new(vifunc_t *parent, vifunc_t *type,
		const char *name, vec2_t pos, uint32_t data_offset, uint32_t flags);
uint32_t vicall_get_size(vicall_t *call);
void vicall_foreach_unlinked_input(vicall_t *call, vil_foreach_input_cb cb,
                                    void *usrptr);
void vicall_iterate_dependencies(vicall_t *self,
                                 uint32_t include, uint32_t exclude, 
								 vil_link_cb link, vil_call_cb call,
								 void *usrptr);

const char *vicall_name(const vicall_t *call);
void vicall_color(vicall_t *self, vec4_t color);
float vicall_gui(vicall_t *call, void *nk, bool_t collapsible, float yb);
void vicall_watch(vicall_t *self, vil_call_cb callback, void *usrptr);
void vicall_unwatch(vicall_t *self);

vifunc_t *vifunc_new(vil_t *ctx, const char *name, vifunc_gui_cb builtin_gui,
                     uint32_t builtin_size, bool_t is_assignable);
void vifunc_destroy(vifunc_t *self);
void vifunc_foreach_call(vifunc_t *self, bool_t recursive, bool_t allow_input,
                         bool_t allow_output, bool_t skip_linked,
                         vil_call_cb callback, void *usrptr);
void vifunc_link(vifunc_t *root, uint32_t from, uint32_t into);
uint32_t vifunc_gui(vifunc_t *type, void *nk);
void vifunc_watch(vifunc_t *self, vil_func_cb callback, void *usrptr);
vicall_t *vifunc_get(vifunc_t *self, const char *name);
void vifunc_save(vifunc_t *self, const char *filename);
bool_t vifunc_load(vifunc_t *self, const char *filename);
void vifunc_slot_to_name(vifunc_t *func, slot_t slot, char *buffer,
                         const char *separator, const char *append);
void vifunc_foreach_unlinked_input(vifunc_t *self, vil_foreach_input_cb cb,
                                   void *usrptr);

slot_t slot_pop(slot_t slt);


#endif /* !VISUAL_LOGIC_H */
