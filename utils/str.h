#ifndef STR_LIB
#define STR_LIB

#if defined(STR_FORMAT_DYNAMIC)
#warning Dynamic size format impacts performance when over-used!
#endif

#define STR_DEFAULT_FORMAT_SIZE 100000

#ifndef STR_DEFAULT_CHUNK
#define STR_DEFAULT_CHUNK 32
#endif

#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

char *str_new(size_t);
void str_cat(char **, const char *);
void str_catf(char **, const char *, ...);
char *str_dup(const char *);
char *str_new_copy(const char *);
char *str_set_chunk(char *, size_t);
size_t str_len(const char *);
void str_free(const char *);

int str_count(const char *, const char *);
char *str_replace(const char *, const char *, const char *);
char *str_replace2(const char *, const char *, const char *);

#endif /* !STR_LIB */
