#include <stdio.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "api\m64p_types.h"
#include "api\m64p_plugin.h"
#include "fuzzer\fuzzer_lualib.h"
#include "fuzzer\fuzzer_inputs.h"

lua_State *L = NULL;

m64p_error fuzzer_main_run(const char * luaFileName) {

	// Don't start the fuzzer if there is nothing to do
	if (luaFileName == NULL) {
		printf("No Lua file given. Dettaching Fuzzer\n");
		return M64ERR_SUCCESS;
	}

	// Open lua
	L = luaL_newstate(); 
	luaL_openlibs(L);

	// Open Fuzzer lib
	luaopen_fuzzerlib(L);
	lua_setglobal(L, "Fuzzer");

	printf("Opening Lua file '%s'\n", luaFileName);
	luaL_dofile(L, luaFileName);
	if (lua_status(L) != 0)
	{
		printf("Lua file '%s' failed to open.\n", luaFileName);
		return M64ERR_PLUGIN_FAIL;
	}
	printf("Lua opened successfully\n");

	// Call lua start event
	lua_getglobal(L, "Fuzzer");
	if (!lua_isnil(L, -1)) {
		lua_getfield(L, -1, "start");
		if (!lua_isnil(L, -1)) {
			lua_pushnil(L);
			if (lua_pcall(L, 1, 0, 0) != 0) {
				printf("error running function `Fuzzer:start()': %s\n", lua_tostring(L, -1));
			}
		} 
		else {
			lua_pop(L, 1);
		}
	}
	lua_pop(L, 1);

	return lua_status(L) == 0 ? M64ERR_SUCCESS : M64ERR_PLUGIN_FAIL;
}

void fuzzer_main_stop(void) {
	if (L == NULL)
		return;

	// Call lua stop event
	lua_getglobal(L, "Fuzzer");
	if (!lua_isnil(L, -1)) {
		lua_getfield(L, -1, "stop");
		if (!lua_isnil(L, -1)) {
			lua_pushnil(L);
			if (lua_pcall(L, 1, 0, 0) != 0) {
				printf("error running function `Fuzzer:stop()': %s\n", lua_tostring(L, -1));
			}
		}
		else {
			lua_pop(L, 1);
		}
	}
	lua_pop(L, 1);
	luaclose_fuzzerlib(L);
	lua_close(L);
	L = NULL;
}

void fuzzer_vi() {
	if (L == NULL)
		return;

	// Call lua VI event
	lua_getglobal(L, "Fuzzer");
	int type = lua_type(L, -1);
	if (!lua_isnil(L, -1)) {
		lua_getfield(L, -1, "update");
		if (!lua_isnil(L, -1)) {
			lua_pushnumber(L, 5);
			type = lua_type(L, -1);
			//lua_pushnil(L);
			if (lua_pcall(L, 1, 0, 0) != 0) {
				printf("error running function `Fuzzer:update()': %s\n", lua_tostring(L, -1));
			}
		}
		else {
			lua_pop(L, 1);
		}
	}
	lua_pop(L, 1);
}

void fuzzer_GetKeys(BUTTONS * Keys) {
	if (L == NULL)
	{
		Keys->Value = 0;
		return;
	}

	// Call lua get keys
	lua_getglobal(L, "Fuzzer");
	if (!lua_isnil(L, -1)) {
		lua_getfield(L, -1, "get_inputs");
		if (!lua_isnil(L, -1)) {
			lua_pushnil(L);
			lua_getfield(L, -3, "Inputs");
			if (lua_pcall(L, 2, 0, 0) != 0) {
				printf("error running function `Fuzzer:update()': %s\n", lua_tostring(L, -1));
			}
		}
		else {
			lua_pop(L, 1);
		}
	}
	lua_pop(L, 1);
	Keys->Value = fuzzerInputs.Value;
}