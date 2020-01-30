#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <tinycthread.h>

struct element
{
	int set;
	char data[1];
};

typedef int(*vector_compare_cb)(const void *a, const void *b);
#define FIXED_INDEX      (0x1 << 0x0)
#define FIXED_ORDER      (0x1 << 0x1)
#define VECTOR_REENTRANT (0x1 << 0x2)

typedef struct vector
{
	int count;
	int alloc;
	int free_count;

	int fixed_index;
	int fixed_order;
	int reentrant;

	int data_size;
	unsigned int elem_size;
	struct element *elements;
	void *fallback;

	vector_compare_cb compare;
	mtx_t mtx;
} vector_t;

vector_t *vector_new(int data_size, int flags, void *fallback,
                     vector_compare_cb compare)
{
	vector_t *self = malloc(sizeof(*self));

	self->fixed_index = !!(flags & FIXED_INDEX);
	self->fixed_order = !!(flags & FIXED_ORDER);
	self->reentrant   = !!(flags & VECTOR_REENTRANT);

	self->compare = compare;
	self->data_size = data_size;
	self->elem_size = data_size + sizeof(struct element);
	self->free_count = 0;
	self->count = 0;
	self->alloc = 0;
	self->elements = NULL;
	self->fallback = NULL;
	if (self->reentrant)
	{
		mtx_init(&self->mtx, mtx_plain);
	}
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
	return (struct element *)(((size_t)self->elements) + i * self->elem_size);
}

void vector_get_copy(vector_t *self, int i, void *copy)
{
	struct element *element;

	if (self->reentrant) mtx_lock(&self->mtx);
	
	element = _vector_get(self, i);

	if (element && element->set)
	{
		void *data = &element->data;
		memcpy(copy, data, self->data_size);
	}

	if (self->reentrant) mtx_unlock(&self->mtx);
}

void *vector_get(vector_t *self, int i)
{
	void *ret = NULL;
	struct element *element;

	if (self->reentrant)
	{
		perror("vector_get cannot be used for reentrant vectors, use vector_get_copy\n");
		exit(1);
	}

	element = _vector_get(self, i);
	if (element && element->set) ret = &element->data;
	return ret;
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

void vector_remove(vector_t *self, int i)
{
	struct element *element;

	if (self->reentrant) mtx_lock(&self->mtx);

	element = _vector_get(self, i);

	if(!element || !element->set)
	{
		goto end;
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
end:
	if (self->reentrant) mtx_unlock(&self->mtx);
}

int vector_index_of(vector_t *self, void *data)
{
	size_t element = ((size_t)data - sizeof(struct element));
	
	return (element - (size_t)self->elements) / self->elem_size;
}

void vector_remove_item(vector_t *self, void *item)
{
	if((item = vector_get_item(self, item)))
	{
		vector_remove(self, vector_index_of(self, item));
	}
}

void vector_alloc(vector_t *self, int num)
{
	self->alloc += num;
	self->elements = realloc(self->elements, self->alloc * self->elem_size);
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


int vector_get_insert(vector_t *self, void *value)
{
	int lower = 0;
	int cur = 0;
	int upper;
	char *base;

	if(vector_count(self) == 0)
	{
		return 0;
	}
	upper = vector_count(self) - 1;

	base = (char *)self->elements;
	while(1)
	{
		struct element *pivot;
		int result;

		cur = (upper + lower) / 2;
		pivot = (struct element *)(base + cur * self->elem_size);
		result = self->compare(&pivot->data, value);

		if(result == 0)
		{
			return cur;
		}
		else if(result < 0)
		{
			lower = cur + 1; /* its in the upper */
			if(lower > upper) return cur + 1;
		}
		else
		{
			upper = cur - 1; /* its in the lower */
			if(lower > upper) return cur;
		}
	}
}

int _vector_reserve(vector_t *self)
{
	int i;
	struct element *element;

	if(!self->free_count)
	{
		i = self->count++;
		_vector_grow(self);
		element = _vector_get(self, i);
		element->set = 1;
		return i;
	}
	for(i = 0; i < self->count; i++)
	{
		element = _vector_get(self, i);
		if(!element->set)
		{
			self->free_count--;
			element->set = 1;
			return i;
		}
	}

	return -1;
}

int vector_reserve(vector_t *self)
{
	int ret;

	if (self->reentrant) mtx_lock(&self->mtx);

	ret = _vector_reserve(self);

	if (self->reentrant) mtx_unlock(&self->mtx);

	return ret;
}

void vector_add(vector_t *self, void *value)
{
	int count;
	int si;

	if (self->reentrant) mtx_lock(&self->mtx);

	count = self->count;
	si = count;
	if(self->compare) /* SORTED INSERT */
	{
		si = vector_get_insert(self, value);
	}

	_vector_reserve(self);

	if(si < count)
	{
		vector_shift(self, si, 1);
	}
	memcpy(&_vector_get(self, si)->data, value, self->data_size);
	if (self->reentrant) mtx_unlock(&self->mtx);
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

