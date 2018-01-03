#ifndef BLUR_H
#define BLUR_H

#include "../glutil.h"
#include "../material.h"
#include "../texture.h"
#include "../mesh.h"
#include "../shader.h"

typedef struct c_renderer_t c_renderer_t;

void filter_blur_init(void);
void filter_blur(c_renderer_t *renderer, texture_t *screen, float scale);


#endif /* !BLUR_H */
