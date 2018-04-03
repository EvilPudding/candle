#include "bone.h"
#include "model.h"
#include "spacial.h"
#include "node.h"


static void c_bone_init(c_bone_t *self)
{
	self->final_transform = mat4();
}

c_bone_t *c_bone_new(int bone_index, mat4_t offset)
{
	c_bone_t *self = component_new("bone");

	self->offset = offset;
	self->bone_index = bone_index;

	return self;
}

int c_bone_spacial_changed(c_bone_t *self)
{
	/* update weights buffer */
	return 1;
}

REG()
{
	ct_t *ct = ct_new("bone", sizeof(c_bone_t), c_bone_init, NULL,
			1, ref("node"));

	ct_listener(ct, ENTITY, sig("spacial_changed"), c_bone_spacial_changed);
}
