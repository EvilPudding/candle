#include "drawable.h"
#include <utils/loader.h>
#include <systems/renderer.h>
#include <candle.h>
#include <string.h>
#include <stdlib.h>

KHASH_MAP_INIT_INT(varray, varray_t*)

static khash_t(varray) *g_varrays;

static int draw_conf_add_instance(draw_conf_t *self, drawable_t *gl);
static void draw_conf_update_inst(draw_conf_t *self, int id);
static int drawable_position_changed(drawable_t *self);
draw_group_t *drawable_get_group(drawable_t *self);

#define MASK_TRANS (1 << 0)
#define MASK_PROPS (1 << 1)

KHASH_MAP_INIT_INT(draw_group, draw_group_t)
khash_t(draw_group) *g_draw_groups;
draw_box_t g_ungrouped;

void drawable_init(drawable_t *self, const char *group)
{
	self->visible = 1;
	self->transform = mat4();

	self->box = -1;
	if(group)
	{
		self->grp = ref(group);
	}
	else
	{
		self->grp = -1;
	}
}

draw_group_t *drawable_get_group(drawable_t *self)
{
	int ret;
	draw_group_t *grp;
	khiter_t k;
	k = kh_get(draw_group, g_draw_groups, self->grp);
	if(k == kh_end(g_draw_groups))
	{
		k = kh_put(draw_group, g_draw_groups, self->grp, &ret);
		grp = &kh_value(g_draw_groups, k);
		*grp = (draw_group_t){ 0 };
	}
	else
	{
		grp = &kh_value(g_draw_groups, k);
	}
	return grp;
}

static void varray_clear(varray_t *self)
{
	self->vert_num = 0;
	self->ind_num = 0;
}

void varray_vert_prealloc(varray_t *self, int size)
{
	self->vert_alloc += size;

	self->pos = realloc(self->pos, self->vert_alloc * sizeof(*self->pos));
	self->nor = realloc(self->nor, self->vert_alloc * sizeof(*self->nor));
	self->tex = realloc(self->tex, self->vert_alloc * sizeof(*self->tex));
	self->tan = realloc(self->tan, self->vert_alloc * sizeof(*self->tan));
	self->col = realloc(self->col, self->vert_alloc * sizeof(*self->col));
	if(self->mesh->skin)
	{
		self->wei = realloc(self->wei, self->vert_alloc * sizeof(*self->wei));
		self->bid = realloc(self->bid, self->vert_alloc * sizeof(*self->bid));
	}
	self->id = realloc(self->id, self->vert_alloc * sizeof(*self->id));
}

void draw_conf_inst_prealloc(draw_conf_t *self, int size)
{
	self->inst_alloc += size;

	self->inst = realloc(self->inst, self->inst_alloc * sizeof(*self->inst));
	self->props = realloc(self->props, self->inst_alloc *
			sizeof(*self->props));
#ifdef MESH4
	self->angle4 = realloc(self->angle4, self->inst_alloc *
			sizeof(*self->angle4));
#endif

	self->comps = realloc(self->comps, self->inst_alloc *
			sizeof(*self->comps));
}
void draw_conf_inst_grow(draw_conf_t *self)
{
	if(self->inst_alloc < self->inst_num)
	{
		self->inst_alloc = self->inst_num;

		draw_conf_inst_prealloc(self, 0);
	}
}

void varray_vert_grow(varray_t *self)
{
	if(self->vert_alloc < self->vert_num)
	{
		self->vert_alloc = self->vert_num;

		varray_vert_prealloc(self, 0);
	}
}

void varray_ind_prealloc(varray_t *self, int size)
{
	self->ind_alloc += size;

	self->ind = realloc(self->ind, self->ind_alloc * sizeof(*self->ind));
}

void varray_ind_grow(varray_t *self)
{
	if(self->ind_alloc < self->ind_num)
	{
		self->ind_alloc = self->ind_num;
		varray_ind_prealloc(self, 0);
	}
}


void drawable_set_mesh(drawable_t *self, mesh_t *mesh)
{
	self->mesh = mesh;
	drawable_model_changed(self);
}

void drawable_set_vs(drawable_t *self, vs_t *vs)
{
	self->vs = vs;
	drawable_model_changed(self);
}

void drawable_set_xray(drawable_t *self, int xray)
{
	self->xray = xray;
	drawable_model_changed(self);
}

void drawable_set_mat(drawable_t *self, mat_t *mat)
{
	self->mat = mat;
	drawable_model_changed(self);
}

#ifdef MESH4
void drawable_set_angel4(drawable_t *self, mat4_t transform)
{
	self->angle4 = angle4;
	drawable_position_changed(self);
}
#endif

void drawable_set_transform(drawable_t *self, mat4_t transform)
{
	self->transform = transform;
	drawable_position_changed(self);
}

void drawable_set_entity(drawable_t *self, entity_t entity)
{
	self->entity = entity;
}

static int draw_conf_add_instance(draw_conf_t *self, drawable_t *draw)
{
	int i = self->inst_num++;
	draw_conf_inst_grow(self);

	self->props[i].x = draw->mat ? draw->mat->id : 0;
	self->props[i].y = draw->mesh ? draw->mesh->has_texcoords : 0;
	self->props[i].zw = entity_to_uvec2(draw->entity);
	self->comps[i] = draw;

	draw->updates = ~0;

	return i;
}

static int varray_add_vert(
		varray_t *self,
		vecN_t p,
		vec3_t n,
		vec2_t t,
		vec3_t tg,
		vec3_t c,
		int id,
		vec4_t wei,
		uvec4_t bid)
{
	/* int i = draw_conf_get_vert(self, p, n, t); */
	/* if(i >= 0) return i; */
	int i = self->vert_num++;
	varray_vert_grow(self);

	self->pos[i] = p;
	self->nor[i] = n;
	self->tan[i] = tg;
	self->tex[i] = t;
	self->col[i] = c;
	self->id[i] = int_to_vec2(id);

	if(self->mesh->skin)
	{
		self->bid[i] = bid;
		self->wei[i] = wei;
		/* printf("%u %u %u %u\n", self->bid[i].x, self->bid[i].y, */
		/* 		self->bid[i].z, self->bid[i].w); */
		/* printf("%f %f %f %f\n", self->wei[i].x, self->wei[i].y, */
		/* 		self->wei[i].z, self->wei[i].w); */
	}


	/* union { */
	/* 	unsigned int i; */
	/* 	struct{ */
	/* 		unsigned char r, g, b, a; */
	/* 	}; */
	/* } convert = {.i = id + 1}; */
	/* printf("%d = [%d]%d(%f) [%d]%d(%f)\n", id + 1, */
	/* 		convert.r, (int)round(self->id[i].x*255), self->id[i].x, */
	/* 		convert.g, (int)round(self->id[i].y*255), self->id[i].y); */

	return i;
}

void varray_add_line(varray_t *self, int v1, int v2)
{
	int i = self->ind_num;

	self->ind_num += 2;
	varray_ind_grow(self);

	self->ind[i + 0] = v1;
	self->ind[i + 1] = v2;
}

void varray_add_triangle(varray_t *self, int v1, int v2, int v3)
{
	int i = self->ind_num;
	self->ind_num += 3;
	varray_ind_grow(self);

	self->ind[i + 0] = v1;
	self->ind[i + 1] = v2;
	self->ind[i + 2] = v3;

	/* vec2_print(self->id[v1]); */
	/* vec2_print(self->id[v2]); */
	/* vec2_print(self->id[v3]); */
	/* printf("\n"); */
}

void varray_edges_to_gl(varray_t *self)
{
	int i;
	mesh_t *mesh = self->mesh;

	varray_ind_prealloc(self, vector_count(mesh->edges) * 2);

	for(i = 0; i < vector_count(mesh->edges); i++)
	{
		edge_t *curr_edge = m_edge(mesh, i);
		if(!curr_edge) continue;

		edge_t *next_edge = m_edge(mesh, curr_edge->next);
		if(!next_edge) continue;

		vertex_t *V1 = e_vert(curr_edge, mesh);
		vertex_t *V2 = e_vert(next_edge, mesh);
		int v1 = varray_add_vert(self, V1->pos,
				vec3(0.0f), vec2(0.0f), vec3(0.0f), V1->color.xyz, 0, vec4(0,0,0,0), uvec4(0,0,0,0));
		int v2 = varray_add_vert(self, V2->pos,
				vec3(0.0f), vec2(0.0f), vec3(0.0f), V2->color.xyz, 0, vec4(0,0,0,0), uvec4(0,0,0,0));

		varray_add_line(self, v1, v2);

	}
}

void varray_face_to_gl(varray_t *self, face_t *f, int id)
{
	mesh_t *mesh = self->mesh;
	int v[4], i;
	for(i = 0; i < f->e_size; i++)
	{
		edge_t *hedge = f_edge(f, i, mesh);
		vertex_t *V = e_vert(hedge, mesh);

		vec4_t  wei = vec4(0,0,0,0);
		uvec4_t bid = uvec4(0,0,0,0);
		if(self->mesh->skin)
		{
			wei = self->mesh->skin->wei[hedge->v];
			bid = self->mesh->skin->bid[hedge->v];
		}
		v[i] = varray_add_vert(self, V->pos, hedge->n,
				hedge->t, hedge->tg, V->color.xyz, id, wei, bid);
	}
	if(f->e_size == 4)
	{
		varray_add_triangle(self, v[0], v[1], v[2]);
		varray_add_triangle(self, v[2], v[3], v[0]);

	}
	else if(f->e_size == 3)
	{
		varray_add_triangle(self, v[0], v[1], v[2]);
	}
	else
	{
		printf("%d\n", f->e_size);
	}

}

/* void draw_conf_get_tg_bt(draw_conf_t *self) */
/* { */
/* 	int i; */

/* 	/1* memset(self->tan, 0, self->ind_num * sizeof(*self->tan)); *1/ */
/* 	/1* memset(self->bit, 0, self->ind_num * sizeof(*self->bit)); *1/ */

/* 	for(i = 0; i < self->ind_num; i += 3) */
/* 	{ */
/* 		int i0 = self->ind[i + 0]; */
/* 		int i1 = self->ind[i + 1]; */
/* 		int i2 = self->ind[i + 2]; */
/* 		self->tan[i0] = vec3(0.0f); */
/* 		self->tan[i1] = vec3(0.0f); */
/* 		self->tan[i2] = vec3(0.0f); */

/* 		/1* self->bit[i0] = vec3(0.0f); *1/ */
/* 		/1* self->bit[i1] = vec3(0.0f); *1/ */
/* 		/1* self->bit[i2] = vec3(0.0f); *1/ */
/* 	} */
/* 	for(i = 0; i < self->ind_num; i += 3) */
/* 	{ */
/* 		int i0 = self->ind[i + 0]; */
/* 		int i1 = self->ind[i + 1]; */
/* 		int i2 = self->ind[i + 2]; */

/* 		vec3_t v0 = ((vec3_t*)self->pos)[i0]; */
/* 		vec3_t v1 = ((vec3_t*)self->pos)[i1]; */
/* 		vec3_t v2 = ((vec3_t*)self->pos)[i2]; */

/* 		vec2_t w0 = ((vec2_t*)self->tex)[i0]; */
/* 		vec2_t w1 = ((vec2_t*)self->tex)[i1]; */
/* 		vec2_t w2 = ((vec2_t*)self->tex)[i2]; */

/* 		vec3_t dp1 = vec3_sub(v1, v0); */
/* 		vec3_t dp2 = vec3_sub(v2, v0); */

/* 		vec2_t duv1 = vec2_sub(w1, w0); */
/* 		vec2_t duv2 = vec2_sub(w2, w0); */

/* 		float r = 1.0f / (duv1.x * duv2.y - duv1.y * duv2.x); */
/* 		vec3_t tangent   = vec3_scale(vec3_sub(vec3_scale(dp1, duv2.y), vec3_scale(dp2, duv1.y)), r); */
/* 		/1* vec3_t bitangent = vec3_scale(vec3_sub(vec3_scale(dp2, duv1.x), vec3_scale(dp1, duv2.x)), r); *1/ */


/* 		self->tan[i0] = vec3_add(self->tan[i0], tangent); */
/* 		self->tan[i1] = vec3_add(self->tan[i1], tangent); */
/* 		self->tan[i2] = vec3_add(self->tan[i2], tangent); */

/* 		/1* self->bit[i0] = vec3_add(self->bit[i0], bitangent); *1/ */
/* 		/1* self->bit[i1] = vec3_add(self->bit[i1], bitangent); *1/ */
/* 		/1* self->bit[i2] = vec3_add(self->bit[i2], bitangent); *1/ */
/* 	} */
/* 	for(i = 0; i < self->ind_num; i++) */
/* 	{ */
/* 		int i0 = self->ind[i]; */
/* 		vec3_t n = self->nor[i0]; */
/* 		vec3_t t = self->tan[i0]; */
/* 		/1* vec3_t b = self->bit[i]; *1/ */

/* 		/1* Gram-Schmidt orthogonalize *1/ */
/* 		t = vec3_norm(vec3_sub(t, vec3_scale(n, vec3_dot(n, t)))); */

/* 		self->tan[i0] = t; */
/* 	} */
/* } */

static void bind_buffer(uint *vbo, int id, uint type, int dim,
		int instanced_divisor)
{
	glBindBuffer(GL_ARRAY_BUFFER, *vbo); glerr();

	// Because OpenGL doesn't support attributes with more than 4
	// elements, each set of 4 elements gets its own attribute.
	uint elementSizeDiv = dim / 4;
	uint remaining = dim % 4;
	uint j;
	for(j = 0; j < elementSizeDiv; j++) {
		glEnableVertexAttribArray(id); glerr();

		if(type == GL_FLOAT)
		{
			glVertexAttribPointer(id, 4, type, GL_FALSE,
					dim * sizeof(GLfloat),
					(const GLvoid*)(sizeof(GLfloat) * j * 4));
		}
		else
		{
			glVertexAttribIPointer(id, 4, type,
					dim * sizeof(GLfloat),
					(const GLvoid*)(sizeof(GLfloat) * j * 4));
		}
		glerr();

		glVertexAttribDivisor(id, instanced_divisor);
		glerr();
		id++;
	}
	if(remaining > 0)
	{
		glEnableVertexAttribArray(id);
		if(type == GL_FLOAT)
		{
			glVertexAttribPointer(id, remaining, type, GL_FALSE,
					remaining * sizeof(GLfloat),
					(const GLvoid*)(sizeof(GLfloat) * elementSizeDiv * 4));
		}
		else
		{
			glVertexAttribIPointer(id, remaining, type,
					remaining * sizeof(GLfloat),
					(const GLvoid*)(sizeof(GLfloat) * elementSizeDiv * 4));
		}

		glVertexAttribDivisor(id, instanced_divisor);
		glerr();
		id++;
	}
}

static inline void create_buffer(uint *vbo, void *arr,
		int dim, int count, int instanced_divisor)
{
	unsigned int usage = instanced_divisor ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW;
	if(!*vbo)
	{
		glGenBuffers(1, vbo); glerr();
	}
	glBindBuffer(GL_ARRAY_BUFFER, *vbo); glerr();
	glBufferData(GL_ARRAY_BUFFER, dim * sizeof(GLfloat) * count, arr, usage);
	glerr();


}

static inline void update_inst(uint *vbo, int id, void *arr, int dim)
{
	glBindBuffer(GL_ARRAY_BUFFER, *vbo);
	glBufferSubData(GL_ARRAY_BUFFER, id * dim * sizeof(GLfloat), dim *
			sizeof(GLfloat) * 1, arr);
}

static inline void update_buffer(uint *vbo, void *arr, int dim,
		int count, int inst)
{
	glBindBuffer(GL_ARRAY_BUFFER, *vbo);
	glBufferSubData(GL_ARRAY_BUFFER, 0, dim * sizeof(GLfloat) * count, arr);
}


void draw_conf_remove_instance(draw_conf_t *self, int id)
{
	int i = --self->inst_num;

	self->inst[id] = self->inst[i];
	self->props[id] = self->props[i];
#ifdef MESH4
	self->angle4[id] = self->angle4[i];
#endif
	self->comps[id]->updates = ~0;
}

varray_t *varray_get(mesh_t *mesh)
{
	uint p = hash_ptr(mesh);

	varray_t *varray;
	khiter_t k = kh_get(varray, g_varrays, p);
	if(k == kh_end(g_varrays))
	{
		int ret;
		k = kh_put(varray, g_varrays, p, &ret);

		varray = kh_value(g_varrays, k) = calloc(1, sizeof(varray_t));
		varray->mesh = mesh;
	}
	else
	{
		varray = kh_value(g_varrays, k);
	}
	return varray;
}

draw_box_t *drawable_refilter(drawable_t *self)
{
	int b;
	draw_group_t *grp = drawable_get_group(self);

	if(grp->filter)
	{
		b = grp->filter(self);
	}
	else
	{
		b = 0;
	}

	if(b >= 0 || b < 8)
	{
		if(!grp->boxes[b]) grp->boxes[b] = kh_init(config);
		return grp->boxes[b];
	}

	return NULL;
}

draw_conf_t *drawable_get_conf(drawable_t *self)
{
	if(!self->vs) return NULL;
	if(!self->mesh) return NULL;

	draw_conf_t *result;
	struct conf_vars conf = {
		.mesh = self->mesh,
		/* .transparent = self->mat ? */
		/* 	self->mat->transparency.color.a > 0.0f || */
		/* 	self->mat->transparency.texture || */
		/* 	self->mat->emissive.color.a > 0.0f || */
		/* 	self->mat->emissive.texture */
		/* 	: 0, */
		.xray = self->xray,
		.vs = self->vs
	};

	if(self->grp == -1)
	{
		if(!self->conf)
		{
			result = calloc(1, sizeof(*self->conf));
		}
		else
		{
			result = self->conf;
		}
		result->vars = conf;
		return result;
	}

	draw_box_t *box = drawable_refilter(self);
	if(box == NULL) return NULL;


	uint p = murmur_hash(&conf, sizeof(conf), 0);

	khiter_t k = kh_get(config, box, p);
	if(k == kh_end(box))
	{
		int ret;
		k = kh_put(config, box, p, &ret);

		result = kh_value(box, k) = calloc(1, sizeof(draw_conf_t));
		result->vars = conf;
	}
	else
	{
		result = kh_value(box, k);
	}
	return result;
}

int drawable_model_changed(drawable_t *self)
{
	draw_conf_t *conf = drawable_get_conf(self);

	/* NON BUFFEREABLE INSTANCE PROPERTIES */
	if(conf != self->conf)
	{
		if(self->conf)
		{
			draw_conf_remove_instance(self->conf, self->instance_id);
			self->conf = NULL;
		}

		if(conf)
		{
			self->conf = conf;
			self->instance_id = draw_conf_add_instance(self->conf, self);

			drawable_position_changed(self);

		}

	}
	if(!conf) return CONTINUE;

	int matid = 0;
	/* BUFFEREABLE INSTANCE PROPERTIES */ 
	uvec4_t *props = &self->conf->props[self->instance_id];
	uvec2_t new_ent = entity_to_uvec2(self->entity);
	if(self->mat)
	{
		matid = self->mat->id;
	}

	uvec4_t new_props = uvec4(
			matid,
			self->mesh ? self->mesh->has_texcoords : 0,
			new_ent.x,
			new_ent.y);

	if(!uvec4_equals(*props, new_props))
	{
		*props = new_props;
		self->updates |= MASK_PROPS;
	}

	return CONTINUE;
}

#ifdef MESH4
vec4_t get_cell_normal(mesh_t *mesh, cell_t *cell)
{
	face_t *f = c_face(cell, 0, mesh);

	edge_t *e0 = f_edge(f, 0, mesh);

	vec4_t p0 = f_vert(f, 0, mesh)->pos;
	vec4_t p1 = f_vert(f, 1, mesh)->pos;
	vec4_t p2 = f_vert(f, 2, mesh)->pos;
	vec4_t p3 = e_vert(e_prev(e_pair(e0, mesh), mesh), mesh)->pos;

	vec4_t n = vec4_unit(vec4_cross(
				vec4_sub(p0, p1),
				vec4_sub(p2, p0),
				vec4_sub(p0, p3)));

	return n;

}
#endif

int varray_update_ram(varray_t *self)
{
	mesh_t *mesh = self->mesh;
	varray_clear(self);

	/* mat_layer_t *layer = &model->layers[self->layer_id]; */
	varray_vert_prealloc(self, vector_count(mesh->verts));

	/* int selection = layer->selection; */

	vector_t *faces = mesh->faces;
	vector_t *edges = mesh->edges;

	/* mesh_selection_t *sel = &mesh->selections[selection]; */
	/* if(selection != -1) */
	/* { */
		/* return 1; */
	/* 	faces = sel->faces; */
	/* 	edges = sel->edges; */
	/* } */

	int i;
	if(vector_count(faces))
	{
		mesh_update_smooth_normals(mesh, 1.0f - mesh->smooth_angle);
		int triangle_count = 0;
		for(i = 0; i < vector_count(faces); i++)
		{
			int id = i;
			face_t *face = m_face(mesh, id); if(!face) continue;

			if(face->e_size == 3) triangle_count++;
			else triangle_count += 2;
		}
		varray_ind_prealloc(self, triangle_count * 3);

		for(i = 0; i < vector_count(faces); i++)
		{
			int id = i;
			face_t *face = m_face(mesh, id); if(!face) continue;

#ifdef MESH4

			/* if(face->pair != -1) */
			/* { */
			/* 	cell_t *cell1 = f_cell(f_pair(face, mesh), mesh); */
			/* 	cell_t *cell0 = f_cell(face, mesh); */
			/* 	if(cell0 && cell1) */
			/* 	{ */
			/* 		vec4_t n0 = get_cell_normal(mesh, cell0); */
			/* 		vec4_t n1 = get_cell_normal(mesh, cell1); */
			/* 		float dot = vec4_dot(n1, n0); */
			/* 		if(dot > 0.999) continue; */
			/* 	} */
			/* } */

#endif

			varray_face_to_gl(self, face, id);
		}
	}
	else if(vector_count(edges))
	{
		varray_edges_to_gl(self);
	}
	return 1;
}

static void varray_update_buffers(varray_t *self)
{
	uint *vbo = self->vbo;
	if(self->vert_num > self->vert_num_gl)
	{
		self->vert_num_gl = self->vert_num;

		/* VERTEX BUFFER */
		create_buffer(&vbo[0], self->pos, N, self->vert_num, 0);

		/* NORMAL BUFFER */
		create_buffer(&vbo[1], self->nor, 3, self->vert_num, 0);

		/* TEXTURE COORDS BUFFER */
		create_buffer(&vbo[2], self->tex, 2, self->vert_num, 0);

		/* TANGENT BUFFER */
		create_buffer(&vbo[3], self->tan, 3, self->vert_num, 0);

		/* ID BUFFER */
		create_buffer(&vbo[4], self->id, 2, self->vert_num, 0);

		/* COLOR BUFFER */
		create_buffer(&vbo[5], self->col, 3, self->vert_num, 0);

		if(self->mesh->skin)
		{
			/* BONEID BUFFER */
			create_buffer(&vbo[6], self->bid, 4, self->vert_num, 0);

			/* BONE WEIGHT BUFFER */
			create_buffer(&vbo[7], self->wei, 4, self->vert_num, 0);
		}
	}
	if(self->ind_num > self->ind_num_gl)
	{
		self->ind_num_gl = self->ind_num;
		if(!vbo[15]) glGenBuffers(1, &vbo[15]);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo[15]);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, self->ind_num *
				sizeof(*self->ind), self->ind, GL_STATIC_DRAW);
		glerr();

	}

	if(self->ind_num_gl)
	{
		/* VERTEX BUFFER */
		update_buffer(&vbo[0], self->pos, N, self->vert_num, 0); glerr();

		/* NORMAL BUFFER */
		update_buffer(&vbo[1], self->nor, 3, self->vert_num, 0); glerr();

		/* TEXTURE COORDS BUFFER */
		update_buffer(&vbo[2], self->tex, 2, self->vert_num, 0); glerr();

		/* TANGENT BUFFER */
		update_buffer(&vbo[3], self->tan, 3, self->vert_num, 0); glerr();

		/* ID BUFFER */
		update_buffer(&vbo[4], self->id, 2, self->vert_num, 0); glerr();

		/* COLOR BUFFER */
		update_buffer(&vbo[5], self->col, 3, self->vert_num, 0); glerr();

		if(self->mesh->skin)
		{
			/* BONEID BUFFER */
			update_buffer(&vbo[6], self->bid, 4, self->vert_num, 0); glerr();

			/* BONE WEIGHT BUFFER */
			update_buffer(&vbo[7], self->wei, 4, self->vert_num, 0); glerr();
		}

		/* INDEX BUFFER */
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo[15]);
		glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0,
				self->ind_num_gl * sizeof(*self->ind), self->ind);
		glerr();

	}
	self->updating = 0;
}

void varray_update(varray_t *self)
{
	if(!self->mesh || self->mesh->locked_read) return;

	if(self->update_id_ram != self->mesh->update_id)
	{
		mesh_lock_write(self->mesh);

		self->updating = 1;
		glBindVertexArray(0); glerr();

		varray_update_ram(self);
		varray_update_buffers(self);

		self->update_id_ram = self->mesh->update_id;
		mesh_unlock_write(self->mesh);
	}
}

static void varray_bind(varray_t *self)
{
	uint *vbo = self->vbo;
	/* VERTEX BUFFER */
	bind_buffer(&vbo[0], 0, GL_FLOAT, N, 0);

	/* NORMAL BUFFER */
	bind_buffer(&vbo[1], 1, GL_FLOAT, 3, 0);

	/* TEXTURE COORDS BUFFER */
	bind_buffer(&vbo[2], 2, GL_FLOAT, 2, 0);

	/* TANGENT BUFFER */
	bind_buffer(&vbo[3], 3, GL_FLOAT, 3, 0);

	/* ID BUFFER */
	bind_buffer(&vbo[4], 4, GL_UNSIGNED_INT, 2, 0);

	/* COLOR BUFFER */
	bind_buffer(&vbo[5], 5, GL_FLOAT, 3, 0);

	if(self->mesh->skin)
	{
		/* BONEID BUFFER */
		bind_buffer(&vbo[6], 6, GL_UNSIGNED_INT, 4, 0);

		/* BONE WEIGHT BUFFER */
		bind_buffer(&vbo[7], 7, GL_FLOAT, 4, 0);
	}

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo[15]);
}

static void draw_conf_update_vao(draw_conf_t *self)
{
	if(!self->vbo[8]) return;

	if(!self->vao) glGenVertexArrays(1, &self->vao);
	glBindVertexArray(self->vao); glerr();
	varray_bind(self->varray);

	/* TRANSFORM BUFFER */
	bind_buffer(&self->vbo[8], 8, GL_FLOAT, 16, 1);

	/* PROPERTY BUFFER */
	bind_buffer(&self->vbo[12], 12, GL_UNSIGNED_INT, 4, 1);

#ifdef MESH4
	/* 4D ANGLE BUFFER */
	bind_buffer(&self->vbo[13], 13, GL_FLOAT, 1, 1);
#endif
	glBindVertexArray(0); glerr();
}

void draw_conf_update(draw_conf_t *self)
{
	if(!self->varray)
	{
		self->varray = varray_get(self->vars.mesh);
	}
	varray_update(self->varray);

	draw_conf_update_vao(self);
}


int draw_conf_draw(draw_conf_t *self, int instance_id)
{

	mesh_t *mesh = self->vars.mesh;
	c_renderer_t *renderer = c_renderer(&SYS);
	/* printf("%d %p %d %s\n", self->vars.transparent, self->vars.mesh, */
			/* self->vars.xray, self->vars.vs->name); */

	draw_conf_update(self);
	draw_conf_update_inst(self, instance_id);

	vs_bind(self->vars.vs);

	int cull_face;
	if(mesh->cull_front)
	{
		if(mesh->cull_back)
		{
			cull_face = GL_FRONT_AND_BACK;
		}
		else
		{
			cull_face = GL_FRONT;
		}
	}
	else if(mesh->cull_back)
	{
		cull_face = GL_BACK;
	}
	else
	{
		cull_face = GL_NONE;
	}

	/* glPolygonOffset(0.0f, self->layers[self->layer_id].offset); */
	glerr();

	/* c_renderer_t *renderer = c_renderer(&SYS); */
	/* if(self->vars.transparent) */
	/* { */
	/* 	if(renderer->depth_inverted) */
	/* 	{ */
	/* 		glDepthFunc(GL_GEQUAL); */
	/* 	} */
	/* 	else */
	/* 	{ */
	/* 		glDepthFunc(GL_LEQUAL); */
	/* 	} */
	/* } */
	/* else */
	/* { */
		if(renderer->depth_inverted)
		{
			glDepthFunc(GL_GREATER);
		}
		else
		{
			glDepthFunc(GL_LESS);
		}
	/* } */

	int cull_was_enabled = glIsEnabled(GL_CULL_FACE);
	if(cull_face == GL_NONE)
	{
		glDisable(GL_CULL_FACE); glerr();
	}
	else
	{
		glCullFace(cull_face); glerr();
	}

	if(self->vbo[8] == 0) return 0;
	if(self->vao == 0) return 0;

	/* printf("%u\n", self->vao); */
	glBindVertexArray(self->vao); glerr();
	glerr();


	if(mesh->wireframe)
	{
		glLineWidth(2);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}
	else glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glerr();

	uint primitive;
	if(vector_count(mesh->faces))
	{
		primitive = GL_TRIANGLES;
	}
	else
	{
		glLineWidth(3); glerr();
		/* glLineWidth(5); */
		primitive = GL_LINES;
	}

	if(instance_id == -1)
	{
		glDrawElementsInstancedBaseInstance(primitive,
				self->varray->ind_num_gl, GL_UNSIGNED_INT, 0, self->inst_num, 0);
		glerr();
	}
	else
	{
		glDrawElementsInstancedBaseInstance(primitive,
				self->varray->ind_num_gl, GL_UNSIGNED_INT, 0, 1, instance_id);
		glerr();
	}
	/* } */

	if(cull_was_enabled)
	{
		glEnable(GL_CULL_FACE);
	}
	/* printf("/GLG_DRAW\n"); */
	return CONTINUE;

}

int drawable_draw(drawable_t *self)
{
	if(!self->conf) return CONTINUE;
	/* int i; */
	int res = CONTINUE;

	if(self->xray)
	{
		glDepthRange(0, 0.01);
	}

	/* if(self->conf->vars.scale_dist > 0.0f) */
	/* { */
	/* 	c_node_update_model(node); */

	/* 	mat4_t trans = node->model; */

	/* 	vec3_t pos = mat4_mul_vec4(trans, vec4(0,0,0,1)).xyz; */
	/* 	float dist = vec3_dist(pos, c_renderer(&SYS)->inputs.camera.pos); */
	/* 	trans = mat4_scale_aniso(trans, vec3(dist * self->conf->vars.scale_dist)); */

	/* 	self->conf->inst[self->instance_id] = trans; */

	/* 	self->conf->comps[self->instance_id]->updates |= MASK_TRANS; */
	/* } */
	/* TODO */
	/* if(self->instance_id != 0) return CONTINUE; */


	/* for(i = 0; i < self->groups_num; i++) */
	/* { */
		res &= draw_conf_draw(self->conf, self->instance_id);
	/* } */
	glDepthRange(0.0, 1.00);

	return res;
}

static void draw_conf_update_inst_single(draw_conf_t *self, int id)
{
	int flags = self->comps[id]->updates;
	if(flags & MASK_TRANS)
	{
		/* TRANSFORM BUFFER */
		update_inst(&self->vbo[8], id, self->inst, 16); glerr();

#ifdef MESH4
		/* 4D ANGLE BUFFER */
		update_inst(&self->vbo[13], id, self->angle4, 1); glerr();
#endif
	}

	if(flags & MASK_PROPS)
	{
		/* PROPERTY BUFFER */
		update_inst(&self->vbo[12], id, self->props, 4); glerr();
	}

	self->comps[id]->updates = 0;
}

static void draw_conf_update_inst(draw_conf_t *self, int id)
{
	if(self->inst_num > self->gl_inst_num)
	{
		self->gl_inst_num = self->inst_num;

		/* TRANSFORM BUFFER */
		create_buffer(&self->vbo[8], self->inst, 16, self->inst_num, 1);

		/* MATERIAL BUFFER */
		create_buffer(&self->vbo[12], self->props, 4, self->inst_num, 1);

#ifdef MESH4
		/* 4D ANGLE BUFFER */
		create_buffer(&self->vbo[13], self->angle4, 1, self->inst_num, 1);
#endif
	}


	if(id == -1)
	{
		int i;
		for(i = 0; i < self->inst_num; i++)
		{
			draw_conf_update_inst_single(self, i);
		}
	}
	else
	{
		draw_conf_update_inst_single(self, id);
	}
}

void varray_destroy(varray_t *self)
{
	int i;
	for(i = 0; i < 24; i++) if(self->vbo[i]) glDeleteBuffers(1, &self->vbo[i]);
	if(self->tex) free(self->tex);
	if(self->nor) free(self->nor);
	if(self->pos) free(self->pos);
	if(self->tan) free(self->tan);
	if(self->col) free(self->col);
	if(self->id) free(self->id);
	if(self->bid) free(self->bid);
	if(self->wei) free(self->wei);
	if(self->ind) free(self->ind);


}

void draw_conf_destroy(draw_conf_t *self)
{
	int i;
	if(self->vao)
	{
		glDeleteVertexArrays(1, &self->vao);
	}
	self->vao = 0;
	for(i = 0; i < 24; i++) if(self->vbo[i]) glDeleteBuffers(1, &self->vbo[i]);
#ifdef MESH4
	if(self->angle4) free(self->angle4);
#endif
	if(self->inst) free(self->inst);
	if(self->props) free(self->props);
	if(self->comps) free(self->comps);

	if(self->varray) varray_destroy(self->varray);


}

static int drawable_position_changed(drawable_t *self)
{
	if(self->conf)
	{
		/* printf("update position %p\n", self); */
		self->conf->inst[self->instance_id] = self->transform;
#ifdef MESH4
		self->conf->angle4[self->instance_id].y = self->angle4;
#endif
		self->conf->comps[self->instance_id]->updates |= MASK_TRANS;
	}
	return CONTINUE;
}

void drawable_destroy(drawable_t *self)
{
	/* int i; */
	/* for(i = 0; i < self->groups_num; i++) */
	/* { */
		draw_conf_destroy(self->conf);
	/* } */
	/* free(self->groups); */
}

void draw_box_draw(draw_box_t *self)
{
	khiter_t k;
	for(k = kh_begin(self); k != kh_end(self); ++k)
	{
		if(!kh_exist(self, k)) continue;
		draw_conf_t *conf = kh_value(self, k);
		draw_conf_draw(conf, -1);
	}
}

void draw_group(int ref, int filter)
{
	int b;
	draw_group_t *group;
	khiter_t k;
	
	k = kh_get(draw_group, g_draw_groups, ref);

	if(k == kh_end(g_draw_groups)) return;

	group = &kh_value(g_draw_groups, k);

	if(filter > -1)
	{
		draw_box_draw(group->boxes[filter]);
	}
	else
	{
		for(b = 0; b < 8; b++)
		{
			if(group->boxes[b])
			{
				draw_box_draw(group->boxes[b]);
			}
		}
	}

}

REG()
{
	g_varrays = kh_init(varray);
	g_draw_groups = kh_init(draw_group);
}
