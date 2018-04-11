#ifndef HEAP_H
#define HEAP_H

struct float_ptr
{
	float n;
	void *ptr;
};

typedef struct heap
{
	unsigned int size; // Size of the allocated memory (in number of items)
	unsigned int count; // Count of the elements in the heap
	struct float_ptr *data; // Array with the elements
} heap_t;

void heap_init(heap_t *restrict h);
void heap_push(heap_t *restrict self, float n, void *value);
void heap_pop(heap_t *restrict h);

int heap_offset_of(heap_t *restrict self, void *ptr);
// Returns the biggest element in the heap
#define heap_front(h) (*(h)->data)

// Frees the allocated memory
#define heap_term(h) (free((h)->data))

void heapify(struct float_ptr data[restrict], unsigned int count);

#endif /* !HEAP_H */
