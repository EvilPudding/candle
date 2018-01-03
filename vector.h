#ifndef VECTOR_H
#define VECTOR_H

typedef struct vector_t vector_t;

vector_t *vector_new(int size);
void *vector_get(vector_t *self, int i);
void vector_set(vector_t *self, int i, void *data);
void vector_destroy(vector_t *self);

#endif /* !VECTOR_H */
