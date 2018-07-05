#include "bone.h"
#include <candle.h>
#include <components/spacial.h>
#include <components/node.h>
#include <components/model.h>
#include <components/mesh_gl.h>
#include <utils/mafs.h>
#include <utils/mesh.h>

entity_t g_bone = entity_null;
entity_t g_end = entity_null;

static void c_bone_init(c_bone_t *self)
{
	if(!g_bone)
	{
		mat_t *m = mat_new("m");
		m->emissive.color = vec4(0.3f, 0.1f, 0.9f, 0.5f);

		vec4_t dir = vec4(0.0f, 0.0f, -1.0f, 0.0f);
		mesh_t *mesh = mesh_new();
		mesh_lock(mesh);
		mesh_circle(mesh, 0.01f, 4, dir);
		mesh_extrude_edges(mesh, 1, vec4_scale(dir, 0.05f), 0.05f / 0.01f, NULL, NULL, NULL);
		mesh_extrude_edges(mesh, 1, vec4_scale(dir, 0.95f), 0.01f, NULL, NULL, NULL);
		mesh_unlock(mesh);

		g_bone = entity_new(c_node_new(), c_model_new(mesh, m, 0, 0));
		c_model(&g_bone)->xray = 1;
		c_node(&g_bone)->ghost = 1;

		mesh = mesh_new();
		mesh_lock(mesh);
		mesh_ico(mesh, 0.02f);
		mesh_unlock(mesh);

		g_end = entity_new(c_node_new(), c_model_new(mesh, m, 0, 0));
		c_model(&g_end)->xray = 1;
		c_node(&g_end)->ghost = 1;
	}
}

c_bone_t *c_bone_new()
{
	c_bone_t *self = component_new("bone");
	return self;
}

int c_bone_render(c_bone_t *self, int flags)
{
	if(!g_bone || !c_mesh_gl(&g_bone)) return STOP;

	shader_t *shader = vs_bind(g_model_vs);
	if(!shader) return STOP;
	c_node_t *n = c_node(self);
	if(!n->parent || !c_bone(&n->parent)) return CONTINUE;
	c_node_t *par = c_node(&n->parent);
	if(!c_bone(par)) return CONTINUE;
	/* c_spacial_t *sc = c_spacial(self); */

	/* vec3_t p = c_spacial(&par->children[0])->pos; */
	vec3_t p = c_spacial(self)->pos;
	float len = vec3_len(p);

	/* mat4_t model = par->model; */
	mat4_t model = mat4_look_at(p, vec3(0.0f), vec3(0.0f, 1.0f, 0.0f));
	model = mat4_invert(model);
	model = mat4_scale_aniso(model, vec3(len, len, len));
	/* model = mat4_translate_in_place(model, p); */
	/* mat4_invert(model); */
	/* mat4_t model = par->model; */

	c_node_update_model(par);
	mat4_t par_model = mat4_mul(par->model, model);

#ifdef MESH4
	shader_update(shader, &par_model, par->angle4);
#else
	shader_update(shader, &par_model);
#endif

	glDepthRange(0, 0.01);
	c_mesh_gl_draw_ent(c_mesh_gl(&g_bone), n->parent, flags);

	if(!n->children_size)
	{
		c_node_update_model(n);
#ifdef MESH4
		shader_update(shader, &n->model, n->angle4);
#else
		shader_update(shader, &n->model);
#endif

		c_mesh_gl_draw_ent(c_mesh_gl(&g_end), c_entity(self), flags);
	}

	return CONTINUE;
}

int c_bone_render_selectable(c_bone_t *self)
{
	return c_bone_render(self, 3);
}

int c_bone_render_transparent(c_bone_t *self)
{
	return c_bone_render(self, 1);
}

REG()
{
	ct_t *ct = ct_new("bone", sizeof(c_bone_t),
			c_bone_init, NULL, 1, ref("node"));
	ct_listener(ct, WORLD, sig("render_transparent"), c_bone_render_transparent);
	ct_listener(ct, WORLD, sig("render_selectable"), c_bone_render_selectable);
}

