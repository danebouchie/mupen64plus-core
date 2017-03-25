#ifndef FUZZER_LUALIB_H_INCLUDED
#define FUZZER_LUALIB_H_INCLUDED

#include <lua.h>
#include "fuzzer\fuzzer.h"

int luaopen_fuzzerlib(lua_State *L);
int luaclose_fuzzerlib(lua_State *L);

#endif