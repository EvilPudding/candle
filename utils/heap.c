#include <unistd.h>
#include <stdlib.h>

#include "heap.h"

#define CMP(a, b) (a) <= (b)
/* #define CMP(a, b) (a) >= (b) */

#define BASE_SIZE 4

/* Prepares the heap for use */
void heap_init(heap_t *self)
{
	self->size = BASE_SIZE;
	self->count = 0;
	self->data = malloc(sizeof(*self->data) * BASE_SIZE);
	if (!self->data) _exit(1); /* Exit if the memory allocation fails */
}

/* Inserts element to the heap */
void heap_push(heap_t *self, float n, void *value)
{
	unsigned int index, parent;

	/* Resize the heap if it is too small to hold all the data */
	if (self->count == self->size)
	{
		self->size <<= 1;
		self->data = realloc(self->data, sizeof(*self->data) * self->size);
		if (!self->data) _exit(1); /* Exit if the memory allocation fails */
	}

	/* Find out where to put the element and put it */
	for(index = self->count++; index; index = parent)
	{
		parent = (index - 1) >> 1;
		if(CMP(self->data[parent].n, n)) break;
		self->data[index] = self->data[parent];
	}
	self->data[index].n = n;
	self->data[index].ptr = value;
}

int heap_offset_of(heap_t *self, void *ptr)
{
	unsigned int i;
	for(i = 0; i < self->count; i++)
	{
		if(self->data[i].ptr == ptr)
		{
			return i;
		}
	}
	return -1;
}

/* Removes the biggest element from the heap */
void heap_pop(heap_t *self)
{
	unsigned int index, swap, other;

	/* Remove the biggest element */
	struct float_ptr temp = self->data[--self->count];

	/* Resize the heap if it's consuming too much memory */
	if ((self->count <= (self->size >> 2)) && (self->size > BASE_SIZE))
	{
		self->size >>= 1;
		self->data = realloc(self->data, sizeof(*self->data) * self->size);
		if (!self->data) _exit(1); /* Exit if the memory allocation fails */
	}

	/* Reorder the elements */
	for(index = 0; 1; index = swap)
	{
		/* Find the child to swap with */
		swap = (index << 1) + 1;
		if (swap >= self->count) break; /* If there are no children, the heap is reordered */
		other = swap + 1;
		if ((other < self->count) &&
				CMP(self->data[other].n, self->data[swap].n)) swap = other;
		if(CMP(temp.n, self->data[swap].n)) break; /* If the bigger child is less than or equal to its parent, the heap is reordered */

		self->data[index] = self->data[swap];
	}
	self->data[index] = temp;
}

/* Heapifies a non-empty array */
void heapify(struct float_ptr data[], unsigned int count)
{
	unsigned int item, index, swap, other;
	struct float_ptr temp;

	/* Move every non-leaf element to the right position in its subtree */
	item = (count >> 1) - 1;
	while (1)
	{
		/* Find the position of the current element in its subtree */
		temp = data[item];
		for(index = item; 1; index = swap)
		{
			/* Find the child to swap with */
			swap = (index << 1) + 1;
			if(swap >= count) break; /* If there are no children, the current element is positioned */
			other = swap + 1;
			if((other < count) && CMP(data[other].n, data[swap].n)) swap = other;
			if(CMP(temp.n, data[swap].n)) break; /* If the bigger child is smaller than or equal to the parent, the heap is reordered */

			data[index] = data[swap];
		}
		if (index != item) data[index] = temp;

		if (!item) return;
		--item;
	}
}
