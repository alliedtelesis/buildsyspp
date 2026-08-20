/******************************************************************************
 Copyright 2020 Allied Telesis Labs Ltd. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice,
       this list of conditions and the following disclaimer.

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

extern "C" {
#include <lauxlib.h>
#include <lualib.h>
}

#include "exceptions.hpp"
#include "lua.hpp"
#include <cstring>
#include <string>

#if LUA_VERSION_NUM < 502
#error "buildsys++ requires Lua 5.2 or newer (luaL_traceback)"
#endif

using namespace buildsys;

Lua::Lua()
{
	this->state = luaL_newstate();
	if(this->state == nullptr) {
		throw CustomException("Lua Error");
	}
	luaL_openlibs(this->state);
}

Lua::~Lua()
{
	lua_close(state);
	this->state = nullptr;
}

/**
 * Message handler for lua_pcall(). This runs before the erroring stack is
 * unwound, which is the only point at which a lua backtrace can still be
 * captured. Errors raised by the li_* bindings arrive here as plain strings.
 *
 * Must hold no non-trivially-destructible C++ locals: the lua API calls it
 * makes can raise a memory error, which longjmps.
 */
static int lua_msg_handler(lua_State *L)
{
	const char *msg = lua_tostring(L, 1);
	if(msg == nullptr) {
		// A non-string error object, e.g. a recipe calling error({}).
		std::string obj_msg =
		    std::string("(error object is a ") + luaL_typename(L, 1) + " value)";
		lua_pushstring(L, obj_msg.c_str());
		msg = lua_tostring(L, -1);
	}
	if(std::strstr(msg, "\nstack traceback:") != nullptr) {
		// A nested require() already added one. Both run on this same
		// lua_State, so we prefer that message.
		lua_pushstring(L, msg);
		return 1;
	}
	luaL_traceback(L, L, msg, 1);
	return 1;
}

/**
 * Load and execute a lua file in this instance
 *
 * @param filename - The name of the lua file to load and run
 * @return the number of return values from the function
 */
int Lua::processFile(const std::string &filename)
{
	int start_sp = lua_gettop(state);

	// The handler has to sit below the chunk so that its (absolute) stack
	// index is still valid once the chunk and its results are pushed.
	lua_pushcfunction(state, lua_msg_handler);
	int msgh = lua_gettop(state);

	if(luaL_loadfile(state, filename.c_str()) != 0) {
		// Build the message before resetting the stack; the const char * that
		// lua_tostring() returns dies with the value it points at.
		std::string err = lua_tostring(state, -1);
		lua_settop(state, start_sp);
		throw CustomException(err);
	}

	if(lua_pcall(state, 0, LUA_MULTRET, msgh) != 0) {
		std::string err = lua_tostring(state, -1);
		lua_settop(state, start_sp);
		throw CustomException(err);
	}

	// Drop the handler from underneath the results: li_require() returns them
	// as its own return values, so a stray function here would misalign them.
	lua_remove(state, msgh);

	return lua_gettop(state) - start_sp;
}

/**
 * Register a function in this lua instance
 *
 * @param name - The name of the function
 * @param fn - The function to call
 */
void Lua::registerFunc(const std::string &name, lua_CFunction fn)
{
	lua_register(state, name.c_str(), fn);
}
