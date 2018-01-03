#ifndef HASH_H
#define HASH_H

typedef struct hash_t hash_t;

hash_t *hash_new(unsigned long offset);
void *hash_get(hash_t *self, const char *key);
void hash_set(hash_t *self, const char *key, void *value);

#endif /* !HASH_H */
