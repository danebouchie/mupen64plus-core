#ifndef FUZZER_M64_INPUTS_H_INCLUDED
#define FUZZER_M64_INPUTS_H_INCLUDED

#include <lua.h>

int luam64_open(lua_State *L);
int luaclose_fuzzerm64inputs(lua_State *L);

#endif