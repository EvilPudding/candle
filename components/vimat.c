#include "vimat.h"
#include <systems/editmode.h>

static vil_t g_mat_ctx;

c_vimat_t *c_vimat_new()
{
	c_vimat_t *self = component_new("vimat");
	return self;
}


static void c_vimat_init(c_vimat_t *self)
{
	self->vil = vil_get_type(&g_mat_ctx, ref("parent"));
}

static void color_gui(vicall_t *call, struct nk_colorf *color, void *ctx) 
{
	nk_layout_row_dynamic(ctx, 29, 1);
	if (nk_combo_begin_color(ctx, nk_rgb_cf(*color), nk_vec2(200,400))) {
		nk_layout_row_dynamic(ctx, 120, 1);
		*color = nk_color_picker(ctx, *color, NK_RGBA);

		nk_combo_end(ctx);
	}
}

static void number_gui(vicall_t *call, float *num, void *ctx) 
{
	/* if(*num < 29) *num = 29; */
	/* nk_layout_row_begin(ctx, NK_DYNAMIC, *num, 2); */
	nk_layout_row_begin(ctx, NK_DYNAMIC, 29, 2);
	nk_layout_row_push(ctx, 0.20);
	nk_label(ctx, vicall_name(call), NK_TEXT_LEFT);
	nk_layout_row_push(ctx, 0.80);
	*num = nk_propertyf(ctx, "#", 0, *num, 1, 0.01, 0.01);
	nk_layout_row_end(ctx);
	/* nk_slider_int(ctx, 29, num, 200, 1); */
}

int c_vimat_menu(c_vimat_t *self, void *nk)
{
	if(nk_button_label(nk, "vil"))
	{
		c_editmode(&SYS)->open_vil = self->vil;
	}
	return CONTINUE;
}

REG()
{
	vicall_t *r, *g, *b, *a;
	vil_t *ctx = &g_mat_ctx;
	vil_context_init(ctx, NULL, NULL);


	vitype_t *tnum = vil_add_type(ctx, "number",
			(vitype_gui_cb)number_gui, sizeof(float));

	vitype_t *tcol = vil_add_type(ctx, "_color_picker",
			(vitype_gui_cb)color_gui, sizeof(vec4_t));

	vitype_t *test = vil_add_type(ctx, "test", NULL, 0);
		vitype_add(test, tnum, "A", vec2(40, 10), V_IN);
		vitype_add(test, tnum, "B", vec2(40, 260), V_IN);
		vitype_add(test, tnum, "C", vec2(200, 260), V_OUT);

	vitype_t *rgba = vil_add_type(ctx, "rgba", NULL, sizeof(vec4_t));
		vitype_add(rgba, tcol, "color", vec2(100, 10), V_IN);
		r = vitype_add(rgba, tnum, "r", vec2(100, 10), V_ALL);
		g = vitype_add(rgba, tnum, "g", vec2(200, 10), V_ALL);
		b = vitype_add(rgba, tnum, "b", vec2(300, 10), V_ALL);
		a = vitype_add(rgba, tnum, "a", vec2(300, 10), V_ALL);
		vicall_color(r, vec4(1, 0, 0, 1));
		vicall_color(g, vec4(0, 1, 0, 1));
		vicall_color(b, vec4(0, 0, 1, 1));
		vicall_color(a, vec4(1, 1, 1, 1));

	vitype_t *par = vil_add_type(ctx, "parent", NULL, 0);

	ct_t *ct = ct_new("vimat", sizeof(c_vimat_t), c_vimat_init, NULL, 0);

	ct_listener(ct, WORLD, sig("component_menu"), c_vimat_menu);
}

