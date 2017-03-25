#include <stdio.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "fuzzer\luaext.h"
#include "api\m64p_plugin.h"
#include <math.h>

BUTTONS fuzzerInputs = { 0 };

#define luainputs_boolfield(name, button) \
static int luainputs_set##name (lua_State * L) { \
    fuzzerInputs.##button = 1; \
	return 0; \
} \
\
static int luainputs_clear##name (lua_State * L) { \
    fuzzerInputs.##button = 0; \
	return 0; \
} \
\
static int luainputs_toggle##name(lua_State * L) { \
    fuzzerInputs.##button = !fuzzerInputs.##button; \
	return 0; \
} \
\
static int luainputs_get##name(lua_State * L) { \
	lua_pushboolean(L, fuzzerInputs.##button); \
	return 1;  \
}

#define luainputs_axis(name, axis) \
static int luainputs_set##name(lua_State * L) { \
	fuzzerInputs.##axis = (int)luaL_checkinteger(L, 2); \
	return 0; \
} \
\
static int luainputs_get##name(lua_State * L) { \
	lua_pushinteger(L, fuzzerInputs.##axis); \
	return 1; \
}

luainputs_boolfield(A, A_BUTTON)
luainputs_boolfield(B, B_BUTTON)
luainputs_boolfield(Start, START_BUTTON)
luainputs_boolfield(Z, Z_TRIG)
luainputs_boolfield(R, R_TRIG)
luainputs_boolfield(L, L_TRIG)
luainputs_boolfield(C_up, U_CBUTTON)
luainputs_boolfield(C_down, D_CBUTTON)
luainputs_boolfield(C_left, L_CBUTTON)
luainputs_boolfield(C_right, R_CBUTTON)
luainputs_boolfield(D_up, U_DPAD)
luainputs_boolfield(D_down, D_DPAD)
luainputs_boolfield(D_left, L_DPAD)
luainputs_boolfield(D_right, R_DPAD)

luainputs_axis(X, X_AXIS)
luainputs_axis(Y, Y_AXIS)

static int luainputs_setanalog(lua_State * L) {
	double magnitude = luaL_checknumber(L, 2);
	double direction = luaL_checknumber(L, 3);
	double x = cos(direction) * magnitude;
	double y = sin(direction) * magnitude;
	if (x > CHAR_MAX)
		x = CHAR_MAX;
	if (x < CHAR_MIN)
		x = CHAR_MIN; 
	if (y > CHAR_MAX)
		y = CHAR_MAX;
	if (y < CHAR_MIN)
		y = CHAR_MIN;
	fuzzerInputs.X_AXIS = (char)x;
	fuzzerInputs.Y_AXIS = (char)y;
	return 0;
}

static int luainputs_getdirection(lua_State * L) {
	double x = fuzzerInputs.X_AXIS;
	double y = fuzzerInputs.Y_AXIS;
	lua_pushnumber(L, atan2(y, x));
	return 1;
}

static int luainputs_getmagnitude(lua_State * L) {
	double x = fuzzerInputs.X_AXIS;
	double y = fuzzerInputs.Y_AXIS;
	lua_pushnumber(L, sqrt(x * x + y * y));
	return 1;
}

static int luainputs_setraw(lua_State * L) {
	fuzzerInputs.Value = luaL_checkinteger(L, 2);
	return 0;
}

static int luainputs_getraw(lua_State * L) {
	lua_pushinteger(L, fuzzerInputs.Value);
	return 1;
}

static const luaL_Reg inputFuncs[] = {
	{ "setA", luainputs_setA },
	{ "setB", luainputs_setB },
	{ "setStart", luainputs_setStart },
	{ "setZ", luainputs_setZ },
	{ "setL", luainputs_setL },
	{ "setR", luainputs_setR },
	{ "setC_up", luainputs_setC_up },
	{ "setC_down", luainputs_setC_down },
	{ "setC_left", luainputs_setC_left },
	{ "setC_right", luainputs_setC_right },
	{ "setD_up", luainputs_setD_up },
	{ "setD_down", luainputs_setD_down },
	{ "setD_left", luainputs_setD_left },
	{ "setD_right", luainputs_setD_right },
	{ "setAnalog", luainputs_setanalog },
	{ "clearA", luainputs_clearA },
	{ "clearB", luainputs_clearB },
	{ "clearStart", luainputs_clearStart },
	{ "clearZ", luainputs_clearZ },
	{ "clearL", luainputs_clearL },
	{ "clearR", luainputs_clearR },
	{ "clearC_up", luainputs_clearC_up },
	{ "clearC_down", luainputs_clearC_down },
	{ "clearC_left", luainputs_clearC_left },
	{ "clearC_right", luainputs_clearC_right },
	{ "clearD_up", luainputs_clearD_up },
	{ "clearD_down", luainputs_clearD_down },
	{ "clearD_left", luainputs_clearD_left },
	{ "clearD_right", luainputs_clearD_right },
	{ "toggleA", luainputs_toggleA },
	{ "toggleB", luainputs_toggleB },
	{ "toggleStart", luainputs_toggleStart },
	{ "toggleZ", luainputs_toggleZ },
	{ "toggleL", luainputs_toggleL },
	{ "toggleR", luainputs_toggleR },
	{ "toggleC_up", luainputs_toggleC_up },
	{ "toggleC_down", luainputs_toggleC_down },
	{ "toggleC_left", luainputs_toggleC_left },
	{ "toggleC_right", luainputs_toggleC_right },
	{ "toggleD_up", luainputs_toggleD_up },
	{ "toggleD_down", luainputs_toggleD_down },
	{ "toggleD_left", luainputs_toggleD_left },
	{ "toggleD_right", luainputs_toggleD_right },
	{ "getA", luainputs_getA },
	{ "getB", luainputs_getB },
	{ "getStart", luainputs_getStart },
	{ "getZ", luainputs_getZ },
	{ "getL", luainputs_getL },
	{ "getR", luainputs_getR },
	{ "getC_up", luainputs_getC_up },
	{ "getC_down", luainputs_getC_down },
	{ "getC_left", luainputs_getC_left },
	{ "getC_right", luainputs_getC_right },
	{ "getD_up", luainputs_getD_up },
	{ "getD_down", luainputs_getD_down },
	{ "getD_left", luainputs_getD_left },
	{ "getD_right", luainputs_getD_right },
	{ "getAnalog", luainputs_setanalog},
	{ "getAnalogDir", luainputs_getdirection },
	{ "getAnalogMag", luainputs_getmagnitude },
	{ "setAnalogX", luainputs_setX },
	{ "getAnalogX", luainputs_getX },
	{ "setAnalogY", luainputs_setY },
	{ "getAnalogY", luainputs_getY },
	{ "setRaw", luainputs_setraw },
	{ "getRaw", luainputs_getraw },
	{ NULL, NULL }  /* sentinel */
};

int luaopen_fuzzerinputs(lua_State *L) {
	luaL_newlib(L, inputFuncs);
	return 1;
}