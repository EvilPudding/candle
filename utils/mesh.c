#include "mesh.h"
#include "formats/obj.h"
#include "formats/ply.h"
#include <candle.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* #if __has_include (<assimp/cimport.h>) */
/* #include <assimp/cimport.h> */
/* #include <assimp/scene.h> */
/* #include <assimp/postprocess.h> */
/* #endif */

static vec3_t mesh_support(mesh_t *self, const vec3_t dir);
static int mesh_get_pair_edge(mesh_t *self, int e);
int mesh_get_face_from_verts(mesh_t *self, int v0, int v1, int v2);
static void mesh_update_cell_pairs(mesh_t *self);
static void _mesh_destroy(mesh_t *self);
static void _mesh_clone(mesh_t *self, mesh_t *into);

#define CHUNK_CELLS 20
#define CHUNK_VERTS 30
#define CHUNK_EDGES 20
#define CHUNK_FACES 10

void mesh_ico(mesh_t *self, float size)
{
	mesh_lock(self);

	//The golden ratio
	const float t = (1.0f + sqrtf(5.0f)) / 2.0f;

	const float tsize = t * size;

	vec3_t p[] = {
		vec3(-size, tsize, 0), vec3(size, tsize, 0),
		vec3(-size, -tsize, 0), vec3(size, -tsize, 0),
		vec3(0, -size, tsize), vec3(0, size, tsize),
		vec3(0, -size, -tsize), vec3(0, size, -tsize),
		vec3(tsize, 0, -size), vec3(tsize, 0, size),
		vec3(-tsize, 0, -size), vec3(-tsize, 0, size)
	};

	//The number of points horizontally
	const float w = 1.0f / 5.5f;
	//The number of points vertically
	const float h = 1.0f / 3.0f;

	vec2_t uvs[] = {

		vec2(0.5f * w, 0.0f),
		vec2(1.5f * w, 0.0f),
		vec2(2.5f * w, 0.0f),
		vec2(3.5f * w, 0.0f),
		vec2(4.5f * w, 0.0f),

		vec2(0.0f, h),

		vec2(1.0f * w, h),
		vec2(2.0f * w, h),
		vec2(3.0f * w, h),
		vec2(4.0f * w, h),
		vec2(5.0f * w, h),

		vec2(0.5f * w, 2.0f * h),
		vec2(1.5f * w, 2.0f * h),
		vec2(2.5f * w, 2.0f * h),
		vec2(3.5f * w, 2.0f * h),
		vec2(4.5f * w, 2.0f * h),

		vec2(1.0f, 2.0f * h),

		vec2(1.0f * w, 1.0f),
		vec2(2.0f * w, 1.0f),
		vec2(3.0f * w, 1.0f),
		vec2(4.0f * w, 1.0f),
		vec2(5.0f * w, 1.0f),
	};

	int v[12] = {
		mesh_add_vert(self, VEC3(_vec3(p[0]))),
		mesh_add_vert(self, VEC3(_vec3(p[1]))),
		mesh_add_vert(self, VEC3(_vec3(p[2]))),
		mesh_add_vert(self, VEC3(_vec3(p[3]))),
		mesh_add_vert(self, VEC3(_vec3(p[4]))),
		mesh_add_vert(self, VEC3(_vec3(p[5]))),
		mesh_add_vert(self, VEC3(_vec3(p[6]))),
		mesh_add_vert(self, VEC3(_vec3(p[7]))),
		mesh_add_vert(self, VEC3(_vec3(p[8]))),
		mesh_add_vert(self, VEC3(_vec3(p[9]))),
		mesh_add_vert(self, VEC3(_vec3(p[10]))),
		mesh_add_vert(self, VEC3(_vec3(p[11])))
	};

	// 5 faces around point 0
	mesh_add_triangle(self, v[0], p[0], uvs[0], v[11], p[11], uvs[5], v[5], p[5], uvs[6]);
	mesh_add_triangle(self, v[0], p[0], uvs[1], v[5], p[5], uvs[6], v[1], p[1], uvs[7]);
	mesh_add_triangle(self, v[0], p[0], uvs[2], v[1], p[1], uvs[7], v[7], p[7], uvs[8]);
	mesh_add_triangle(self, v[0], p[0], uvs[3], v[7], p[7], uvs[8], v[10], p[10], uvs[9]);
	mesh_add_triangle(self, v[0], p[0], uvs[4], v[10], p[10], uvs[9], v[11], p[11], uvs[10]);
	
	// 5 adjacent faces
	mesh_add_triangle(self, v[1], p[1], uvs[7], v[5], p[5], uvs[6], v[9], p[9], uvs[12]);
	mesh_add_triangle(self, v[5], p[5], uvs[6], v[11], p[11], uvs[5], v[4], p[4], uvs[11]);
	mesh_add_triangle(self, v[11], p[11], uvs[10], v[10], p[10], uvs[9], v[2], p[2], uvs[15]);
	mesh_add_triangle(self, v[10], p[10], uvs[9], v[7], p[7], uvs[8], v[6], p[6], uvs[14]);
	mesh_add_triangle(self, v[7], p[7], uvs[8], v[1], p[1], uvs[7], v[8], p[8], uvs[13]);
	
	// 5 faces around point 3
	mesh_add_triangle(self, v[3], p[3], uvs[17], v[9], p[9], uvs[12], v[4], p[4], uvs[11]);
	mesh_add_triangle(self, v[3], p[3], uvs[21], v[4], p[4], uvs[16], v[2], p[2], uvs[15]);
	mesh_add_triangle(self, v[3], p[3], uvs[20], v[2], p[2], uvs[15], v[6], p[6], uvs[14]);
	mesh_add_triangle(self, v[3], p[3], uvs[19], v[6], p[6], uvs[14], v[8], p[8], uvs[13]);
	mesh_add_triangle(self, v[3], p[3], uvs[18], v[8], p[8], uvs[13], v[9], p[9], uvs[12]);
	
	// 5 adjacent faces
	mesh_add_triangle(self, v[4], p[4], uvs[11], v[9], p[9], uvs[12], v[5], p[5], uvs[6]);
	mesh_add_triangle(self, v[2], p[2], uvs[15], v[4], p[4], uvs[16], v[11], p[11], uvs[10]);
	mesh_add_triangle(self, v[6], p[6], uvs[14], v[2], p[2], uvs[15], v[10], p[10], uvs[9]);
	mesh_add_triangle(self, v[8], p[8], uvs[13], v[6], p[6], uvs[14], v[7], p[7], uvs[8]);
	mesh_add_triangle(self, v[9], p[9], uvs[12], v[8], p[8], uvs[13], v[1], p[1], uvs[7]);

	mesh_unlock(self);
}

void mesh_modified(mesh_t *self)
{
	self->changes++;
}

void mesh_selection_init(mesh_selection_t *self)
{
	self->faces = kh_init(id);
	self->edges = kh_init(id);
	self->verts = kh_init(id);

#ifdef MESH4
	self->cells = kh_init(id);
#endif
}

mesh_t *mesh_new()
{
	mesh_t *self = calloc(1, sizeof *self);

	self->receive_shadows = 1;
	self->cull = 0x2; // CULL_BACK
	self->smooth_angle = 0.2f;
	self->transformation = mat4();
	self->has_texcoords = 1;
	self->triangulated = 1;
	self->current_cell = -1;
	self->current_surface = -1;

	self->faces_hash = kh_init(id);
	self->edges_hash = kh_init(id);
	self->ref_num = 1;

	int i;
	for(i = 0; i < 16; i++)
	{
		mesh_selection_init(&self->selections[i]);
	}

	self->support = (support_cb)mesh_support;

	self->verts = vector_new(sizeof(vertex_t), FIXED_INDEX, NULL, NULL);
	self->faces = vector_new(sizeof(face_t), FIXED_INDEX, NULL, NULL);
	self->edges = vector_new(sizeof(edge_t), FIXED_INDEX, NULL, NULL);
#ifdef MESH4
	self->cells = vector_new(sizeof(cell_t), FIXED_INDEX, NULL, NULL);

	vector_alloc(self->cells, 10);
#endif
	vector_alloc(self->verts, 100);
	vector_alloc(self->faces, 100);
	vector_alloc(self->edges, 100);

	self->semaphore = SDL_CreateSemaphore(1);

	return self;
}

static void vert_init(vertex_t *self)
{
	self->color = vec4(0.0f, 0.0f, 0.0f, 0.0f);
	self->tmp = -1;
	self->half = -1;
	self->wei = vec4(0.0f, 0.0f, 0.0f, 0.0f);
	self->bid = vec4(0.0f, 0.0f, 0.0f, 0.0f);
}


static void edge_init(edge_t *self)
{
	self->pair = -1;
	self->next = -1;
	self->prev = -1;
	self->cell_pair = -1;

	self->tmp = -1;

	self->n = vec3(0.0f, 0.0f, 0.0f);
	self->tg = vec3(0.0f, 0.0f, 0.0f);

	self->v = -1;

	self->face = -1;
}


#ifdef MESH4
static void cell_init(cell_t *self)
{
	self->f_size = 0;
	self->f[0] = self->f[1] = self->f[2] = self->f[3] = -1;
}
#endif

static void face_init(face_t *self)
{
	self->n = vec3(0.0f, 0.0f, 0.0f);
	self->e_size = 0;
	self->tmp = 0;
	self->e[0] = self->e[1] = self->e[2] = -1;

#ifdef MESH4
	self->pair = -1;
	self->cell = -1;
#endif
}



static void _mesh_destroy(mesh_t *self)
{
#ifdef MESH4
	if(self->cells) free(self->cells);
#endif
	if(self->faces) free(self->faces);
	if(self->verts) free(self->verts);
	if(self->edges) free(self->edges);

	/* TODO destroy selections */
	kh_destroy(id, self->faces_hash);
	kh_destroy(id, self->edges_hash);

	int i;
	for(i = 0; i < 16; i++)
	{
		kh_destroy(id, self->selections[i].faces);
		kh_destroy(id, self->selections[i].edges);
		kh_destroy(id, self->selections[i].verts);
		/* vector_destroy(self->selections[i].faces); */
		/* vector_destroy(self->selections[i].edges); */
		/* vector_destroy(self->selections[i].verts); */
#ifdef MESH4
		kh_destroy(id, self->selections[i].cells);
		/* vector_destroy(self->selections[i].cells); */
#endif
	}
}

void mesh_destroy(mesh_t *self)
{
	if(self)
	{
		self->ref_num--;
		if(self->ref_num == 0)
		{
			mesh_lock(self);
			_mesh_destroy(self);
			SDL_DestroySemaphore(self->semaphore);
			self->semaphore = NULL;
			free(self);
		}
	}
}

static vec3_t get_normal(vec3_t p1, vec3_t p2, vec3_t p3)
{
	vec3_t res;

	vec3_t U = vec3_sub(p2, p1);
	vec3_t V = vec3_sub(p3, p1);

    res = vec3(
			U.y * V.z - U.z * V.y,
			U.z * V.x - U.x * V.z,
			U.x * V.y - U.y * V.x);

	return vec3_norm(res);
}

void mesh_update_smooth_normals(mesh_t *self, float smooth_max)
{
	int i;
	for(i = 0; i < vector_count(self->edges); i++)
	{
		edge_t *ee = m_edge(self, i);
		if(!ee) continue;
		ee->tmp = 0;
	}
	for(i = 0; i < vector_count(self->edges); i++)
	/* for(i = 0; i < vector_count(self->verts); i++) */
	{
		edge_t *ee = m_edge(self, i);
		if(!ee) continue;
		if(ee->tmp) continue;
		ee->tmp = 1;
		/* vertex_t *v = e_vert(ee, self); */
		
		/* vertex_t *v = vector_get(self->verts, i); */
		/* if(!v) continue; */

		/* int start = mesh_vert_get_half(self, v); */
		/* start = mesh_edge_rotate_to_unpaired(self, start); */
		int start = i;

		edge_t *edge = m_edge(self, start);
		if(!edge) continue;
		vec3_t start_n = edge->n;
		vec3_t start_t = edge->tg;
		vec3_t smooth_normal = start_n;
		vec3_t smooth_tangent = start_t;
		edge_t *pair = e_pair(edge, self);
		if(!pair) continue;

		int e = pair->next;


		for(; e != start; e = pair->next)
		{
			edge = m_edge(self, e);
			if(!edge) break;

			if(fabs(vec3_dot(edge->n, start_n)) >= smooth_max)
			{
				edge->tmp |= 2;
				smooth_normal = vec3_add(smooth_normal, edge->n);
				smooth_tangent = vec3_add(smooth_tangent, edge->tg);
			}
			/* else */
			/* { */
			/* 	edge->tmp &= ~2; */
			/* } */

			pair = e_pair(edge, self);
			if(!pair) break;
		}

		edge = m_edge(self, start);
		edge->n = smooth_normal = vec3_norm(smooth_normal);
		edge->tg = smooth_tangent = vec3_norm(smooth_tangent);

		pair = e_pair(edge, self);

		e = pair->next;

		for(; e != start; e = pair->next)
		{
			edge = m_edge(self, e);
			if(!edge) break;

			if(edge->tmp & 2)
			{
				edge->n = smooth_normal;
				edge->tg = smooth_tangent;
				edge->tmp = 1;
			}

			pair = e_pair(edge, self);
			if(!pair) break;
		}
	}
}


static vec3_t get_tangent(vec3_t p0, vec2_t t0, vec3_t p1, vec2_t t1,
		vec3_t p2, vec2_t t2, vec3_t n)
{
	vec3_t dp1 = vec3_sub(p1, p0);
	vec3_t dp2 = vec3_sub(p2, p0);

	vec2_t duv1 = vec2_sub(t1, t0);
	vec2_t duv2 = vec2_sub(t2, t0);

	float r = 1.0f / (duv1.x * duv2.y - duv1.y * duv2.x);
	vec3_t tangent = vec3_scale(vec3_sub(vec3_scale(dp1, duv2.y),
				vec3_scale(dp2, duv1.y)), r);

	return vec3_norm(vec3_sub(tangent, vec3_scale(n, vec3_dot(n, tangent))));

}

void mesh_face_calc_flat_tangents(mesh_t *self, face_t *f)
{
	edge_t *e0 = f_edge(f, 0, self);
	edge_t *e1 = f_edge(f, 1, self);
	edge_t *e2 = f_edge(f, 2, self);
	vertex_t *v0 = e_vert(e0, self);
	vertex_t *v1 = e_vert(e1, self);
	vertex_t *v2 = e_vert(e2, self);

	vec3_t tangent = get_tangent(XYZ(v0->pos), e0->t, XYZ(v1->pos), e1->t,
			XYZ(v2->pos), e2->t, f->n);


	if(f->e_size == 4)
	{
		/* QUAD */
		edge_t *e3 = f_edge(f, 3, self);

		e0->tg = tangent;
		e1->tg = tangent;
		e2->tg = tangent;
		e3->tg = tangent;
	}
	else
	{
		/* TRIANGLE */
		e0->tg = tangent;
		e1->tg = tangent;
		e2->tg = tangent;
	}

	/* Gram-Schmidt orthogonalize */
	/* e0->tg = vec3_norm(vec3_sub(tangent, vec3_scale(e0->n, vec3_dot(e0->n, tangent)))); */
	/* e1->tg = vec3_norm(vec3_sub(tangent, vec3_scale(e1->n, vec3_dot(e1->n, tangent)))); */
	/* e2->tg = vec3_norm(vec3_sub(tangent, vec3_scale(e2->n, vec3_dot(e2->n, tangent)))); */
}

void mesh_face_calc_flat_normals(mesh_t *self, face_t *f)
{
	vec3_t p0 = XYZ(f_vert(f, 0, self)->pos);
	vec3_t p1 = XYZ(f_vert(f, 1, self)->pos);
	vec3_t p2 = XYZ(f_vert(f, 2, self)->pos);

	if(f->e_size == 4)
	{
		/* QUAD */
		
		vec3_t p3 = XYZ(f_vert(f, 3, self)->pos);
		
		f_edge(f, 0, self)->n = get_normal(p3, p0, p1);
		f_edge(f, 1, self)->n = get_normal(p0, p1, p2);
		f_edge(f, 2, self)->n = get_normal(p1, p2, p3);
		f_edge(f, 3, self)->n = get_normal(p2, p3, p0);
	}
	else
	{
		/* TRIANGLE */
		vec3_t n = get_normal(p0, p1, p2);
		f->n =
			f_edge(f, 0, self)->n =
			f_edge(f, 1, self)->n =
			f_edge(f, 2, self)->n = n;
	}
}

static int mesh_get_vert(mesh_t *self, vecN_t p)
{
	int i;
	for(i = 0; i < vector_count(self->verts); i++)
	{
		vertex_t *v = m_vert(self, i);
		if(vecN_(equals)(v->pos, p)) return i;
	}
	return -1;
}


int mesh_assert_vert(mesh_t *self, vecN_t pos)
{
	int i = mesh_get_vert(self, pos);
	return  i < 0 ? mesh_add_vert(self, pos) : i;
}

int mesh_face_extrude_possible(mesh_t *self, int face_id)
{
	face_t *face = m_face(self, face_id); if(!face) return 0;
	edge_t *start = f_edge(face, 0, self), *e; if(!start) return 0;

	int last = start->tmp;
	for(e = e_next(start, self); e && e != start; e = e_next(e, self))
	{
		if(last != e->tmp) return 1;
	}
	return 0;
}

void mesh_invert_normals(mesh_t *self)
{
	mesh_lock(self);

	khash_t(id) *selected_faces = self->selections[SEL_EDITING].faces;
	khiter_t k;

	for(k = kh_begin(selected_faces); k != kh_end(selected_faces); ++k)
	{
		if(!kh_exist(selected_faces, k)) continue;
		int f_id = kh_key(selected_faces, k);
		face_t *f = m_face(self, f_id); if(!f) continue;

		edge_t *e0 = f_edge(f, 0, self),
			   *e1 = f_edge(f, 1, self),
			   *e2 = f_edge(f, 2, self);

		int p0 = e0->pair, p1 = e1->pair, p2 = e2->pair;

		int tmp = f->e[0];
		f->e[0] = f->e[2];
		f->e[2] = tmp;


		e0 = f_edge(f, 0, self),
		e1 = f_edge(f, 1, self),
		e2 = f_edge(f, 2, self);
		e0->pair = p1;
		e1->pair = p0;
		e2->pair = p2;

		e0->next = f->e[1]; e0->prev = f->e[2];
		e1->next = f->e[2]; e1->prev = f->e[0];
		e2->next = f->e[0]; e2->prev = f->e[1];
	};
	mesh_modified(self);
	mesh_unlock(self);
}

/* removes faces that don't belong to a cell*/
#ifdef MESH4
int mesh_remove_lone_faces(mesh_t *self)
{
	int i;
	int count = 0;
	mesh_lock(self);
	for(i = 0; i < vector_count(self->faces); i++)
	{
		face_t *f = m_face(self, i); if(!f) continue;
		if(!f_cell(f, self))
		{
			mesh_remove_face(self, i, false);
			count++;
		}
	}
	mesh_unlock(self);

	return count;
}
#endif
/* removes edges that don't belong to a face */
int mesh_remove_lone_edges(mesh_t *self)
{
	int i;
	int count = 0;
	mesh_lock(self);
	for(i = 0; i < vector_count(self->edges); i++)
	{
		edge_t *e = m_edge(self, i); if(!e) continue;
		if(!e_face(e, self))
		{
			mesh_remove_edge(self, i);
			count++;
		}
	}
	mesh_unlock(self);
	return count;
}

#ifdef MESH4
int mesh_edge_cell_pair(mesh_t *self, edge_t *e)
{
	int v0 = e->v;

	e = e_pair(e, self); if(!e) return -1;

	face_t *f = e_face(e, self); if(!f) return -1;

	f = f_pair(f, self); if(!f) return -1;

	int i;
	for(i = 0; i < f->e_size; i++)
	{
		e = f_edge(f, i, self);
		if(e->v == v0)
		{
			return e->pair;
		}
	}
	return -1;
}
#endif

int mesh_update_flips(mesh_t *self)
{
	int j, fix_iterations = 8;

	mesh_update_cell_pairs(self);

	khash_t(id) *selected_faces = self->selections[SEL_EDITING].faces;
	khiter_t k;

	for(k = kh_begin(selected_faces); k != kh_end(selected_faces); ++k)
	{
		if(!kh_exist(selected_faces, k)) continue;
		int f_id = kh_key(selected_faces, k);
		face_t *face = m_face(self, f_id);
		if(!face) continue;

		int last = 0;
		int e;
		for(e = 0; e < face->e_size; e++)
		{
			f_edge(face, e, self)->tmp = last;
			last = !last;
		}
	}
	for(j = 0; j < fix_iterations; j++)
	{
		int fixes = 0;

		khash_t(id) *selected_edges = self->selections[SEL_EDITING].edges;
		khiter_t k;

		for(k = kh_begin(selected_edges); k != kh_end(selected_edges); ++k)
		{
			if(!kh_exist(selected_edges, k)) continue;
			int e = kh_key(selected_edges, k);
			edge_t *edge = m_edge(self, e); if(!edge) continue;

			/* int cpair = mesh_edge_cell_pair(self, edge); */
			/* edge_t *pair = m_edge(self, cpair); if(!pair) continue; */

			edge_t *pair = e_cpair(edge, self); if(!pair) continue;
			edge_t *change;

			if(edge->tmp == pair->tmp)
			{
				int value = !edge->tmp;
				edge->tmp = !edge->tmp;
				if(!mesh_face_extrude_possible(self, edge->face))
				{
					change = (j & 2) ? e_next(edge, self) : e_prev(edge, self);
					if(change) change->tmp = !value;
				}
				fixes++;
			}
		}
		if(fixes == 0)
		{
			return 1;
		}
	}

	return 0;
}

void mesh_unselect(mesh_t *self, int selection, geom_t geom, int id)
{
	mesh_selection_t *sel = &self->selections[selection];
	if(geom == MESH_ANY || geom == MESH_FACE) 
	{
		if(id >= 0) /* UNSELECT SINGLE FACE */
		{
			khiter_t fk = kh_get(id, sel->faces, id);
			if(fk == kh_end(sel->faces)) return;

			int e;
			face_t *face = m_face(self, id); if(!face) return;
			for(e = 0; e < face->e_size; e++)
			{
				khiter_t ek = kh_get(id, sel->edges, face->e[e]);
				if(ek != kh_end(sel->edges)) kh_del(id, sel->edges, ek);
			}
			kh_del(id, sel->faces, fk);
		}
		else /* UNSELECT ALL FACES */
		{
			kh_clear(id, sel->faces);
			kh_clear(id, sel->edges);
		}
	}
	else if(geom == MESH_ANY || geom == MESH_EDGE) 
	{
		if(id >= 0) /* UNSELECT SINGLE EDGE */
		{
			khiter_t ek = kh_get(id, sel->edges, id);
			if(ek != kh_end(sel->edges)) kh_del(id, sel->edges, ek);
		}
		else /* UNSELECT ALL EDGES */
		{
			kh_clear(id, sel->edges);
		}
	}
	else if(geom == MESH_ANY || geom == MESH_VERT) 
	{
		if(id >= 0) /* UNSELECT SINGLE EDGE */
		{
			khiter_t ek = kh_get(id, sel->verts, id);
			if(ek != kh_end(sel->verts)) kh_del(id, sel->verts, ek);
		}
		else /* UNSELECT ALL EDGES */
		{
			kh_clear(id, sel->verts);
		}
	}
}

void mesh_select(mesh_t *self, int selection, geom_t geom, int id)
{
	mesh_unselect(self, selection, geom, id);
	mesh_selection_t *sel = &self->selections[selection];
	int ret;
	if(geom == MESH_ANY || geom == MESH_FACE) 
	{
		if(id >= 0) /* SELECT SINGLE FACE */
		{
			int e;
			face_t *face = m_face(self, id); if(!face) return;
			for(e = 0; e < face->e_size; e++)
			{
				/* vector_add(sel->edges, &face->e[e]); */
				khiter_t k = kh_put(id, sel->edges, face->e[e], &ret);
				kh_value(sel->edges, k) = 1;
			}

			/* vector_add(sel->faces, &id); */
			khiter_t k = kh_put(id, sel->faces, id, &ret);
			kh_value(sel->faces, k) = 1;
		}
		else /* SELECT ALL FACES */
		{
			int e;
			for(id = 0; id < vector_count(self->faces); id++)
			{
				face_t *face = m_face(self, id); if(!face) continue;
				for(e = 0; e < face->e_size; e++)
				{
					/* vector_add(sel->edges, &face->e[e]); */
					khiter_t k = kh_put(id, sel->edges, face->e[e], &ret);
					kh_value(sel->edges, k) = 1;
				}

				/* vector_add(sel->faces, &id); */
				khiter_t k = kh_put(id, sel->faces, id, &ret);
				kh_value(sel->faces, k) = 1;
			}
		}
	}
	else if(geom == MESH_ANY || geom == MESH_EDGE) 
	{
		if(id >= 0) /* SELECT SINGLE EDGE */
		{
			edge_t *edge = m_edge(self, id); if(!edge) return;
			/* vector_add(sel->edges, &id); */
			khiter_t k = kh_put(id, sel->edges, id, &ret);
			kh_value(sel->edges, k) = 1;
		}
		else /* SELECT ALL EDGES */
		{
			for(id = 0; id < vector_count(self->edges); id++)
			{
				edge_t *edge = m_edge(self, id); if(!edge) return;
				/* vector_add(sel->edges, &id); */
				khiter_t k = kh_put(id, sel->edges, id, &ret);
				kh_value(sel->edges, k) = 1;
			}
		}
	}
	else if(geom == MESH_ANY || geom == MESH_VERT) 
	{
		if(id >= 0) /* SELECT SINGLE VERT */
		{
			vertex_t *vert = m_vert(self, id); if(!vert) return;
			/* vector_add(sel->verts, &id); */
			khiter_t k = kh_put(id, sel->verts, id, &ret);
			kh_value(sel->verts, k) = 1;
		}
		else /* SELECT ALL VERTS */
		{
			for(id = 0; id < vector_count(self->verts); id++)
			{
				vertex_t *vert = m_vert(self, id); if(!vert) return;
				/* vector_add(sel->verts, &id); */
				khiter_t k = kh_put(id, sel->verts, id, &ret);
				kh_value(sel->verts, k) = 1;
			}
		}
	}
}

void mesh_weld(mesh_t *self, geom_t geom)
{
	if(geom == MESH_EDGE)
	{
		int vert_id = -1;
		khash_t(id) *selected_edges = self->selections[SEL_EDITING].edges;
		khiter_t k;

		for(k = kh_begin(selected_edges); k != kh_end(selected_edges); ++k)
		{
			if(!kh_exist(selected_edges, k)) continue;
			int e_id = kh_key(selected_edges, k);
			edge_t *e = m_edge(self, e_id); if(!e) continue;
			if(vert_id == -1) vert_id = e->v;
			e->v = vert_id;

			edge_t *prev = e_prev(e, self);
			edge_t *next = e_next(e, self);

			prev->next = e->next;
			next->prev = e->prev;
			e->next = -1;
			e->prev = -1;
			/* mesh_edge_select(self, e_id, SEL_EDITING); */
			mesh_remove_edge(self, e_id);
		}
	}
	if(geom == MESH_FACE)
	{
		khash_t(id) *selected_faces = self->selections[SEL_EDITING].faces;
		khiter_t k;

		for(k = kh_begin(selected_faces); k != kh_end(selected_faces); ++k)
		{
			if(!kh_exist(selected_faces, k)) continue;
			int f_id = kh_key(selected_faces, k);

			face_t *face = m_face(self, f_id); if(!face) continue;

			/* for(i = 0; i < face->e_size; i++) */
			/* { */
			/* 	mesh_edge_select(self, SEL_EDITING, 1); */
			/* } */
			mesh_remove_face(self, f_id, false);
		}
	}
}

void mesh_for_each_selected(mesh_t *self, geom_t geom, iter_cb cb, void *usrptr)
{
	if(geom == MESH_FACE)
	{
		khash_t(id) *selected_faces = self->selections[SEL_EDITING].faces;
		khiter_t k;

		for(k = kh_begin(selected_faces); k != kh_end(selected_faces); ++k)
		{
			if(!kh_exist(selected_faces, k)) continue;
			int f_id = kh_key(selected_faces, k);

			face_t *face = m_face(self, f_id); if(!face) continue;
			if(!cb(self, face, usrptr)) break;
		}
	}
	else if(geom == MESH_EDGE)
	{
		khash_t(id) *selected_edges = self->selections[SEL_EDITING].edges;
		khiter_t k;

		for(k = kh_begin(selected_edges); k != kh_end(selected_edges); ++k)
		{
			if(!kh_exist(selected_edges, k)) continue;
			int e_id = kh_key(selected_edges, k);
			edge_t *e = m_edge(self, e_id); if(!e) continue;
			int res;
			/* if(geom == MESH_EDGE) */
			/* { */
				res = cb(self, e, usrptr);
			/* } */
			/* else */
			/* { */
				/* res = cb(self, e_vert(e, self), usrptr); */
			/* } */
			if(res == 0) break;
		}
	}
	else if(geom == MESH_VERT)
	{
		khash_t(id) *selected_verts = self->selections[SEL_EDITING].verts;
		khiter_t k;

		for(k = kh_begin(selected_verts); k != kh_end(selected_verts); ++k)
		{
			if(!kh_exist(selected_verts, k)) continue;
			int v_id = kh_key(selected_verts, k);

			vertex_t *vert = m_vert(self, v_id); if(!vert) continue;
			if(!cb(self, vert, usrptr)) break;
		}
	}
}

void mesh_paint(mesh_t *self, vec4_t color)
{
	khash_t(id) *selected_edges = self->selections[SEL_EDITING].edges;
	khiter_t k;

	for(k = kh_begin(selected_edges); k != kh_end(selected_edges); ++k)
	{
		if(!kh_exist(selected_edges, k)) continue;
		int e_id = kh_key(selected_edges, k);
		edge_t *e = m_edge(self, e_id); if(!e) continue;
		e_vert(e, self)->color = color;
	}
	mesh_modified(self);
}

void mesh_unlock_write(mesh_t *self)
{
	SDL_AtomicDecRef((SDL_atomic_t*)&self->locked_write);
	if(self->locked_write == 0) SDL_SemPost(self->semaphore);
}

void mesh_lock_write(mesh_t *self)
{
	if(self->locked_write == 0) SDL_SemWait(self->semaphore);
	SDL_AtomicIncRef((SDL_atomic_t*)&self->locked_write);
}

void mesh_unlock(mesh_t *self)
{
	self->locked_read--;
	if(self->locked_read == 0)
	{
		self->owner_thread = 0;
		SDL_SemPost(self->semaphore);
	}
	mesh_update(self);
}

void mesh_lock(mesh_t *self)
{
	uint64_t current_thread = SDL_ThreadID();
	if(self->owner_thread != current_thread)
	{
		mesh_wait(self);
		SDL_SemWait(self->semaphore);
		self->owner_thread = current_thread;
	}
	self->locked_read++;
}

static void mesh_1_subdivide(mesh_t *self)
{
	/* mesh has to be triangulated */
	mesh_lock(self);

	khash_t(id) *selected_faces = self->selections[SEL_EDITING].faces;
	khiter_t k;

	for(k = kh_begin(selected_faces); k != kh_end(selected_faces); ++k)
	{
		if(!kh_exist(selected_faces, k)) continue;
		int f_id = kh_key(selected_faces, k);

		face_t *face = m_face(self, f_id); if(!face) continue;

		edge_t *e0 = f_edge(face, 0, self);
		edge_t *e1 = f_edge(face, 1, self);
		edge_t *e2 = f_edge(face, 2, self);

		vertex_t *v0 = e_vert(e0, self);
		vertex_t *v1 = e_vert(e1, self);
		vertex_t *v2 = e_vert(e2, self);

		vecN_t p01, p12, p20;    

		p01 = vecN_(scale)(vecN_(add)(v0->pos, v1->pos), 0.5);
		p12 = vecN_(scale)(vecN_(add)(v1->pos, v2->pos), 0.5);
		p20 = vecN_(scale)(vecN_(add)(v2->pos, v0->pos), 0.5);

		vec2_t t0 = e0->t, t1 = e1->t, t2 = e2->t;    
		vec2_t t01, t12, t20;    
		t01 = vec2_scale(vec2_add(e0->t, e1->t), 0.5);
		t12 = vec2_scale(vec2_add(e1->t, e2->t), 0.5);
		t20 = vec2_scale(vec2_add(e2->t, e0->t), 0.5);

		int i0 = e0->v;
		int i1 = e1->v;
		int i2 = e2->v;

		int i01 = mesh_assert_vert(self, p01);
		int i12 = mesh_assert_vert(self, p12);
		int i20 = mesh_assert_vert(self, p20);

		vertex_t *v01 = m_vert(self, i01);
		vertex_t *v12 = m_vert(self, i12);
		vertex_t *v20 = m_vert(self, i20);
		v01->color = vec4_scale(vec4_add(v0->color, v1->color), 0.5);
		v12->color = vec4_scale(vec4_add(v1->color, v2->color), 0.5);
		v20->color = vec4_scale(vec4_add(v2->color, v0->color), 0.5);

		mesh_add_triangle(self, i0, Z3, t0,
								i01, Z3, t01,
								i20, Z3, t20);

		mesh_add_triangle(self, i1, Z3, t1,
								i12, Z3, t12,
								i01, Z3, t01);

		mesh_add_triangle(self, i2, Z3, t2,
								i20, Z3, t20,
								i12, Z3, t12);

		mesh_add_triangle(self, i01, Z3, t01,
								i12, Z3, t12,
								i20, Z3, t20);
		mesh_remove_face(self, f_id, false);

		/* mesh_select(self, TMP, MESH_FACE, f0); */
		/* mesh_select(self, TMP, MESH_FACE, f1); */
		/* mesh_select(self, TMP, MESH_FACE, f2); */
		/* mesh_select(self, TMP, MESH_FACE, f3); */
	}

	mesh_select(self, SEL_EDITING, MESH_ANY, -1);

	/* mesh_selection_t swap = self->selections[SEL_EDITING]; */
	/* self->selections[SEL_EDITING] = self->selections[TMP]; */
	/* self->selections[TMP] = swap; */

	mesh_unlock(self);

}

void mesh_spherize(mesh_t *self, float roundness)
{
	mesh_lock(self);
	khash_t(id) *selected_edges = self->selections[SEL_EDITING].edges;
	khiter_t k;

	float rad = mesh_get_selection_radius(self, ZN);
	for(k = kh_begin(selected_edges); k != kh_end(selected_edges); ++k)
	{
		if(!kh_exist(selected_edges, k)) continue;
		int e_id = kh_key(selected_edges, k);

		edge_t *e = m_edge(self, e_id); if(!e) continue;

		vertex_t *v = e_vert(e, self);

		vecN_t norm = vecN_(norm)(v->pos);
		norm = vecN_(scale)(norm, rad);
		v->pos = vecN_(mix)(v->pos, norm, roundness);

	}
	mesh_unlock(self);
}

void mesh_subdivide(mesh_t *self, int subdivisions)
{
	int i;
	mesh_lock(self);
	mesh_triangulate(self);
	mesh_select(self, SEL_EDITING, MESH_FACE, -1);
	for(i = 0; i < subdivisions; i++)
	{
		mesh_1_subdivide(self);
	}

	mesh_unlock(self);
}

void mesh_clear(mesh_t *self)
{
	mesh_lock(self);
	mesh_unselect(self, SEL_EDITING, MESH_ANY, -1);

	kh_clear(id, self->edges_hash);
	kh_clear(id, self->faces_hash);
	vector_clear(self->verts);
	vector_clear(self->edges);
	vector_clear(self->faces);
#ifdef MESH4
	vector_clear(self->cells);
#endif

	mesh_modified(self);
	mesh_unlock(self);
}

int mesh_edge_rotate_to_unpaired(mesh_t *self, int edge_id)
{
	edge_t *edge = m_edge(self, edge_id);
	if(!edge) return -1;
	int last_working = edge_id;
	edge_t *prev = e_prev(edge, self);
	int e = prev->pair;
	if(e == -1) return last_working;

	for(; e != edge_id; e = prev->pair)
	{
		edge = m_edge(self, e);
		if(!edge) break;
		prev = e_prev(edge, self);
		if(prev->pair == -1) break;
		last_working = e;
	}
	return last_working;
}

int mesh_vert_has_face(mesh_t *self, vertex_t *vert, int face_id)
{
	int edge_id = vert->half;
	edge_id = mesh_edge_rotate_to_unpaired(self, edge_id);

	edge_t *edge = m_edge(self, edge_id);
	edge_t *pair = e_pair(edge, self);
	if(!pair) return 0;
	int e = pair->next;

	for(; e != edge_id; e = pair->next)
	{
		edge = m_edge(self, e);
		if(!edge) return 0;
		if(edge->face == face_id) return 1;

		pair = e_pair(edge, self);
		if(!pair) return 0;
	}
	return 0;
}

void mesh_update(mesh_t *self)
{
	if(self->locked_read) return;
	if(!self->changes) return;

	int i;
	for(i = 0; i < vector_count(self->faces); i++)
	{
		face_t *f = m_face(self, i);
		if(!f) continue;

		/* vec3_t n = f_edge(f, 0, self)->n; */
		/* if(vec3_null(n)) */
		{
			mesh_face_calc_flat_normals(self, f);
			mesh_face_calc_flat_tangents(self, f);
		}
	}
	self->changes = 0;
	self->update_id++;
}

int mesh_add_vert(mesh_t *self, vecN_t pos)
{
	int i = vector_reserve(self->verts);

	vertex_t *vert = vector_get(self->verts, i);
	vert_init(vert);

#ifdef MESH4
	vert->pos = mat4_mul_vec4(self->transformation,
			vec4(_vec3(pos), 1.0));
	vert->pos.w = pos.w;
#else
	vert->pos = mat4_mul_vec4(self->transformation,
			vec4(_vec3(pos), 1.0)).xyz;
#endif

	/* mesh_check_duplicate_verts(self, i); */

	mesh_modified(self);

	return i;
}

int mesh_append_edge(mesh_t *self, vecN_t p)
{
	mesh_lock(self);
	int e = mesh_add_vert(self, p);
	mesh_add_edge_s(self, e, vector_count(self->edges) - 1);
	mesh_unlock(self);
	return e;
}

int mesh_add_edge_s(mesh_t *self, int v, int prev)
{
	return mesh_add_edge(self, v, -1, prev, vec3(0.0f, 0.0f, 0.0f), vec2(0.0f, 0.0f));
}

static inline void sort3(int *v)
{
	int A = v[0], B = v[1], C = v[2];
	if(A > B)
	{   
		v[1] = A;    
		v[0] = B;   
	}
	if(v[1] > C)
	{ 
		v[2] = v[1];    
		if(v[0] > C)
		{         
			v[1] = v[0];                
			v[0] = C;
		}
		else v[1] = C;      
	}
}

unsigned int mesh_face_to_id(mesh_t *self, face_t *face)
{
#define swap(a, b) do{int tmp = a; a = b; b = tmp;}while(0)

	edge_t *e0 = f_edge(face, 0, self); if(!e0) return 0;
	edge_t *e1 = e_next(e0, self); if(!e1) return 0;
	edge_t *e2 = e_prev(e0, self); if(!e2) return 0;

	int v[3] = {e0->v, e1->v, e2->v};
	sort3(v);
	
	return murmur_hash((char*)v, sizeof(v), 123);
}

unsigned int mesh_edge_to_id(mesh_t *self, edge_t *edge)
{
	edge_t *next = e_next(edge, self);
	if(!next) return 0;
	int result[2];
	if(edge->v > next->v)
	{
		result[0] = next->v;
		result[1] = edge->v;
	}
	else
	{
		result[0] = edge->v;
		result[1] = next->v;
	}
	return murmur_hash((char*)result, sizeof(result), 123);
}

int mesh_add_edge(mesh_t *self, int v, int next, int prev, vec3_t vn, vec2_t vt)
{
	int i = vector_reserve(self->edges);

	edge_t *edge = vector_get(self->edges, i);
	edge_init(edge);

	edge->v = v;
	edge->n = vn;
	edge->t = vt;
	edge->prev = prev;
	edge->next = next;

	m_vert(self, v)->half = i;

	if(prev >= 0)
	{
		m_edge(self, prev)->next = i;
		mesh_get_pair_edge(self, prev);
	}
	if(next >= 0)
	{
		m_edge(self, next)->prev = i;
	}

	mesh_get_pair_edge(self, i);

	return i;
}

int mesh_add_regular_quad( mesh_t *self,
	vecN_t p1, vec3_t n1, vec2_t t1, vecN_t p2, vec3_t n2, vec2_t t2,
	vecN_t p3, vec3_t n3, vec2_t t3, vecN_t p4, vec3_t n4, vec2_t t4
)
{
	int v1 = mesh_add_vert(self, p1);
	int v2 = mesh_add_vert(self, p2);
	int v3 = mesh_add_vert(self, p3);
	int v4 = mesh_add_vert(self, p4);

	mesh_add_quad(self, v1, n1, t1,
			v2, n2, t2,
			v3, n3, t3,
			v4, n4, t4);

	return 1;
}

/* static int mesh_face_are_pairs(mesh_t *self, */
/* 		int f0, int f1, int f2, int v0, int v1, int v2) */
/* { */
/* 	if(f0 == v0 && f1 == v2 && f2 == v1 ) return 0; */
/* 	if(f1 == v0 && f2 == v2 && f0 == v1 ) return 1; */
/* 	if(f2 == v0 && f0 == v2 && f1 == v1 ) return 2; */
/* 	return -1; */
/* } */

int mesh_get_face_from_verts(mesh_t *self,
		int v0, int v1, int v2)
{
	int v[3] = {v0, v1, v2};
	sort3(v);
	uint32_t hash =  murmur_hash((char*)v, sizeof(v), 123);

	khiter_t k = kh_get(id, self->faces_hash, hash);
	if(k == kh_end(self->faces_hash))
	{
		return -1;
	}
	else
	{
		return kh_value(self->faces_hash, k);
	}
	return -1;
}
void mesh_edge_cpair(mesh_t *self, int e1, int e2)
{
	m_edge(self, e1)->cell_pair = e2;
	m_edge(self, e2)->cell_pair = e1;
}
void mesh_edge_pair(mesh_t *self, int e1, int e2)
{
	m_edge(self, e1)->pair = e2;
	m_edge(self, e2)->pair = e1;
}

#ifdef MESH4
static int mesh_get_pair_face(mesh_t *self, int face_id)
{
	face_t *face = m_face(self, face_id); if(!face) return 0;

	uint32_t hash = mesh_face_to_id(self, face);
	if(hash)
	{
		khiter_t k = kh_get(id, self->faces_hash, hash);
		if(k == kh_end(self->faces_hash))
		{
			int ret;
			k = kh_put(id, self->faces_hash, hash, &ret);
			kh_value(self->faces_hash, k) = face_id;
		}
		else
		{
			int pair_id = kh_value(self->faces_hash, k);
			face_t *pair = m_face(self, pair_id);

			face->pair = pair_id;
			kh_del(id, self->faces_hash, k);

			pair->pair = face_id;

			/* update edge cell pairs */

			int f0 = f_edge(face, 0, self)->v;
			int f1 = f_edge(face, 1, self)->v;
			int f2 = f_edge(face, 2, self)->v;

			int v0 = f_edge(pair, 0, self)->v;

			if(f0 == v0)
			{
				mesh_edge_cpair(self, face->e[0], pair->e[2]);
				mesh_edge_cpair(self, face->e[1], pair->e[1]);
				mesh_edge_cpair(self, face->e[2], pair->e[0]);
			}
			else if(f1 == v0)
			{
				mesh_edge_cpair(self, face->e[1], pair->e[2]);
				mesh_edge_cpair(self, face->e[2], pair->e[1]);
				mesh_edge_cpair(self, face->e[0], pair->e[0]);
			}
			else if(f2 == v0)
			{
				mesh_edge_cpair(self, face->e[2], pair->e[2]);
				mesh_edge_cpair(self, face->e[0], pair->e[1]);
				mesh_edge_cpair(self, face->e[1], pair->e[0]);
			}
			mesh_modified(self);
			return 1;
		}
	}
	return 0;
}
#endif

static int mesh_get_cell_pair(mesh_t *self, int e)
{
	edge_t *edge = vector_get(self->edges, e);
	int v1 = edge->v;
	if(edge->next < 0 || edge->cell_pair >= 0) return -1;
	int v2 = e_next(edge, self)->v;

	khash_t(id) *selected_edges = self->selections[SEL_EDITING].edges;
	khiter_t k;
	for(k = kh_begin(selected_edges); k != kh_end(selected_edges); ++k)
	{
		if(!kh_exist(selected_edges, k)) continue;
		int e_id = kh_key(selected_edges, k);

		if(e_id == e) continue;

		edge_t *try1 = m_edge(self, e_id);
		if(!try1 || try1->cell_pair != -1) continue;

		edge_t *try2 = e_next(try1, self);
		if(!try2) continue;

		if(try1->v == v2 && try2->v == v1)
		{
			edge->cell_pair = e_id;
			try1->cell_pair = e;
			return 1;
		}
	}
	return -1;
}

static void mesh_update_cell_pairs(mesh_t *self)
{
	khash_t(id) *selected_edges = self->selections[SEL_EDITING].edges;
	khiter_t k;
	for(k = kh_begin(selected_edges); k != kh_end(selected_edges); ++k)
	{
		if(!kh_exist(selected_edges, k)) continue;
		int e_id = kh_key(selected_edges, k);
		edge_t *edge = m_edge(self, e_id);
		if(!edge || edge->cell_pair != -1) continue;
		mesh_get_cell_pair(self, e_id);
	}
}

int mesh_get_pair_edge(mesh_t *self, int edge_id)
{
	edge_t *edge = m_edge(self, edge_id); if(!edge) return 0;
	if(edge->pair >= 0) return 0;
	edge_t *next = e_next(edge, self); if(!next) return 0;

	uint32_t hash = mesh_edge_to_id(self, edge);
	/* printf("looking	%d	(%p) [%d %d] ", edge_id, hash, edge->v, next->v); */
	if(hash)
	{
		khiter_t k = kh_get(id, self->edges_hash, hash);
		if(k == kh_end(self->edges_hash))
		{
			int ret;
			k = kh_put(id, self->edges_hash, hash, &ret);
			kh_value(self->edges_hash, k) = edge_id;
			assert(m_edge(self, edge_id));
		}
		else
		{
			int pair_id = kh_value(self->edges_hash, k);
			if(pair_id == edge_id) return 0;
			kh_del(id, self->edges_hash, k);

			edge_t *pair = m_edge(self, pair_id);
			assert(pair);

			/* if(edge->v != e_next(pair, self)->v || */
			/* 	pair->v != e_next(edge, self)->v ) */
			/* { */
			/* 	exit(1); */
			/* } */

			edge->pair = pair_id;

			pair->pair = edge_id;
			mesh_modified(self);
			/* puts(""); */
			return 1;
		}
	}
	/* puts(""); */
	return 0;
}

void mesh_add_quad(mesh_t *self,
		int v0, vec3_t v0n, vec2_t v0t,
		int v1, vec3_t v1n, vec2_t v1t,
		int v2, vec3_t v2n, vec2_t v2t,
		int v3, vec3_t v3n, vec2_t v3t)
{

	mesh_lock(self);

	self->triangulated = 0;

	int face_id = vector_reserve(self->faces);
	face_t *face = vector_get(self->faces, face_id);
	face_init(face);

#ifdef MESH4
	face->cell = self->current_cell;
#endif
	face->e_size = 4;
	
	int ie0 = vector_reserve(self->edges);
	int ie1 = vector_reserve(self->edges);
	int ie2 = vector_reserve(self->edges);
	int ie3 = vector_reserve(self->edges);

	edge_t *e0 = vector_get(self->edges, ie0); edge_init(e0);
	edge_t *e1 = vector_get(self->edges, ie1); edge_init(e1);
	edge_t *e2 = vector_get(self->edges, ie2); edge_init(e2);
	edge_t *e3 = vector_get(self->edges, ie3); edge_init(e3);

	v0n = mat4_mul_vec4(self->transformation, vec4(_vec3(v0n), 0.0)).xyz;
	v1n = mat4_mul_vec4(self->transformation, vec4(_vec3(v1n), 0.0)).xyz;
	v2n = mat4_mul_vec4(self->transformation, vec4(_vec3(v2n), 0.0)).xyz;
	v3n = mat4_mul_vec4(self->transformation, vec4(_vec3(v3n), 0.0)).xyz;

	face->e[0] = ie0;
	face->e[1] = ie1;
	face->e[2] = ie2;
	face->e[3] = ie3;

	*e0 = (edge_t){ .v = v0, .n = v0n, .t = v0t, .face = face_id,
		.next = ie1, .prev = ie3, .pair = -1, .cell_pair = -1};
	*e1 = (edge_t){ .v = v1, .n = v1n, .t = v1t, .face = face_id,
		.next = ie2, .prev = ie0, .pair = -1, .cell_pair = -1};
	*e2 = (edge_t){ .v = v2, .n = v2n, .t = v2t, .face = face_id,
		.next = ie3, .prev = ie1, .pair = -1, .cell_pair = -1};
	*e3 = (edge_t){ .v = v3, .n = v3n, .t = v3t, .face = face_id,
		.next = ie0, .prev = ie2, .pair = -1, .cell_pair = -1};

	m_vert(self, v0)->half = ie0;
	m_vert(self, v1)->half = ie1;
	m_vert(self, v2)->half = ie2;
	m_vert(self, v3)->half = ie3;

	mesh_get_pair_edge(self, ie0);
	mesh_get_pair_edge(self, ie1);
	mesh_get_pair_edge(self, ie2);
	mesh_get_pair_edge(self, ie3);

#ifdef MESH4
	mesh_get_pair_face(self, face_id);
#endif

	mesh_modified(self);
	mesh_unlock(self);
}

void mesh_remove_vert(mesh_t *self, int vert_i)
{
	vector_remove(self->verts, vert_i);
	mesh_modified(self);
}

void mesh_edge_remove_from_hash(mesh_t *self, edge_t *edge)
{
	uint32_t hash = mesh_edge_to_id(self, edge);
	if(hash)
	{
		khiter_t k = kh_get(id, self->edges_hash, hash);
		if(k != kh_end(self->edges_hash))
		{
			kh_del(id, self->edges_hash, k);
		}
	}

}
void mesh_remove_edge(mesh_t *self, int edge_i)
{
	edge_t *edge = m_edge(self, edge_i);
	if(!edge) return;

	mesh_edge_remove_from_hash(self, edge);

	edge_t *cpair = e_cpair(edge, self);
	if(cpair)
	{
		cpair->cell_pair = -1;
	}
	edge_t *next = e_next(edge, self);
	if(next)
	{
		next->prev = -1;
	}
	edge_t *prev = e_prev(edge, self);
	if(prev)
	{
		mesh_edge_remove_from_hash(self, prev);
		prev->next = -1;
	}
	face_t *face = e_face(edge, self);

	vector_remove(self->edges, edge_i);

	if(face)
	{
		mesh_remove_face(self, edge->face, false);
	}

	edge_t *pair = e_pair(edge, self);
	if(pair)
	{
		pair->pair = -1;
		mesh_get_pair_edge(self, edge->pair);
	}

	mesh_modified(self);
}

void mesh_remove_face(mesh_t *self, int face_i, bool_t face_only)
{
	int i;

	face_t *face = m_face(self, face_i);
	if(!face) return;

#ifdef MESH4
	if(face->pair == -1)
	{
		/* remove from faces_hash */
		uint32_t hash = mesh_face_to_id(self, face);
		if(hash)
		{
			khiter_t k = kh_get(id, self->faces_hash, hash);
			if(k != kh_end(self->faces_hash))
			{
				kh_del(id, self->faces_hash, k);
			}
		}
	}
	else
	{
		face_t *pair = f_pair(face, self);
		if(pair)
		{
			pair->pair = -1;
		}
	}
#endif
	vector_remove(self->faces, face_i);
	for(i = 0; i < face->e_size; i++)
	{
		if (face_only)
		{
			edge_t *edge = m_edge(self, face->e[i]);
			if(!edge) continue;
			edge->face = -1;
		}
		else
		{
			mesh_remove_edge(self, face->e[i]);
		}
	}
	mesh_modified(self);
}

int mesh_add_triangle_s(mesh_t *self, int v1, int v2, int v3)
{
	return mesh_add_triangle(self, v1, Z3, Z2, v2, Z3, Z2, v3, Z3, Z2);
}

int mesh_add_triangle(mesh_t *self,
		int v0, vec3_t v0n, vec2_t v0t,
		int v1, vec3_t v1n, vec2_t v1t,
		int v2, vec3_t v2n, vec2_t v2t)
{
	int face_id = vector_reserve(self->faces);
	face_t *face = vector_get(self->faces, face_id);
	face_init(face);

#ifdef MESH4
	face->cell = self->current_cell;
#endif

	face->e_size = 3;

	int ie0 = vector_reserve(self->edges);
	int ie1 = vector_reserve(self->edges);
	int ie2 = vector_reserve(self->edges);

	edge_t *e0 = vector_get(self->edges, ie0); edge_init(e0);
	edge_t *e1 = vector_get(self->edges, ie1); edge_init(e1);
	edge_t *e2 = vector_get(self->edges, ie2); edge_init(e2);

	v0n = mat4_mul_vec4(self->transformation, vec4(_vec3(v0n), 0.0)).xyz;
	v1n = mat4_mul_vec4(self->transformation, vec4(_vec3(v1n), 0.0)).xyz;
	v2n = mat4_mul_vec4(self->transformation, vec4(_vec3(v2n), 0.0)).xyz;

	face->e[0] = ie0;
	face->e[1] = ie1;
	face->e[2] = ie2;

	*e0 = (edge_t){ .v = v0, .n = v0n, .t = v0t, .face = face_id,
		.next = ie1, .prev = ie2, .pair = -1, .cell_pair = -1};
	*e1 = (edge_t){ .v = v1, .n = v1n, .t = v1t, .face = face_id,
		.next = ie2, .prev = ie0, .pair = -1, .cell_pair = -1};
	*e2 = (edge_t){ .v = v2, .n = v2n, .t = v2t, .face = face_id,
		.next = ie0, .prev = ie1, .pair = -1, .cell_pair = -1};

	/* if(vec3_null(v1n) || vec3_null(v2n) || vec3_null(v3n)) */
	/* { */
	/* 	mesh_face_calc_flat_normals(self, face); */

	/* 	if(vec3_null(v1n)) self->edges[ie0].n = face->n; */
	/* 	if(vec3_null(v2n)) self->edges[ie1].n = face->n; */
	/* 	if(vec3_null(v3n)) self->edges[ie2].n = face->n; */
	/* } */

	m_vert(self, v0)->half = ie0;
	m_vert(self, v1)->half = ie1;
	m_vert(self, v2)->half = ie2;

	mesh_get_pair_edge(self, ie0);
	mesh_get_pair_edge(self, ie1);
	mesh_get_pair_edge(self, ie2);

#ifdef MESH4
	mesh_get_pair_face(self, face_id);
#endif

	mesh_modified(self);
	return face_id;
}

static inline float to_radians(float angle)
{
	return angle * (M_PI / 180.0);
}

void mesh_translate(mesh_t *self, vec3_t t)
{
	self->transformation = mat4_translate_in_place(self->transformation, t);
}

void mesh_rotate(mesh_t *self, float angle, int x, int y, int z)
{
	mat4_t new = mat4_rotate(self->transformation, x, y, z, to_radians(angle));
	self->transformation = new;
}

void mesh_restore(mesh_t *self, mat4_t saved)
{
	self->transformation = saved;
}

mat4_t mesh_save(mesh_t *self)
{
	return self->transformation;
}

void mesh_translate_uv(mesh_t *self, vec2_t p)
{
	int i;
	mesh_lock(self);
	for(i = 0; i < vector_count(self->edges); i++)
	{
		edge_t *edge = m_edge(self, i); if(!edge) continue;
		edge->t = vec2_add(edge->t, p);
	}
	mesh_modified(self);
	mesh_unlock(self);
}

void mesh_scale_uv(mesh_t *self, float scale)
{
	int i;
	mesh_lock(self);
	for(i = 0; i < vector_count(self->edges); i++)
	{
		edge_t *edge = m_edge(self, i); if(!edge) continue;
		edge->t = vec2_scale(edge->t, scale);
	}
	mesh_modified(self);
	mesh_unlock(self);
}

void mesh_quad(mesh_t *self)
{
	const vec3_t n = vec3(0,0,1);
	mesh_lock(self);
	mesh_add_regular_quad(self,
			VEC3(-1.0, -1.0, 0.0), n, vec2(0, 0),
			VEC3( 1.0, -1.0, 0.0), n, vec2(1, 0),
			VEC3( 1.0,  1.0, 0.0), n, vec2(1, 1),
			VEC3(-1.0,  1.0, 0.0), n, vec2(0, 1));
	mesh_unlock(self);
}

struct int_int {int a, b;};
struct int_int_int {int a, b;};

struct int_int mesh_face_triangulate(mesh_t *self, int i, int flip)
{
	face_t *face = vector_get(self->faces, i);

	khash_t(id) *sel = self->selections[SEL_EDITING].faces;
	int selected = kh_get(id, sel, i) != kh_end(sel);

	edge_t *e0 = f_edge(face, 0, self);
	edge_t *e1 = f_edge(face, 1, self);
	edge_t *e2 = f_edge(face, 2, self);
	edge_t *e3 = f_edge(face, 3, self);

	int i0 = e0->v;
	int i1 = e1->v;
	int i2 = e2->v;
	int i3 = e3->v;

	vec2_t t0 = e0->t;
	vec2_t t1 = e1->t;
	vec2_t t2 = e2->t;
	vec2_t t3 = e3->t;

	mesh_remove_face(self, i, false);

	struct int_int result = 

	result = flip? (struct int_int) {
		mesh_add_triangle(self, i1, Z3, t1,
								i2, Z3, t2,
								i3, Z3, t3),

		mesh_add_triangle(self, i3, Z3, t3,
								i0, Z3, t0,
								i1, Z3, t1)
	} : (struct int_int) {
		mesh_add_triangle(self, i0, Z3, t0,
								i1, Z3, t1,
								i2, Z3, t2),

		mesh_add_triangle(self, i2, Z3, t2,
								i3, Z3, t3,
								i0, Z3, t0)
	};
	if(selected)
	{
		mesh_select(self, SEL_EDITING, MESH_FACE, result.a);
		mesh_select(self, SEL_EDITING, MESH_FACE, result.b);
	}
	return result;
}

void mesh_triangulate(mesh_t *self)
{
	int i;
	if(self->triangulated) return;
	mesh_lock(self);
	/* for(i = 0; i < vector_count(self->faces); i++) */
	/* { */
	/* 	face_t *face = m_face(self, i); if(!face) continue; */
	/* 	if(face->e_size == 3) */
	/* 	{ */
	/* 		mesh_select(self, 4, MESH_FACE, i); */
	/* 	} */
	/* } */
	for(i = 0; i < vector_count(self->faces); i++)
	{
		face_t *face = m_face(self, i); if(!face) continue;
		if(face->e_size != 4) continue;

#ifndef MESH4

		mesh_face_triangulate(self, i, 0);

#else
		struct int_int new_faces = mesh_face_triangulate(self, i, 0);

		face = m_face(self, i);
		face_t *pair = f_pair(face, self);
		if(pair)
		{
			struct int_int pair_faces =
				mesh_face_triangulate(self, face->pair, 1);

			m_face(self, new_faces.a)->pair = pair_faces.a;
			m_face(self, new_faces.b)->pair = pair_faces.b;
		}
#endif
	}
	/* mesh_unselect(self, SEL_EDITING, MESH_ANY, -1); */
	/* mesh_selection_t swap = self->selections[SEL_EDITING]; */
	/* self->selections[SEL_EDITING] = self->selections[4]; */
	/* self->selections[4] = swap; */


	self->triangulated = 1;
	mesh_unlock(self);
}

void mesh_disk(mesh_t *self, float radius, float inner_radius, int segments,
               vecN_t dir)
{
	mesh_lock(self);
	mesh_circle(self, radius, segments, dir);
	mesh_extrude_edges(self, 1, ZN, inner_radius / radius, NULL, NULL, NULL);
	mesh_unlock(self);
}

void mesh_remove_faces(mesh_t *self)
{
	int i;
	mesh_lock(self);
	for(i = 0; i < vector_count(self->faces); i++)
	{
		face_t *f = m_face(self, i); if(!f) continue;
		mesh_remove_face(self, i, true);
	}
	vector_clear(self->faces);
	mesh_unlock(self);
}

void mesh_frame_sphere(mesh_t *self, float radius)
{
	mesh_lock(self);
	mesh_circle(self, radius, 24, VEC3(1.0f, 0.0f, 0.0f));
	mesh_circle(self, radius, 24, VEC3(0.0f, 1.0f, 0.0f));
	mesh_circle(self, radius, 24, VEC3(0.0f, 0.0f, 1.0f));
	mesh_unlock(self);
}

void mesh_frame_cuboid(mesh_t *self, vec3_t p2, vec3_t p1)
{
	mesh_cuboid(self, 1.0f, p2, p1);
	mesh_remove_faces(self);
}

void mesh_frame_capsule(mesh_t *self, float radius, vec3_t dir)
{
	mesh_lock(self);

	mat4_t saved = mesh_save(self);

	float height = vec3_len(dir);
	vec4_t q;
	vec3_t v2 = vec3(0.0f, 0.0f, 1.0f);
	vec3_t cross = vec3_cross(v2, dir);
	q.xyz = cross;
	q.w = vec3_len(dir) + vec3_dot(dir, v2);
	q = vec4_norm(q);

	self->transformation = mat4_mul(self->transformation, quat_to_mat4(q));
	mesh_translate(self, vec3(0.0f, 0.0f, -height / 2.0f));

	mesh_circle(self, radius, 24, VEC3(0.0f, 0.0f, 1.0f));
	mesh_arc(self, radius, 12, VEC3(1.0f, 0.0f, 0.0f), M_PI / 2.0f, M_PI + M_PI / 2.0f, false);
	mesh_arc(self, radius, 12, VEC3(0.0f, 1.0f, 0.0f), 0.0f, M_PI, false);

	int32_t b0 = mesh_add_vert(self, VEC3( radius,    0.0f,   0.0f));
	int32_t b1 = mesh_add_vert(self, VEC3(-radius,    0.0f,   0.0f));
	int32_t b2 = mesh_add_vert(self, VEC3(   0.0f,  radius,   0.0f));
	int32_t b3 = mesh_add_vert(self, VEC3(   0.0f, -radius,   0.0f));
	int32_t t0 = mesh_add_vert(self, VEC3( radius,    0.0f, height));
	int32_t t1 = mesh_add_vert(self, VEC3(-radius,    0.0f, height));
	int32_t t2 = mesh_add_vert(self, VEC3(   0.0f,  radius, height));
	int32_t t3 = mesh_add_vert(self, VEC3(   0.0f, -radius, height));

	int32_t e0 = mesh_add_edge_s(self, b0, -1);
	int32_t e1 = mesh_add_edge_s(self, b1, -1);
	int32_t e2 = mesh_add_edge_s(self, b2, -1);
	int32_t e3 = mesh_add_edge_s(self, b3, -1);
	mesh_add_edge_s(self, t0, e0);
	mesh_add_edge_s(self, t1, e1);
	mesh_add_edge_s(self, t2, e2);
	mesh_add_edge_s(self, t3, e3);

	mesh_translate(self, vec3(0.0f, 0.0f, height));
	mesh_circle(self, radius, 24, VEC3(0.0f, 0.0f, 1.0f));
	mesh_arc(self, radius, 12, VEC3(1.0f, 0.0f, 0.0f), -M_PI / 2.0f, M_PI / 2.0f, false);
	mesh_arc(self, radius, 12, VEC3(0.0f, 1.0f, 0.0f), 0.0f, -M_PI, false);

	mesh_restore(self, saved);
	mesh_unlock(self);
}

void mesh_arc(mesh_t *self, float radius, int segments, vecN_t dir,
              float start, float end, bool_t closed)
{
#define DIMS(s, c) \
	(dir.x ? VEC3(0.0f, s, c) : \
	 dir.z ? VEC3(c, s, 0.0f) : \
			 VEC3(c, 0.0f, s))

	mesh_lock(self);

	int prev_e, first_e;

	prev_e = first_e = mesh_add_edge_s(self,
			mesh_add_vert(self, DIMS(sin(-start) * radius, cos(-start) * radius)), -1);
	mesh_select(self, SEL_EDITING, MESH_EDGE, first_e);

	float inc = (end - start) / segments;
	float a;

	int ai;
	for(ai = 1, a = start + inc; ai <= segments - closed; a += inc, ai++)
	{
		int e = mesh_add_edge_s(self,
				mesh_add_vert(self, DIMS(
						sin(-a) * radius,
						cos(-a) * radius)), prev_e);
		mesh_select(self, SEL_EDITING, MESH_EDGE, e);
		prev_e = e;
	}
	self->has_texcoords = 0;

	if(prev_e != first_e && closed)
	{
		mesh_link_edges(self, prev_e, first_e);
	}

	mesh_unlock(self);
#undef DIMS
}

void mesh_link_edges(mesh_t *self, int e0, int e1)
{
	m_edge(self, e0)->next = e1;
	m_edge(self, e1)->prev = e0;

	mesh_get_pair_edge(self, e0);
}

void mesh_circle(mesh_t *self, float radius, int segments, vecN_t dir)
{
	mesh_arc(self, radius, segments, dir, 0.0f, M_PI * 2, true);
}

float mesh_get_selection_radius(mesh_t *self, vecN_t center)
{
	mesh_selection_t *editing = &self->selections[SEL_EDITING];

	khiter_t i;
	int ei = -1;
	for (i = kh_begin(editing->edges); i != kh_end(editing->edges); ++i)
	{
		if (kh_exist(editing->edges, i))
		{
			ei = kh_key(editing->edges, i);
			break;
		}
	}

	if(ei == -1) return 0.0f;

	vecN_t pos = e_vert(m_edge(self, ei), self)->pos;

	return vecN_(len)(vecN_(sub)(pos, center));
}

vecN_t mesh_get_selection_center(mesh_t *self)
{
	vecN_t center = ZN;
	int count = 0;

	khash_t(id) *selected_edges = self->selections[SEL_EDITING].edges;
	khiter_t k;
	for(k = kh_begin(selected_edges); k != kh_end(selected_edges); ++k)
	{
		if(!kh_exist(selected_edges, k)) continue;
		int e_id = kh_key(selected_edges, k);
		edge_t *e = m_edge(self, e_id); if(!e) continue;
		count++;
		center = vecN_(add)(center, e_vert(e, self)->pos);
	}
	center = vecN_(scale)(center, 1.0f / count);

	return center;
}

#ifdef MESH4
int mesh_add_tetrahedron(mesh_t *self, int v0, int v1, int v2, int v3)
{
	mesh_lock(self);

	self->has_texcoords = 0;

	int cell_id = vector_reserve(self->cells);
	cell_t *cell = vector_get(self->cells, cell_id);
	cell_init(cell);

	self->current_cell = cell_id;

	cell->f[0] = mesh_add_triangle_s(self, v0, v1, v2);
	cell->f[1] = mesh_add_triangle_s(self, v0, v2, v3);
	cell->f[2] = mesh_add_triangle_s(self, v0, v3, v1);
	cell->f[3] = mesh_add_triangle_s(self, v1, v3, v2);

	/* v0 -> v1 */
	mesh_edge_pair(self, c_face(cell, 0, self)->e[0], c_face(cell, 2, self)->e[2]);
	/* v1 -> v2 */
	mesh_edge_pair(self, c_face(cell, 0, self)->e[1], c_face(cell, 3, self)->e[2]);
	/* v2 -> v0 */
	mesh_edge_pair(self, c_face(cell, 0, self)->e[2], c_face(cell, 1, self)->e[0]);
	/* v2 -> v3 */
	mesh_edge_pair(self, c_face(cell, 1, self)->e[1], c_face(cell, 3, self)->e[1]);
	/* v3 -> v0 */
	mesh_edge_pair(self, c_face(cell, 1, self)->e[2], c_face(cell, 2, self)->e[0]);
	/* v1 -> v3 */
	mesh_edge_pair(self, c_face(cell, 2, self)->e[1], c_face(cell, 3, self)->e[0]);

	cell->f_size = 4;

	self->current_cell = -1;

	mesh_modified(self);
	mesh_unlock(self);

	return cell_id;
}

void mesh_check_pairs(mesh_t *self)
{
	int e;
	for(e = 0; e < vector_count(self->edges); e++)
	{
		edge_t *edge = m_edge(self, e);
		if(edge && edge->pair == -1)
		{
			printf("EDGE %d of face %d and cell %d is unpaired\n", e,
					edge->face, e_face(edge, self)->cell);
			exit(1);
		}
	}
}

int mesh_add_tetrahedral_prism(mesh_t *self, int fid, int v0, int v1, int v2)
/* returns the new exposed face */
{
	const int table[8][3][4] = {
		{{-1, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}},
		{{ 0,  1,  2,  5}, { 0,  1,  5,  4}, { 0,  4,  5,  3}},
		{{ 0,  1,  2,  4}, { 0,  4,  2,  3}, { 3,  4,  2,  5}},
		{{ 0,  1,  2,  4}, { 0,  4,  5,  3}, { 0,  4,  2,  5}},
		{{ 0,  1,  2,  3}, { 1,  2,  3,  5}, { 1,  5,  3,  4}},
		{{ 0,  1,  2,  5}, { 0,  1,  5,  3}, { 1,  3,  4,  5}},
		{{ 0,  1,  2,  3}, { 1,  4,  2,  3}, { 2,  4,  5,  3}},
		{{-1, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}}
	};

	face_t *f = m_face(self, fid);
	edge_t *e0 = f_edge(f, 0, self),
		   *e1 = f_edge(f, 1, self),
		   *e2 = f_edge(f, 2, self);

	int verts[6] = { v0, v1, v2, e0->v, e1->v, e2->v };

	for(int i = 0; i < 5; i++) for(int j = i + 1; j < 6; j++)
	{
		if(verts[i] == verts[j])
		{
			printf("EQUALS %d %d %d %d %d %d\n", v0, v1, v2, verts[3],
					verts[4], verts[5]);
			exit(1);
		}
	}

	int i = (e0->tmp << 2) | (e1->tmp << 1) | (e2->tmp << 0);

	#define config table[i]
	if(i == 0 || i == 7) printf("Invalid configuration\n");

	int t;
	int next = -1;
	for(t = 0; t < 3; t++)
	{
		int tet = mesh_add_tetrahedron(self,
				verts[config[t][0]],
				verts[config[t][1]],
				verts[config[t][2]],
				verts[config[t][3]]);

		if(next == -1) next = tet;
	}

	f = m_face(self, fid);
	int new_f = m_cell(self, next)->f[0];
	face_t *face = m_face(self, new_f);
	f_edge(face, 0, self)->tmp = f_edge(f, 0, self)->tmp;
	f_edge(face, 1, self)->tmp = f_edge(f, 1, self)->tmp;
	f_edge(face, 2, self)->tmp = f_edge(f, 2, self)->tmp;
	
	return new_f;
}
#endif

int mesh_check_duplicate_verts(mesh_t *self, int edge_id)
{
	int j;
	vecN_t pos = m_vert(self, edge_id)->pos;
	for(j = 0; j < vector_count(self->verts); j++)
	{
		if(j == edge_id) continue;
		if(vecN_(dist)(m_vert(self, j)->pos, pos) == 0.0f)
		{
			printf("duplicated verts! %d %d ", edge_id, j);
			vecN_(print)(pos);
			return 1;
		}
	}
	return 0;
}

void mesh_vert_modify(mesh_t *self, vecN_t pivot,
		float factor, vecN_t offset)
{

	khash_t(id) *selected_edges = self->selections[SEL_EDITING].edges;
	khiter_t k;
	for(k = kh_begin(selected_edges); k != kh_end(selected_edges); ++k)
	{
		if(!kh_exist(selected_edges, k)) continue;
		int e_id = kh_key(selected_edges, k);
		edge_t *e = m_edge(self, e_id); if(!e) continue;
		/* if(e_vert(e, self)->tmp >= 0) continue; */

		vertex_t *v = e_vert(e, self);

		vecN_t new_pos = vecN_(sub)(v->pos, pivot);

		new_pos = vecN_(add)(vecN_(scale)(new_pos, factor), pivot);

		new_pos = vecN_(add)(new_pos, offset);

		v->pos = new_pos;
		/* e_vert(e, self)->tmp = 1; */
	}
}

void mesh_vert_dup_and_modify(mesh_t *self, vecN_t pivot,
		float factor, vecN_t offset)
{

	khash_t(id) *selected_edges = self->selections[SEL_EDITING].edges;
	khiter_t k;
	for(k = kh_begin(selected_edges); k != kh_end(selected_edges); ++k)
	{
		if(!kh_exist(selected_edges, k)) continue;
		int e_id = kh_key(selected_edges, k);
		edge_t *e = m_edge(self, e_id); if(!e) continue;
		if(e_vert(e, self)->tmp >= 0) continue;

		vertex_t *v = e_vert(e, self);

		vecN_t new_pos = vecN_(sub)(v->pos, pivot);

		new_pos = vecN_(add)(vecN_(scale)(new_pos, factor), pivot);

		new_pos = vecN_(add)(new_pos, offset);

		int tmp = mesh_add_vert(self, new_pos);
		e_vert(e, self)->tmp = tmp;
	}
}

#ifdef MESH4
void mesh_extrude_faces(mesh_t *self, int steps, vecN_t offset,
		float scale, modifier_cb scale_cb, modifier_cb offset_cb,
		void *usrptr)
{
	if(!self->triangulated)
	{
		printf("Can't extrude untriangulated mesh.\n");
		return;
	}

	int step;

	mesh_lock(self);

	self->has_texcoords = 0;

	float percent = 0.0f;
	float percent_inc = 1.0f / steps;
	float prev_factor = 1.0f;
	/* vecN_t center = mesh_get_selection_center(self); */

	vecN_t center = mesh_get_selection_center(self);
	float radius = mesh_get_selection_radius(self, center);

	if(!mesh_update_flips(self))
	{
		printf("Extrude tetrahedral not possible in this mesh.\n");
		mesh_unlock(self);
		return;
	}

	int TMP = 4;
	for(step = 0; step < steps; step++)
	{
		self->current_surface++;

		mesh_selection_t *editing = &self->selections[SEL_EDITING];

		center = mesh_get_selection_center(self);

		percent += percent_inc;
		float factor, current_factor;
		if(scale_cb)
		{
			current_factor = scale_cb(self, percent, usrptr);
		}
		else
		{
			current_factor = 1.0f + (scale - 1.0f) * percent;
		}
		factor = current_factor / prev_factor;
		float o_current_factor;
		vecN_t current_offset;
		if(offset_cb)
		{
			radius *= factor;

			o_current_factor = offset_cb(self, radius, usrptr);
			current_offset = vecN_(sub)(vecN_(scale)(offset, o_current_factor),
					center);
		}
		else
		{
			o_current_factor = percent_inc;
			current_offset = vecN_(scale)(offset, o_current_factor);
		}

		mesh_vert_dup_and_modify(self, center, factor, current_offset);

		prev_factor = current_factor;

		khash_t(id) *selected_faces = editing->faces;
		khiter_t k;

		for(k = kh_begin(selected_faces); k != kh_end(selected_faces); ++k)
		{
			if(!kh_exist(selected_faces, k)) continue;
			int f_id = kh_key(selected_faces, k);

			face_t *f = m_face(self, f_id); if(!f) continue;

			int v0 = f_vert(f, 0, self)->tmp;
			int v1 = f_vert(f, 1, self)->tmp;
			int v2 = f_vert(f, 2, self)->tmp;

			int new_exposed_face =
				mesh_add_tetrahedral_prism(self, f_id, v0, v1, v2);

			mesh_select(self, TMP, MESH_FACE, new_exposed_face);

		}
		mesh_unselect(self, SEL_EDITING, MESH_ANY, -1);

		mesh_selection_t swap = self->selections[SEL_EDITING];
		self->selections[SEL_EDITING] = self->selections[TMP];
		self->selections[TMP] = swap;

		/* mesh_unlock(self); */

	}

	mesh_unlock(self);
}
#endif

void mesh_translate_points(mesh_t *self, float percent, vecN_t offset,
		float scale, modifier_cb scale_cb, modifier_cb offset_cb, void *usrptr)
{
	mesh_lock(self);
	vecN_t center = mesh_get_selection_center(self);

	float factor = scale_cb ? scale_cb(self, percent, usrptr) : 1.0f;
	factor *= 1 + (scale - 1) * percent;

	float o_current_factor = offset_cb ? offset_cb(self, factor, usrptr) : 1.0f;
	float o_factor = o_current_factor;
	vecN_t inc = vecN_(mul_number)(offset, o_factor);

	mesh_vert_modify(self, center, factor, inc);
	mesh_unlock(self);
}

void mesh_extrude_edges(mesh_t *self, int steps, vecN_t offset,
		float scale, modifier_cb scale_cb,
		modifier_cb offset_cb, void *usrptr)
{
	int step;

	mesh_lock(self);

	float percent = 0.0f;
	float percent_inc = 1.0f / steps;
	float prev_factor = 1.0f;

	vecN_t center = mesh_get_selection_center(self);
	float radius = 0;
	if(offset_cb)
	{
		radius = mesh_get_selection_radius(self, center);
	}

	int TMP = 4;
	for(step = 0; step < steps; step++)
	{
		mesh_selection_t *editing = &self->selections[SEL_EDITING];

		center = mesh_get_selection_center(self);

		percent += percent_inc;
		float factor, current_factor;
		if(scale_cb)
		{
			current_factor = scale_cb(self, percent, usrptr);
		}
		else
		{
			current_factor = 1.0f + (scale - 1.0f) * percent;
		}
		factor = current_factor / prev_factor;
		float o_current_factor;
		vecN_t current_offset;
		if(offset_cb)
		{
			radius *= factor;

			o_current_factor = offset_cb(self, radius, usrptr);
			current_offset = vecN_(sub)(vecN_(scale)(offset, o_current_factor),
					center);
		}
		else
		{
			o_current_factor = percent_inc;
			current_offset = vecN_(scale)(offset, o_current_factor);
		}

		mesh_vert_dup_and_modify(self, center, factor, current_offset);

		prev_factor = current_factor;

		khash_t(id) *selected_edges = editing->edges;
		khiter_t k;

		for(k = kh_begin(selected_edges); k != kh_end(selected_edges); ++k)
		{
			if(!kh_exist(selected_edges, k)) continue;
			int e_id = kh_key(selected_edges, k);

			edge_t *e = m_edge(self, e_id); if(!e) continue;

			edge_t *ne = e_next(e, self); if(!ne) continue;

			int et = e_vert(e, self)->tmp;
			int nt = e_vert(ne, self)->tmp;
			if(et == -1 || nt == -1)
			{
				continue;
			}

			vec2_t new_e_t = vec2_scale(e->t, scale * 0.9);
			vec2_t new_n_t = vec2_scale(ne->t, scale * 0.9);
			mesh_add_quad(self,
					ne->v, Z3, ne->t,
					e->v, Z3, e->t,
					et, Z3, new_e_t,
					nt, Z3, new_n_t
			);
			/* mesh_edge_select(self, e_id, SEL_UNSELECTED); */

			e = m_edge(self, e_id);
			int new = e_prev(e_pair(e, self), self)->prev;
			if(new == -1) exit(1);
			mesh_select(self, TMP, MESH_EDGE, new);

		}
		mesh_unselect(self, SEL_EDITING, MESH_ANY, -1);

		mesh_selection_t swap = self->selections[SEL_EDITING];
		self->selections[SEL_EDITING] = self->selections[TMP];
		self->selections[TMP] = swap;
	}

	mesh_unlock(self);
}

void mesh_assign(mesh_t *self, mesh_t *other)
{
	mesh_lock(self);

	void *semaphore = self->semaphore;
	uint64_t owner = self->owner_thread;
	int locked_read = self->locked_read;

	_mesh_destroy(self);
	_mesh_clone(other, self);

	self->semaphore = semaphore;
	self->owner_thread = owner;
	self->locked_read = locked_read;

	mesh_modified(self);
	mesh_unlock(self);
}

void _mesh_clone(mesh_t *self, mesh_t *into)
{
	*into = *self;
	into->ref_num = 1;
	/* into->changes = 0; */
	into->faces_hash = kh_clone(id, self->faces_hash);
	into->edges_hash = kh_clone(id, self->edges_hash);
	into->verts = vector_clone(self->verts);
	into->faces = vector_clone(self->faces);
	into->edges = vector_clone(self->edges);
#ifdef MESH4
	into->cells = vector_clone(self->cells);
#endif

	int i;
	for(i = 0; i < 16; i++)
	{
		into->selections[i].faces = kh_clone(id, self->selections[i].faces);
		into->selections[i].edges = kh_clone(id, self->selections[i].edges);
		into->selections[i].verts = kh_clone(id, self->selections[i].verts);
#ifdef MESH4
		into->selections[i].cells = kh_clone(id, self->selections[i].cells);
#endif
	}
}

mesh_t *mesh_clone(mesh_t *self)
{
	mesh_t *clone;
	if(self)
	{
		clone = malloc(sizeof(*self));
		_mesh_clone(self, clone);
		clone->semaphore = SDL_CreateSemaphore(1);
		if(self->locked_read)
		{
			SDL_SemWait(clone->semaphore);
		}
	}
	else
	{
		clone = mesh_new();
	}

	return clone;
}

mesh_t *mesh_torus(float radius, float inner_radius, int segments,
		int inner_segments)
{
	mesh_t *tmp = mesh_new();

	float inner_inc = (M_PI * 2) / inner_segments;
	float a;

	int prev_e, first_e;

	prev_e = first_e = mesh_add_edge_s(tmp,
			mesh_add_vert(tmp, VEC3(
					radius + sin(0) * inner_radius,
					cos(0) * inner_radius,
					0.0
			)), -1);

	int ai;
	for(ai = 1, a = inner_inc; ai < inner_segments; a += inner_inc, ai++)
	{
		int e = mesh_add_edge_s(tmp, mesh_add_vert(tmp, VEC3(
					radius + sin(a) * inner_radius,
					cos(a) * inner_radius,
					0.0
		)), prev_e);
		prev_e = e;
	}

	if(prev_e != first_e) m_edge(tmp, prev_e)->next = first_e;

	mesh_t *self = mesh_lathe(tmp, M_PI * 2, segments, 0, 1, 0);

	mesh_destroy(tmp);

	return self;
}

mesh_t *mesh_lathe(mesh_t *mesh, float angle, int segments,
		float x, float y, float z)
{
	int ei;
	float a;
	mat4_t  rot = mat4();

	mesh_t *self = mesh_new();
	mesh_lock(self);
	self->has_texcoords = 0;

	float inc = angle / segments;

	int ai;

	int verts[vector_count(mesh->edges) * segments];
 

	for(ai = 0, a = inc; ai < segments; a += inc, ai++)
	{
		rot = mat4_rotate(rot, x, y, z, inc);
		for(ei = 0; ei < vector_count(mesh->edges); ei++)
		{
			edge_t *e = m_edge(mesh, ei);
			if(!e) continue;

			verts[ei * segments + ai] = mesh_add_vert(self,
					VEC3(_vec3(mat4_mul_vec4( rot, vec4(_vec3(e_vert(e,
											mesh)->pos), 1.0)).xyz)));
		}
	}

	float pa = 0.0;
	for(ai = 0, a = inc; ai < segments; a += inc, ai++)
	{
		for(ei = 0; ei < vector_count(mesh->edges); ei++)
		{
			edge_t *e = m_edge(mesh, ei);
			if(!e) continue;

			edge_t *ne = m_edge(mesh, e->next);
			if(!ne) continue;

			int next_ei = ((ei + 1) == vector_count(mesh->edges)) ? 0 : ei + 1;
			/* ei = edge index */
			int next_ai = ((ai + 1) == segments)		 ? 0 : ai + 1;
			/* ai = angle index */

			int v1 = verts[ei      * segments + ai];
			int v2 = verts[next_ei * segments + ai];

			int v3 = verts[next_ei * segments + next_ai];
			int v4 = verts[ei      * segments + next_ai];

			mesh_add_quad(self, v1, Z3, vec2( e->t.x, pa),
								v2, Z3, vec2(ne->t.x, pa),
								v3, Z3, vec2(ne->t.x,  a),
								v4, Z3, vec2( e->t.x,  a));
		}
		pa = a;
	}

	/* self->wireframe = 1; */
	mesh_unlock(self);

	return self;
}

void mesh_cuboid(mesh_t *self, float tex_scale, vec3_t p2, vec3_t p1)
{
	mesh_lock(self);
	int v[8] = {
		/* 0 */ mesh_add_vert(self, VEC3(p1.x, p1.y, p1.z)),
		/* 1 */ mesh_add_vert(self, VEC3(p2.x, p1.y, p1.z)),
		/* 2 */ mesh_add_vert(self, VEC3(p2.x, p2.y, p1.z)),
		/* 3 */ mesh_add_vert(self, VEC3(p1.x, p2.y, p1.z)),
		/* 4 */ mesh_add_vert(self, VEC3(p1.x, p1.y, p2.z)),
		/* 5 */ mesh_add_vert(self, VEC3(p2.x, p1.y, p2.z)),
		/* 6 */ mesh_add_vert(self, VEC3(p2.x, p2.y, p2.z)),
		/* 7 */ mesh_add_vert(self, VEC3(p1.x, p2.y, p2.z))
	};

	mesh_add_quad(self,
			v[0], Z3, vec2(p1.x, p1.y),
			v[1], Z3, vec2(p2.x, p1.y),
			v[2], Z3, vec2(p2.x, p2.y),
			v[3], Z3, vec2(p1.x, p2.y));

	mesh_add_quad(self,
			v[5], Z3, vec2(p2.x, p1.y),
			v[4], Z3, vec2(p1.x, p1.y),
			v[7], Z3, vec2(p1.x, p2.y),
			v[6], Z3, vec2(p2.x, p2.y));

	mesh_add_quad(self,
			v[6], Z3, vec2(p2.y, p2.z),
			v[2], Z3, vec2(p2.y, p1.z),
			v[1], Z3, vec2(p1.y, p1.z),
			v[5], Z3, vec2(p1.y, p2.z));

	mesh_add_quad(self,
			v[3], Z3, vec2(p2.y, p1.z),
			v[7], Z3, vec2(p2.y, p2.z),
			v[4], Z3, vec2(p1.y, p2.z),
			v[0], Z3, vec2(p1.y, p1.z));

	mesh_add_quad(self,
			v[7], Z3, vec2(p1.x, p2.z),
			v[3], Z3, vec2(p1.x, p1.z),
			v[2], Z3, vec2(p2.x, p1.z),
			v[6], Z3, vec2(p2.x, p2.z));

	mesh_add_quad(self,
			v[5], Z3, vec2(p2.x, p2.z),
			v[1], Z3, vec2(p2.x, p1.z),
			v[0], Z3, vec2(p1.x, p1.z),
			v[4], Z3, vec2(p1.x, p2.z));

	/* mesh_translate_uv(self, 0.5, 0.5); */
	mesh_scale_uv(self, tex_scale);

	mesh_unlock(self);
}

static
void _mesh_point_grid(mesh_t *self, vecN_t start, vecN_t size,
                      uvecN_t segments, uint32_t dim)
{
	if (dim == N)
	{
		mesh_add_vert(self, start);
	}
	else
	{
		vecN_t iter = start;
		for (uint32_t i = 0; i < segments._[dim]; i++)
		{
			iter._[dim] = start._[dim] + size._[dim] * i;
			_mesh_point_grid(self, iter, size, segments, dim + 1);
		}
	}
}

void mesh_point_grid(mesh_t *self, vecN_t start, vecN_t size, uvecN_t segments)
{
	mesh_lock(self);
	_mesh_point_grid(self, start, size, segments, 0);
	mesh_unlock(self);
}

void mesh_cube(mesh_t *self, float size, float tex_scale)
{
	size /= 2.0f;
	mesh_cuboid(self, tex_scale, vec3(-size, -size, -size),
	            vec3(size, size, size));
}

void mesh_wait(mesh_t *self)
{
	SDL_SemWait(self->semaphore);
	SDL_SemPost(self->semaphore);
}

void mesh_load(mesh_t *self, const char *filename)
{
	char ext[16];
	mesh_lock(self);

	strncpy(ext, strrchr(filename, '.') + 1, sizeof(ext) - 1);

	if(!strncmp(ext, "ply", sizeof(ext) - 1))
	{
		mesh_load_ply(self, filename);
	}
	else if(!strncmp(ext, "obj", sizeof(ext) - 1))
	{
		mesh_load_obj(self, filename);
	}
	mesh_unlock(self);
}

vertex_t *mesh_farthest(mesh_t *self, const vec3_t dir)
{
	/* TODO NEEDS OPTIMIZATION */
	vertex_t *v = vector_get_set(self->verts, 0);
	if(!v) return NULL;

	float last_dist = vec3_dot(dir, XYZ(v->pos));

	int si;
	for(si = 1; si < vector_count(self->verts); si++)
	{
		vertex_t *vi = m_vert(self, si);
		float temp = vec3_dot(dir, XYZ(vi->pos));
		if(temp > last_dist)
		{
			last_dist = temp;
			v = vi;
		}
	}
	return v;
}

static vec3_t mesh_support(mesh_t *self, const vec3_t dir)
{
	return XYZ(mesh_farthest(self, dir)->pos);
}

float mesh_get_margin(const mesh_t *self)
{
	return 0.001f; /* FIXME */
}

int load_mesh(mesh_t *mesh)
{
	char buffer[256];
	strcpy(buffer, mesh->name);
	printf("loading %s\n", mesh->name);
	mesh_load(mesh, buffer);
	return 1;
}

void *mesh_loader(const char *path, const char *name, uint32_t ext)
{
	mesh_t *mesh = mesh_new();
	strcpy(mesh->name, path);

#ifndef __EMSCRIPTEN__
	SDL_CreateThread((int(*)(void*))load_mesh, "load_mesh", mesh);
#else
	load_mesh(mesh);
#endif

	return mesh;
}

void meshes_reg()
{
	sauces_loader(ref("obj"), mesh_loader);
	sauces_loader(ref("ply"), mesh_loader);
}

/* http://paulbourke.net/papers/triangulate/ */

void mesh_triangulate_poly(mesh_t *self)
{
	khash_t(id) *selected_edges = kh_clone(id,
			self->selections[SEL_EDITING].edges);
	khiter_t k;

	int *bounds = malloc(sizeof(int) * kh_size(selected_edges));
	int num_bounds = 0;

	while(kh_size(selected_edges))
	{
		int first_edge = -1;

		for(k = kh_begin(selected_edges); k != kh_end(selected_edges); ++k)
		{
			if(!kh_exist(selected_edges, k)) continue;
			first_edge = kh_key(selected_edges, k);
			break;
		}
		if(first_edge == -1) break;

		bounds[num_bounds++] = first_edge;

		/* remove connected edges from hash table */
		int e = first_edge;
		do
		{
			khiter_t k2 = kh_get(id, selected_edges, e);
			if(kh_exist(selected_edges, k2)) kh_del(id, selected_edges, k2);
			e = m_edge(self, e)->next;
		}
		while(e != first_edge && e != -1);
		
	}
	printf("%d\n", num_bounds);

	free(bounds);
	kh_destroy(id, selected_edges);
}
