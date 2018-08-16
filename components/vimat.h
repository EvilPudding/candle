#ifndef VIMAT_H
#define VIMAT_H

#include <ecs/ecm.h>
#include <vil/vil.h>

typedef struct
{
	c_t super; /* extends c_t */
	vitype_t *vil;
} c_vimat_t;

DEF_CASTER("vimat", c_vimat, c_vimat_t)

c_vimat_t *c_vimat_new(void);



#endif /* !VIMAT_H */
