#ifndef NODEGRAPH_H
#define NODEGRAPH_H


typedef struct c_nodegraph_t
{
	c_t super;
	/* currently, nodegraph has no options */
} c_nodegraph_t;

DEF_CASTER("nodegraph", c_nodegraph, c_nodegraph_t)
c_nodegraph_t *c_nodegraph_new(void);

#endif /* !NODEGRAPH_H */
