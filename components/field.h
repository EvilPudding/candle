#ifndef FIELD_H
#define FIELD_H

#include "../ecs/ecm.h"
#include "../utils/drawable.h"

typedef struct
{
	c_t super; /* extends c_t */
	mat_t *mat;
	mesh_t *mesh;
	texture_t *values;
	int visible;
	int cast_shadow;
	int xray;
	vecN_t start;
	vecN_t end;
	float cell_size;
	drawable_t draw;
	bool_t use_geometry_shader;
	float time;
} c_field_t;

DEF_CASTER(ct_field, c_field, c_field_t)

vs_t *field_vs(void);
mesh_t *field_mesh(void);

c_field_t *c_field_new(mat_t *mat, vecN_t start, vecN_t end, float cell_size,
                       bool_t cast_shadow, bool_t use_geometry_shader);

#endif /* !FIELD_H */
