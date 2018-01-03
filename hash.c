#include "hash.h"

#include <stdlib.h>
#include <stdio.h>

typedef struct hash_t
{
	void **data;
	unsigned long size;
	unsigned long offset;
} hash_t;

hash_t *hash_new(unsigned long offset)
{
	hash_t *self = malloc(sizeof *self);
	self->offset = offset;
	return self;
}

static void **hash_getp(hash_t *self, const char *key)
{
	/* unsigned long start = 0; */
	/* unsigned long imin = 0; */
	/* unsigned long imax = self->size; */

    /* while (imax >= imin) */
	/* { */
    /*     const int i = (imin + ((imax-imin)/2)); */
    /*     int c = strcmp(name, functions[i].name, len); */
    /*     if (!c) c = '\0' - functions[i].name[len]; */
    /*     if (c == 0) { */
    /*         return functions + i; */
    /*     } else if (c > 0) { */
    /*         imin = i + 1; */
    /*     } else { */
    /*         imax = i - 1; */
    /*     } */
    /* } */
	/* return &self->data[start]; */
	return NULL;
}

void *hash_get(hash_t *self, const char *key)
{
	void **ptr = hash_getp(self, key);
	/* if(ptr */

	return *ptr;
}

void hash_set(hash_t *self, const char *key, void *value)
{
	/* void **ptr = hash_getp(self, key); */
	/* if(ptr */
}
