#ifndef FUZZER_INPUTS_H_INCLUDED
#define FUZZER_INPUTS_H_INCLUDED

#include <lua.h>

extern BUTTONS fuzzerInputs;

int luaopen_fuzzerinputs(lua_State *L);

#endif