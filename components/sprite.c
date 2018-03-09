#include "../ext.h"
#include "sprite.h"
#include "mesh_gl.h"
#include "node.h"
#include "spacial.h"
#include "light.h"
#include <nk.h>
#include <systems/renderer.h>
#include "shader.h"
#include <systems/editmode.h>

DEC_CT(ct_sprite);

DEC_SIG(mesh_changed);

vs_t *g_sprite_vs = NULL;

static int c_sprite_render_visible(c_sprite_t *self);

static mat_t *g_missing_mat = NULL;
static mesh_t *g_sprite_mesh = NULL;

int c_sprite_menu(c_sprite_t *self, void *ctx);

static void c_sprite_init(c_sprite_t *self)
{
	if(!g_missing_mat)
	{
		g_missing_mat = mat_new("missing");
		mat_set_diffuse(g_missing_mat, (prop_t){.color=vec4(0.0, 0.9, 1.0, 1.0)});

	}
	if(!g_sprite_mesh)
	{
		g_sprite_mesh = mesh_quad();
	}

	self->visible = 1;
}

c_sprite_t *c_sprite_new(mat_t *mat, int cast_shadow)
{
	c_sprite_t *self = component_new(ct_sprite);

	self->cast_shadow = cast_shadow;

	return self;
}

int c_sprite_created(c_sprite_t *self)
{
	g_update_id++;
	entity_signal_same(c_entity(self), mesh_changed, NULL);
	return 1;
}

/* #include "components/name.h" */
static int c_sprite_render_shadows(c_sprite_t *self)
{
	if(self->cast_shadow) c_sprite_render_visible(self);
	return 1;
}

static int c_sprite_render_transparent(c_sprite_t *self)
{
	if(!self->visible) return 1;

	shader_t *shader = vs_bind(g_sprite_vs);
	c_node_t *node = c_node(self);
	if(node)
	{
		shader_update(shader, &node->model);
	}

	c_mesh_gl_draw(c_mesh_gl(self), 1);
	return 1;
}

static int c_sprite_render_visible(c_sprite_t *self)
{
	if(!self->visible) return 1;

	shader_t *shader = vs_bind(g_sprite_vs);
	c_node_t *node = c_node(self);
	if(node)
	{
		shader_update(shader, &node->model);
	}

	c_mesh_gl_draw(c_mesh_gl(self), 0);
	return 1;
}

int c_sprite_scene_changed(c_sprite_t *self, entity_t *entity)
{
	if(self->visible)
	{
		g_update_id++;
	}
	return 1;
}


void c_sprite_register()
{
	signal_init(&mesh_changed, sizeof(mesh_t));

	ct_t *ct = ct_new("c_sprite", &ct_sprite, sizeof(c_sprite_t),
			(init_cb)c_sprite_init, 2, ct_spacial, ct_node);

	ct_listener(ct, ENTITY, entity_created, c_sprite_created);

	ct_listener(ct, WORLD, component_menu, c_sprite_menu);

	ct_listener(ct, ENTITY, spacial_changed, c_sprite_scene_changed);

	ct_listener(ct, WORLD, render_visible, c_sprite_render_visible);

	ct_listener(ct, WORLD, render_transparent, c_sprite_render_transparent);

	ct_listener(ct, WORLD, render_shadows, c_sprite_render_shadows);
}
