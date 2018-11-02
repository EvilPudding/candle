#include "vector.h"
#include <stdlib.h>
#include <string.h>
#include <ecs/ecm.h>

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

	int fixed_index;
	int fixed_order;

	int data_size;
	uint32_t elem_size;
	struct element *elements;
	void *fallback;

	vector_compare_cb compare;
} vector_t;

vector_t *vector_new(int data_size, int flags, void *fallback,
		vector_compare_cb compare)
{
	vector_t *self = malloc(sizeof(*self));

	self->fixed_index = !!(flags & FIXED_INDEX);
	self->fixed_order = !!(flags & FIXED_ORDER);

	self->compare = compare;
	self->data_size = data_size;
	self->elem_size = data_size + sizeof(struct element);
	self->free_count = 0;
	self->count = 0;
	self->alloc = 0;
	self->elements = NULL;
	self->fallback = NULL;
	if(fallback)
	{
		self->fallback = malloc(data_size);
		memcpy(self->fallback, fallback, data_size);
	}

	return self;
}

vector_t *vector_clone(vector_t *self)
{
	vector_t *clone = malloc(sizeof(*self));
	*clone = *self;

	if(self->fallback)
	{
		clone->fallback = malloc(self->data_size);
		memcpy(clone->fallback, self->fallback, self->data_size);
	}
	clone->elements = malloc(self->elem_size * self->alloc);
	memcpy(clone->elements, self->elements, self->elem_size * self->alloc);
	return clone;
}

static struct element *_vector_get(vector_t *self, int i)
{
	if(i < 0 || i >= self->count) return NULL;
	return (struct element *)(((uintptr_t)self->elements) + i * self->elem_size);
}

void *vector_get(vector_t *self, int i)
{
	struct element *element = _vector_get(self, i);
	if(element && element->set) return &element->data;
	return NULL;
}

void *_vector_value(vector_t *self, int i)
{
	struct element *element = _vector_get(self, i);
	if(element && element->set) return &element->data;
	return self->fallback;
}

void *vector_get_item(vector_t *self, void *item)
{
	if(self->compare)
	{
		int res;
		struct element *pivot = NULL;
		char *base = (char *)self->elements;
		int num = self->count;

		while(num > 0)
		{
			num >>= 1;
			pivot = (struct element *)(base + num * self->elem_size);
			res = self->compare(item, pivot->data);

			if(res == 0)
			{
				return pivot->data;
			}
			else if(res < 0)
			{
				pivot = (struct element *)(base + (num + 1) * self->elem_size);
			}
			else if(res > 0)
			{
				base = ((char *)pivot) + self->elem_size;
				num--;
			}
		}
		if(pivot)
		{
			return pivot->data;
		}
	}
	else
	{
		int i;
		for(i = 0; i < self->count; i++)
		{
			void *element = vector_get(self, i);
			if(element)
			{
				if(!memcmp(item, element, self->data_size))
				{
					return element;
				}
			}
		}
	}
	return NULL;
}

void vector_remove_item(vector_t *self, void *item)
{
	if((item = vector_get_item(self, item)))
	{
		vector_remove(self, vector_index_of(self, item));
	}
}

void vector_remove(vector_t *self, int i)
{
	struct element *element = _vector_get(self, i);

	if(!element || !element->set)
	{
		return;
	}
	if(self->fixed_index)
	{
		element->set = 0;
		self->free_count++;
	}
	else
	{

		if(i != self->count - 1)
		{
			if(self->fixed_order)
			{
				vector_shift(self, i, -1);
			}
			else
			{
				void *data = vector_get(self, i);
				void *last = vector_get(self, self->count - 1);

				memmove(data, last, self->data_size);
			}
		}
		self->count--;
	}
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

void vector_shift(vector_t *self, int id, int count)
{
	/* if(id > self->count) exit(1); */
	if(count > 0)
	{
		memmove(_vector_get(self, id + count), _vector_get(self, id),
				self->elem_size * (self->count - count - id));
	}
	else
	{
		count = -count;
		memmove(_vector_get(self, id), _vector_get(self, id + count),
				self->elem_size * (self->count - count - id));
	}
}


int vector_get_insert(vector_t *self, void *value)
{
	if(vector_count(self) == 0)
	{
		return 0;
	}
	int lower = 0;
	int upper = vector_count(self) - 1;
	int cur = 0;

	char *base = (char *)self->elements;
	while(1)
	{
		cur = (upper + lower) / 2;
		struct element *pivot = (struct element *)(base + cur * self->elem_size);
		int result = self->compare(pivot->data, value);

		if(result == 0)
		{
			return cur;
		}
		else if(result < 0)
		{
			lower = cur + 1; // its in the upper
			if(lower > upper) return cur + 1;
		}
		else
		{
			upper = cur - 1; // its in the lower
			if(lower > upper) return cur;
		}
	}
}


void vector_add(vector_t *self, void *value)
{
	int count = self->count;
	int si = count;
	if(self->compare) /* SORTED INSERT */
	{
		si = vector_get_insert(self, value);
	}

	vector_reserve(self);

	if(si < count)
	{
		vector_shift(self, si, 1);
	}
	memcpy(vector_get(self, si), value, self->data_size);
}

int vector_reserve(vector_t *self)
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

