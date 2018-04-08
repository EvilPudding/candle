#include "sprite.h"
#include "mesh_gl.h"
#include "model.h"
#include "node.h"
#include "spacial.h"
#include "light.h"
#include <nk.h>
#include <systems/renderer.h>
#include "shader.h"
#include <systems/editmode.h>
#include <candle.h>


vs_t *g_sprite_vs = NULL;

static int c_sprite_render_visible(c_sprite_t *self);

static mat_t *g_missing_mat = NULL;
static mesh_t *g_sprite_mesh = NULL;

/* int c_sprite_menu(c_sprite_t *self, void *ctx); */

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
		g_sprite_vs = vs_new("sprite", 1, vertex_modifier_new(
			"	{\n"
			"		vec4 center = vec4(0.0f, 0.0f, 0.0f, 1.0f);\n"
			"		pos = MVP * center;\n"
			"		vec2 size = vec2(P.x * (screen_size.y / screen_size.x), P.y);\n"
			"		pos = vec4(pos.xy + 0.5 * size, pos.z, pos.w);\n"
			"		vertex_position = (camera.view * M * center).xyz + P.xyz * 0.5;\n"

			"		vertex_normal    = (vec4( N, 0.0f)).xyz;\n"
			"		vertex_tangent   = (vec4(TG, 0.0f)).xyz;\n"
			"		vertex_bitangent = cross(vertex_normal, vertex_tangent);\n"
			"		texcoord = vec2(-UV.y, UV.x);\n"

			"		object_id = id;\n"
			"		poly_id = ID;\n"
			"		TM = mat3(vertex_tangent, vertex_bitangent, vertex_normal);\n"
			"	}\n"
		));
	}

	self->visible = 1;
}

c_sprite_t *c_sprite_new(mat_t *mat, int cast_shadow)
{
	c_sprite_t *self = component_new("sprite");

	self->cast_shadow = cast_shadow;

	entity_add_component(c_entity(self),
			c_model_new(g_sprite_mesh, mat, 0, 0));

	return self;
}

int c_sprite_created(c_sprite_t *self)
{
	g_update_id++;
	entity_signal_same(c_entity(self), sig("mesh_changed"), NULL);
	return CONTINUE;
}

/* #include "components/name.h" */
static int c_sprite_render_shadows(c_sprite_t *self)
{
	if(self->cast_shadow) c_sprite_render_visible(self);
	return CONTINUE;
}

static int c_sprite_render_transparent(c_sprite_t *self)
{
	if(!self->visible) return CONTINUE;

	shader_t *shader = vs_bind(g_sprite_vs);
	if(!shader) return STOP;
	c_node_t *node = c_node(self);
	if(node)
	{
		c_node_update_model(node);
		shader_update(shader, &node->model);
	}

	c_mesh_gl_draw(c_mesh_gl(self), 1);
	return CONTINUE;
}

static int c_sprite_render_visible(c_sprite_t *self)
{
	if(!self->visible) return CONTINUE;

	shader_t *shader = vs_bind(g_sprite_vs);
	if(!shader) return STOP;
	c_node_t *node = c_node(self);
	if(node)
	{
		c_node_update_model(node);
		shader_update(shader, &node->model);
	}

	c_mesh_gl_draw(c_mesh_gl(self), 0);
	return CONTINUE;
}

int c_sprite_scene_changed(c_sprite_t *self, entity_t *entity)
{
	if(self->visible)
	{
		g_update_id++;
	}
	return CONTINUE;
}


REG()
{
	ct_t *ct = ct_new("sprite", sizeof(c_sprite_t),
			c_sprite_init, NULL, 1, ref("node"));

	ct_listener(ct, ENTITY, sig("entity_created"), c_sprite_created);

	/* ct_listener(ct, WORLD, component_menu, c_sprite_menu); */

	ct_listener(ct, ENTITY, sig("spacial_changed"), c_sprite_scene_changed);

	ct_listener(ct, WORLD, sig("render_visible"), c_sprite_render_visible);

	ct_listener(ct, WORLD, sig("render_transparent"), c_sprite_render_transparent);

	ct_listener(ct, WORLD, sig("render_shadows"), c_sprite_render_shadows);
}
