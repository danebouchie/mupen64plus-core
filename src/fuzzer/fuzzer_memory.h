#ifndef FUZZER_MEMORY_H_INCLUDED
#define FUZZER_MEMORY_H_INCLUDED

#include <lua.h>

int luaopen_fuzzermemory(lua_State *L);

#endif