#include "bone.h"
#include "skin.h"
#include "../candle.h"
#include "../components/spatial.h"
#include "../components/node.h"
#include "../components/model.h"
#include "../utils/drawable.h"
#include "../utils/mafs.h"
#include "../utils/mesh.h"

static mesh_t *g_bone;
static mesh_t *g_joint;
static mat_t *g_mat;

static void c_bone_init(c_bone_t *self)
{
	if(!g_bone)
	{
#ifdef MESH4
		const vec4_t dir = vec4(0.0f, 0.0f, -1.0f, 0.0f);
#else
		const vec3_t dir = vec3(0.0f, 0.0f, -1.0f);
#endif
		g_mat = mat_new("m", "default");
		mat4f(g_mat, ref("emissive.color"), vec4(0.3f, 0.1f, 0.9f, 0.5f));
		mat4f(g_mat, ref("albedo.color"), vec4(1, 1, 1, 1.0f));

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

	drawable_init(&self->draw, ref("widget"));
	drawable_init(&self->joint, ref("widget"));
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
	vec3_t p;
	float len;
	mat4_t par_model, model;
	c_node_t *par;
	c_node_t *n = c_node(self);
	if(!n->parent || !c_bone(&n->parent))
	{
		drawable_set_mesh(&self->draw, NULL);
		drawable_set_mesh(&self->joint, NULL);
		return CONTINUE;
	}
	par = c_node(&n->parent);

	drawable_set_mesh(&self->draw, g_bone);
	drawable_set_mesh(&self->joint, g_joint);

	c_node_update_model(par);
	c_node_update_model(n);

	p = c_spatial(self)->pos;
	len = vec3_len(p);

	model = mat4_look_at(p, vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
	model = mat4_scale_aniso(mat4_invert(model), vec3(len, len, len));

	par_model = mat4_mul(par->model, model);

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

static void c_bone_destroy(c_bone_t *self)
{
	drawable_set_mesh(&self->draw, NULL);
	drawable_set_mesh(&self->joint, NULL);
}


void ct_bone(ct_t *self)
{
	ct_init(self, "bone", sizeof(c_bone_t));
	ct_set_init(self, (init_cb)c_bone_init);
	ct_set_destroy(self, (destroy_cb)c_bone_destroy);
	ct_add_dependency(self, ct_node);
	ct_add_listener(self, ENTITY, 0, sig("node_changed"), c_bone_position_changed);
}

c_bone_t *c_bone_new(entity_t skin, mat4_t offset)
{
	c_bone_t *self = component_new(ct_bone);
	self->offset = offset;
	self->skin = skin;
	return self;
}

