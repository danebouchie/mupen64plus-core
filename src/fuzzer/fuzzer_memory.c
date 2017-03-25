#include <stdio.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "fuzzer\fuzzer_lualib.h"
#include "main\main.h"
#include "device\memory\memory.h"
#include "fuzzer\luaext.h"

typedef uint16_t hword;
typedef uint32_t word;
typedef uint64_t dword;

#define luamem(T, lookup, luaType, luaExplictType) \
static int lua_get##T (lua_State *L) { \
	uint64_t raw; \
	*memory_address() = (uint32_t)luaL_checkinteger(L, 2); \
	g_dev.mem.rdword = &raw; \
	read_##lookup##_in_memory(); \
	lua_push##luaType(L, (luaExplictType) *((T *) &raw)); \
	return 1; \
} \
\
static int lua_set##T (lua_State *L) { \
	T value = (T)luaL_check##luaType(L, 3); \
	*memory_address() = (uint32_t)luaL_checkinteger(L, 2); \
	*memory_w##lookup() = *((lookup *)&value); \
	write_##lookup##_in_memory(); \
	return 0; \
} 

luamem(int8_t, byte, integer, lua_Integer)
luamem(uint8_t, byte, integer, lua_Integer)
luamem(int16_t, hword, integer, lua_Integer)
luamem(uint16_t, hword, integer, lua_Integer)
luamem(int32_t, word, integer, lua_Integer)
luamem(uint32_t, word, integer, lua_Integer)
luamem(int64_t, dword, number, lua_Number)
luamem(uint64_t, dword, number, lua_Number)
luamem(float, word, number, lua_Number)
luamem(double, dword, number, lua_Number)

// TODO: set flag

static const luaL_Reg memoryFuncs[] = {
	{ "setChar", lua_setint8_t },
	{ "setByte", lua_setuint8_t },
	{ "setShort", lua_setint16_t },
	{ "setUShort", lua_setuint16_t },
	{ "setInt", lua_setint32_t },
	{ "setUInt", lua_setuint32_t },
	{ "setLong", lua_setint64_t },
	{ "setULong", lua_setuint64_t },
	{ "setFloat", lua_setfloat },
	{ "setDouble", lua_setdouble },
	{ "getChar", lua_getint8_t },
	{ "getByte", lua_getuint8_t },
	{ "getShort", lua_getint16_t },
	{ "getUShort", lua_getuint16_t },
	{ "getInt", lua_getint32_t },
	{ "getUInt", lua_getuint32_t },
	{ "getLong", lua_getint64_t },
	{ "getULong", lua_getuint64_t },
	{ "getFloat", lua_getfloat },
	{ "getDouble", lua_getdouble },
	{ NULL, NULL }  /* sentinel */
};

int luaopen_fuzzermemory(lua_State *L) {
	luaL_setfuncs(L, memoryFuncs, 0);
	return 1;
}