#ifndef FUZZER_H_INCLUDED
#define FUZZER_H_INCLUDED

#include "api\m64p_types.h"
#include "api\m64p_plugin.h"

m64p_error fuzzer_main_run(const char * luaFileName);
void fuzzer_main_stop(void);

void fuzzer_GetKeys(BUTTONS * Keys);
void fuzzer_vi();

#endif