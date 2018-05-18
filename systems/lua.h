#ifndef LUA_H
#define LUA_H

#include <ecm.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <luaconf.h>

typedef struct c_lua_t
{
	c_t super;
	lua_State *state;
} c_lua_t;

DEF_CASTER("lua", c_lua, c_lua_t)
c_lua_t *c_lua_new(void);

int c_lua_loadexpr(c_lua_t *self, char *expr, char **pmsg);
double c_lua_eval(c_lua_t *self, int ref, char **pmsg);
void c_lua_unref(c_lua_t *self, int ref);
void c_lua_setvar(c_lua_t *self, char *name, double value);
double c_lua_getvar(c_lua_t *self, char *name);

#endif /* !LUA_H */
