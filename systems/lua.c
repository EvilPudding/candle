#include "lua.h"

#include <stdlib.h>
#include <string.h>

c_lua_t *c_lua_new()
{
	c_lua_t *self = component_new("lua");
	self->state = luaL_newstate();
	if(self->state) luaL_openlibs(self->state);
	return self;
}

int c_lua_loadexpr(c_lua_t *self, char *expr, char **pmsg)
{
	int err;
	char *buf;

	if(!self->state)
	{
		if(pmsg) *pmsg = strdup("LE library not initialized");
		return LUA_NOREF;
	}
	buf = malloc(strlen(expr)+8);
	if(!buf)
	{
		if(pmsg) *pmsg = strdup("Insufficient memory");
		return LUA_NOREF;
	}
	strcpy(buf, "return ");
	strcat(buf, expr);
	err = luaL_loadstring(self->state,buf);
	free(buf);
	if(err)
	{
		if(pmsg) *pmsg = strdup(lua_tostring(self->state,-1));
		lua_pop(self->state,1);
		return LUA_NOREF;
	}
	if(pmsg) *pmsg = NULL;
	return luaL_ref(self->state, LUA_REGISTRYINDEX);
}

double c_lua_eval(c_lua_t *self, int ref, char **pmsg)
{
	int err;
	double ret;

	if(!self->state)
	{
		if(pmsg) *pmsg = strdup("LE library not initialized");
		return 0;
	}
	lua_rawgeti(self->state, LUA_REGISTRYINDEX, ref);
	err = lua_pcall(self->state,0,1,0);
	if(err)
	{
		if(pmsg) *pmsg = strdup(lua_tostring(self->state,-1));
		lua_pop(self->state,1);
		return 0;
	}
	if(pmsg) *pmsg = NULL;
	ret = (double)lua_tonumber(self->state,-1);
	lua_pop(self->state,1);
	return ret;
}

void c_lua_unref(c_lua_t *self, int ref)
{
	if(!self->state) return;
	luaL_unref(self->state, LUA_REGISTRYINDEX, ref);    
}

void c_lua_setvar(c_lua_t *self, char *name, double value)
{
	if(!self->state) return;
	lua_pushnumber(self->state,value);
	lua_setglobal(self->state,name);
}

double c_lua_getvar(c_lua_t *self, char *name)
{
	double ret;

	if(!self->state)
		return 0;
	lua_getglobal(self->state,name);
	ret = (double)lua_tonumber(self->state,-1);
	lua_pop(self->state, 1);
	return ret;
}

REG()
{
	ct_new("lua", sizeof(c_lua_t), NULL, NULL, 0);
}
