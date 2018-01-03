#ifndef EXT_H
#define EXT_H

#include <stdlib.h>

#define new(st) malloc(sizeof(*st))
#define ext(a, st) realloc(a, sizeof(*st))

#endif /* !EXT_H */
