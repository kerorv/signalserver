#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "lua/lua.h"
#include "lua/lualib.h"
#include "lua/lauxlib.h"
#include "server.h"
#include "lsession.h"

#define LUA_SESSION_FILE	"session.lua"

struct lsession
{
	lua_State* L;
	int ref_oninit;
	int ref_onmessage;
	int ref_onclosing;
};

int lsession_sendmsg(lua_State* L);
int lsession_closeconn(lua_State* L);
void registerfunction(lua_State* L, const char* name, lua_CFunction func, void* upvalue);
int makefunctionref(lua_State* L, const char* name);

lsession_t* lsession_new(tcpserver_t* server)
{
	lsession_t* ls = (lsession_t*)malloc(sizeof(lsession_t));
	assert(ls);

	ls->ref_oninit = LUA_NOREF;
	ls->ref_onmessage = LUA_NOREF;
	ls->ref_onclosing = LUA_NOREF;

	ls->L = luaL_newstate();
	assert(ls->L);
	luaL_openlibs(ls->L);
	int ret = luaL_dofile(ls->L, LUA_SESSION_FILE);
	if (ret != 0)
	{
		printf("luaL_dofile() fail: 0x%x, %s\n", ret, lua_tostring(ls->L, -1));
		lua_close(ls->L);
		free(ls);
		return NULL;
	}

	registerfunction(ls->L, "sendmsg", lsession_sendmsg, server);
	registerfunction(ls->L, "close", lsession_closeconn, server);
	ls->ref_oninit = makefunctionref(ls->L, "oninit");
	ls->ref_onmessage = makefunctionref(ls->L, "onmessage");
	ls->ref_onclosing = makefunctionref(ls->L, "onbreak");
	if (ls->ref_oninit == LUA_REFNIL ||
		ls->ref_onmessage == LUA_REFNIL ||
		ls->ref_onclosing == LUA_REFNIL)
	{
		printf("makefunctionref fail.\n");
		lua_close(ls->L);
		free(ls);
		return NULL;
	}

	return ls;
}

void lsession_init(lsession_t* ls, size_t id)
{
	int type = lua_rawgeti(ls->L, LUA_REGISTRYINDEX, ls->ref_oninit);
	assert(type == LUA_TFUNCTION);

	lua_pushinteger(ls->L, (lua_Integer)id);

	int ret = lua_pcall(ls->L, 1, 0, 0);
	if (ret != LUA_OK)
	{
		printf("call lua oninit() fail: 0x%x, %s\n", ret, lua_tostring(ls->L, -1));
	}
}

void lsession_onmessage(lsession_t* ls, size_t id, const char* packet, size_t len)
{
	int type = lua_rawgeti(ls->L, LUA_REGISTRYINDEX, ls->ref_onmessage);
	assert(type == LUA_TFUNCTION);

	lua_pushinteger(ls->L, (lua_Integer)id);
	lua_pushlstring(ls->L, packet, len);

	int ret = lua_pcall(ls->L, 2, 0, 0);
	if (ret != LUA_OK)
	{
		printf("call lua onpacket() fail: 0x%x, %s\n", ret, lua_tostring(ls->L, -1));
	}
}

void lsession_onclosing(lsession_t* ls, size_t id)
{
	int type = lua_rawgeti(ls->L, LUA_REGISTRYINDEX, ls->ref_onclosing);
	assert(type == LUA_TFUNCTION);

	lua_pushinteger(ls->L, (lua_Integer)id);

	int ret = lua_pcall(ls->L, 1, 0, 0);
	if (ret != LUA_OK)
	{
		printf("call lua onbreak() fail: 0x%x, %s\n", ret, lua_tostring(ls->L, -1));
	}
}

void lsession_close(lsession_t* ls)
{
	lua_close(ls->L);
	free(ls);
}

static int lsession_sendmsg(lua_State* L)
{
	tcpserver_t* server = (tcpserver_t*)lua_touserdata(L, lua_upvalueindex(1));
	assert(server);

	size_t id = (size_t)luaL_checkinteger(L, 1);
	size_t len = 0;
	const char* msg = luaL_checklstring(L, 2, &len);
	server_sendmsg(server, id, msg, len);
	return 0;
}

static int lsession_closeconn(lua_State* L)
{
	tcpserver_t* server = (tcpserver_t*)lua_touserdata(L, lua_upvalueindex(1));
	assert(server);

	size_t id = (size_t)luaL_checkinteger(L, 1);
	server_closeconn(server, id);
	return 0;
}

static void registerfunction(lua_State* L, const char* name, lua_CFunction func, void* upvalue)
{
	lua_pushlightuserdata(L, upvalue);
	lua_pushcclosure(L, func, 1);
	lua_setglobal(L, name);
}

static int makefunctionref(lua_State* L, const char* name)
{
	lua_getglobal(L, name);
	return luaL_ref(L, LUA_REGISTRYINDEX);
}
