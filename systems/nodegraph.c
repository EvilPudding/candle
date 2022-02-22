#include "../ecs/ecm.h"
#include "nodegraph.h"
#include "../components/node.h"
#include "../components/name.h"
#include "editmode.h"
#include "../utils/nk.h"

c_nodegraph_t *c_nodegraph_new()
{
	c_nodegraph_t *self = component_new(ct_nodegraph);

	return self;
}

void node_entity(c_nodegraph_t *self, entity_t entity, void *ctx)
{
	char buffer[64];
	char *final_name = buffer;
	c_name_t *name = c_name(&entity);
	c_editmode_t *emc = c_editmode(self);
	c_node_t *node;
	if(name && name->name[0])
	{
		final_name = name->name;
	}
	else
	{
#ifdef _WIN32
		sprintf(buffer, "ENT_%I64u", entity);
#elif __EMSCRIPTEN__
		sprintf(buffer, "NODE_%lld", entity);
#else
		sprintf(buffer, "NODE_%ld", entity);
#endif
	}
	node = c_node(&entity);
	if(node)
	{
		int i, has_children, is_selected, res;
		if(node->ghost) return;
		has_children = 0;
		for(i = 0; i < node->children_size; i++)
		{
			if(!c_node(&node->children[i])->ghost)
			{
				has_children = 1;
				break;
			}
		}
		is_selected = emc->selected == entity;
		res = nk_tree_entity_push_id(ctx, NK_TREE_NODE, final_name,
		                             NK_MINIMIZED, &is_selected,
		                             has_children, (int)entity);
		if(is_selected)
		{
			if(is_selected && emc->selected != entity)
			{
				c_editmode_select(emc, entity);
			}
		}
		if(res)
		{
			for(i = 0; i < node->children_size; i++)
			{
				node_entity(self, node->children[i], ctx);
			}

			nk_tree_entity_pop(ctx);
		}
	}
}

int c_nodegraph_component_menu(c_nodegraph_t *self, void *ctx)
{
	unsigned long i;

	c_node_t *nc = c_node(self);
	for(i = 0; i < nc->children_size; i++)
	{
		c_node_t *node = c_node(&nc->children[i]);

		if(node->ghost) continue;
		node_entity(self, nc->children[i], ctx);
	}
	return CONTINUE;
}


void ct_nodegraph(ct_t *self)
{
	ct_init(self, "node graph", sizeof(c_nodegraph_t));
	ct_add_listener(self, WORLD, 100, sig("component_menu"), c_nodegraph_component_menu);
}

