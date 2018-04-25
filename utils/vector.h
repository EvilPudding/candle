#ifndef VECTOR_H
#define VECTOR_H

typedef struct vector_t vector_t;

typedef int(*vector_compare_cb)(const void *a, const void *b);

vector_t *vector_new(int data_size, int fixed_index, void *fallback,
		vector_compare_cb compare);

#define vector_value(vec, i, type) *((type*)_vector_value(vec, i))

#define FIXED_INDEX 0x01
#define FIXED_ORDER 0x02

void vector_add(vector_t *self, void *value);
vector_t *vector_clone(vector_t *self);
void *vector_get(vector_t *self, int i);
void *_vector_value(vector_t *self, int i);

void *vector_get_set(vector_t *self, int i);
int vector_index_of(vector_t *self, void *data);
void vector_shift(vector_t *self, int id, int count);
void vector_set(vector_t *self, int i, void *data);
int vector_count(vector_t *self);
int vector_reserve(vector_t *self);
void vector_clear(vector_t *self);
void vector_alloc(vector_t *self, int num);
void vector_remove(vector_t *self, int i);
void vector_remove_item(vector_t *self, void *item);
void vector_destroy(vector_t *self);

#endif /* !VECTOR_H */
