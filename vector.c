#include "vector.h"
#include <stdlib.h>

struct element
{
	int set;
	char data[];
};

typedef struct vector_t
{
	int count;
	int alloc;
	int free_count;

	int data_size;
	int elem_size;
	struct element *elements;
} vector_t;


vector_t *vector_new(int size)
{
	vector_t *self = malloc(sizeof(*self));

	self->data_size = size;
	self->elem_size = size + sizeof(struct element);
	self->free_count = 0;
	self->count = 0;
	self->alloc = 0;
	self->elements = NULL;

	return self;
}

static struct element *_vector_get(vector_t *self, int i)
{
	if(i < 0 || i >= self->count) return NULL;
	return (struct element *)(((unsigned long)self->elements) + i * self->elem_size);
}

void *vector_get(vector_t *self, int i)
{
	struct element *element = _vector_get(self, i);
	if(element && element->set) return &element->data;
	return NULL;
}

void vector_remove(vector_t *self, int i)
{
	struct element *element = _vector_get(self, i);
	element->set = 0;
	self->free_count++;
}

void vector_alloc(vector_t *self, int num)
{
	self->alloc += num;
	self->elements = realloc(self->elements, self->alloc * self->elem_size);
}

int vector_index_of(vector_t *self, void *data)
{
	size_t element = ((size_t)data - sizeof(struct element));
	
	return (element - (size_t)self->elements) / self->elem_size;
}

void _vector_grow(vector_t *self)
{
	int dif = self->alloc - self->count;
	if(dif < 0)
	{
		vector_alloc(self, -dif);
	}
}

int vector_count(vector_t *self)
{
	return self->count;
}

int vector_add(vector_t *self)
{
	int i;
	if(!self->free_count)
	{
		i = self->count++;
		_vector_grow(self);
		struct element *element = _vector_get(self, i);
		element->set = 1;
		return i;
	}
	for(i = 0; i < self->count; i++)
	{
		struct element *element = _vector_get(self, i);
		if(!element->set)
		{
			self->free_count--;
			element->set = 1;
			return i;
		}
	}
	return -1;
}

void *vector_get_set(vector_t *self, int i)
{
	if(i < 0 || i >= self->count) return NULL;
	for(; i < self->count; i++)
	{
		void *data = vector_get(self, i);
		if(data) return data;
	}
	return NULL;
}

void vector_clear(vector_t *self)
{
	self->count = 0;
	self->free_count = 0;
}

void vector_destroy(vector_t *self)
{
	free(self->elements);
	free(self);
}

