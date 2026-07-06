/******************************************************************************
 Copyright 2017 Allied Telesis Labs Ltd. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.

    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************/

#ifndef INTERFACE_LUAINTERFACE_H_
#define INTERFACE_LUAINTERFACE_H_

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#include "include/buildsys.h"
#include <exception>
#include <string>

/**
 * Adapt a lua_CFunction that may throw a C++ exception into one that reports the
 * error to Lua via lua_error(). The li_* functions are called directly by the
 * Lua VM, whose call frames are C code using setjmp/longjmp; letting a C++
 * exception unwind through them is undefined behaviour and leaves the lua_State
 * corrupt. Routing errors through lua_error() keeps them on Lua's own error
 * path so luaL_dofile/lua_pcall report them cleanly.
 */
template <lua_CFunction fn> int lua_guard(lua_State *L)
{
	try {
		return fn(L);
	} catch(std::exception &e) {
		lua_pushstring(L, e.what());
	} catch(...) {
		lua_pushstring(L, "unknown C++ exception");
	}
	// Raise the pushed message via Lua's own error mechanism. lua_error() is
	// used rather than the variadic luaL_error(), and it is called after the
	// catch blocks so its longjmp does not jump out over the caught exception's
	// destructor.
	return lua_error(L); // does not return
}

#define LUA_SET_TABLE_TYPE(L, T)                                                           \
	lua_pushstring(L, #T);                                                                 \
	lua_pushstring(L, "yes");                                                              \
	lua_settable(L, -3);

#define LUA_ADD_TABLE_FUNC(L, N, F)                                                        \
	lua_pushstring(L, N);                                                                  \
	lua_pushcfunction(L, (lua_guard<F>) );                                                 \
	lua_settable(L, -3);

#define CREATE_TABLE(L, D)                                                                 \
	lua_newtable(L);                                                                       \
	lua_pushstring(L, "data");                                                             \
	lua_pushlightuserdata(L, D);                                                           \
	lua_settable(L, -3);

#define SET_TABLE_TYPE(L, T)                                                               \
	lua_pushstring(L, #T);                                                                 \
	lua_pushstring(L, "yes");                                                              \
	lua_settable(L, -3);

#define LUA_ADD_TABLE_STRING(L, N, S)                                                      \
	lua_pushstring(L, N);                                                                  \
	lua_pushstring(L, S);                                                                  \
	lua_settable(L, -3);

template <typename T>
T *CHECK_ARGUMENT_TYPE(lua_State *L, const std::string &func, int num_args,
                       const std::string &type_str)
{
	if(!lua_istable(L, num_args)) {
		throw CustomException(func + "() requires a " + type_str + " as argument " +
		                      std::to_string(num_args));
	}
	lua_pushstring(L, type_str.c_str());
	lua_gettable(L, num_args);
	if(!lua_isstring(L, -1) || strcmp(lua_tostring(L, -1), "yes") != 0) {
		throw CustomException(func + "() requires an object of type " + type_str +
		                      " as argument " + std::to_string(num_args));
	}
	lua_pop(L, 1);
	lua_pushstring(L, "data");
	lua_gettable(L, num_args);
	if(!lua_islightuserdata(L, -1)) {
		throw CustomException(func + "() requires data of argument " +
		                      std::to_string(num_args) + " to contain something of type " +
		                      type_str);
	}

	auto *V = const_cast<T *>(reinterpret_cast<const T *>(lua_topointer(L, -1))); // NOLINT
	lua_pop(L, 1);

	return V;
}

void li_set_package(Package *p);
Package *li_get_package();
FeatureMap *li_get_feature_map();

#endif // INTERFACE_LUAINTERFACE_H_
