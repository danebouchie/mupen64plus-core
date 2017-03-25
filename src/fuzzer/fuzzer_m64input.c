#include <stdio.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "api\m64p_types.h"
#include "api\m64p_plugin.h"
#include "fuzzer\fuzzer_lualib.h"
#include "fuzzer\luaext.h"
#include <math.h>

#define LUA_M64_TABLE_BUFFER_FIELD "_m64InputBuffer"

typedef struct M64File {
	int pos;
	int count;
	uint32_t * inputs;
} M64File;

typedef struct M64List {
	M64File * file;
	struct M64List * next;
	struct M64List * prev;
} M64List;

M64List * m64List = NULL;

void freeM64(M64File * m64) {
	free(m64->inputs);
	free(m64);
}

void freeM64List()
{
	M64List * next;
	while (m64List != NULL) {
		next = m64List->next;
		free(m64List);
		m64List = next;
	}
}

M64List * findM64(M64File * m64) {
	M64List * find = m64List;
	if (m64 == NULL)
		return NULL;

	while (find != NULL) {
		if (m64 == find->file) {
			return find;
		}
	}
	return NULL;
}

void closeM64(M64File * m64) {
	M64List * list = findM64(m64);
	if (list == NULL)
		return;

	freeM64(list->file);
	list->prev->next = list->next;
	list->next->prev = list->prev;
	free(list);
}

M64File * luam64_checkm64(lua_State *L, int n) {
	M64File *m64;
	// Check for table
	if (!lua_istable(L, n))
		return NULL;

	// Get databuffer
	lua_getfield(L, n, LUA_M64_TABLE_BUFFER_FIELD);
	if (!lua_islightuserdata(L, -1)) {
		// databuffer not found
		lua_pop(L, 1);
		return NULL;
	}
	m64 = (M64File *)lua_touserdata(L, -1);
	lua_pop(L, 1);

	return m64;
}

int luam64_close(lua_State *L) {
	M64File * m64 = luam64_checkm64(L, 1);
	closeM64(m64);
	return 0;
}

int luam64_getinputat(lua_State *L) {
	M64File * m64 = luam64_checkm64(L, 1);
	int pos = (int)luaL_checkinteger(L, 2);
	if (m64 == NULL) {
		lua_pushnil(L);
		return 1;
	}
	luaL_argcheck(L, pos <= m64->count, 2, "input position past maximum");
	luaL_argcheck(L, pos >= 0, 2, "input position negative");
	lua_pushnumber(L, m64->inputs[pos]);
	return 1;
}

volatile int luam64_setpos(lua_State *L) {
	M64File * m64 = luam64_checkm64(L, 1);
	if (m64 == NULL) {
		return 0;
	}
	m64->pos = (int)luaL_checkinteger(L, 2);
	if (m64->pos > m64->count)
		m64->pos = m64->count;
	else if (m64->pos < 0)
		m64->pos = 0;
	return 0;
}

int luam64_getpos(lua_State *L) {
	M64File * m64 = luam64_checkm64(L, 1);
	if (m64 == NULL) {
		lua_pushnil(L);
		return 1;
	}
	lua_pushinteger(L, m64->pos);
	return 1;
}

int luam64_getinputcount(lua_State *L) {
	M64File * m64 = luam64_checkm64(L, 1);
	if (m64 == NULL) {
		lua_pushnil(L);
		return 1;
	}
	lua_pushinteger(L, m64->count);
	return 1;
}

int luam64_getfinished(lua_State *L) {
	M64File * m64 = luam64_checkm64(L, 1);
	if (m64 == NULL) {
		lua_pushnil(L);
		return 1;
	}
	lua_pushboolean(L, m64->pos >= m64->count);
	return 1;
}

int luam64_getnextinput(lua_State *L) {
	M64File * m64 = luam64_checkm64(L, 1);
	if (m64 == NULL) {
		lua_pushnil(L);
		return 1;
	}
	if (m64->pos < m64->count) {
		lua_pushinteger(L, m64->inputs[m64->pos]);
		m64->pos++;
	}
	else {
		lua_pushinteger(L, 0x00);
	}

	return 1;
}

static const luaL_Reg m64FileFuncs[] = {
	{ "close", luam64_close },
	{ "getPosition", luam64_getpos },
	{ "setPosition", luam64_setpos },
	{ "isFinished", luam64_getfinished },
	{ "getInputAt", luam64_getinputat },
	{ "getNextInput", luam64_getnextinput },
	{ NULL, NULL }  /* sentinel */
};

int luam64_open(lua_State *L) {
	M64File * m64;
	M64List * newLink;
	char signature[4];
	int version;
	int fLen = 0;
	int readCount = 0;
	int headerSize;

	// Open file
	FILE * f = fopen(luaL_checkstring(L, 2), "rb");
	if (f == NULL) 
		goto luam64_open_fail;

	// Read file length
	fseek(f, 0, SEEK_END);
	fLen = ftell(f);
	rewind(f);

	// Verifiy version and signature
	if (fLen < 8)
		goto luam64_open_fail;
	readCount += fread(signature, sizeof(char), 4, f);
	readCount += fread(&version, sizeof(int), 1, f);
	if (readCount != 5 || memcmp(signature, "M64\x1a", 4) != 0)
		goto luam64_open_fail;
	switch (version) {
		case 1:
		case 2:
			headerSize = 0x200;
			break;
		case 3:
			headerSize = 0x400;
			break;
		default:
			goto luam64_open_fail;
	}

	// Skip header
	fseek(f, headerSize, SEEK_SET);

	// Buffer data
	m64 = (M64File *)malloc(sizeof(M64File));
	m64->inputs = malloc(fLen - headerSize);
	m64->count = (fLen - headerSize) / sizeof(uint32_t);
	m64->pos = 0;
	if (fread(m64->inputs, sizeof(uint32_t), m64->count, f) != m64->count) {
		freeM64(m64);
		goto luam64_open_fail;
	}

	// Add new linklist
	newLink = (M64List *) malloc(sizeof(M64List));
	newLink->file = m64;
	newLink->next = m64List;
	newLink->prev = NULL;
	if (m64List != NULL)
		m64List->prev = newLink;
	m64List = newLink;

	// Push value
	lua_newtable(L);
	luaL_setfuncs(L, m64FileFuncs, 0);
	lua_pushlightuserdata(L, m64);
	lua_setfield(L, -2, LUA_M64_TABLE_BUFFER_FIELD);
	return 1;

luam64_open_fail:
	lua_pushnil(L);
	return 1;
}

int luaclose_fuzzerm64inputs(lua_State * L) {
	// Free all data
	freeM64List();
	return 1;
}