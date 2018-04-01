#include "light.h"
#include "node.h"
#include "probe.h"
#include "spacial.h"
#include "model.h"
#include "mesh_gl.h"
#include <candle.h>
#include <systems/editmode.h>
#include <systems/window.h>
#include <systems/renderer.h>
#include <stdlib.h>

static fs_t *g_depth_fs = NULL;
entity_t g_light = entity_null;

void c_light_init(c_light_t *self)
{
	self->color = vec4(1.0f);
	self->intensity = 1;
	self->shadow_size = 512;
	self->radius = 5.0f;

	if(!g_light)
	{
		g_depth_fs = fs_new("depth");
		g_light = entity_new(c_node_new(),
				c_model_new(sauces_mesh("light_mesh.obj"), NULL, 0));
		c_model(&g_light)->visible = 0;

	}
}

c_light_t *c_light_new(float intensity, float radius,
		vec4_t color, int shadow_size)
{
	c_light_t *self = component_new("c_light");

	self->intensity = intensity;
	self->color = color;
	self->shadow_size = shadow_size;
	self->radius = radius;

	return self;
}

int c_light_render(c_light_t *self)
{
	c_renderer(&candle->systems)->bound_light = c_entity(self);

	if(!g_light || !c_mesh_gl(&g_light)) return 0;
	if(self->radius > 0.0f)
	{
		shader_t *shader = vs_bind(g_model_vs);
		if(!shader) return 0;
		c_node_t *node = c_node(self);
		c_spacial_t *sc = c_spacial(self);

		c_node_update_model(node);

		float rad = self->radius * 1.1f;
		/* float rad = self->radius * 0.5f; */
		mat4_t model = mat4_scale_aniso(node->model,
				rad / sc->scale.x, rad / sc->scale.y, rad / sc->scale.z);

		/* mat4_t model = node->model; */
		shader_update(shader, &model);

		c_mesh_gl_draw(c_mesh_gl(&g_light), 0);
		return 1;
	}
	else
	{
		return c_window_render_quad(c_window(&candle->systems), NULL);
	}
}


int c_light_menu(c_light_t *self, void *ctx)
{
	nk_layout_row_dynamic(ctx, 0, 1);
	nk_property_float(ctx, "intensity:", 0, &self->intensity, 1000, 0.1, 0.05);

	int ambient = self->radius == -1.0f;
	nk_checkbox_label(ctx, "ambient", &ambient);

	if(!ambient)
	{
		if(self->radius < 0.0f) self->radius = 0.01;
		nk_property_float(ctx, "radius:", 0.01, &self->radius, 1000, 0.1, 0.05);
	}
	else
	{
		self->radius = -1;
	}

	nk_layout_row_dynamic(ctx, 180, 1);

	union { struct nk_colorf *nk; vec4_t *v; } color = { .v = &self->color };
	*color.nk = nk_color_picker(ctx, *color.nk, NK_RGBA);

	return 1;
}

void c_light_destroy(c_light_t *self)
{
	free(self);
}

int c_light_probe_render(c_light_t *self)
{
	if(self->radius < 0.0f) return 1.0f;
	c_probe_t *probe = c_probe(self);
	if(!probe)
	{
		entity_add_component(c_entity(self), (c_t*)c_probe_new(self->shadow_size));
		entity_signal(c_entity(self), sig("spacial_changed"), &c_entity(self));
		return 0;
	}
	if(!g_depth_fs) return 0;

	fs_bind(g_depth_fs);

	glDisable(GL_CULL_FACE);
	c_probe_render(probe, sig("render_shadows"));
	glEnable(GL_CULL_FACE);
	return 1;
}

REG()
{
	ct_t *ct = ct_new("c_light", sizeof(c_light_t), (init_cb)c_light_init,
			2, ref("c_spacial"), ref("c_node"));

	ct_listener(ct, WORLD, sig("offscreen_render"), c_light_probe_render);

	ct_listener(ct, WORLD, sig("component_menu"), c_light_menu);

	ct_listener(ct, WORLD, sig("render_lights"), c_light_render);

	signal_init(sig("render_shadows"), 0);
}
