#ifndef BLOOM_H
#define BLOOM_H

#include "../glutil.h"
#include "../material.h"
#include "../texture.h"
#include "../mesh.h"
#include "../shader.h"

typedef struct c_renderer_t c_renderer_t;

void filter_bloom_init(void);
void filter_bloom(c_renderer_t *renderer, float scale, texture_t *screen);


#endif /* !BLOOM_H */
