#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "fuzzer\fuzzer.h"
#include "main\main.h"
#include "main\savestates.h"
#include "fuzzer\fuzzer_inputs.h"
#include "fuzzer\fuzzer_memory.h"
#include "fuzzer\fuzzer_m64input.h"
#include <fuzzer\luaext.h>

static int lua_loadstate(lua_State *L) {
	const char * fileName;
	fileName = luaL_checkstring(L, 2);
	main_state_load(fileName);
	return 0;
}

static int lua_savestate(lua_State *L) {
	const char * fileName = luaL_checkstring(L, 2);
	main_state_save(savestates_type_m64p, fileName);
	return 0;
}

static const luaL_Reg mylib[] = {
	{ "saveState", lua_savestate },
	{ "loadState", lua_loadstate },
	{ "openM64", luam64_open },
	{ NULL, NULL }  /* sentinel */
};

int luaopen_fuzzerlib(lua_State *L) {

	// Initiate lua fuzzer table
	luaL_newlib(L, mylib);
	luaopen_fuzzerinputs(L);
	lua_setfield(L, -2, "Inputs");
	luaopen_fuzzermemory(L);
	return 1;
}

int luaclose_fuzzerlib(lua_State *L) {
	luaclose_fuzzerm64inputs(L);
	return 1;
}