#ifndef PHYSICS_H
#define PHYSICS_H

#include "../glutil.h"
#include "../material.h"
#include "../texture.h"
#include "../mesh.h"
#include "../shader.h"
#include <ecm.h>

typedef float(*collider_cb)(c_t *self, vec3_t pos);
typedef float(*velocity_cb)(c_t *self, vec3_t pos);

void physics_register(ecm_t *ecm);

extern unsigned long collider_callback;

#endif /* !PHYSICS_H */
