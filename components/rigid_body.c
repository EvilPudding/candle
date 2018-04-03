#include "rigid_body.h"
#include "model.h"
#include "aabb.h"
#include "spacial.h"
#include <float.h>

int gjk_intersects(c_rigid_body_t *self, c_rigid_body_t *other,
		contact_t *contact);

const float REL_ERROR = (float)(1.0e-3);


c_rigid_body_t *c_rigid_body_new(collider_cb costum)
{
	c_rigid_body_t *self = component_new("rigid_body");

	self->costum = costum;

	return self;
}

int c_rigid_body_narrow(c_rigid_body_t *self, c_rigid_body_t *other,
		contact_t *contact)
{
	c_model_t *mc1 = c_model(self);
	c_model_t *mc2 = c_model(other);
	if(mc1 && mc2)
	{
		return gjk_intersects(self, other, contact);
	}
	return 0;
}

int c_rigid_body_intersects(c_rigid_body_t *self, c_rigid_body_t *other,
		contact_t *contact)
{
	c_aabb_t *aabb1 = c_aabb(self);
	c_aabb_t *aabb2 = c_aabb(other);
	if(aabb1 && aabb2)
	{
		if(!c_aabb_intersects(aabb1, aabb2)) return 0;
		/* if(c_aabb_intersects(aabb1, aabb2)) */
	}
	return c_rigid_body_narrow(self, other, contact);
}

REG()
{
	ct_new("rigid_body", sizeof(c_rigid_body_t), NULL, NULL,
			1, ref("spacial"));
}

/* GJK */

struct simplex
{
	int ns;
	vec3_t a, b, c, d;
	vec3_t points[4];

	/// pointsLengthSquare[i] = (points[i].length)^2
	float points_length_square[4];

	/// Maximum length of pointsLengthSquare[i]
	float max_length_square;

	/// Support points of object A in local coordinates
	vec3_t supp_pointsA[4];

	/// Support points of object B in local coordinates
	vec3_t supp_pointsB[4];

	/// diff[i][j] contains points[i] - points[j]
	vec3_t diff_length[4][4];

	/// Cached determinant values
	float det[16][4];

	/// norm[i][j] = (diff[i][j].length())^2
	float norm_square[4][4];

	/// 4 bits that identify the current points of the simplex
	/// For instance, 0101 means that points[1] and points[3] are in the simplex
	int bits_current_simplex;

	/// Number between 1 and 4 that identify the last found support point
	int last_found;

	/// Position of the last found support point (lastFoundBit = 0x1 << lastFound)
	int last_found_bit;

	/// allBits = bitsCurrentSimplex | lastFoundBit;
	int all_bits;
};

int gjk_check_tetra(struct simplex *simp, vec3_t ao, vec3_t ab, vec3_t ac, vec3_t abc,
		vec3_t *dir)
{
	vec3_t ab_abc = vec3_cross(ab, abc);

	if (vec3_dot(ab_abc, ao) > 0)
	{
		simp->c = simp->b;
		simp->b = simp->a;

		*dir = vec3_double_cross(ab, ao);

		simp->ns = 2;

		return 0;
	}

	vec3_t acp = vec3_cross(abc, ac);

	if (vec3_dot(acp, ao) > 0)
	{
		simp->b = simp->a;

		//dir is not abc_ac because it's not point towards the origin;
		//ACxA0xAC direction we are looking for
		*dir = vec3_double_cross(ac, ao);

		//build new triangle
		// d will be lost
		simp->ns = 2;

		return 0;
	}

	//build new tetrahedron with new base
	simp->d = simp->c;
	simp->c = simp->b;
	simp->b = simp->a;

	*dir = abc;

	simp->ns = 3;

	return 1;
}

int gjk_tetrahedron(struct simplex *simp, vec3_t *dir)
{
	vec3_t ao = vec3_inv(simp->a);
	vec3_t ab = vec3_sub(simp->b, simp->a);
	vec3_t ac = vec3_sub(simp->c, simp->a);

	vec3_t abc = vec3_cross(ab, ac);

	//CASE 1
	if (vec3_dot(abc, ao) > 0)
	{
		//in front of triangle ABC
		//we don't have to change the ao,ab,ac,abc meanings
		gjk_check_tetra(simp, ao,ab,ac,abc,dir);
	}


	//CASE 2:

	vec3_t ad = vec3_sub(simp->d, simp->a);

	//build acd triangle
	vec3_t acd = vec3_cross(ac, ad);

	//same direaction with ao
	if (vec3_dot(acd, ao) > 0)
	{

		//in front of triangle ACD
		simp->b = simp->c;
		simp->c = simp->d;
		ab = ac;
		ac = ad;
		abc = acd;

		gjk_check_tetra(simp, ao, ab, ac, abc, dir);
	}

	//build adb triangle
	vec3_t adb = vec3_cross(ad, ab);

	//same direaction with ao
	if(vec3_dot(adb, ao) > 0)
	{

		//in front of triangle ADB

		simp->c = simp->b;
		simp->b = simp->d;

		ac = ab;
		ab = ad;

		abc = adb;
		gjk_check_tetra(simp, ao, ab, ac, abc, dir);
	}

	//origin in tetrahedron
	return 1;

}

int gjk_line(struct simplex *simp, vec3_t *dir)
{
	vec3_t ab = vec3_sub(simp->b, simp->a);
	vec3_t ao = vec3_inv(simp->a);

	//can t be behind b;

	//new direction towards a0
	*dir = vec3_double_cross(ab, ao);

	simp->c = simp->b;
	simp->b = simp->a;
	simp->ns = 2;

	return 0;
}


int gjk_triangle(struct simplex *simp, vec3_t *dir)
{
	vec3_t ao = vec3_inv(simp->a);
	vec3_t ab = vec3_sub(simp->b, simp->a);
	vec3_t ac = vec3_sub(simp->c, simp->a);
	vec3_t abc = vec3_cross(ab, ac);

	//point is can't be behind/in the direction of B,C or BC


	vec3_t ab_abc = vec3_cross(ab, abc);
	// is the origin away from ab edge? in the same plane
	//if a0 is in that direction than
	if (vec3_dot(ab_abc, ao) > 0)
	{
		//change points
		simp->c = simp->b;
		simp->b = simp->a;

		//dir is not ab_abc because it's not point towards the origin
		*dir = vec3_double_cross(ab,ao);

		//direction change; can't build tetrahedron
		return 0;
	}


	vec3_t abc_ac = vec3_cross(abc, ac); 

	// is the origin away from ac edge? or it is in abc?
	//if a0 is in that direction than
	if (vec3_dot(abc_ac, ao) > 0)
	{
		//keep c the same
		simp->b = simp->a;

		//dir is not abc_ac because it's not point towards the origin
		*dir = vec3_double_cross(ac, ao);

		//direction change; can't build tetrahedron
		return 0;
	}

	//now can build tetrahedron; check if it's above or below
	if (vec3_dot(abc, ao) > 0)
	{
		//base of tetrahedron
		simp->d = simp->c;
		simp->c = simp->b;
		simp->b = simp->a;

		//new direction
		*dir = abc;
	}
	else
	{
		//upside down tetrahedron
		simp->d = simp->b;
		simp->b = simp->a;
		*dir = vec3_inv(abc);
	}

	simp->ns = 3;

	return 0;
}

int gjk_contains_origin(struct simplex *simp, vec3_t *dir)
{
	/* if(simp->ns == 1) */
	/* { */
	/* 	return gjk_line(simp, dir); */
	/* } */

	if(simp->ns == 2)
	{
		return gjk_triangle(simp, dir);
	}
	else if(simp->ns == 3)
	{
		return gjk_tetrahedron(simp, dir);
	}

	return 0;
}

static inline vec3_t gjk_support(mesh_t *self, mesh_t *other,
		mat4_t other_to_self, mat4_t rotate_to_other, const vec3_t dir)
{
	vec3_t dir2 = mat4_mul_vec4(rotate_to_other, vec4(_vec3(dir), 0.0)).xyz;

	// Compute the support points for original objects (without margins) A and B
	vec3_t supportA = self->support(self, vec3_inv(dir));
	vec3_t supportB = other->support(other, dir2);

	supportB = mat4_mul_vec4(other_to_self, vec4(_vec3(supportB), 1.0)).xyz;

	// Compute the support point for the Minkowski difference A-B
	return vec3_sub(supportA, supportB);
}

int gjk_intersects_bak(c_rigid_body_t *self, c_rigid_body_t *other)
{
	mesh_t *mesh1 = c_model(self)->mesh;
	mesh_t *mesh2 = c_model(other)->mesh;

	c_spacial_t *sc1 = c_spacial(self);
	c_spacial_t *sc2 = c_spacial(other);

	mat4_t other_to_self;
	mat4_t rotate_to_other;

	other_to_self = mat4_invert(sc1->model_matrix);
	other_to_self = mat4_mul(other_to_self, sc2->model_matrix);

	rotate_to_other = mat4_transpose(sc2->rot_matrix);
	rotate_to_other = mat4_mul(rotate_to_other, sc1->rot_matrix);

	vec3_t dir = vec3_sub(sc2->pos, sc1->pos);
	if(vec3_null(dir)) dir = vec3(1.0);

	struct simplex simp;

	simp.c = gjk_support(mesh1, mesh2, other_to_self, rotate_to_other, dir);

	dir = vec3_inv(simp.c);

	simp.b = gjk_support(mesh1, mesh2, other_to_self, rotate_to_other, dir);

	if(vec3_dot(simp.b, dir) < 0) return 0;

	dir = vec3_double_cross(vec3_sub(simp.c, simp.b), vec3_inv(simp.b));
	
	simp.ns = 2;

	int steps;
	for(steps = 0; steps < 1000; steps++) /* avoid infinite loop */
	{
		/* vec3_print(dir); */
		simp.a = gjk_support(mesh1, mesh2, other_to_self, rotate_to_other, dir);

		if(vec3_dot(simp.a, dir) <= 0) return 0;

		if(gjk_contains_origin(&simp, &dir)) return 1;
	}
	return 0;
}

static inline int overlap(int a, int b)
{
    return (a & b) != 0x0;
}

int simplex_has_point(struct simplex *self, const vec3_t point)
{
    int i;
    int bit;

    // For each four possible points in the simplex
    for (i=0, bit = 0x1; i<4; i++, bit <<= 1)
	{
        // Check if the current point is in the simplex
		if(overlap(self->all_bits, bit) && vec3_equals(point, self->points[i]))
		{
			return 1;
		}
    }

    return 0;
}

void simplex_closest_of_A_B(struct simplex *self, vec3_t *pA, vec3_t *pB)
{
    float deltaX = 0.0;
    *pA = vec3(0.0);
    *pB = vec3(0.0);
    int i;
    int bit;

    // For each four points in the possible simplex set
    for (i=0, bit=0x1; i<4; i++, bit <<= 1) {
        // If the current point is part of the simplex
        if (overlap(self->bits_current_simplex, bit)) {
            deltaX += self->det[self->bits_current_simplex][i];
			*pA = vec3_add(*pA, vec3_mul_number(self->supp_pointsA[i],
					self->det[self->bits_current_simplex][i]));
			*pB = vec3_add(*pB, vec3_mul_number(self->supp_pointsB[i],
				self->det[self->bits_current_simplex][i]));
        }
    }

    /* assert(deltaX > 0.0); */
    float factor = 1.0f / deltaX;
    *pA = vec3_mul_number(*pA, factor);
    *pB = vec3_mul_number(*pB, factor);
}

void simplex_update_cache(struct simplex *self)
{
    int i;
    int bit;

    // For each of the four possible points of the simplex
    for (i=0, bit = 0x1; i<4; i++, bit <<= 1) {
        // If the current points is in the simplex
        if (overlap(self->bits_current_simplex, bit)) {
            
            // Compute the distance between two points in the possible simplex set
            self->diff_length[i][self->last_found] = vec3_sub(self->points[i], self->points[self->last_found]);
            self->diff_length[self->last_found][i] = vec3_inv(self->diff_length[i][self->last_found]);

            // Compute the squared length of the vector
            // distances from points in the possible simplex set
			self->norm_square[i][self->last_found] =
				self->norm_square[self->last_found][i] =
				vec3_dot(self->diff_length[i][self->last_found],
						self->diff_length[i][self->last_found]);
        }
    }
}

static inline int simplex_is_empty(struct simplex *self)
{
    return (self->bits_current_simplex == 0x0);
}

void simplex_compute_determinants(struct simplex *self)
{
    self->det[self->last_found_bit][self->last_found] = 1.0;

    // If the current simplex is not empty
    if (!simplex_is_empty(self)) {
        int i;
        int bitI;

        // For each possible four points in the simplex set
        for (i=0, bitI = 0x1; i<4; i++, bitI <<= 1) {

            // If the current point is in the simplex
            if (overlap(self->bits_current_simplex, bitI)) {
                int bit2 = bitI | self->last_found_bit;

                self->det[bit2][i] = vec3_dot(self->diff_length[self->last_found][i], self->points[self->last_found]);
                self->det[bit2][self->last_found] = vec3_dot(self->diff_length[i][self->last_found], self->points[i]);
      

                int j;
                int bitJ;

                for (j=0, bitJ = 0x1; j<i; j++, bitJ <<= 1) {
                    if (overlap(self->bits_current_simplex, bitJ)) {
                        int k;
                        int bit3 = bitJ | bit2;

                        k = self->norm_square[i][j] < self->norm_square[self->last_found][j] ? i : self->last_found;
                        self->det[bit3][j] = self->det[bit2][i] * vec3_dot(self->diff_length[k][j], self->points[i]) +
                                       self->det[bit2][self->last_found] *
                                       vec3_dot(self->diff_length[k][j], self->points[self->last_found]);

                        k = self->norm_square[j][i] < self->norm_square[self->last_found][i] ? j : self->last_found;
                        self->det[bit3][i] = self->det[bitJ | self->last_found_bit][j] *
                                        vec3_dot(self->diff_length[k][i], self->points[j]) +
                                        self->det[bitJ | self->last_found_bit][self->last_found] *
                                        vec3_dot(self->diff_length[k][i], self->points[self->last_found]);

                        k = self->norm_square[i][self->last_found] < self->norm_square[j][self->last_found] ? i : j;
                        self->det[bit3][self->last_found] = self->det[bitJ | bitI][j] *
                                                 vec3_dot(self->diff_length[k][self->last_found], self->points[j]) +
                                                 self->det[bitJ | bitI][i] *
                                                 vec3_dot(self->diff_length[k][self->last_found], self->points[i]);
                    }
                }
            }
        }

        if (self->all_bits == 0xf) {
            int k;

            k = self->norm_square[1][0] < self->norm_square[2][0] ?
                                   (self->norm_square[1][0] < self->norm_square[3][0] ? 1 : 3) :
                                   (self->norm_square[2][0] < self->norm_square[3][0] ? 2 : 3);
            self->det[0xf][0] = self->det[0xe][1] * vec3_dot(self->diff_length[k][0], self->points[1]) +
                        self->det[0xe][2] * vec3_dot(self->diff_length[k][0], self->points[2]) +
                        self->det[0xe][3] * vec3_dot(self->diff_length[k][0], self->points[3]);

            k = self->norm_square[0][1] < self->norm_square[2][1] ?
                                   (self->norm_square[0][1] < self->norm_square[3][1] ? 0 : 3) :
                                   (self->norm_square[2][1] < self->norm_square[3][1] ? 2 : 3);
            self->det[0xf][1] = self->det[0xd][0] * vec3_dot(self->diff_length[k][1], self->points[0]) +
                        self->det[0xd][2] * vec3_dot(self->diff_length[k][1], self->points[2]) +
                        self->det[0xd][3] * vec3_dot(self->diff_length[k][1], self->points[3]);

            k = self->norm_square[0][2] < self->norm_square[1][2] ?
                                   (self->norm_square[0][2] < self->norm_square[3][2] ? 0 : 3) :
                                   (self->norm_square[1][2] < self->norm_square[3][2] ? 1 : 3);
            self->det[0xf][2] = self->det[0xb][0] * vec3_dot(self->diff_length[k][2], self->points[0]) +
                        self->det[0xb][1] * vec3_dot(self->diff_length[k][2], self->points[1]) +
                        self->det[0xb][3] * vec3_dot(self->diff_length[k][2], self->points[3]);

            k = self->norm_square[0][3] < self->norm_square[1][3] ?
                                   (self->norm_square[0][3] < self->norm_square[2][3] ? 0 : 2) :
                                   (self->norm_square[1][3] < self->norm_square[2][3] ? 1 : 2);
            self->det[0xf][3] = self->det[0x7][0] * vec3_dot(self->diff_length[k][3], self->points[0]) +
                        self->det[0x7][1] * vec3_dot(self->diff_length[k][3], self->points[1]) +
                        self->det[0x7][2] * vec3_dot(self->diff_length[k][3], self->points[2]);
        }
    }
}

int simplex_is_proper_subset(const struct simplex *self, int subset)
{
    int i;
    int bit;

    // For each four point of the possible simplex set
    for (i=0, bit=0x1; i<4; i++, bit <<=1) {
        if (overlap(subset, bit) && self->det[subset][i] <= 0.0) {
            return 0;
        }
    }

    return 1;
}

static inline int is_subset(int a, int b)
{
    return ((a & b) == a);
}

vec3_t simplex_compute_closest_for_subset(struct simplex *self, int subset)
{
    vec3_t v = vec3(0.0);      // Closet point v = sum(lambda_i * points[i])
    self->max_length_square = 0.0;
    float deltaX = 0.0;            // deltaX = sum of all det[subset][i]
    int i;
    int bit;

    // For each four point in the possible simplex set
    for (i=0, bit=0x1; i<4; i++, bit <<= 1) {
        // If the current point is in the subset
        if (overlap(subset, bit)) {
            // deltaX = sum of all det[subset][i]
            deltaX += self->det[subset][i];

            if (self->max_length_square < self->points_length_square[i]) {
                self->max_length_square = self->points_length_square[i];
            }

            // Closest point v = sum(lambda_i * points[i])
            v = vec3_add(v, vec3_mul_number(self->points[i], self->det[subset][i]));
        }
    }

    /* assert(deltaX > 0.0); */

    // Return the closet point "v" in the convex hull for the given subset
    return vec3_mul_number(v, (1.0f / deltaX));
}

void simplex_backup_closest_point(struct simplex *self, vec3_t *v)
{
    float minDistSquare = 1000000;
    int bit;

    for (bit = self->all_bits; bit != 0x0; bit--)
	{
        if (is_subset(bit, self->all_bits) && simplex_is_proper_subset(self, bit))
		{
            vec3_t u = simplex_compute_closest_for_subset(self, bit);
            float distSquare = vec3_len_square(u);
            if (distSquare < minDistSquare) {
                minDistSquare = distSquare;
                self->bits_current_simplex = bit;
                *v = u;
            }
        }
    }
}

void simplex_print(const struct simplex *self)
{
	printf("simplex:\n");
	int i;
	for(i = 0; i < self->last_found; i++)
	{
		printf("\t");vec3_print(self->points[i]);
	}
}

void simplex_add_point(struct simplex *self, const vec3_t point, const vec3_t suppA, const vec3_t suppB)
{
    /* assert(!simplex_is_full(self)); */

    self->last_found = 0;
    self->last_found_bit = 0x1;

    // Look for the bit corresponding to one of the four point that is not in
    // the current simplex
    while(overlap(self->bits_current_simplex, self->last_found_bit))
	{
        self->last_found++;
        self->last_found_bit <<= 1;
    }

    /* assert(self->last_found < 4); */

    // Add the point into the simplex
    self->points[self->last_found] = point;
    self->points_length_square[self->last_found] = vec3_len_square(point);
    self->all_bits = self->bits_current_simplex | self->last_found_bit;

    // Update the cached values
    simplex_update_cache(self);
    
    // Compute the cached determinant values
	simplex_compute_determinants(self);
    
    // Add the support points of objects A and B
    self->supp_pointsA[self->last_found] = suppA;
    self->supp_pointsB[self->last_found] = suppB;
}

int simplex_is_affinely_dependent(const struct simplex *self)
{
    float sum = 0.0;
    int i;
    int bit;

    // For each four point of the possible simplex set
    for (i=0, bit=0x1; i<4; i++, bit <<= 1) {
        if (overlap(self->all_bits, bit)) {
            sum += self->det[self->all_bits][i];
        }
    }

    return (sum <= 0.0);
}


int simplex_is_valid_subset(const struct simplex *self, int subset)
{
    int i;
    int bit;

    // For each four point in the possible simplex set
    for (i=0, bit=0x1; i<4; i++, bit <<= 1) {
        if (overlap(self->all_bits, bit)) {
            // If the current point is in the subset
            if (overlap(subset, bit)) {
                // If one delta(X)_i is smaller or equal to zero
                if (self->det[subset][i] <= 0.0) {
                    // The subset is not valid
                    return 0;
                }
            }
            // If the point is not in the subset and the value delta(X U {y_j})_j
            // is bigger than zero
            else if (self->det[subset | bit][i] > 0.0) {
                // The subset is not valid
                return 0;
            }
        }
    }

    return 1;
}


int simplex_compute_closest_point(struct simplex *self, vec3_t *v)
{
    int subset;

    // For each possible simplex set
    for (subset=self->bits_current_simplex; subset != 0x0; subset--) {
        // If the simplex is a subset of the current simplex and is valid for the Johnson's
        // algorithm test
		if (is_subset(subset, self->bits_current_simplex) &&
				simplex_is_valid_subset(self, subset | self->last_found_bit))
		{
           self->bits_current_simplex = subset | self->last_found_bit;              // Add the last added point to the current simplex
           *v = simplex_compute_closest_for_subset(self, self->bits_current_simplex);    // Compute the closest point in the simplex
           return 1;
        }
    }

    // If the simplex that contains only the last added point is valid for the Johnson's algorithm test
    if (simplex_is_valid_subset(self, self->last_found_bit))
	{
        self->bits_current_simplex = self->last_found_bit;
        self->max_length_square = self->points_length_square[self->last_found];
        *v = self->points[self->last_found];                              // The closest point of the simplex "v" is the last added point
        return 1;
    }

    // The algorithm failed to find a point
    return 0;
}

static inline int simplex_is_full(const struct simplex *self)
{
    return self->bits_current_simplex == 0xf;
}

int gjk_intersects(c_rigid_body_t *self, c_rigid_body_t *other,
		contact_t *contact)
{
	vec3_t suppA;             // Support point of object A
	vec3_t suppB;             // Support point of object B
	vec3_t w;                 // Support point of Minkowski difference A-B
	vec3_t pA;                // Closest point of object A
	vec3_t pB;                // Closest point of object B
	float vDotw;
	float prevDistSquare;

	mesh_t *mesh1 = c_model(self)->mesh;
	mesh_t *mesh2 = c_model(other)->mesh;

	c_spacial_t *sc1 = c_spacial(self);
	c_spacial_t *sc2 = c_spacial(other);

	mat4_t other_to_self;
	mat4_t rotate_to_other;
	mat4_t inv_rotate_to_other;

	// Transform a point from local space of body 2 to local
	// space of body 1 (the GJK algorithm is done in local space of body 1)
	other_to_self = mat4_invert(sc1->model_matrix);
	other_to_self = mat4_mul(other_to_self, sc2->model_matrix);

	// Matrix that transform a direction from local
	// space of body 1 into local space of body 2
	rotate_to_other = mat4_transpose(sc2->rot_matrix);
	rotate_to_other = mat4_mul(rotate_to_other, sc1->rot_matrix);
	inv_rotate_to_other = mat4_invert(rotate_to_other);

	vec3_t v = vec3_sub(sc2->pos, sc1->pos);

	/* assert(shape1Info.collisionShape->isConvex()); */
	/* assert(shape2Info.collisionShape->isConvex()); */

	// Initialize the margin (sum of margins of both objects)
	/* float margin = shape1->getMargin() + shape2->getMargin(); */
	float margin = mesh_get_margin(mesh1) + mesh_get_margin(mesh2);
	float marginSquare = margin * margin;

	// Create a simplex set
	struct simplex simp = {0};

	// Initialize the upper bound for the square distance
	float distSquare = 1000000;

	do
	{

		vec3_t v2 = mat4_mul_vec4(rotate_to_other, vec4(_vec3(v), 0.0)).xyz;

		// Compute the support points for original objects (without margins) A and B
		suppA = XYZ(mesh_farthest(mesh1, vec3_inv(v))->pos);
		suppB = XYZ(mesh_farthest(mesh2, v2)->pos);
		suppB = mat4_mul_vec4(other_to_self, vec4(_vec3(suppB), 1.0)).xyz;

		// Compute the support point for the Minkowski difference A-B
		w = vec3_sub(suppA, suppB);

		vDotw = vec3_dot(v, w);

		if(vDotw > 0.0 && vDotw * vDotw > distSquare * marginSquare)
		{
			return 0;
		}

		const float REL_ERROR_SQUARE = REL_ERROR * REL_ERROR;

		// If the objects intersect only in the margins
		if(simplex_has_point(&simp, w) || distSquare - vDotw <= distSquare * REL_ERROR_SQUARE)
		{
			goto contact_info;
		}

		// Add the new support point to the simp
		simplex_add_point(&simp, w, suppA, suppB);

		// If the simp is affinely dependent
		if(simplex_is_affinely_dependent(&simp))
		{
			goto contact_info;
		}

		// Compute the point of the simp closest to the origin
		// If the computation of the closest point fail
		if(!simplex_compute_closest_point(&simp, &v))
		{
			goto contact_info;
		}

		// Store and update the squared distance of the closest point
		prevDistSquare = distSquare;
		distSquare = vec3_len_square(v);

		// If the distance to the closest point doesn't improve a lot
		if (prevDistSquare - distSquare <= FLT_EPSILON * prevDistSquare)
		{
			goto contact_info;
		}
		/* simplex_print(&simp); */
	} while(!simplex_is_full(&simp) && distSquare > FLT_EPSILON * simp.max_length_square);

	if(!simplex_is_full(&simp)) return 0;

contact_info:
	if(contact)
	{
		// Compute the closet points of both objects (without the margins)
		simplex_closest_of_A_B(&simp, &pA, &pB);

		// Project those two points on the margins to have the closest points of both
		// object with the margins
		float dist = sqrt(distSquare);
		/* assert(dist > 0.0); */
		pA = vec3_sub(pA, vec3_mul_number(v, mesh_get_margin(mesh1) / dist));
		pB = vec3_add(pB, vec3_mul_number(v, mesh_get_margin(mesh2) / dist));
		pB = mat4_mul_vec4(inv_rotate_to_other, vec4(_vec3(pB), 0.0)).xyz;

		// Compute the contact info
		contact->normal = mat4_mul_vec4(sc1->rot_matrix,
				(vec4(_vec3(vec3_inv(vec3_get_unit(v))), 0.0))).xyz;
		contact->depth = margin - dist;
	}
	return 1;
}
