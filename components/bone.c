#include "bone.h"
#include "skin.h"
#include <candle.h>
#include <components/spatial.h>
#include <components/node.h>
#include <components/model.h>
#include <utils/drawable.h>
#include <utils/mafs.h>
#include <utils/mesh.h>

static mesh_t *g_bone;
static mesh_t *g_joint;
static mat_t *g_mat;

static void c_bone_init(c_bone_t *self)
{
	if(!g_bone)
	{
		g_mat = mat_new("m");
		mat4f(g_mat, ref("emissive.color"), vec4(0.3f, 0.1f, 0.9f, 0.5f));
		mat4f(g_mat, ref("albedo.color"), vec4(1, 1, 1, 1.0f));

#ifdef MESH4
		vec4_t dir = vec4(0.0f, 0.0f, -1.0f, 0.0f);
#else
		vec3_t dir = vec3(0.0f, 0.0f, -1.0f);
#endif
		g_bone = mesh_new();
		mesh_lock(g_bone);
		mesh_circle(g_bone, 0.01f, 4, dir);
		mesh_extrude_edges(g_bone, 1, vecN_(scale)(dir, 0.05f), 0.05f / 0.01f, NULL, NULL, NULL);
		mesh_extrude_edges(g_bone, 1, vecN_(scale)(dir, 0.95f), 0.01f, NULL, NULL, NULL);
		mesh_unlock(g_bone);

		g_joint = mesh_new();
		mesh_lock(g_joint);
		mesh_ico(g_joint, 0.02f);
		mesh_unlock(g_joint);
	}

	drawable_init(&self->draw, ref("transparent"));
	drawable_init(&self->joint, ref("transparent"));
	drawable_add_group(&self->draw, ref("selectable"));
	drawable_add_group(&self->joint, ref("selectable"));

	/* drawable_set_mesh(&self->draw, g_bone); */
	drawable_set_mesh(&self->joint, g_joint);

	drawable_set_vs(&self->draw, model_vs());
	drawable_set_vs(&self->joint, model_vs());

	drawable_set_xray(&self->draw, true);
	drawable_set_xray(&self->joint, true);

	drawable_set_mat(&self->draw, g_mat);
	drawable_set_mat(&self->joint, g_mat);
}

static int c_bone_position_changed(c_bone_t *self)
{
	c_node_t *n = c_node(self);
	if(!n->parent || !c_bone(&n->parent))
	{
		drawable_set_mesh(&self->draw, NULL);
		drawable_set_mesh(&self->joint, NULL);
		return CONTINUE;
	}
	c_node_t *par = c_node(&n->parent);

	drawable_set_mesh(&self->draw, g_bone);
	drawable_set_mesh(&self->joint, g_joint);

	c_node_update_model(par);
	c_node_update_model(n);

	vec3_t p = c_spatial(self)->pos;
	float len = vec3_len(p);

	mat4_t model = mat4_look_at(p, vec3(0.0f), vec3(0.0f, 1.0f, 0.0f));
	model = mat4_scale_aniso(mat4_invert(model), vec3(len, len, len));

	mat4_t par_model = mat4_mul(par->model, model);

	drawable_set_transform(&self->draw, par_model);
	drawable_set_entity(&self->draw, n->parent);

	/* if(!n->children_size) */
	{
		drawable_set_transform(&self->joint, n->model);
		drawable_set_entity(&self->joint, c_entity(self));
	}
	c_skin_changed(c_skin(&self->skin));

	return CONTINUE;
}

c_bone_t *c_bone_new(entity_t skin, mat4_t offset)
{
	c_bone_t *self = component_new("bone");
	self->offset = offset;
	self->skin = skin;
	return self;
}

REG()
{
	ct_t *ct = ct_new("bone", sizeof(c_bone_t), c_bone_init, NULL, 1,
			ref("node"));
	ct_listener(ct, ENTITY, sig("node_changed"), c_bone_position_changed);
}

