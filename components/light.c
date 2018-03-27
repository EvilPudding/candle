#include "light.h"
#include "node.h"
#include "probe.h"
#include "spacial.h"
#include "candle.h"
#include <systems/editmode.h>
#include <systems/renderer.h>
#include <stdlib.h>

DEC_SIG(render_shadows);

static fs_t *g_depth_fs = NULL;

void c_light_init(c_light_t *self)
{
	self->color = vec4(1.0f);
	self->intensity = 1;
	self->shadow_size = 512;
	if(!g_depth_fs) g_depth_fs = fs_new("depth");
	self->ambient = 0.0f;
	self->radius = 5.0f;
}

c_light_t *c_light_new(float intensity, float ambient, float radius,
		vec4_t color, int shadow_size)
{
	c_light_t *self = component_new(ct_light);

	self->intensity = intensity;
	self->color = color;
	self->shadow_size = shadow_size;
	self->ambient = ambient;
	self->radius = radius;

	return self;
}

int c_light_created(c_light_t *self)
{
	entity_add_component(c_entity(self), (c_t*)c_probe_new(self->shadow_size));

	c_probe_update_position(c_probe(self));

	return 1;
}

int c_light_menu(c_light_t *self, void *ctx)
{

	union {
		struct nk_colorf *nk;
		vec4_t *v;
	} color = { .v = &self->color };

	nk_layout_row_dynamic(ctx, 180, 1);
	*color.nk = nk_color_picker(ctx, *color.nk, NK_RGBA);

	nk_layout_row_dynamic(ctx, 0, 1);
	nk_property_float(ctx, "intensity:", 0, &self->intensity, 1000, 0.1, 0.05);
	nk_property_float(ctx, "ambient:", 0, &self->ambient, 1.0f, 0.1, 0.05);
	nk_property_float(ctx, "radius:", 0, &self->radius, 1000, 0.1, 0.05);

	return 1;
}

void c_light_destroy(c_light_t *self)
{
	free(self);
}

int c_light_render(c_light_t *self)
{
	c_probe_t *probe = c_probe(self);
	if(!probe) return 0;
	if(!g_depth_fs) return 0;

	fs_bind(g_depth_fs);

	c_probe_render(probe, render_shadows);
	return 1;
}

DEC_CT(ct_light)
{
	ct_t *ct = ct_new("c_light", &ct_light,
			sizeof(c_light_t), (init_cb)c_light_init, 2, ct_spacial, ct_node);

	ct_listener(ct, ENTITY, entity_created, c_light_created);

	ct_listener(ct, WORLD, offscreen_render, c_light_render);

	ct_listener(ct, WORLD, component_menu, c_light_menu);

	signal_init(&render_shadows, 0);
}
