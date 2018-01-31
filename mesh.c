#include "mesh.h"
#include "formats/obj.h"
#include "formats/ply.h"
#include <candle.h>
#include <stdlib.h>
#include <stdio.h>

static vec3_t mesh_support(mesh_t *self, const vec3_t dir);
#ifdef MESH4
static void mesh_cells_grow(mesh_t *self);
#endif
static void mesh_verts_grow(mesh_t *self);
static void mesh_edges_grow(mesh_t *self);
static void mesh_faces_grow(mesh_t *self);
static int mesh_get_pair_edge(mesh_t *self, int e);
int mesh_get_face_from_verts(mesh_t *self, int v0, int v1, int v2);
static void mesh_update_cell_pairs(mesh_t *self);
static void mesh_udpate_selection_list(mesh_t *self);

#ifdef MESH4
void mesh_cells_prealloc(mesh_t *self, int size);
#endif

#define CHUNK_CELLS 20
#define CHUNK_VERTS 30
#define CHUNK_EDGES 20
#define CHUNK_FACES 10

void mesh_modified(mesh_t *self)
{
	self->changes++;
}

mesh_t *mesh_new()
{
	mesh_t *self = calloc(1, sizeof *self);

	self->transformation = mat4();
	self->backup = mat4();
	self->has_texcoords = 1;
	self->triangulated = 1;
	self->current_cell = -1;
	self->current_surface = -1;
	self->smooth_max = 0.4;
	self->first_edge = 0;

	self->unpaired_faces = kl_init(int);
	self->selected_faces = kl_init(int);
	self->selected_edges = kl_init(int);

	self->support = (support_cb)mesh_support;

#ifdef MESH4
	mesh_cells_prealloc(self, 10);
#endif
	mesh_edges_prealloc(self, 100);
	mesh_faces_prealloc(self, 100);
	mesh_verts_prealloc(self, 100);

	return self;
}

/* TODO: split this to a vector */

static void vert_init(vertex_t *self)
{
	self->removed = 0;
	self->selected = 0;
	self->color = vec4(0.0f);
	self->tmp = -1;
	int i;
	for(i = 0; i < 16; i++) self->halves[i] = -1;
}

static int mesh_get_free_vert(mesh_t *self)
{
	int i;

	if(!self->free_verts)
	{
		i = self->verts_size++;
		mesh_verts_grow(self);

		vert_init(&self->verts[i]);
		return i;
	}
	else
	{
		for(i = 0; i < self->verts_size; i++)
		{
			if(self->verts[i].removed)
			{
				self->free_verts--;
				vert_init(&self->verts[i]);

				return i;
			}
		}
	}
	return -1;
}

static void edge_init(edge_t *self)
{
	self->removed = 0;
	self->selected = 0;
	self->pair = -1;
	self->next = -1;
	self->prev = -1;
	self->cell_pair = -1;

	self->extrude_flip = -1;

	self->n = vec3(0.0f);

	self->v = -1;

	self->face = -1;
}

static int mesh_get_free_edge(mesh_t *self)
{
	int i;

	if(!self->free_edges)
	{
		i = self->edges_size++;
		mesh_edges_grow(self);
		edge_init(&self->edges[i]);
		return i;
	}
	else
	{
		for(i = 0; i < self->edges_size; i++)
		{
			if(self->edges[i].removed)
			{
				self->free_edges--;

				edge_init(&self->edges[i]);
				return i;
			}
		}
	}
	return -1;
}

#ifdef MESH4
static void cell_init(cell_t *self)
{
	self->removed = 0;
	self->selected = 0;
	self->f_size = 0;
	self->f[0] = self->f[1] = self->f[2] = self->f[3] = -1;
}
#endif

static void face_init(face_t *self)
{
	self->removed = 0;
	self->selected = 0;
	self->n = vec3(0.0f);
	self->e_size = 0;
	self->triangulate_flip = 0;
	self->e[0] = self->e[1] = self->e[2] = -1;

#ifdef MESH4
	self->pair = -1;
	self->cell = -1;
	self->surface = -1;
#endif
}

#ifdef MESH4
static int mesh_get_free_cell(mesh_t *self)
{
	int i;

	if(!self->free_cells)
	{
		i = self->cells_size++;
		mesh_cells_grow(self);
		cell_init(&self->cells[i]);
		return i;
	}
	else
	{
		for(i = 0; i < self->cells_size; i++)
		{
			if(self->cells[i].removed)
			{
				self->free_cells--;
				cell_init(&self->cells[i]);

				return i;
			}
		}
	}
	return -1;
}
#endif

static int mesh_get_free_face(mesh_t *self)
{
	int i;

	if(!self->free_faces)
	{
		i = self->faces_size++;
		mesh_faces_grow(self);
		face_init(&self->faces[i]);
		return i;
	}
	else
	{
		for(i = 0; i < self->faces_size; i++)
		{
			if(self->faces[i].removed)
			{
				self->free_faces--;
				face_init(&self->faces[i]);

				return i;
			}
		}
	}
	return -1;
}
/* /TODO */


void mesh_destroy(mesh_t *self)
{
#ifdef MESH4
	if(self->cells) free(self->cells);
#endif
	if(self->faces) free(self->faces);
	if(self->verts) free(self->verts);
	if(self->edges) free(self->edges);

	kl_destroy(int, self->unpaired_faces);
	kl_destroy(int, self->selected_faces);
	kl_destroy(int, self->selected_edges);

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
	for(i = 0; i < self->verts_size; i++)
	{
		if(vecN_(equals)(self->verts[i].pos, p)) return i;
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

	int last = start->extrude_flip;
	for(e = e_next(start, self); e && e != start; e = e_next(e, self))
	{
		if(last != e->extrude_flip) return 1;
	}
	return 0;
}

void mesh_invert_normals(mesh_t *self)
{
	mesh_lock(self);

	kliter_t(int) *sf;
	for(sf = self->selected_faces->head;
			sf != self->selected_faces->tail;
			sf = sf->next)
	{
		face_t *f = m_face(self, sf->data); if(!f) continue;

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
		/* FIXME */

		e0->next = f->e[1]; e0->prev = f->e[2];
		e1->next = f->e[2]; e1->prev = f->e[0];
		e2->next = f->e[0]; e2->prev = f->e[1];
	}
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
	for(i = 0; i < self->faces_size; i++)
	{
		face_t *f = m_face(self, i); if(!f) continue;
		if(!f_cell(f, self))
		{
			mesh_remove_face(self, i);
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
	for(i = 0; i < self->edges_size; i++)
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
	int i, e;
	int j, fix_iterations = 8;

	mesh_update_cell_pairs(self);

	kliter_t(int) *uf;
	for(uf = self->selected_faces->head;
			uf != self->selected_faces->tail;
			uf = uf->next)
	{
		i = uf->data;
		face_t *face = m_face(self, i);
		if(!face || face->selected != 1) continue;

		int last = 0;
		for(e = 0; e < face->e_size; e++)
		{
			f_edge(face, e, self)->extrude_flip = last;
			last = !last;
		}
	}
	for(j = 0; j < fix_iterations; j++)
	{
		int fixes = 0;
		kliter_t(int) *uf;
		for(uf = self->selected_edges->head;
				uf != self->selected_edges->tail;
				uf = uf->next)
		{
			e = uf->data;
			edge_t *edge = m_edge(self, e);
			if(!edge || edge->selected != 1) continue;

			/* int cpair = mesh_edge_cell_pair(self, edge); */
			/* edge_t *pair = m_edge(self, cpair); if(!pair) continue; */

			edge_t *pair = e_cpair(edge, self); if(!pair) continue;
			edge_t *change;

			if(edge->extrude_flip == pair->extrude_flip)
			{
				int value = !edge->extrude_flip;
				edge->extrude_flip = !edge->extrude_flip;
				if(!mesh_face_extrude_possible(self, edge->face))
				{
					change = (j & 2) ? e_next(edge, self) : e_prev(edge, self);
					if(change) change->extrude_flip = !value;
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

/* static void mesh_check_selection_list(mesh_t *self) */
/* { */
/* 	kliter_t(int) **uf; */
/* 	kliter_t(int) **next; */
/* 	for(uf = &self->selected_faces->head; */
/* 			*uf && *uf != self->selected_faces->tail; */
/* 			uf = next) */
/* 	{ */
/* 		face_t *face = m_face(self, (*uf)->data); */
/* 		next = &(*uf)->next; */
/* 		if(!face) printf("x"); */
/* 		if(!face->selected) printf("!"); */
/* 	} */
/* 	printf("\n"); */
/* } */

static void mesh_udpate_selection_list(mesh_t *self)
{
	kliter_t(int) **uf;
	if(self->selected_faces->modified)
	{
		for(uf = &self->selected_faces->head;
				*uf != self->selected_faces->tail;)
		{
			face_t *face = m_face(self, (*uf)->data);
			if(!face || !face->selected)
			{
				kliter_t(int) *p = *uf;
				*uf = p->next;
				free(p);
				self->selected_faces->size--;
			}
			else
			{
				uf = &(*uf)->next;
			}
		}
		self->selected_faces->modified = 0;
	}
	if(self->selected_edges->modified)
	{
		for(uf = &self->selected_edges->head;
				*uf != self->selected_edges->tail;)
		{
			edge_t *edge = m_edge(self, (*uf)->data);
			if(!edge || !edge->selected)
			{
				kliter_t(int) *p = *uf;
				*uf = p->next;
				free(p);
				self->selected_edges->size--;														\
			}
			else
			{
				uf = &(*uf)->next;
			}
		}
		self->selected_edges->modified = 0;
	}
}

void mesh_edge_set_selection(mesh_t *self, int edge_id, int selection)
{
	edge_t *edge = m_edge(self, edge_id); if(!edge) return;
	if(!edge->selected && selection)
	{
		*kl_pushp(int, self->selected_edges) = edge_id;
	}
	else if(edge->selected && !selection)
	{
		self->selected_edges->modified = 1;
	}

	edge->selected = selection;
}

void mesh_face_set_selection(mesh_t *self, int face_id, int selection)
{
	int e;
	face_t *face = m_face(self, face_id); if(!face) return;
	for(e = 0; e < face->e_size; e++)
	{
		mesh_edge_set_selection(self, face->e[e], selection);
	}

	if(!face->selected && selection)
	{
		*kl_pushp(int, self->selected_faces) = face_id;
	}
	else if(face->selected && !selection)
	{
		self->selected_faces->modified = 1;
	}
	face->selected = selection;

}

void mesh_unselect_faces(mesh_t *self)
{
	int i;
	for(i = 0; i < self->faces_size; i++)
	{
		mesh_face_set_selection(self, i, 0);
	}
	mesh_udpate_selection_list(self);
}

void mesh_select_faces(mesh_t *self)
{
	int i;
	for(i = 0; i < self->faces_size; i++)
	{
		mesh_face_set_selection(self, i, 1);
	}
}

void mesh_select_edges(mesh_t *self)
{
	int i;
	for(i = 0; i < self->edges_size; i++)
	{
		mesh_edge_set_selection(self, i, 1);
	}
}

void mesh_unselect_edges(mesh_t *self)
{
	int i;
	for(i = 0; i < self->edges_size; i++)
	{
		mesh_edge_set_selection(self, i, 0);
	}
	mesh_udpate_selection_list(self);
}

void mesh_for_each_selected(mesh_t *self, geom_t geom, iter_cb cb)
{
	if(geom == MESH_FACE)
	{
		kliter_t(int) *uf;
		for(uf = self->selected_faces->head;
				uf != self->selected_faces->tail;
				uf = uf->next)
		{
			face_t *face = m_face(self, uf->data); if(!face) continue;
			if(face->selected != 1) continue;
			if(!cb(self, face)) break;
		}
	}
	else
	{
		kliter_t(int) *se;
		for(se = self->selected_edges->head;
				se != self->selected_edges->tail;
				se = se->next)
		{
			edge_t *e = m_edge(self, se->data);
			if(!e || e->selected != 1) continue;
			int res;
			if(geom == MESH_EDGE)
			{
				res = cb(self, e);
			}
			else
			{
				res = cb(self, e_vert(e, self));
			}
			if(res == 0) break;
		}
	}
}

void mesh_paint(mesh_t *self, vec4_t color)
{
	kliter_t(int) *se;
	for(se = self->selected_edges->head;
			se != self->selected_edges->tail;
			se = se->next)
	{
		edge_t *e = m_edge(self, se->data);
		if(!e || e->selected != 1) continue;
		e_vert(e, self)->color = color;
	}
	mesh_modified(self);
}

void mesh_unlock(mesh_t *self)
{
	self->update_locked--;
	mesh_update(self);
}

void mesh_lock(mesh_t *self)
{
	self->update_locked++;
}

static void mesh_sphere_1_subdivide(mesh_t *self)
{
	/* mesh has to be triangulated */
	int i;

	mesh_lock(self);
	kliter_t(int) *uf;
	for(uf = self->selected_faces->head;
			uf != self->selected_faces->tail;
			uf = uf->next)
	{
		i = uf->data;
		face_t *face = m_face(self, i); if(!face) continue;
		if(face->selected != 1) continue;

		edge_t *e0 = f_edge(face, 0, self);
		edge_t *e1 = f_edge(face, 1, self);
		edge_t *e2 = f_edge(face, 2, self);

		vertex_t *v0 = e_vert(e0, self);
		vertex_t *v1 = e_vert(e1, self);
		vertex_t *v2 = e_vert(e2, self);

		vecN_t v01, v12, v20;    
		v01 = vecN_(norm)(vecN_(add)(v0->pos, v1->pos));
		v12 = vecN_(norm)(vecN_(add)(v1->pos, v2->pos));
		v20 = vecN_(norm)(vecN_(add)(v2->pos, v0->pos));

		vec2_t t0 = e0->t, t1 = e1->t, t2 = e2->t;    
		vec2_t t01, t12, t20;    
		t01 = vec2_scale(vec2_add(e0->t, e1->t), 0.5);
		t12 = vec2_scale(vec2_add(e1->t, e2->t), 0.5);
		t20 = vec2_scale(vec2_add(e2->t, e0->t), 0.5);

		int i0 = e0->v;
		int i1 = e1->v;
		int i2 = e2->v;

		int i01 = mesh_assert_vert(self, v01);
		int i12 = mesh_assert_vert(self, v12);
		int i20 = mesh_assert_vert(self, v20);

		int f0 = mesh_add_triangle(self, i0, Z3, t0,
								i01, Z3, t01,
								i20, Z3, t20, 1);

		int f1 = mesh_add_triangle(self, i1, Z3, t1,
								i12, Z3, t12,
								i01, Z3, t01, 1);

		int f2 = mesh_add_triangle(self, i2, Z3, t2,
								i20, Z3, t20,
								i12, Z3, t12, 1);

		int f3 = mesh_add_triangle(self, i01, Z3, t01,
								i12, Z3, t12,
								i20, Z3, t20, 1);
		mesh_remove_face(self, i);

		mesh_face_set_selection(self, f0, 2);
		mesh_face_set_selection(self, f1, 2);
		mesh_face_set_selection(self, f2, 2);
		mesh_face_set_selection(self, f3, 2);
	}
	mesh_udpate_selection_list(self);

	for(uf = self->selected_faces->head;
			uf != self->selected_faces->tail;
			uf = uf->next)
	{
		i = uf->data;
		face_t *face = m_face(self, i); if(!face) continue;
		if(face->selected == 2) mesh_face_set_selection(self, i, 1);
	}

	mesh_update_unpaired_edges(self);

	mesh_unlock(self);

}

void mesh_sphere_subdivide(mesh_t *self, int subdivisions)
{
	int i;
	mesh_lock(self);
	for(i = 0; i < subdivisions; i++)
	{
		mesh_sphere_1_subdivide(self);
	}
	mesh_unlock(self);
}

void mesh_clear(mesh_t *self)
{
	mesh_lock(self);

	self->verts_size = 0;
	self->free_verts = 0;
	self->edges_size = 0;
	self->free_edges = 0;

	mesh_modified(self);
	mesh_unlock(self);
}

int c_mesh_edge_rotate_to_unpaired(mesh_t *self, int edge_id)
{
	edge_t *edge = m_edge(self, edge_id);
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
	int edge_id = mesh_vert_get_half(self, vert);
	edge_id = c_mesh_edge_rotate_to_unpaired(self, edge_id);

	edge_t *edge = m_edge(self, edge_id);
	edge_t *pair = e_pair(edge, self);
	if(!pair) return 0;
	int e = pair->next;

	for(; e != edge_id; e = pair->next)
	{
		edge = m_edge(self, e);
		if(edge->face == face_id) return 1;

		if(!edge) return 0;
		pair = e_pair(edge, self);
		if(!pair) return 0;
	}
	return 0;
}

int mesh_vert_get_half(mesh_t *self, vertex_t *vert)
{
	int i;
	for(i = 0; i < 16; i++)
	{
		int e = vert->halves[i];
		edge_t *edge = m_edge(self, e);
		if(edge) return e;
	}
	return -1;
}

#ifdef MESH4
int mesh_update_unpaired_faces(mesh_t *self)
{
	int i;
	int count = 0;

	for(i = 0; i < self->faces_size; i++)
	{
		face_t *e = m_face(self, i);
		if(e && e->pair == -1)
		{
			count++;
			/* mesh_get_pair_face(self, i); */
		}
	}
	return count;
}
#endif

int mesh_update_unpaired_edges(mesh_t *self)
{
	int i;
	int count = 0;

	for(i = 0; i < self->edges_size; i++)
	{
		edge_t *e = m_edge(self, i);
		if(e && e->pair == -1)
		{
			count++;
			mesh_get_pair_edge(self, i);
		}
	}
	return count;
}

void mesh_update(mesh_t *self)
{
	if(self->update_locked) return;
	if(!self->changes) return;

	int i;
	for(i = 0; i < self->faces_size; i++)
	{
		face_t *f = m_face(self, i);
		if(!f) continue;

		vec3_t n = f_edge(f, 0, self)->n;
		if(vec3_null(n))
		{
			mesh_face_calc_flat_normals(self, f);
		}
		else
		{
			f->n = vec3_unit( vec3_cross(
						vec3_sub(XYZ(f_vert(f, 1, self)->pos),
								 XYZ(f_vert(f, 0, self)->pos)),
						vec3_sub(XYZ(f_vert(f, 2, self)->pos),
								 XYZ(f_vert(f, 0, self)->pos))));
		}
	}
	self->changes = 0;
	self->update_id++;
}

int mesh_add_vert(mesh_t *self, vecN_t pos)
{

	int i = mesh_get_free_vert(self);

	vertex_t *vert = &self->verts[i];
	vert->color = vec4(0.0f);

#ifdef MESH4
	vert->pos = pos; /* 4d meshes dont support transformations yet */
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
	mesh_add_edge_s(self, self->edges_size, self->edges_size - 1);
	mesh_unlock(self);
	return e;
}

int mesh_add_edge_s(mesh_t *self, int v, int prev)
{
	return mesh_add_edge(self, v, -1, prev, vec3(0.0f), vec2(0.0f));
}

static void vert_remove_half(mesh_t *self, vertex_t *vert, int half)
{
	int i;
	for(i = 0; i < 16; i++)
	{
		int *h = &vert->halves[i];
		if(*h == half)
		{
			*h = -1;
			return;
		}
	}
}

static void vert_add_half(mesh_t *self, vertex_t *vert, int half)
{
	int i;
	for(i = 0; i < 16; i++)
	{
		int h = vert->halves[i];
		edge_t *edge = m_edge(self, h);
		if(!edge)
		{
			vert->halves[i] = half;
			return;
		}
#ifdef MESH4
		else
		{
			face_t *f = e_face(edge, self);
			if(f && f->cell != self->current_cell) return;
		}
#endif
	}
	printf("[31mhalves full[0m\n");
}

int mesh_add_edge(mesh_t *self, int v, int next, int prev, vec3_t vn, vec2_t vt)
{
	int i = mesh_get_free_edge(self);

	edge_t *edge = m_edge(self, i);

	edge->v = v;
	edge->n = vn;
	edge->t = vt;
	edge->prev = prev;
	edge->next = next;

	if(prev >= 0) m_edge(self, prev)->next = i;

	if(!mesh_get_pair_edge(self, i))
		vert_add_half(self, m_vert(self, v), i);

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

static int mesh_face_are_pairs(mesh_t *self,
		int f0, int f1, int f2, int v0, int v1, int v2)
{
	if(f0 == v0 && f1 == v2 && f2 == v1 ) return 0;
	if(f1 == v0 && f2 == v2 && f0 == v1 ) return 1;
	if(f2 == v0 && f0 == v2 && f1 == v1 ) return 2;
	return -1;
}

int mesh_get_face_from_verts(mesh_t *self,
		int v0, int v1, int v2)
{
	int i;
	kliter_t(int) *uf;
	for(uf = self->unpaired_faces->head;
			uf != self->unpaired_faces->tail;
			uf = uf->next)
	{
		i = uf->data;
		face_t *try = m_face(self, i); if(!try) continue;
#ifdef MESH4
		if(try->pair != -1) continue;
#endif

		int f0 = f_edge(try, 0, self)->v;
		int f1 = f_edge(try, 1, self)->v;
		int f2 = f_edge(try, 2, self)->v;

		
		if(mesh_face_are_pairs(self, f2, f1, f0, v0, v1, v2) >= 0)
		{
			return i;
		}
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
static int mesh_get_pair_face(mesh_t *self, int f)
{
	face_t *face = &self->faces[f];

	int f0 = self->edges[face->e[0]].v;
	int f1 = self->edges[face->e[1]].v;
	int f2 = self->edges[face->e[2]].v;

	int i;
	kliter_t(int) **uf;
	for(uf = &self->unpaired_faces->head;
			*uf != self->unpaired_faces->tail;
			uf = &((*uf)->next))
	{
		i = (*uf)->data;

		face_t *try = m_face(self, i);
		if(!try) continue;

		int v0 = self->edges[try->e[0]].v;
		int v1 = self->edges[try->e[1]].v;
		int v2 = self->edges[try->e[2]].v;

		if(f0 == v0 && f1 == v2 && f2 == v1 )
		{
			mesh_edge_cpair(self, face->e[0], try->e[2]);
			mesh_edge_cpair(self, face->e[1], try->e[1]);
			mesh_edge_cpair(self, face->e[2], try->e[0]);
		}
		else if(f1 == v0 && f2 == v2 && f0 == v1 )
		{
			mesh_edge_cpair(self, face->e[1], try->e[2]);
			mesh_edge_cpair(self, face->e[2], try->e[1]);
			mesh_edge_cpair(self, face->e[0], try->e[0]);
		}
		else if(f2 == v0 && f0 == v2 && f1 == v1 )
		{
			mesh_edge_cpair(self, face->e[2], try->e[2]);
			mesh_edge_cpair(self, face->e[0], try->e[1]);
			mesh_edge_cpair(self, face->e[1], try->e[0]);
		}
		else continue;

		{
			face->pair = i;
			try->pair = f;

			kl_rem(int, self->unpaired_faces, uf);

			return 1;
		}
	}

	/* if(!kl_get_item(int, self->unpaired_faces, f)) */
	/* { */
		*kl_pushp(int, self->unpaired_faces) = f;
	/* } */

	return -1;
}
#endif

static int mesh_get_cell_pair(mesh_t *self, int e)
{
	int i;
	edge_t *edge = &self->edges[e];
	int v1 = edge->v;
	if(edge->next < 0 || edge->cell_pair >= 0) return -1;
	int v2 = e_next(edge, self)->v;

	kliter_t(int) *se;
	for(se = self->selected_edges->head;
			se != self->selected_edges->tail;
			se = se->next)
	{
		i = se->data;
		if(i == e) continue;

		edge_t *try1 = m_edge(self, i);
		if(!try1 || try1->cell_pair != -1) continue;
		if(try1->selected == 0) continue;

		edge_t *try2 = e_next(try1, self);
		if(!try2) continue;

		if(try1->v == v2 && try2->v == v1)
		{
			edge->cell_pair = i;
			try1->cell_pair = e;
			return 1;
		}
	}
	return -1;
}

static void mesh_update_cell_pairs(mesh_t *self)
{
	int i;
	kliter_t(int) *se;
	for(se = self->selected_edges->head;
			se != self->selected_edges->tail;
			se = se->next)
	{
		i = se->data;
		edge_t *edge = m_edge(self, i);
		if(!edge || edge->selected != 1 || edge->cell_pair != -1) continue;
		mesh_get_cell_pair(self, i);
	}
}

static int mesh_edge_is_pair(mesh_t *self, edge_t *e1, edge_t *e2)
{
	int v1 = e1->v;
	int v2 = e_next(e1, self)->v;

#ifdef MESH4
	face_t *f1 = e_face(e1, self);
	face_t *f2 = e_face(e2, self);
	if(f1 && f2 && f1->cell != f2->cell) return 0;
#endif

	edge_t *try2 = e_next(e2, self);
	if(!try2) return 0;

	return (e2->v == v2 && try2->v == v1);
}

int mesh_get_pair_edge(mesh_t *self, int edge_id)
{
	edge_t *edge = m_edge(self, edge_id); if(!edge) goto end;
	edge_t *next = e_next(edge, self); if(!next) goto end;
	vertex_t *vert = e_vert(next, self); if(!vert) goto end;

	int i;
	for(i = 0; i < 16; i++)
	{
		int e = vert->halves[i];
		edge_t *try = m_edge(self, e); if(!try) continue;
		if(try->pair >= 0) continue;
		if(mesh_edge_is_pair(self, edge, try))
		{
			/* vert->halves[i] = -1; */
			edge->pair = e;
			try->pair = edge_id;
			mesh_modified(self);
			return 1;
		}

	}


end:
	return 0;
}

#ifdef MESH4
static void mesh_cells_grow(mesh_t *self)
{
	if(self->cells_alloc < self->cells_size)
	{
		self->cells_alloc = self->cells_size + CHUNK_CELLS;
		self->cells = realloc(self->cells, self->cells_alloc *
				sizeof(*self->cells));
	}
}
#endif

static void mesh_edges_grow(mesh_t *self)
{
	if(self->edges_alloc < self->edges_size)
	{
		self->edges_alloc = self->edges_size + CHUNK_EDGES;
		self->edges = realloc(self->edges, self->edges_alloc *
				sizeof(*self->edges));
	}
}

static void mesh_verts_grow(mesh_t *self)
{
	if(self->verts_alloc < self->verts_size)
	{
		self->verts_alloc = self->verts_size + CHUNK_VERTS;
		self->verts = realloc(self->verts, self->verts_alloc *
				sizeof(*self->verts));
	}
}

static void mesh_faces_grow(mesh_t *self)
{
	if(self->faces_alloc < self->faces_size)
	{
		self->faces_alloc = self->faces_size + CHUNK_FACES;
		self->faces = realloc(self->faces, self->faces_alloc * sizeof(*self->faces));
	}
}

void mesh_verts_prealloc(mesh_t *self, int size)
{
	self->verts_alloc += size;
	self->verts = realloc(self->verts, self->verts_alloc * sizeof(*self->verts));

	/* draw_mesh_vert_prealloc(&self->d, size); */
}

#ifdef MESH4
void mesh_cells_prealloc(mesh_t *self, int size)
{
	self->cells_alloc += size;
	self->cells = realloc(self->cells, self->cells_alloc * sizeof(*self->cells));

	/* draw_mesh_ind_prealloc(&self->d, 3 * size); */
}
#endif

void mesh_faces_prealloc(mesh_t *self, int size)
{
	self->faces_alloc += size;
	self->faces = realloc(self->faces, self->faces_alloc * sizeof(*self->faces));
}

void mesh_edges_prealloc(mesh_t *self, int size)
{
	self->edges_alloc += size;
	self->edges = realloc(self->edges, self->edges_alloc * sizeof(*self->edges));
}

void mesh_add_quad(mesh_t *self,
		int v1, vec3_t v1n, vec2_t v1t,
		int v2, vec3_t v2n, vec2_t v2t,
		int v3, vec3_t v3n, vec2_t v3t,
		int v4, vec3_t v4n, vec2_t v4t)
{

	mesh_lock(self);

	self->triangulated = 0;

	int face_id = mesh_get_free_face(self);
	face_t *face = m_face(self, face_id);

#ifdef MESH4
	face->surface = self->current_surface;
	face->cell = self->current_cell;
#endif
	face->e_size = 4;
	
	int ie0 = mesh_get_free_edge(self);
	int ie1 = mesh_get_free_edge(self);
	int ie2 = mesh_get_free_edge(self);
	int ie3 = mesh_get_free_edge(self);

	v1n = mat4_mul_vec4(self->transformation, vec4(_vec3(v1n), 0.0)).xyz;
	v2n = mat4_mul_vec4(self->transformation, vec4(_vec3(v2n), 0.0)).xyz;
	v3n = mat4_mul_vec4(self->transformation, vec4(_vec3(v3n), 0.0)).xyz;
	v4n = mat4_mul_vec4(self->transformation, vec4(_vec3(v4n), 0.0)).xyz;

	face->e[0] = ie0;
	face->e[1] = ie1;
	face->e[2] = ie2;
	face->e[3] = ie3;


	self->edges[ie0] = (edge_t){ .v = v1, .n = v1n, .t = v1t, .face = face_id,
		.next = ie1, .prev = ie3, .pair = -1, .cell_pair = -1};
	self->edges[ie1] = (edge_t){ .v = v2, .n = v2n, .t = v2t, .face = face_id,
		.next = ie2, .prev = ie0, .pair = -1, .cell_pair = -1};
	self->edges[ie2] = (edge_t){ .v = v3, .n = v3n, .t = v3t, .face = face_id,
		.next = ie3, .prev = ie1, .pair = -1, .cell_pair = -1};
	self->edges[ie3] = (edge_t){ .v = v4, .n = v4n, .t = v4t, .face = face_id,
		.next = ie0, .prev = ie2, .pair = -1, .cell_pair = -1};

	if(!mesh_get_pair_edge(self, ie0))
		vert_add_half(self, m_vert(self, v1), ie0);
	if(!mesh_get_pair_edge(self, ie1))
		vert_add_half(self, m_vert(self, v2), ie1);
	if(!mesh_get_pair_edge(self, ie2))
		vert_add_half(self, m_vert(self, v3), ie2);
	if(!mesh_get_pair_edge(self, ie3))
		vert_add_half(self, m_vert(self, v4), ie3);


#ifdef MESH4
	mesh_get_pair_face(self, face_id);
#endif

	mesh_modified(self);
	mesh_unlock(self);
}

void mesh_remove_vert(mesh_t *self, int vert_i)
{
	vertex_t *vert = m_vert(self, vert_i);
	if(!vert) return;

	vert->removed = 1;
	self->free_verts++;
	mesh_modified(self);
}

void mesh_remove_edge(mesh_t *self, int edge_i)
{
	edge_t *edge = m_edge(self, edge_i);
	if(!edge) return;

	if(edge->selected)
	{
		self->selected_edges->modified = 1;
	}
	edge_t *pair = e_pair(edge, self);
	if(pair)
	{
		pair->pair = -1;
	}
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
		prev->next = -1;
	}

	vertex_t *vert = m_vert(self, edge->v);
	vert_remove_half(self, vert, edge_i);

	if(edge_i == self->first_edge) self->first_edge++;

	edge->removed = 1;
	self->free_edges++;
	mesh_modified(self);
}

void mesh_remove_face(mesh_t *self, int face_i)
{
	int i;

	face_t *face = m_face(self, face_i);
	if(!face) return;

	for(i = 0; i < face->e_size; i++)
	{
		mesh_remove_edge(self, face->e[i]);
	}
#ifdef MESH4
	if(face->selected)
	{
		self->selected_faces->modified = 1;
	}
	if(face->pair == -1)
	{
		kl_rem_item(int, self->unpaired_faces, face_i);
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
	face->removed = 1;
	self->free_faces++;
	mesh_modified(self);
}

int mesh_add_triangle_s(mesh_t *self, int v1, int v2, int v3,
		int pair_up)
{
	return mesh_add_triangle(self, v1, Z3, Z2, v2, Z3, Z2, v3, Z3, Z2,
			pair_up);
}

int mesh_add_triangle(mesh_t *self,
		int v1, vec3_t v1n, vec2_t v1t,
		int v2, vec3_t v2n, vec2_t v2t,
		int v3, vec3_t v3n, vec2_t v3t, int pair_up)
{
	int face_id = mesh_get_free_face(self);
	face_t *face = &self->faces[face_id];
#ifdef MESH4
	face->cell = self->current_cell;
	face->surface = self->current_surface;
#endif

	face->e_size = 3;

	int ie0 = mesh_get_free_edge(self);
	int ie1 = mesh_get_free_edge(self);
	int ie2 = mesh_get_free_edge(self);

	v1n = mat4_mul_vec4(self->transformation, vec4(_vec3(v1n), 0.0)).xyz;
	v2n = mat4_mul_vec4(self->transformation, vec4(_vec3(v2n), 0.0)).xyz;
	v3n = mat4_mul_vec4(self->transformation, vec4(_vec3(v3n), 0.0)).xyz;

	face->e[0] = ie0;
	face->e[1] = ie1;
	face->e[2] = ie2;

	self->edges[ie0] = (edge_t){ .v = v1, .n = v1n, .t = v1t, .face = face_id,
		.next = ie1, .prev = ie2, .pair = -1, .cell_pair = -1};
	self->edges[ie1] = (edge_t){ .v = v2, .n = v2n, .t = v2t, .face = face_id,
		.next = ie2, .prev = ie0, .pair = -1, .cell_pair = -1};
	self->edges[ie2] = (edge_t){ .v = v3, .n = v3n, .t = v3t, .face = face_id,
		.next = ie0, .prev = ie1, .pair = -1, .cell_pair = -1};

	/* if(vec3_null(v1n) || vec3_null(v2n) || vec3_null(v3n)) */
	/* { */
	/* 	mesh_face_calc_flat_normals(self, face); */

	/* 	if(vec3_null(v1n)) self->edges[ie0].n = face->n; */
	/* 	if(vec3_null(v2n)) self->edges[ie1].n = face->n; */
	/* 	if(vec3_null(v3n)) self->edges[ie2].n = face->n; */
	/* } */

	if(pair_up)
	{
		if(!mesh_get_pair_edge(self, ie0))
			vert_add_half(self, m_vert(self, v1), ie0);
		if(!mesh_get_pair_edge(self, ie1))
			vert_add_half(self, m_vert(self, v2), ie1);
		if(!mesh_get_pair_edge(self, ie2))
			vert_add_half(self, m_vert(self, v3), ie2);
	}



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

void mesh_translate(mesh_t *self, float x, float y, float z)
{
	self->transformation = mat4_translate_in_place(self->transformation, x, y, z);
}

void mesh_rotate(mesh_t *self, float angle, int x, int y, int z)
{
	mat4_t new = mat4_rotate(self->transformation, x, y, z, to_radians(angle));
	self->transformation = new;
}

void mesh_restore(mesh_t *self)
{
	self->transformation = self->backup;
}

void mesh_save(mesh_t *self)
{
	self->backup = self->transformation;
}

void mesh_translate_uv(mesh_t *self, vec2_t p)
{
	int i;
	mesh_lock(self);
	for(i = 0; i < self->edges_size; i++)
	{
		self->edges[i].t = vec2_add(self->edges[i].t, p);
	}
	mesh_modified(self);
	mesh_unlock(self);
}

void mesh_scale_uv(mesh_t *self, float scale)
{
	int i;
	mesh_lock(self);
	for(i = 0; i < self->edges_size; i++)
	{
		self->edges[i].t = vec2_scale(self->edges[i].t, scale);
	}
	mesh_modified(self);
	mesh_unlock(self);
}

mesh_t *mesh_quad()
{
	mesh_t *self = mesh_new();

	vec3_t n = vec3(0,-1,0);
	mesh_add_regular_quad(self,
			VEC3(-1.0, -1.0, 0.0), n, vec2(0, 0),
			VEC3( 1.0, -1.0, 0.0), n, vec2(1, 0),
			VEC3( 1.0,  1.0, 0.0), n, vec2(1, 1),
			VEC3(-1.0,  1.0, 0.0), n, vec2(0, 1));

	mesh_update(self);
	return self;
}

struct int_int {int a, b;};
struct int_int_int {int a, b;};

struct int_int mesh_face_triangulate(mesh_t *self, int i, int flip)
{
	face_t *face = &self->faces[i];

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

	mesh_remove_face(self, i);

	if(flip) return (struct int_int)
	{
		mesh_add_triangle(self, i1, Z3, t1,
								i2, Z3, t2,
								i3, Z3, t3, 1),

		mesh_add_triangle(self, i3, Z3, t3,
								i0, Z3, t0,
								i1, Z3, t1, 1)
	};
	else return (struct int_int)
	{
		mesh_add_triangle(self, i0, Z3, t0,
								i1, Z3, t1,
								i2, Z3, t2, 1),

		mesh_add_triangle(self, i2, Z3, t2,
								i3, Z3, t3,
								i0, Z3, t0, 1)
	};
}

void mesh_triangulate(mesh_t *self)
{
	int i;
	if(self->triangulated) return;
	for(i = 0; i < self->faces_size; i++)
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
	self->triangulated = 1;
}

mesh_t *mesh_circle(float radius, int segments)
{
	mesh_t *self = mesh_new();

	int prev_e, first_e;

	prev_e = first_e = mesh_add_edge_s(self,
			mesh_add_vert(self, VEC3(sin(0) * radius, 0.0, cos(0) * radius)), -1);
	mesh_edge_set_selection(self, first_e, 1);

	float inc = (M_PI * 2) / segments;
	float a;

	int ai;
	for(ai = 1, a = inc; ai < segments; a += inc, ai++)
	{
		int e = mesh_add_edge_s(self,
				mesh_add_vert(self, VEC3(
						sin(-a) * radius,
						0.0,
						cos(-a) * radius)), prev_e);
		mesh_edge_set_selection(self, e, 1);
		prev_e = e;
	}
	self->has_texcoords = 0;

	if(prev_e != first_e) m_edge(self, prev_e)->next = first_e;

	mesh_update(self);

	return self;
}

vecN_t mesh_get_selection_center(mesh_t *self)
{
	int i;
	vecN_t center = vecN(0.0f);
	int count = 0;
	kliter_t(int) *se;
	for(se = self->selected_edges->head;
			se != self->selected_edges->tail;
			se = se->next)
	{
		i = se->data;
		edge_t *e = m_edge(self, i);
		if(!e || e->selected != 1) continue;
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

	int cell_id = mesh_get_free_cell(self);
	cell_t *cell = m_cell(self, cell_id);

	self->current_cell = cell_id;

	cell->f[0] = mesh_add_triangle_s(self, v0, v1, v2, 0);
	cell->f[1] = mesh_add_triangle_s(self, v0, v2, v3, 0);
	cell->f[2] = mesh_add_triangle_s(self, v0, v3, v1, 0);
	cell->f[3] = mesh_add_triangle_s(self, v1, v3, v2, 0);

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
	for(e = 0; e < self->edges_size; e++)
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

int mesh_add_tetrahedral_prism(mesh_t *self,
		face_t *f, int v0, int v1, int v2)
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

	edge_t *e0 = f_edge(f, 0, self),
		   *e1 = f_edge(f, 1, self),
		   *e2 = f_edge(f, 2, self);

	int verts[6] = { v0, v1, v2, e0->v, e1->v, e2->v };

	for(int i = 0; i < 5; i++) for(int j = i+1; j < 6; j++)
		if(verts[i] == verts[j])
		{
			printf("EQUALS %d %d %d %d %d %d\n", v0, v1, v2, verts[3],
					verts[4], verts[5]);
			exit(1);
		}

	int i = (e0->extrude_flip << 2)
		  | (e1->extrude_flip << 1)
		  | (e2->extrude_flip << 0);
	#define config table[i]
	if(i == 0 || i == 7) printf("Invalid configuration\n");

	int t;
	int next = -1;
	for(t = 0; t < 3; t++)
	{
		int c0 = config[t][0];
		int c1 = config[t][1];
		int c2 = config[t][2];
		int c3 = config[t][3];
		int tet = mesh_add_tetrahedron(self, verts[c0], verts[c1],
				verts[c2], verts[c3]);

		if(next == -1) next = tet;
		/* printf("f1: %d %d %d\n", config[t][1], config[t][2], config[t][0]); */
		/* printf("f2: %d %d %d\n", config[t][2], config[t][3], config[t][0]); */
		/* printf("f3: %d %d %d\n", config[t][3], config[t][1], config[t][0]); */
		/* printf("f4: %d %d %d\n", config[t][3], config[t][2], config[t][1]); */
	}

	/* return mesh_get_face_from_verts(self, v0, v1, v2); */
	return m_cell(self, next)->f[0];
}
#endif

int mesh_check_duplicate_verts(mesh_t *self, int edge_id)
{
	int j;
	vecN_t pos = m_vert(self, edge_id)->pos;
	for(j = 0; j < self->verts_size; j++)
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

void mesh_vert_dup_and_modify(mesh_t *self, vecN_t pivot,
		float factor, vecN_t offset)
{
	int i;

	kliter_t(int) *se;
	/* for(se = self->selected_edges->head; */
	/* 		se != self->selected_edges->tail; */
	/* 		se = se->next) */
	/* { */
	/* 	i = se->data; */
	/* 	edge_t *e = m_edge(self, i); */
	/* 	if(!e || e->selected != 1) continue; */
	/* 	e_vert(e, self)->tmp = -1; */
	/* } */

	for(se = self->selected_edges->head;
			se != self->selected_edges->tail;
			se = se->next)
	{
		i = se->data;
		edge_t *e = m_edge(self, i);
		if(!e || e->selected != 1) continue;
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
		float scale, modifier_cb modifier)
{
	if(!self->triangulated)
	{
		printf("Can't extrude untriangulated mesh.\n");
		return;
	}

	int i, step;

	mesh_lock(self);

	self->has_texcoords = 0;

	float percent = 0.0f;
	float percent_inc = 1.0f / steps;
	vecN_t inc = vecN_(scale)(offset, percent_inc);
	float prev_factor = 1.0f;
	/* vecN_t center = mesh_get_selection_center(self); */
	for(step = 0; step < steps; step++)
	{
		self->current_surface++;
		printf("step %d %d %d\n", step, self->selected_faces->size,
				self->selected_edges->size);
		if(!mesh_update_flips(self))
		{
			printf("Extrude tetrahedral not possible in this mesh.\n");
			break;
		}

		vecN_t center = mesh_get_selection_center(self);

		percent += percent_inc;
		float current_factor = modifier ? modifier(self, percent) : 1.0f;
		current_factor *= scale;
		float factor = current_factor / prev_factor;

		mesh_vert_dup_and_modify(self, center, factor, inc);

		prev_factor = current_factor;

		/* EXTRUDE FACES */

		/* klist(int) *next_faces = kl_init(int); */
		/* TODO */

		kliter_t(int) *uf;
		for(uf = self->selected_faces->head;
				uf != self->selected_faces->tail;
				uf = uf->next)
		{
			i = uf->data;
			face_t *f = m_face(self, i); if(!f) continue;
			if(f->selected != 1) continue;

			int v0 = f_vert(f, 0, self)->tmp;
			int v1 = f_vert(f, 1, self)->tmp;
			int v2 = f_vert(f, 2, self)->tmp;

			int new_exposed_face =
				mesh_add_tetrahedral_prism(self, f, v0, v1, v2);

			mesh_face_set_selection(self, i, 0);

			mesh_face_set_selection(self, new_exposed_face, 2);

			/* *kl_pushp(int, next_faces) = new_exposed_face; */
		}
		mesh_udpate_selection_list(self);

		for(uf = self->selected_faces->head;
				uf != self->selected_faces->tail;
				uf = uf->next)
		{
			i = uf->data;
			face_t *face = m_face(self, i); if(!face) continue;
			if(face->selected == 2) mesh_face_set_selection(self, i, 1);
		}

	}

	printf("end of extrusion\n");
	mesh_unlock(self);
}
#endif

void mesh_extrude_edges(mesh_t *self, int steps, vecN_t offset,
		float scale, modifier_cb modifier)
{
	int i, step;

	mesh_lock(self);


	float percent = 0.0f;
	float percent_inc = 1.0f / steps;
	vecN_t inc = vecN_(scale)(offset, percent_inc);
	float prev_factor = 1.0f;

	for(step = 0; step < steps; step++)
	{
		vecN_t center = mesh_get_selection_center(self);

		percent += percent_inc;
		float current_factor = modifier ? modifier(self, percent) : 1.0f;
		current_factor *= scale;
		float factor = current_factor / prev_factor;

		mesh_vert_dup_and_modify(self, center, factor, inc);

		prev_factor = current_factor;

		kliter_t(int) *se;
		for(se = self->selected_edges->head;
				se != self->selected_edges->tail;
				se = se->next)
		{
			i = se->data;
			edge_t *e = m_edge(self, i);
			if(!e || e->selected != 1) continue;

			edge_t *ne = e_next(e, self); if(!ne) continue;

			int et = e_vert(e, self)->tmp;
			int nt = e_vert(ne, self)->tmp;
			if(et == -1 || nt == -1)
			{
				continue;
			}

			mesh_add_quad(self,
					ne->v, Z3, ne->t,
					e->v, Z3, e->t,
					et, Z3, e->t,
					nt, Z3, ne->t
			);
			mesh_edge_set_selection(self, i, 0);

			e = m_edge(self, i);
			int new = e_prev(e_pair(e, self), self)->prev;
			if(new == -1) exit(1);
			mesh_edge_set_selection(self, new, 2);

		}
		mesh_udpate_selection_list(self);
		for(se = self->selected_edges->head;
				se != self->selected_edges->tail;
				se = se->next)
		{
			i = se->data;
			edge_t *edge = m_edge(self, i); if(!edge) continue;
			if(edge->selected == 2)
			{
				mesh_edge_set_selection(self, i, 1);
			}
		}
	}

	mesh_unlock(self);
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
					0.0,	 cos(0) * inner_radius)), -1);

	int ai;
	for(ai = 1, a = inner_inc; ai < inner_segments; a += inner_inc, ai++)
	{
		int e = mesh_add_edge_s(tmp, mesh_add_vert(tmp, VEC3(
					radius + sin(a) * inner_radius,
					0.0,	 cos(a) * inner_radius)), prev_e);
		prev_e = e;
	}

	if(prev_e != first_e) m_edge(tmp, prev_e)->next = first_e;

	mesh_t *self = mesh_lathe(tmp, M_PI * 2, segments, 0, 0, 1);

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

	float inc = angle / segments;

	int ai;

	int verts[mesh->edges_size * segments];
 

	for(ai = 0, a = inc; ai < segments; a += inc, ai++)
	{
		rot = mat4_rotate(rot, x, y, z, inc);
		for(ei = 0; ei < mesh->edges_size; ei++)
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
		for(ei = 0; ei < mesh->edges_size; ei++)
		{
			edge_t *e = m_edge(mesh, ei);
			if(!e) continue;

			edge_t *ne = m_edge(mesh, e->next);
			if(!ne) continue;

			int next_ei = ((ei + 1) == mesh->edges_size) ? 0 : ei + 1;
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
	mesh_update(self);

	return self;
}

mesh_t *mesh_cuboid(float tex_scale, vec3_t p1, vec3_t p2)
		/* float x1, float y1, float z1, */
		/* float x2, float y2, float z2) */
{
	vec3_t s = vec3_scale(vec3_sub(p2, p1), 4);
	mesh_t *self = mesh_new();

	vec3_t n = vec3(0,-1,0);
	mesh_add_regular_quad(self,
			VEC3(_vec3(p1)),      n, vec2(0,  0),
			VEC3(p2.x,p1.y,p1.z), n, vec2(s.x, 0),
			VEC3(p2.x,p1.y,p2.z), n, vec2(s.x, s.z),
			VEC3(p1.x,p1.y,p2.z), n, vec2(0,  s.z));

	n = vec3(0,1,0);
	mesh_add_regular_quad(self,
			VEC3(p1.x,p2.y,p2.z), n, vec2( 0,s.z),
			VEC3(_vec3(p2)),	  n, vec2(s.x,s.z),
			VEC3(p2.x,p2.y,p1.z), n, vec2(s.x, 0),
			VEC3(p1.x,p2.y,p1.z), n, vec2( 0, 0));

	n = vec3(0.0, 0.0, -1.0);
	mesh_add_regular_quad(self,
			VEC3(p1.x,p2.y,p1.z), n, vec2(s.x,0),
			VEC3(p2.x,p2.y,p1.z), n, vec2(0,0),
			VEC3(p2.x,p1.y,p1.z), n, vec2(0, s.y),
			VEC3(_vec3(p1)),	  n, vec2(s.x, s.y));

	n = vec3(0.0, 0.0, 1.0);
	mesh_add_regular_quad(self,
			VEC3(p1.x,p1.y,p2.z), n, vec2(s.x, s.y),
			VEC3(p2.x,p1.y,p2.z), n, vec2(0, s.y),
			VEC3(_vec3(p2)),	  n, vec2(0,0),
			VEC3(p1.x,p2.y,p2.z), n, vec2(s.x,0));

	n = vec3(-1.0, 0.0, 0.0);
	mesh_add_regular_quad(self,
			VEC3(p1.x,p1.y,p2.z), n, vec2( 0,s.z),
			VEC3(p1.x,p2.y,p2.z), n, vec2(s.y,s.z),
			VEC3(p1.x,p2.y,p1.z), n, vec2(s.y, 0),
			VEC3(_vec3(p1)),	  n, vec2( 0, 0));

	n = vec3(1.0, 0.0, 0.0);
	mesh_add_regular_quad(self,
			VEC3(p2.x,p1.y,p1.z), n, vec2( 0, 0),
			VEC3(p2.x,p2.y,p1.z), n, vec2(s.y, 0),
			VEC3(_vec3(p2)),	  n, vec2(s.y,s.z),
			VEC3(p2.x,p1.y,p2.z), n, vec2( 0,s.z));

	/* mesh_translate_uv(self, 0.5, 0.5); */
	mesh_scale_uv(self, tex_scale);

	mesh_update(self);

	return self;
}


mesh_t *mesh_cube(float size, float tex_scale, int inverted_normals)
{
	size /= 2;
	return mesh_cuboid(tex_scale, vec3(-size, -size, -size),
			vec3(-size, -size, -size));
}

mesh_t *mesh_from_file(const char *filename)
{
	mesh_t *self = NULL;
	char ext[16];

	strncpy(ext, strrchr(filename, '.') + 1, sizeof(ext));
	if(!strncmp(ext, "ply", sizeof(ext)))
	{
		self = mesh_from_ply(filename);
	}
	else if(!strncmp(ext, "obj", sizeof(ext)))
	{
		self = mesh_from_obj(filename);
	}

	if(self)
	{
		mesh_update(self);
	}

	return self;
}

vertex_t *mesh_farthest(mesh_t *self, const vec3_t dir)
{
	/* NEEDS OPTIMIZATION */
	vertex_t *v = &self->verts[0];
	float last_dist = vec3_dot(dir, XYZ(v->pos));

	int i;
	for(i = 1; i < self->verts_size; i++)
	{
		vertex_t *vi = &self->verts[i];
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

