#ifndef VECTOR_H
#define VECTOR_H

typedef struct vector_t vector_t;

vector_t *vector_new(int size);
void *vector_get(vector_t *self, int i);
void *vector_get_set(vector_t *self, int i);
void vector_set(vector_t *self, int i, void *data);
int vector_count(vector_t *self);
int vector_add(vector_t *self);
void vector_clear(vector_t *self);
void vector_alloc(vector_t *self, int num);
void vector_remove(vector_t *self, int i);
void vector_destroy(vector_t *self);

#endif /* !VECTOR_H */
